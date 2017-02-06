// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/gpu_process_transport_factory.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/base/histograms.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/output/vulkan_in_process_context_provider.h"
#include "cc/raster/single_thread_task_graph_runner.h"
#include "cc/raster/task_graph_runner.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/display_scheduler.h"
#include "cc/surfaces/surface_display_output_surface.h"
#include "cc/surfaces/surface_manager.h"
#include "components/display_compositor/compositor_overlay_candidate_validator.h"
#include "components/display_compositor/gl_helper.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "content/browser/compositor/gpu_browser_compositor_output_surface.h"
#include "content/browser/compositor/gpu_surfaceless_browser_compositor_output_surface.h"
#include "content/browser/compositor/offscreen_browser_compositor_output_surface.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/browser/compositor/software_browser_compositor_output_surface.h"
#include "content/browser/compositor/software_output_device_mus.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu_process_launch_causes.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "content/public/common/content_switches.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_constants.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"

#if defined(MOJO_RUNNER_CLIENT)
#include "services/shell/runner/common/client_util.h"
#endif

#if defined(OS_WIN)
#include "content/browser/compositor/software_output_device_win.h"
#include "ui/gfx/win/rendering_window_manager.h"
#elif defined(USE_OZONE)
#include "components/display_compositor/compositor_overlay_candidate_validator_ozone.h"
#include "content/browser/compositor/software_output_device_ozone.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/overlay_manager_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#elif defined(USE_X11)
#include "content/browser/compositor/software_output_device_x11.h"
#elif defined(OS_MACOSX)
#include "components/display_compositor/compositor_overlay_candidate_validator_mac.h"
#include "content/browser/compositor/gpu_output_surface_mac.h"
#include "content/browser/compositor/software_output_device_mac.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/base/ui_base_switches.h"
#elif defined(OS_ANDROID)
#include "components/display_compositor/compositor_overlay_candidate_validator_android.h"
#endif
#if !defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
#include "content/browser/gpu/gpu_surface_tracker.h"
#endif

#if defined(ENABLE_VULKAN)
#include "content/browser/compositor/vulkan_browser_compositor_output_surface.h"
#endif

using cc::ContextProvider;
using gpu::gles2::GLES2Interface;

namespace {

const int kNumRetriesBeforeSoftwareFallback = 4;

scoped_refptr<content::ContextProviderCommandBuffer> CreateContextCommon(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    gpu::SurfaceHandle surface_handle,
    bool support_locking,
    content::ContextProviderCommandBuffer* shared_context_provider,
    content::command_buffer_metrics::ContextType type) {
  DCHECK(
      content::GpuDataManagerImpl::GetInstance()->CanUseGpuBrowserCompositor());
  DCHECK(gpu_channel_host);

  // This is called from a few places to create different contexts:
  // - The shared main thread context (offscreen).
  // - The compositor context, which is used by the browser compositor
  //   (offscreen) for synchronization mostly, and by the display compositor
  //   (onscreen) for actual GL drawing.
  // - The compositor worker context (offscreen) used for GPU raster.
  // So ask for capabilities needed by any of these cases (we can optimize by
  // branching on |surface_handle| being null if these needs diverge).
  //
  // The default framebuffer for an offscreen context is not used, so it does
  // not need alpha, stencil, depth, antialiasing. The display compositor does
  // not use these things either, so we can request nothing here.
  gpu::gles2::ContextCreationAttribHelper attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;

  constexpr bool automatic_flushes = false;

  GURL url("chrome://gpu/GpuProcessTransportFactory::CreateContextCommon");
  return make_scoped_refptr(new content::ContextProviderCommandBuffer(
      std::move(gpu_channel_host), gpu::GPU_STREAM_DEFAULT,
      gpu::GpuStreamPriority::NORMAL, surface_handle, url, automatic_flushes,
      support_locking, gpu::SharedMemoryLimits(), attributes,
      shared_context_provider, type));
}

#if defined(OS_MACOSX)
bool IsCALayersDisabledFromCommandLine() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kDisableMacOverlays);
}
#endif

}  // namespace

namespace content {

struct GpuProcessTransportFactory::PerCompositorData {
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  BrowserCompositorOutputSurface* display_output_surface = nullptr;
  cc::SyntheticBeginFrameSource* begin_frame_source = nullptr;
  ReflectorImpl* reflector = nullptr;
  std::unique_ptr<cc::Display> display;
  bool output_is_secure = false;
};

GpuProcessTransportFactory::GpuProcessTransportFactory()
    : next_surface_id_namespace_(1u),
      task_graph_runner_(new cc::SingleThreadTaskGraphRunner),
      callback_factory_(this) {
  cc::SetClientNameForMetrics("Browser");

  surface_manager_ = base::WrapUnique(new cc::SurfaceManager);

  task_graph_runner_->Start("CompositorTileWorker1",
                            base::SimpleThread::Options());
#if defined(OS_WIN)
  software_backing_.reset(new OutputDeviceBacking);
#endif
}

GpuProcessTransportFactory::~GpuProcessTransportFactory() {
  DCHECK(per_compositor_data_.empty());

  // Make sure the lost context callback doesn't try to run during destruction.
  callback_factory_.InvalidateWeakPtrs();

  task_graph_runner_->Shutdown();
}

std::unique_ptr<cc::SoftwareOutputDevice>
GpuProcessTransportFactory::CreateSoftwareOutputDevice(
    ui::Compositor* compositor) {
#if defined(MOJO_RUNNER_CLIENT)
  if (shell::ShellIsRemote()) {
    return std::unique_ptr<cc::SoftwareOutputDevice>(
        new SoftwareOutputDeviceMus(compositor));
  }
#endif

#if defined(OS_WIN)
  return std::unique_ptr<cc::SoftwareOutputDevice>(
      new SoftwareOutputDeviceWin(software_backing_.get(), compositor));
#elif defined(USE_OZONE)
  return SoftwareOutputDeviceOzone::Create(compositor);
#elif defined(USE_X11)
  return std::unique_ptr<cc::SoftwareOutputDevice>(
      new SoftwareOutputDeviceX11(compositor));
#elif defined(OS_MACOSX)
  return std::unique_ptr<cc::SoftwareOutputDevice>(
      new SoftwareOutputDeviceMac(compositor));
#else
  NOTREACHED();
  return std::unique_ptr<cc::SoftwareOutputDevice>();
#endif
}

std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
CreateOverlayCandidateValidator(gfx::AcceleratedWidget widget) {
  std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
      validator;
#if defined(USE_OZONE)
  std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates =
      ui::OzonePlatform::GetInstance()
          ->GetOverlayManager()
          ->CreateOverlayCandidates(widget);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (overlay_candidates &&
      (command_line->HasSwitch(switches::kEnableHardwareOverlays) ||
       command_line->HasSwitch(switches::kOzoneTestSingleOverlaySupport))) {
    validator.reset(
        new display_compositor::CompositorOverlayCandidateValidatorOzone(
            std::move(overlay_candidates)));
  }
#elif defined(OS_MACOSX)
  // Overlays are only supported through the remote layer API.
  if (ui::RemoteLayerAPISupported()) {
    static bool overlays_disabled_at_command_line =
        IsCALayersDisabledFromCommandLine();
    const bool ca_layers_disabled =
        overlays_disabled_at_command_line ||
        GpuDataManagerImpl::GetInstance()->IsDriverBugWorkaroundActive(
            gpu::DISABLE_OVERLAY_CA_LAYERS);
    validator.reset(
        new display_compositor::CompositorOverlayCandidateValidatorMac(
            ca_layers_disabled));
  }
#elif defined(OS_ANDROID)
  validator.reset(
      new display_compositor::CompositorOverlayCandidateValidatorAndroid());
#endif

  return validator;
}

static bool ShouldCreateGpuOutputSurface(ui::Compositor* compositor) {
#if defined(MOJO_RUNNER_CLIENT)
  // Chrome running as a mojo app currently can only use software compositing.
  // TODO(rjkroege): http://crbug.com/548451
  if (shell::ShellIsRemote()) {
    return false;
  }
#endif

#if defined(OS_CHROMEOS)
  // Software fallback does not happen on Chrome OS.
  return true;
#endif

#if defined(OS_WIN)
  if (::GetProp(compositor->widget(), kForceSoftwareCompositor) &&
      ::RemoveProp(compositor->widget(), kForceSoftwareCompositor))
    return false;
#endif

  return GpuDataManagerImpl::GetInstance()->CanUseGpuBrowserCompositor();
}

void GpuProcessTransportFactory::CreateOutputSurface(
    base::WeakPtr<ui::Compositor> compositor) {
  DCHECK(!!compositor);
  PerCompositorData* data = per_compositor_data_[compositor.get()];
  if (!data) {
    data = CreatePerCompositorData(compositor.get());
  } else {
    // TODO(danakj): We can destroy the |data->display| here when the compositor
    // destroys its OutputSurface before calling back here.
    data->display_output_surface = nullptr;
    data->begin_frame_source = nullptr;
  }

#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->UnregisterParent(
      compositor->widget());
#endif

  const bool use_vulkan = static_cast<bool>(SharedVulkanContextProvider());

  const bool create_gpu_output_surface =
      ShouldCreateGpuOutputSurface(compositor.get());
  if (create_gpu_output_surface && !use_vulkan) {
    BrowserGpuChannelHostFactory::instance()->EstablishGpuChannel(
        CAUSE_FOR_GPU_LAUNCH_SHARED_WORKER_THREAD_CONTEXT,
        base::Bind(&GpuProcessTransportFactory::EstablishedGpuChannel,
                   callback_factory_.GetWeakPtr(), compositor,
                   create_gpu_output_surface, 0));
  } else {
    EstablishedGpuChannel(compositor, create_gpu_output_surface, 0);
  }
}

void GpuProcessTransportFactory::EstablishedGpuChannel(
    base::WeakPtr<ui::Compositor> compositor,
    bool create_gpu_output_surface,
    int num_attempts) {
  if (!compositor)
    return;

  // The widget might have been released in the meantime.
  PerCompositorDataMap::iterator it =
      per_compositor_data_.find(compositor.get());
  if (it == per_compositor_data_.end())
    return;

  PerCompositorData* data = it->second;
  DCHECK(data);

  if (num_attempts > kNumRetriesBeforeSoftwareFallback) {
#if defined(OS_CHROMEOS)
    LOG(FATAL) << "Unable to create a UI graphics context, and cannot use "
               << "software compositing on ChromeOS.";
#endif
    create_gpu_output_surface = false;
  }

#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->RegisterParent(
      compositor->widget());
#endif

  scoped_refptr<cc::VulkanInProcessContextProvider> vulkan_context_provider =
      SharedVulkanContextProvider();

  scoped_refptr<ContextProviderCommandBuffer> context_provider;
  if (create_gpu_output_surface && !vulkan_context_provider) {
    // Try to reuse existing worker context provider.
    if (shared_worker_context_provider_) {
      bool lost;
      {
        // Note: If context is lost, we delete reference after releasing the
        // lock.
        base::AutoLock lock(*shared_worker_context_provider_->GetLock());
        lost = shared_worker_context_provider_->ContextGL()
                   ->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
      }
      if (lost)
        shared_worker_context_provider_ = nullptr;
    }

    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host;
    if (GpuDataManagerImpl::GetInstance()->CanUseGpuBrowserCompositor()) {
      // We attempted to do EstablishGpuChannel already, so we just use
      // GetGpuChannel() instead of EstablishGpuChannelSync().
      gpu_channel_host =
          BrowserGpuChannelHostFactory::instance()->GetGpuChannel();
    }

    if (!gpu_channel_host) {
      shared_worker_context_provider_ = nullptr;
    } else {
      if (!shared_worker_context_provider_) {
        const bool support_locking = true;
        shared_worker_context_provider_ = CreateContextCommon(
            gpu_channel_host, gpu::kNullSurfaceHandle, support_locking, nullptr,
            command_buffer_metrics::BROWSER_WORKER_CONTEXT);
        // TODO(vadimt): Remove ScopedTracker below once crbug.com/125248 is
        // fixed. Tracking time in BindToCurrentThread.
        tracked_objects::ScopedTracker tracking_profile(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "125248"
                " GpuProcessTransportFactory::EstablishedGpuChannel"
                "::Worker"));
        if (!shared_worker_context_provider_->BindToCurrentThread())
          shared_worker_context_provider_ = nullptr;
      }

      // The |context_provider| is used for both the browser compositor and the
      // display compositor. It shares resources with the worker context, so if
      // we failed to make a worker context, just start over and try again.
      if (shared_worker_context_provider_) {
        bool support_locking = false;
        context_provider = CreateContextCommon(
            std::move(gpu_channel_host), data->surface_handle, support_locking,
            shared_worker_context_provider_.get(),
            command_buffer_metrics::DISPLAY_COMPOSITOR_ONSCREEN_CONTEXT);
        // TODO(vadimt): Remove ScopedTracker below once crbug.com/125248 is
        // fixed. Tracking time in BindToCurrentThread.
        tracked_objects::ScopedTracker tracking_profile(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "125248"
                " GpuProcessTransportFactory::EstablishedGpuChannel"
                "::Compositor"));
#if defined(OS_MACOSX)
        // On Mac, GpuCommandBufferMsg_SwapBuffersCompleted must be handled in
        // a nested message loop during resize.
        context_provider->SetDefaultTaskRunner(
            ui::WindowResizeHelperMac::Get()->task_runner());
#endif
        if (!context_provider->BindToCurrentThread())
          context_provider = nullptr;
      }
    }

    bool created_gpu_browser_compositor =
        !!context_provider && !!shared_worker_context_provider_;

    UMA_HISTOGRAM_BOOLEAN("Aura.CreatedGpuBrowserCompositor",
                          created_gpu_browser_compositor);

    if (!created_gpu_browser_compositor) {
      // Try again.
      BrowserGpuChannelHostFactory::instance()->EstablishGpuChannel(
          CAUSE_FOR_GPU_LAUNCH_SHARED_WORKER_THREAD_CONTEXT,
          base::Bind(&GpuProcessTransportFactory::EstablishedGpuChannel,
                     callback_factory_.GetWeakPtr(), compositor,
                     create_gpu_output_surface, num_attempts + 1));
      return;
    }
  }

  std::unique_ptr<cc::SyntheticBeginFrameSource> begin_frame_source;
  if (!compositor->GetRendererSettings().disable_display_vsync) {
    begin_frame_source.reset(new cc::DelayBasedBeginFrameSource(
        base::MakeUnique<cc::DelayBasedTimeSource>(
            compositor->task_runner().get())));
  } else {
    begin_frame_source.reset(new cc::BackToBackBeginFrameSource(
        base::MakeUnique<cc::DelayBasedTimeSource>(
            compositor->task_runner().get())));
  }

  std::unique_ptr<BrowserCompositorOutputSurface> display_output_surface;
#if defined(ENABLE_VULKAN)
  std::unique_ptr<VulkanBrowserCompositorOutputSurface> vulkan_surface;
  if (vulkan_context_provider) {
    vulkan_surface.reset(new VulkanBrowserCompositorOutputSurface(
        vulkan_context_provider, compositor->vsync_manager(),
        compositor->task_runner().get()));
    if (!vulkan_surface->Initialize(compositor.get()->widget())) {
      vulkan_surface->Destroy();
      vulkan_surface.reset();
    } else {
      display_output_surface = std::move(vulkan_surface);
    }
  }
#endif

  if (!display_output_surface) {
    if (!create_gpu_output_surface) {
      display_output_surface =
          base::WrapUnique(new SoftwareBrowserCompositorOutputSurface(
              CreateSoftwareOutputDevice(compositor.get()),
              compositor->vsync_manager(), begin_frame_source.get()));
    } else {
      DCHECK(context_provider);
      const auto& capabilities = context_provider->ContextCapabilities();
      if (data->surface_handle == gpu::kNullSurfaceHandle) {
        display_output_surface =
            base::WrapUnique(new OffscreenBrowserCompositorOutputSurface(
                context_provider, compositor->vsync_manager(),
                begin_frame_source.get(),
                std::unique_ptr<display_compositor::
                                    CompositorOverlayCandidateValidator>()));
      } else if (capabilities.surfaceless) {
#if defined(OS_MACOSX)
        display_output_surface = base::WrapUnique(new GpuOutputSurfaceMac(
            context_provider, data->surface_handle, compositor->vsync_manager(),
            begin_frame_source.get(),
            CreateOverlayCandidateValidator(compositor->widget()),
            BrowserGpuMemoryBufferManager::current()));
#else
        display_output_surface =
            base::WrapUnique(new GpuSurfacelessBrowserCompositorOutputSurface(
                context_provider, data->surface_handle,
                compositor->vsync_manager(), begin_frame_source.get(),
                CreateOverlayCandidateValidator(compositor->widget()),
                GL_TEXTURE_2D, GL_RGB,
                BrowserGpuMemoryBufferManager::current()));
#endif
      } else {
        std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
            validator;
#if !defined(OS_MACOSX)
        // Overlays are only supported on surfaceless output surfaces on Mac.
        validator = CreateOverlayCandidateValidator(compositor->widget());
#endif
        display_output_surface =
            base::WrapUnique(new GpuBrowserCompositorOutputSurface(
                context_provider, compositor->vsync_manager(),
                begin_frame_source.get(), std::move(validator)));
      }
    }
  }

  data->display_output_surface = display_output_surface.get();
  data->begin_frame_source = begin_frame_source.get();
  if (data->reflector)
    data->reflector->OnSourceSurfaceReady(data->display_output_surface);

#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->DoSetParentOnChild(
      compositor->widget());
#endif

  std::unique_ptr<cc::DisplayScheduler> scheduler(new cc::DisplayScheduler(
      begin_frame_source.get(), compositor->task_runner().get(),
      display_output_surface->capabilities().max_frames_pending));

  // The Display owns and uses the |display_output_surface| created above.
  data->display = base::MakeUnique<cc::Display>(
      surface_manager_.get(), HostSharedBitmapManager::current(),
      BrowserGpuMemoryBufferManager::current(),
      compositor->GetRendererSettings(),
      compositor->surface_id_allocator()->id_namespace(),
      std::move(begin_frame_source), std::move(display_output_surface),
      std::move(scheduler), base::MakeUnique<cc::TextureMailboxDeleter>(
                                compositor->task_runner().get()));

  // The |delegated_output_surface| is given back to the compositor, it
  // delegates to the Display as its root surface. Importantly, it shares the
  // same ContextProvider as the Display's output surface.
  std::unique_ptr<cc::SurfaceDisplayOutputSurface> delegated_output_surface(
      vulkan_context_provider
          ? new cc::SurfaceDisplayOutputSurface(
                surface_manager_.get(), compositor->surface_id_allocator(),
                data->display.get(),
                static_cast<scoped_refptr<cc::VulkanContextProvider>>(
                    vulkan_context_provider))
          : new cc::SurfaceDisplayOutputSurface(
                surface_manager_.get(), compositor->surface_id_allocator(),
                data->display.get(), context_provider,
                shared_worker_context_provider_));
  data->display->Resize(compositor->size());
  data->display->SetOutputIsSecure(data->output_is_secure);
  compositor->SetOutputSurface(std::move(delegated_output_surface));
}

std::unique_ptr<ui::Reflector> GpuProcessTransportFactory::CreateReflector(
    ui::Compositor* source_compositor,
    ui::Layer* target_layer) {
  PerCompositorData* source_data = per_compositor_data_[source_compositor];
  DCHECK(source_data);

  std::unique_ptr<ReflectorImpl> reflector(
      new ReflectorImpl(source_compositor, target_layer));
  source_data->reflector = reflector.get();
  if (auto* source_surface = source_data->display_output_surface)
    reflector->OnSourceSurfaceReady(source_surface);
  return std::move(reflector);
}

void GpuProcessTransportFactory::RemoveReflector(ui::Reflector* reflector) {
  ReflectorImpl* reflector_impl = static_cast<ReflectorImpl*>(reflector);
  PerCompositorData* data =
      per_compositor_data_[reflector_impl->mirrored_compositor()];
  DCHECK(data);
  data->reflector->Shutdown();
  data->reflector = nullptr;
}

void GpuProcessTransportFactory::RemoveCompositor(ui::Compositor* compositor) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second;
  DCHECK(data);
#if !defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
  if (data->surface_handle)
    GpuSurfaceTracker::Get()->RemoveSurface(data->surface_handle);
#endif
  delete data;
  per_compositor_data_.erase(it);
  if (per_compositor_data_.empty()) {
    // Destroying the GLHelper may cause some async actions to be cancelled,
    // causing things to request a new GLHelper. Due to crbug.com/176091 the
    // GLHelper created in this case would be lost/leaked if we just reset()
    // on the |gl_helper_| variable directly. So instead we call reset() on a
    // local std::unique_ptr.
    std::unique_ptr<display_compositor::GLHelper> helper =
        std::move(gl_helper_);

    // If there are any observer left at this point, make sure they clean up
    // before we destroy the GLHelper.
    FOR_EACH_OBSERVER(ui::ContextFactoryObserver, observer_list_,
                      OnLostResources());

    helper.reset();
    DCHECK(!gl_helper_) << "Destroying the GLHelper should not cause a new "
                           "GLHelper to be created.";
  }
#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->UnregisterParent(
      compositor->widget());
#endif
}

bool GpuProcessTransportFactory::DoesCreateTestContexts() { return false; }

uint32_t GpuProcessTransportFactory::GetImageTextureTarget(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return BrowserGpuMemoryBufferManager::GetImageTextureTarget(format, usage);
}

cc::SharedBitmapManager* GpuProcessTransportFactory::GetSharedBitmapManager() {
  return HostSharedBitmapManager::current();
}

gpu::GpuMemoryBufferManager*
GpuProcessTransportFactory::GetGpuMemoryBufferManager() {
  return BrowserGpuMemoryBufferManager::current();
}

cc::TaskGraphRunner* GpuProcessTransportFactory::GetTaskGraphRunner() {
  return task_graph_runner_.get();
}

ui::ContextFactory* GpuProcessTransportFactory::GetContextFactory() {
  return this;
}

std::unique_ptr<cc::SurfaceIdAllocator>
GpuProcessTransportFactory::CreateSurfaceIdAllocator() {
  std::unique_ptr<cc::SurfaceIdAllocator> allocator = base::WrapUnique(
      new cc::SurfaceIdAllocator(next_surface_id_namespace_++));
  if (GetSurfaceManager())
    allocator->RegisterSurfaceIdNamespace(GetSurfaceManager());
  return allocator;
}

void GpuProcessTransportFactory::ResizeDisplay(ui::Compositor* compositor,
                                               const gfx::Size& size) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second;
  DCHECK(data);
  if (data->display)
    data->display->Resize(size);
}

void GpuProcessTransportFactory::SetDisplayColorSpace(
    ui::Compositor* compositor,
    const gfx::ColorSpace& color_space) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second;
  DCHECK(data);
  if (data->display)
    data->display->SetColorSpace(color_space);
}

void GpuProcessTransportFactory::SetAuthoritativeVSyncInterval(
    ui::Compositor* compositor,
    base::TimeDelta interval) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second;
  DCHECK(data);
  if (data->begin_frame_source)
    data->begin_frame_source->SetAuthoritativeVSyncInterval(interval);
}

void GpuProcessTransportFactory::SetOutputIsSecure(ui::Compositor* compositor,
                                                   bool secure) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second;
  DCHECK(data);
  data->output_is_secure = secure;
  if (data->display)
    data->display->SetOutputIsSecure(secure);
}

void GpuProcessTransportFactory::AddObserver(
    ui::ContextFactoryObserver* observer) {
  observer_list_.AddObserver(observer);
}

void GpuProcessTransportFactory::RemoveObserver(
    ui::ContextFactoryObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

cc::SurfaceManager* GpuProcessTransportFactory::GetSurfaceManager() {
  return surface_manager_.get();
}

display_compositor::GLHelper* GpuProcessTransportFactory::GetGLHelper() {
  if (!gl_helper_ && !per_compositor_data_.empty()) {
    scoped_refptr<cc::ContextProvider> provider =
        SharedMainThreadContextProvider();
    if (provider.get())
      gl_helper_.reset(new display_compositor::GLHelper(
          provider->ContextGL(), provider->ContextSupport()));
  }
  return gl_helper_.get();
}

#if defined(OS_MACOSX)
void GpuProcessTransportFactory::SetCompositorSuspendedForRecycle(
    ui::Compositor* compositor,
    bool suspended) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second;
  DCHECK(data);
  if (data->display_output_surface)
    data->display_output_surface->SetSurfaceSuspendedForRecycle(suspended);
}
#endif

scoped_refptr<cc::ContextProvider>
GpuProcessTransportFactory::SharedMainThreadContextProvider() {
  if (shared_main_thread_contexts_)
    return shared_main_thread_contexts_;

  if (!GpuDataManagerImpl::GetInstance()->CanUseGpuBrowserCompositor())
    return nullptr;
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      BrowserGpuChannelHostFactory::instance()->EstablishGpuChannelSync(
          CAUSE_FOR_GPU_LAUNCH_BROWSER_SHARED_MAIN_THREAD_CONTEXT));
  if (!gpu_channel_host)
    return nullptr;

  // We need a separate context from the compositor's so that skia and gl_helper
  // don't step on each other.
  bool support_locking = false;
  shared_main_thread_contexts_ = CreateContextCommon(
      std::move(gpu_channel_host), gpu::kNullSurfaceHandle, support_locking,
      nullptr, command_buffer_metrics::BROWSER_OFFSCREEN_MAINTHREAD_CONTEXT);
  shared_main_thread_contexts_->SetLostContextCallback(base::Bind(
      &GpuProcessTransportFactory::OnLostMainThreadSharedContextInsideCallback,
      callback_factory_.GetWeakPtr()));
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/125248 is
  // fixed. Tracking time in BindToCurrentThread.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "125248"
          " GpuProcessTransportFactory::SharedMainThreadContextProvider"));
  if (!shared_main_thread_contexts_->BindToCurrentThread())
    shared_main_thread_contexts_ = nullptr;
  return shared_main_thread_contexts_;
}

GpuProcessTransportFactory::PerCompositorData*
GpuProcessTransportFactory::CreatePerCompositorData(
    ui::Compositor* compositor) {
  DCHECK(!per_compositor_data_[compositor]);

  gfx::AcceleratedWidget widget = compositor->widget();

  PerCompositorData* data = new PerCompositorData;
  if (widget == gfx::kNullAcceleratedWidget) {
    data->surface_handle = gpu::kNullSurfaceHandle;
  } else {
#if defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
    data->surface_handle = widget;
#else
    GpuSurfaceTracker* tracker = GpuSurfaceTracker::Get();
    data->surface_handle = tracker->AddSurfaceForNativeWidget(widget);
#endif
  }

  per_compositor_data_[compositor] = data;

  return data;
}

void GpuProcessTransportFactory::OnLostMainThreadSharedContextInsideCallback() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&GpuProcessTransportFactory::OnLostMainThreadSharedContext,
                 callback_factory_.GetWeakPtr()));
}

void GpuProcessTransportFactory::OnLostMainThreadSharedContext() {
  LOG(ERROR) << "Lost UI shared context.";

  // Keep old resources around while we call the observers, but ensure that
  // new resources are created if needed.
  // Kill shared contexts for both threads in tandem so they are always in
  // the same share group.
  scoped_refptr<cc::ContextProvider> lost_shared_main_thread_contexts =
      shared_main_thread_contexts_;
  shared_main_thread_contexts_  = NULL;

  std::unique_ptr<display_compositor::GLHelper> lost_gl_helper =
      std::move(gl_helper_);

  FOR_EACH_OBSERVER(ui::ContextFactoryObserver, observer_list_,
                    OnLostResources());

  // Kill things that use the shared context before killing the shared context.
  lost_gl_helper.reset();
  lost_shared_main_thread_contexts  = NULL;
}

scoped_refptr<cc::VulkanInProcessContextProvider>
GpuProcessTransportFactory::SharedVulkanContextProvider() {
  if (!shared_vulkan_context_provider_initialized_) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableVulkan)) {
      shared_vulkan_context_provider_ =
          cc::VulkanInProcessContextProvider::Create();
    }

    shared_vulkan_context_provider_initialized_ = true;
  }
  return shared_vulkan_context_provider_;
}

}  // namespace content

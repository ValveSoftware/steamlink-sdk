// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/gpu_process_transport_factory.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/base/histograms.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/output/vulkan_in_process_context_provider.h"
#include "cc/raster/single_thread_task_graph_runner.h"
#include "cc/raster/task_graph_runner.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/surfaces/direct_compositor_frame_sink.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/display_scheduler.h"
#include "cc/surfaces/surface_manager.h"
#include "components/display_compositor/compositor_overlay_candidate_validator.h"
#include "components/display_compositor/gl_helper.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "content/browser/compositor/gpu_browser_compositor_output_surface.h"
#include "content/browser/compositor/gpu_surfaceless_browser_compositor_output_surface.h"
#include "content/browser/compositor/offscreen_browser_compositor_output_surface.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/browser/compositor/software_browser_compositor_output_surface.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "content/public/common/content_switches.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/service_manager/runner/common/client_util.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_constants.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/geometry/size.h"

#if defined(USE_AURA)
#include "content/browser/compositor/mus_browser_compositor_output_surface.h"
#include "content/public/common/service_manager_connection.h"
#include "ui/aura/window_tree_host.h"
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
#include "gpu/ipc/common/gpu_surface_tracker.h"
#endif

#if defined(ENABLE_VULKAN)
#include "content/browser/compositor/vulkan_browser_compositor_output_surface.h"
#endif

using cc::ContextProvider;
using gpu::gles2::GLES2Interface;

namespace {

const int kNumRetriesBeforeSoftwareFallback = 4;

bool IsUsingMus() {
  return service_manager::ServiceManagerIsRemote();
}

scoped_refptr<content::ContextProviderCommandBuffer> CreateContextCommon(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    gpu::SurfaceHandle surface_handle,
    bool need_alpha_channel,
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
  //   (onscreen, except for with mus) for actual GL drawing.
  // - The compositor worker context (offscreen) used for GPU raster.
  // So ask for capabilities needed by any of these cases (we can optimize by
  // branching on |surface_handle| being null if these needs diverge).
  //
  // The default framebuffer for an offscreen context is not used, so it does
  // not need alpha, stencil, depth, antialiasing. The display compositor does
  // not use these things either, so we can request nothing here.
  // The display compositor does not use these things either (except for alpha
  // when using mus for non-opaque ui that overlaps the system's window
  // borders), so we can request only that when needed.
  gpu::gles2::ContextCreationAttribHelper attributes;
  attributes.alpha_size = need_alpha_channel ? 8 : -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;
  attributes.buffer_preserved = false;

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
    : task_graph_runner_(new cc::SingleThreadTaskGraphRunner),
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
#if defined(USE_AURA)
  if (service_manager::ServiceManagerIsRemote()) {
    NOTREACHED();
    return nullptr;
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
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableHardwareOverlays)) {
    std::string enable_overlay_flag =
        command_line->GetSwitchValueASCII(switches::kEnableHardwareOverlays);
    std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates =
        ui::OzonePlatform::GetInstance()
            ->GetOverlayManager()
            ->CreateOverlayCandidates(widget);
    validator.reset(
        new display_compositor::CompositorOverlayCandidateValidatorOzone(
            std::move(overlay_candidates),
            enable_overlay_flag == "single-fullscreen"));
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

static bool ShouldCreateGpuCompositorFrameSink(ui::Compositor* compositor) {
#if defined(OS_CHROMEOS)
  // Software fallback does not happen on Chrome OS.
  return true;
#endif
  if (IsUsingMus())
    return true;

#if defined(OS_WIN)
  if (::GetProp(compositor->widget(), kForceSoftwareCompositor) &&
      ::RemoveProp(compositor->widget(), kForceSoftwareCompositor))
    return false;
#endif

  return GpuDataManagerImpl::GetInstance()->CanUseGpuBrowserCompositor();
}

void GpuProcessTransportFactory::CreateCompositorFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
  DCHECK(!!compositor);
  PerCompositorData* data = per_compositor_data_[compositor.get()].get();
  if (!data) {
    data = CreatePerCompositorData(compositor.get());
  } else {
    // TODO(danakj): We can destroy the |data->display| here when the compositor
    // destroys its CompositorFrameSink before calling back here.
    data->display_output_surface = nullptr;
    data->begin_frame_source = nullptr;
  }

#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->UnregisterParent(
      compositor->widget());
#endif

  const bool use_vulkan = static_cast<bool>(SharedVulkanContextProvider());
  const bool create_gpu_output_surface =
      ShouldCreateGpuCompositorFrameSink(compositor.get());
  if (create_gpu_output_surface && !use_vulkan) {
    gpu::GpuChannelEstablishedCallback callback(
        base::Bind(&GpuProcessTransportFactory::EstablishedGpuChannel,
                   callback_factory_.GetWeakPtr(), compositor,
                   create_gpu_output_surface, 0));
    DCHECK(gpu_channel_factory_);
    gpu_channel_factory_->EstablishGpuChannel(callback);
  } else {
    EstablishedGpuChannel(compositor, create_gpu_output_surface, 0, nullptr);
  }
}

void GpuProcessTransportFactory::EstablishedGpuChannel(
    base::WeakPtr<ui::Compositor> compositor,
    bool create_gpu_output_surface,
    int num_attempts,
    scoped_refptr<gpu::GpuChannelHost> established_channel_host) {
  if (!compositor)
    return;

  // The widget might have been released in the meantime.
  PerCompositorDataMap::iterator it =
      per_compositor_data_.find(compositor.get());
  if (it == per_compositor_data_.end())
    return;

  PerCompositorData* data = it->second.get();
  DCHECK(data);

  if (num_attempts > kNumRetriesBeforeSoftwareFallback) {
    bool fatal = IsUsingMus();
#if defined(OS_CHROMEOS)
    fatal = true;
#endif
    LOG_IF(FATAL, fatal) << "Unable to create a UI graphics context, and "
                         << "cannot use software compositing on ChromeOS.";
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
    if (GpuDataManagerImpl::GetInstance()->CanUseGpuBrowserCompositor())
      gpu_channel_host = std::move(established_channel_host);

    if (!gpu_channel_host) {
      shared_worker_context_provider_ = nullptr;
    } else {
      if (!shared_worker_context_provider_) {
        bool need_alpha_channel = false;
        const bool support_locking = true;
        shared_worker_context_provider_ =
            CreateContextCommon(gpu_channel_host, gpu::kNullSurfaceHandle,
                                need_alpha_channel, support_locking, nullptr,
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
        // For mus, we create an offscreen context for a mus window, and we will
        // use CommandBufferProxyImpl::TakeFrontBuffer() to take the context's
        // front buffer into a mailbox, insert a sync token, and send the
        // mailbox+sync to the ui service process.
        gpu::SurfaceHandle surface_handle =
            IsUsingMus() ? gpu::kNullSurfaceHandle : data->surface_handle;
        bool need_alpha_channel = IsUsingMus();
        bool support_locking = false;
        context_provider = CreateContextCommon(
            std::move(gpu_channel_host), surface_handle, need_alpha_channel,
            support_locking, shared_worker_context_provider_.get(),
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
      gpu::GpuChannelEstablishedCallback callback(
          base::Bind(&GpuProcessTransportFactory::EstablishedGpuChannel,
                     callback_factory_.GetWeakPtr(), compositor,
                     create_gpu_output_surface, num_attempts + 1));
      DCHECK(gpu_channel_factory_);
      gpu_channel_factory_->EstablishGpuChannel(callback);
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
        begin_frame_source.get()));
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
          base::MakeUnique<SoftwareBrowserCompositorOutputSurface>(
              CreateSoftwareOutputDevice(compositor.get()),
              compositor->vsync_manager(), begin_frame_source.get(),
              compositor->task_runner());
    } else {
      DCHECK(context_provider);
      const auto& capabilities = context_provider->ContextCapabilities();
      if (data->surface_handle == gpu::kNullSurfaceHandle) {
        display_output_surface =
            base::MakeUnique<OffscreenBrowserCompositorOutputSurface>(
                context_provider, compositor->vsync_manager(),
                begin_frame_source.get(),
                std::unique_ptr<
                    display_compositor::CompositorOverlayCandidateValidator>());
      } else if (capabilities.surfaceless) {
#if defined(OS_MACOSX)
        display_output_surface = base::MakeUnique<GpuOutputSurfaceMac>(
            compositor->widget(), context_provider, data->surface_handle,
            compositor->vsync_manager(), begin_frame_source.get(),
            CreateOverlayCandidateValidator(compositor->widget()),
            GetGpuMemoryBufferManager());
#else
        display_output_surface =
            base::MakeUnique<GpuSurfacelessBrowserCompositorOutputSurface>(
                context_provider, data->surface_handle,
                compositor->vsync_manager(), begin_frame_source.get(),
                CreateOverlayCandidateValidator(compositor->widget()),
                GL_TEXTURE_2D, GL_RGB, ui::DisplaySnapshot::PrimaryFormat(),
                GetGpuMemoryBufferManager());
#endif
      } else {
        std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
            validator;
        const bool use_mus = IsUsingMus();
#if !defined(OS_MACOSX)
        // Overlays are only supported on surfaceless output surfaces on Mac.
        if (!use_mus)
          validator = CreateOverlayCandidateValidator(compositor->widget());
#endif
        if (!use_mus) {
          display_output_surface =
              base::MakeUnique<GpuBrowserCompositorOutputSurface>(
                  context_provider, compositor->vsync_manager(),
                  begin_frame_source.get(), std::move(validator));
        } else {
#if defined(USE_AURA) && !defined(TOOLKIT_QT)
          if (compositor->window()) {
            // TODO(mfomitchev): Remove this clause once we complete the switch
            // to Aura-Mus.
            display_output_surface =
                base::MakeUnique<MusBrowserCompositorOutputSurface>(
                    compositor->window(), context_provider,
                    GetGpuMemoryBufferManager(), compositor->vsync_manager(),
                    begin_frame_source.get(), std::move(validator));
          } else {
            aura::WindowTreeHost* host =
                aura::WindowTreeHost::GetForAcceleratedWidget(
                    compositor->widget());
            display_output_surface =
                base::MakeUnique<MusBrowserCompositorOutputSurface>(
                    host->window(), context_provider,
                    GetGpuMemoryBufferManager(), compositor->vsync_manager(),
                    begin_frame_source.get(), std::move(validator));
          }
#else
          NOTREACHED();
#endif
        }
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
      HostSharedBitmapManager::current(), GetGpuMemoryBufferManager(),
      compositor->GetRendererSettings(), compositor->frame_sink_id(),
      std::move(begin_frame_source), std::move(display_output_surface),
      std::move(scheduler), base::MakeUnique<cc::TextureMailboxDeleter>(
                                compositor->task_runner().get()));

  // The |delegated_output_surface| is given back to the compositor, it
  // delegates to the Display as its root surface. Importantly, it shares the
  // same ContextProvider as the Display's output surface.
  auto compositor_frame_sink =
      vulkan_context_provider
          ? base::MakeUnique<cc::DirectCompositorFrameSink>(
                compositor->frame_sink_id(), surface_manager_.get(),
                data->display.get(),
                static_cast<scoped_refptr<cc::VulkanContextProvider>>(
                    vulkan_context_provider))
          : base::MakeUnique<cc::DirectCompositorFrameSink>(
                compositor->frame_sink_id(), surface_manager_.get(),
                data->display.get(), context_provider,
                shared_worker_context_provider_, GetGpuMemoryBufferManager(),
                HostSharedBitmapManager::current());
  data->display->Resize(compositor->size());
  data->display->SetOutputIsSecure(data->output_is_secure);
  compositor->SetCompositorFrameSink(std::move(compositor_frame_sink));
}

std::unique_ptr<ui::Reflector> GpuProcessTransportFactory::CreateReflector(
    ui::Compositor* source_compositor,
    ui::Layer* target_layer) {
  PerCompositorData* source_data =
      per_compositor_data_[source_compositor].get();
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
      per_compositor_data_[reflector_impl->mirrored_compositor()].get();
  DCHECK(data);
  data->reflector->Shutdown();
  data->reflector = nullptr;
}

void GpuProcessTransportFactory::RemoveCompositor(ui::Compositor* compositor) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);
#if !defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
  if (data->surface_handle)
    gpu::GpuSurfaceTracker::Get()->RemoveSurface(data->surface_handle);
#endif
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
    for (auto& observer : observer_list_)
      observer.OnLostResources();

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

gpu::GpuMemoryBufferManager*
GpuProcessTransportFactory::GetGpuMemoryBufferManager() {
  return gpu_channel_factory_->GetGpuMemoryBufferManager();
}

cc::TaskGraphRunner* GpuProcessTransportFactory::GetTaskGraphRunner() {
  return task_graph_runner_.get();
}

ui::ContextFactory* GpuProcessTransportFactory::GetContextFactory() {
  return this;
}

cc::FrameSinkId GpuProcessTransportFactory::AllocateFrameSinkId() {
  return cc::FrameSinkId(0, next_sink_id_++);
}

void GpuProcessTransportFactory::SetDisplayVisible(ui::Compositor* compositor,
                                                   bool visible) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);
  // The compositor will always SetVisible on the Display once it is set up, so
  // do nothing if |display| is null.
  if (data->display)
    data->display->SetVisible(visible);
}

void GpuProcessTransportFactory::ResizeDisplay(ui::Compositor* compositor,
                                               const gfx::Size& size) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
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
  PerCompositorData* data = it->second.get();
  DCHECK(data);
  // The compositor will always SetColorSpace on the Display once it is set up,
  // so do nothing if |display| is null.
  if (data->display)
    data->display->SetColorSpace(color_space);
}

void GpuProcessTransportFactory::SetAuthoritativeVSyncInterval(
    ui::Compositor* compositor,
    base::TimeDelta interval) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);
  if (data->begin_frame_source)
    data->begin_frame_source->SetAuthoritativeVSyncInterval(interval);
}

void GpuProcessTransportFactory::SetDisplayVSyncParameters(
    ui::Compositor* compositor,
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);
  if (data->begin_frame_source)
    data->begin_frame_source->OnUpdateVSyncParameters(timebase, interval);
}

void GpuProcessTransportFactory::SetOutputIsSecure(ui::Compositor* compositor,
                                                   bool secure) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
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

void GpuProcessTransportFactory::SetGpuChannelEstablishFactory(
    gpu::GpuChannelEstablishFactory* factory) {
  DCHECK(!gpu_channel_factory_ || !factory);
  gpu_channel_factory_ = factory;
}

#if defined(OS_MACOSX)
void GpuProcessTransportFactory::SetCompositorSuspendedForRecycle(
    ui::Compositor* compositor,
    bool suspended) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
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

  DCHECK(gpu_channel_factory_);
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
      gpu_channel_factory_->EstablishGpuChannelSync();
  if (!gpu_channel_host)
    return nullptr;

  // We need a separate context from the compositor's so that skia and gl_helper
  // don't step on each other.
  bool need_alpha_channel = false;
  bool support_locking = false;
  shared_main_thread_contexts_ = CreateContextCommon(
      std::move(gpu_channel_host), gpu::kNullSurfaceHandle, need_alpha_channel,
      support_locking, nullptr,
      command_buffer_metrics::BROWSER_OFFSCREEN_MAINTHREAD_CONTEXT);
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

  auto data = base::MakeUnique<PerCompositorData>();
  if (widget == gfx::kNullAcceleratedWidget) {
    data->surface_handle = gpu::kNullSurfaceHandle;
  } else {
#if defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
    data->surface_handle = widget;
#else
    gpu::GpuSurfaceTracker* tracker = gpu::GpuSurfaceTracker::Get();
    data->surface_handle = tracker->AddSurfaceForNativeWidget(widget);
#endif
  }

  PerCompositorData* return_ptr = data.get();
  per_compositor_data_[compositor] = std::move(data);
  return return_ptr;
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

  for (auto& observer : observer_list_)
    observer.OnLostResources();

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

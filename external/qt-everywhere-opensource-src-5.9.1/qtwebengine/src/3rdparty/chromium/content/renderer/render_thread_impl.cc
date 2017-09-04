// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_thread_impl.h"

#include <algorithm>
#include <limits>
#include <map>
#include <utility>
#include <vector>

#include "base/allocator/allocator_extension.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/memory_coordinator_client_registry.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/process/process_metrics.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/base/histograms.h"
#include "cc/base/switches.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/output/buffer_to_texture_target_map.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/vulkan_in_process_context_provider.h"
#include "cc/raster/task_graph_runner.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "content/child/appcache/appcache_dispatcher.h"
#include "content/child/appcache/appcache_frontend_impl.h"
#include "content/child/blob_storage/blob_message_filter.h"
#include "content/child/child_gpu_memory_buffer_manager.h"
#include "content/child/child_histogram_message_filter.h"
#include "content/child/child_resource_message_filter.h"
#include "content/child/child_shared_bitmap_manager.h"
#include "content/child/content_child_helpers.h"
#include "content/child/db_message_filter.h"
#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/indexed_db/indexed_db_message_filter.h"
#include "content/child/memory/child_memory_coordinator_impl.h"
#include "content/child/resource_dispatcher.h"
#include "content/child/resource_scheduling_filter.h"
#include "content/child/runtime_features.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/web_database_observer_impl.h"
#include "content/child/worker_thread_registry.h"
#include "content/common/child_process_messages.h"
#include "content/common/content_constants_internal.h"
#include "content/common/dom_storage/dom_storage_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_owner_properties.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/render_process_messages.h"
#include "content/common/resource_messages.h"
#include "content/common/service_worker/embedded_worker_setup.mojom.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_thread_observer.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/cache_storage/cache_storage_dispatcher.h"
#include "content/renderer/cache_storage/cache_storage_message_filter.h"
#include "content/renderer/categorized_worker_pool.h"
#include "content/renderer/devtools/devtools_agent_filter.h"
#include "content/renderer/dom_storage/dom_storage_dispatcher.h"
#include "content/renderer/dom_storage/webstoragearea_impl.h"
#include "content/renderer/dom_storage/webstoragenamespace_impl.h"
#include "content/renderer/gpu/compositor_external_begin_frame_source.h"
#include "content/renderer/gpu/compositor_forwarding_message_filter.h"
#include "content/renderer/gpu/frame_swap_message_queue.h"
#include "content/renderer/gpu/renderer_compositor_frame_sink.h"
#include "content/renderer/input/input_event_filter.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/input/main_thread_input_event_filter.h"
#include "content/renderer/media/audio_input_message_filter.h"
#include "content/renderer/media/audio_message_filter.h"
#include "content/renderer/media/audio_renderer_mixer_manager.h"
#include "content/renderer/media/media_stream_center.h"
#include "content/renderer/media/midi_message_filter.h"
#include "content/renderer/media/render_media_client.h"
#include "content/renderer/media/renderer_gpu_video_accelerator_factories.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/net_info_helper.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/scheduler/resource_dispatch_throttler.h"
#include "content/renderer/service_worker/embedded_worker_dispatcher.h"
#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "content/renderer/service_worker/service_worker_context_message_filter.h"
#include "content/renderer/shared_worker/embedded_shared_worker_stub.h"
#include "gin/public/debug.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_platform_file.h"
#include "media/base/media.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/net_errors.h"
#include "net/base/port_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "skia/ext/event_tracer_impl.h"
#include "skia/ext/skia_memory_dump_provider.h"
#include "third_party/WebKit/public/platform/WebImageGenerator.h"
#include "third_party/WebKit/public/platform/WebMemoryCoordinator.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "third_party/WebKit/public/platform/scheduler/child/compositor_worker_scheduler.h"
#include "third_party/WebKit/public/platform/scheduler/child/webthread_impl_for_worker_scheduler.h"
#include "third_party/WebKit/public/platform/scheduler/renderer/renderer_scheduler.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "third_party/WebKit/public/web/WebDatabase.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebNetworkStateNotifier.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebScriptController.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_ANDROID)
#include <cpu-features.h>
#include "content/renderer/android/synchronous_compositor_filter.h"
#include "content/renderer/android/synchronous_compositor_frame_sink.h"
#include "content/renderer/media/android/stream_texture_factory.h"
#include "media/base/android/media_codec_util.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "content/renderer/theme_helper_mac.h"
#include "content/renderer/webscrollbarbehavior_impl_mac.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#include <objbase.h>
#endif

#if defined(ENABLE_WEBRTC)
#include "content/renderer/media/aec_dump_message_filter.h"
#include "content/renderer/media/peer_connection_tracker.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#endif

#ifdef ENABLE_VTUNE_JIT_INTERFACE
#include "v8/src/third_party/vtune/v8-vtune.h"
#endif

#if defined(USE_AURA) && !defined(TOOLKIT_QT)
#include "content/public/common/service_manager_connection.h"
#include "content/renderer/mus/render_widget_mus_connection.h"
#include "content/renderer/mus/render_widget_window_tree_client_factory.h"
#include "services/ui/public/cpp/gpu_service.h"
#endif

#if defined(ENABLE_IPC_FUZZER)
#include "content/common/external_ipc_dumper.h"
#endif

#if defined(OS_MACOSX)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

using base::ThreadRestrictions;
using blink::WebDocument;
using blink::WebFrame;
using blink::WebNetworkStateNotifier;
using blink::WebRuntimeFeatures;
using blink::WebScriptController;
using blink::WebSecurityPolicy;
using blink::WebString;
using blink::WebView;
using blink::scheduler::WebThreadImplForWorkerScheduler;

namespace content {

namespace {

const int64_t kInitialIdleHandlerDelayMs = 1000;
const int64_t kLongIdleHandlerDelayMs = 30 * 1000;

#if defined(OS_ANDROID)
// On Android, resource messages can each take ~1.5ms to dispatch on the browser
// IO thread. Limiting the message rate to 3/frame at 60hz ensures that the
// induced work takes but a fraction (~1/4) of the overall frame budget.
const int kMaxResourceRequestsPerFlushWhenThrottled = 3;
#else
const int kMaxResourceRequestsPerFlushWhenThrottled = 8;
#endif
const double kThrottledResourceRequestFlushPeriodS = 1. / 60.;

// Maximum allocation size allowed for image scaling filters that
// require pre-scaling. Skia will fallback to a filter that doesn't
// require pre-scaling if the default filter would require an
// allocation that exceeds this limit.
const size_t kImageCacheSingleAllocationByteLimit = 64 * 1024 * 1024;

// Unique identifier for each output surface created.
uint32_t g_next_compositor_frame_sink_id = 1;

// An implementation of mojom::RenderMessageFilter which can be mocked out
// for tests which may indirectly send messages over this interface.
mojom::RenderMessageFilter* g_render_message_filter_for_testing;

// Keep the global RenderThreadImpl in a TLS slot so it is impossible to access
// incorrectly from the wrong thread.
base::LazyInstance<base::ThreadLocalPointer<RenderThreadImpl> >
    lazy_tls = LAZY_INSTANCE_INITIALIZER;

// v8::MemoryPressureLevel should correspond to base::MemoryPressureListener.
static_assert(static_cast<v8::MemoryPressureLevel>(
    base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) ==
        v8::MemoryPressureLevel::kNone, "none level not align");
static_assert(static_cast<v8::MemoryPressureLevel>(
    base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) ==
        v8::MemoryPressureLevel::kModerate, "moderate level not align");
static_assert(static_cast<v8::MemoryPressureLevel>(
    base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) ==
        v8::MemoryPressureLevel::kCritical, "critical level not align");

// WebMemoryPressureLevel should correspond to base::MemoryPressureListener.
static_assert(static_cast<blink::WebMemoryPressureLevel>(
    base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) ==
        blink::WebMemoryPressureLevelNone,
            "blink::WebMemoryPressureLevelNone not align");
static_assert(static_cast<blink::WebMemoryPressureLevel>(
    base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) ==
        blink::WebMemoryPressureLevelModerate,
            "blink::WebMemoryPressureLevelModerate not align");
static_assert(static_cast<blink::WebMemoryPressureLevel>(
    base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) ==
        blink::WebMemoryPressureLevelCritical,
            "blink::WebMemoryPressureLevelCritical not align");

class WebThreadForCompositor : public WebThreadImplForWorkerScheduler {
 public:
  explicit WebThreadForCompositor(base::Thread::Options options)
      : WebThreadImplForWorkerScheduler("Compositor", options) {
    Init();
  }
  ~WebThreadForCompositor() override {}

 private:
  // WebThreadImplForWorkerScheduler:
  std::unique_ptr<blink::scheduler::WorkerScheduler> CreateWorkerScheduler()
      override {
    return base::MakeUnique<blink::scheduler::CompositorWorkerScheduler>(
        thread());
  }

  DISALLOW_COPY_AND_ASSIGN(WebThreadForCompositor);
};

void* CreateHistogram(
    const char *name, int min, int max, size_t buckets) {
  if (min <= 0)
    min = 1;
  std::string histogram_name;
  RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
  if (render_thread_impl) {  // Can be null in tests.
    histogram_name = render_thread_impl->
        histogram_customizer()->ConvertToCustomHistogramName(name);
  } else {
    histogram_name = std::string(name);
  }
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      histogram_name, min, max, buckets,
      base::Histogram::kUmaTargetedHistogramFlag);
  return histogram;
}

void AddHistogramSample(void* hist, int sample) {
  base::Histogram* histogram = static_cast<base::Histogram*>(hist);
  histogram->Add(sample);
}

class FrameFactoryImpl : public mojom::FrameFactory {
 public:
  FrameFactoryImpl() : routing_id_highmark_(-1) {}

 private:
  // mojom::FrameFactory:
  void CreateFrame(int32_t frame_routing_id,
                   mojom::FrameRequest frame_request,
                   mojom::FrameHostPtr frame_host) override {
    // TODO(morrita): This is for investigating http://crbug.com/415059 and
    // should be removed once it is fixed.
    CHECK_LT(routing_id_highmark_, frame_routing_id);
    routing_id_highmark_ = frame_routing_id;

    RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(frame_routing_id);
    // We can receive a GetServiceProviderForFrame message for a frame not yet
    // created due to a race between the message and a
    // mojom::Renderer::CreateView IPC that triggers creation of the RenderFrame
    // we want.
    if (!frame) {
      RenderThreadImpl::current()->RegisterPendingFrameCreate(
          frame_routing_id, std::move(frame_request), std::move(frame_host));
      return;
    }

    frame->Bind(std::move(frame_request), std::move(frame_host));
  }

 private:
  int32_t routing_id_highmark_;
};

void CreateFrameFactory(mojom::FrameFactoryRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<FrameFactoryImpl>(),
                          std::move(request));
}

void SetupEmbeddedWorkerOnWorkerThread(
    service_manager::mojom::InterfaceProviderRequest request,
    service_manager::mojom::InterfaceProviderPtrInfo remote_interfaces) {
  ServiceWorkerContextClient* client =
      ServiceWorkerContextClient::ThreadSpecificInstance();
  // It is possible for client to be null if for some reason the worker died
  // before this call made it to the worker thread. In that case just do
  // nothing and let mojo close the connection.
  if (!client)
    return;
  client->BindInterfaceProviders(std::move(request),
                                 mojo::MakeProxy(std::move(remote_interfaces)));
}

class EmbeddedWorkerSetupImpl : public mojom::EmbeddedWorkerSetup {
 public:
  EmbeddedWorkerSetupImpl() = default;

  void ExchangeInterfaceProviders(
      int32_t thread_id,
      service_manager::mojom::InterfaceProviderRequest request,
      service_manager::mojom::InterfaceProviderPtr remote_interfaces) override {
    WorkerThreadRegistry::Instance()->GetTaskRunnerFor(thread_id)->PostTask(
        FROM_HERE,
        base::Bind(&SetupEmbeddedWorkerOnWorkerThread, base::Passed(&request),
                   base::Passed(remote_interfaces.PassInterface())));
  }
};

void CreateEmbeddedWorkerSetup(mojom::EmbeddedWorkerSetupRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<EmbeddedWorkerSetupImpl>(),
                          std::move(request));
}

scoped_refptr<ContextProviderCommandBuffer> CreateOffscreenContext(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    const gpu::SharedMemoryLimits& limits,
    bool support_locking,
    command_buffer_metrics::ContextType type,
    int32_t stream_id,
    gpu::GpuStreamPriority stream_priority) {
  DCHECK(gpu_channel_host);
  // This is used to create a few different offscreen contexts:
  // - The shared main thread context (offscreen) used by blink for canvas.
  // - The worker context (offscreen) used for GPU raster and video decoding.
  // This is for an offscreen context, so the default framebuffer doesn't need
  // alpha, depth, stencil, antialiasing.
  gpu::gles2::ContextCreationAttribHelper attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;
  const bool automatic_flushes = false;
  return make_scoped_refptr(new ContextProviderCommandBuffer(
      std::move(gpu_channel_host), stream_id, stream_priority,
      gpu::kNullSurfaceHandle,
      GURL("chrome://gpu/RenderThreadImpl::CreateOffscreenContext"),
      automatic_flushes, support_locking, limits, attributes, nullptr, type));
}

bool IsRunningInMash() {
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  return cmdline->HasSwitch(switches::kIsRunningInMash);
}

}  // namespace

// For measuring memory usage after each task. Behind a command line flag.
class MemoryObserver : public base::MessageLoop::TaskObserver {
 public:
  MemoryObserver() {}
  ~MemoryObserver() override {}

  void WillProcessTask(const base::PendingTask& pending_task) override {}

  void DidProcessTask(const base::PendingTask& pending_task) override {
    LOCAL_HISTOGRAM_MEMORY_KB("Memory.RendererUsed", GetMemoryUsageKB());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryObserver);
};

RenderThreadImpl::HistogramCustomizer::HistogramCustomizer() {
  custom_histograms_.insert("V8.MemoryExternalFragmentationTotal");
  custom_histograms_.insert("V8.MemoryHeapSampleTotalCommitted");
  custom_histograms_.insert("V8.MemoryHeapSampleTotalUsed");
  custom_histograms_.insert("V8.MemoryHeapUsed");
  custom_histograms_.insert("V8.MemoryHeapCommitted");
}

RenderThreadImpl::HistogramCustomizer::~HistogramCustomizer() {}

void RenderThreadImpl::HistogramCustomizer::RenderViewNavigatedToHost(
    const std::string& host, size_t view_count) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHistogramCustomizer)) {
    return;
  }
  // Check if all RenderViews are displaying a page from the same host. If there
  // is only one RenderView, the common host is this view's host. If there are
  // many, check if this one shares the common host of the other
  // RenderViews. It's ok to not detect some cases where the RenderViews share a
  // common host. This information is only used for producing custom histograms.
  if (view_count == 1)
    SetCommonHost(host);
  else if (host != common_host_)
    SetCommonHost(std::string());
}

std::string RenderThreadImpl::HistogramCustomizer::ConvertToCustomHistogramName(
    const char* histogram_name) const {
  std::string name(histogram_name);
  if (!common_host_histogram_suffix_.empty() &&
      custom_histograms_.find(name) != custom_histograms_.end())
    name += common_host_histogram_suffix_;
  return name;
}

void RenderThreadImpl::HistogramCustomizer::SetCommonHost(
    const std::string& host) {
  if (host != common_host_) {
    common_host_ = host;
    common_host_histogram_suffix_ = HostToCustomHistogramSuffix(host);
    blink::mainThreadIsolate()->SetCreateHistogramFunction(CreateHistogram);
  }
}

std::string RenderThreadImpl::HistogramCustomizer::HostToCustomHistogramSuffix(
    const std::string& host) {
  if (host == "mail.google.com")
    return ".gmail";
  if (host == "docs.google.com" || host == "drive.google.com")
    return ".docs";
  if (host == "plus.google.com")
    return ".plus";
  if (host == "inbox.google.com")
    return ".inbox";
  if (host == "calendar.google.com")
    return ".calendar";
  if (host == "www.youtube.com")
    return ".youtube";
  if (IsAlexaTop10NonGoogleSite(host))
    return ".top10";

  return std::string();
}

bool RenderThreadImpl::HistogramCustomizer::IsAlexaTop10NonGoogleSite(
    const std::string& host) {
  // The Top10 sites have different TLD and/or subdomains depending on the
  // localization.
  if (host == "sina.com.cn")
    return true;

  std::string sanitized_host =
      net::registry_controlled_domains::GetDomainAndRegistry(
          host, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  if (sanitized_host == "facebook.com")
    return true;
  if (sanitized_host == "baidu.com")
    return true;
  if (sanitized_host == "qq.com")
    return true;
  if (sanitized_host == "twitter.com")
    return true;
  if (sanitized_host == "taobao.com")
    return true;
  if (sanitized_host == "live.com")
    return true;

  if (!sanitized_host.empty()) {
    std::vector<base::StringPiece> host_tokens = base::SplitStringPiece(
        sanitized_host, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    if (host_tokens.size() >= 2) {
      if ((host_tokens[0] == "yahoo") || (host_tokens[0] == "amazon") ||
          (host_tokens[0] == "wikipedia")) {
        return true;
      }
    }
  }
  return false;
}

// static
RenderThreadImpl* RenderThreadImpl::Create(
    const InProcessChildThreadParams& params) {
  std::unique_ptr<blink::scheduler::RendererScheduler> renderer_scheduler =
      blink::scheduler::RendererScheduler::Create();
  scoped_refptr<base::SingleThreadTaskRunner> test_task_counter;
  return new RenderThreadImpl(
      params, std::move(renderer_scheduler), test_task_counter);
}

// static
RenderThreadImpl* RenderThreadImpl::Create(
    std::unique_ptr<base::MessageLoop> main_message_loop,
    std::unique_ptr<blink::scheduler::RendererScheduler> renderer_scheduler) {
  return new RenderThreadImpl(std::move(main_message_loop),
                              std::move(renderer_scheduler));
}

// static
RenderThreadImpl* RenderThreadImpl::current() {
  return lazy_tls.Pointer()->Get();
}

// static
mojom::RenderMessageFilter* RenderThreadImpl::current_render_message_filter() {
  if (g_render_message_filter_for_testing)
    return g_render_message_filter_for_testing;
  DCHECK(current());
  return current()->render_message_filter();
}

// static
void RenderThreadImpl::SetRenderMessageFilterForTesting(
    mojom::RenderMessageFilter* render_message_filter) {
  g_render_message_filter_for_testing = render_message_filter;
}

// In single-process mode used for debugging, we don't pass a renderer client
// ID via command line because RenderThreadImpl lives in the same process as
// the browser
RenderThreadImpl::RenderThreadImpl(
    const InProcessChildThreadParams& params,
    std::unique_ptr<blink::scheduler::RendererScheduler> scheduler,
    scoped_refptr<base::SingleThreadTaskRunner>& resource_task_queue)
    : ChildThreadImpl(Options::Builder()
                          .InBrowserProcess(params)
                          .AutoStartServiceManagerConnection(false)
                          .ConnectToBrowser(true)
                          .Build()),
      renderer_scheduler_(std::move(scheduler)),
      categorized_worker_pool_(new CategorizedWorkerPool()),
      renderer_binding_(this),
      client_id_(1) {
  Init(resource_task_queue);
}

// When we run plugins in process, we actually run them on the render thread,
// which means that we need to make the render thread pump UI events.
RenderThreadImpl::RenderThreadImpl(
    std::unique_ptr<base::MessageLoop> main_message_loop,
    std::unique_ptr<blink::scheduler::RendererScheduler> scheduler)
    : ChildThreadImpl(Options::Builder()
                          .AutoStartServiceManagerConnection(false)
                          .ConnectToBrowser(true)
                          .Build()),
      renderer_scheduler_(std::move(scheduler)),
      main_message_loop_(std::move(main_message_loop)),
      categorized_worker_pool_(new CategorizedWorkerPool()),
      renderer_binding_(this) {
  scoped_refptr<base::SingleThreadTaskRunner> test_task_counter;
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kRendererClientId));
  base::StringToInt(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                        switches::kRendererClientId),
                    &client_id_);
  Init(test_task_counter);
}

void RenderThreadImpl::Init(
    scoped_refptr<base::SingleThreadTaskRunner>& resource_task_queue) {
  TRACE_EVENT0("startup", "RenderThreadImpl::Init");

  base::trace_event::TraceLog::GetInstance()->SetThreadSortIndex(
      base::PlatformThread::CurrentId(),
      kTraceEventRendererMainThreadSortIndex);

#if defined(USE_EXTERNAL_POPUP_MENU)
  // On Mac and Android Java UI, the select popups are rendered by the browser.
  blink::WebView::setUseExternalPopupMenus(true);
#endif

  lazy_tls.Pointer()->Set(this);

  // Register this object as the main thread.
  ChildProcess::current()->set_main_thread(this);

#if defined(USE_AURA) && !defined(TOOLKIT_QT)
  if (IsRunningInMash()) {
    gpu_service_ =
        ui::GpuService::Create(GetServiceManagerConnection()->GetConnector(),
                               ChildProcess::current()->io_task_runner());
  }
#endif
  gpu_memory_buffer_manager_ =
      base::MakeUnique<ChildGpuMemoryBufferManager>(thread_safe_sender());

  InitializeWebKit(resource_task_queue);

  // In single process the single process is all there is.
  webkit_shared_timer_suspended_ = false;
  widget_count_ = 0;
  hidden_widget_count_ = 0;
  idle_notification_delay_in_ms_ = kInitialIdleHandlerDelayMs;
  idle_notifications_to_skip_ = 0;

  appcache_dispatcher_.reset(
      new AppCacheDispatcher(Get(), new AppCacheFrontendImpl()));
  dom_storage_dispatcher_.reset(new DomStorageDispatcher());
  main_thread_indexed_db_dispatcher_.reset(new IndexedDBDispatcher(
      thread_safe_sender()));
  main_thread_cache_storage_dispatcher_.reset(
      new CacheStorageDispatcher(thread_safe_sender()));
  embedded_worker_dispatcher_.reset(new EmbeddedWorkerDispatcher());

  // Note: This may reorder messages from the ResourceDispatcher with respect to
  // other subsystems.
  resource_dispatch_throttler_.reset(new ResourceDispatchThrottler(
      static_cast<RenderThread*>(this), renderer_scheduler_.get(),
      base::TimeDelta::FromSecondsD(kThrottledResourceRequestFlushPeriodS),
      kMaxResourceRequestsPerFlushWhenThrottled));
  resource_dispatcher()->set_message_sender(resource_dispatch_throttler_.get());

  media_stream_center_ = nullptr;

  blob_message_filter_ = new BlobMessageFilter(GetFileThreadTaskRunner());
  AddFilter(blob_message_filter_.get());
  db_message_filter_ = new DBMessageFilter();
  AddFilter(db_message_filter_.get());

  vc_manager_.reset(new VideoCaptureImplManager());

  browser_plugin_manager_.reset(new BrowserPluginManager());
  AddObserver(browser_plugin_manager_.get());

#if defined(ENABLE_WEBRTC)
  peer_connection_tracker_.reset(new PeerConnectionTracker());
  AddObserver(peer_connection_tracker_.get());

  p2p_socket_dispatcher_ = new P2PSocketDispatcher(GetIOTaskRunner().get());
  AddFilter(p2p_socket_dispatcher_.get());

  peer_connection_factory_.reset(
      new PeerConnectionDependencyFactory(p2p_socket_dispatcher_.get()));

  aec_dump_message_filter_ = new AecDumpMessageFilter(
      GetIOTaskRunner(), message_loop()->task_runner());

  AddFilter(aec_dump_message_filter_.get());

#endif  // defined(ENABLE_WEBRTC)

  audio_input_message_filter_ = new AudioInputMessageFilter(GetIOTaskRunner());
  AddFilter(audio_input_message_filter_.get());

  audio_message_filter_ = new AudioMessageFilter(GetIOTaskRunner());
  AddFilter(audio_message_filter_.get());

  midi_message_filter_ = new MidiMessageFilter(GetIOTaskRunner());
  AddFilter(midi_message_filter_.get());

  AddFilter((new IndexedDBMessageFilter(thread_safe_sender()))->GetFilter());

  AddFilter((new CacheStorageMessageFilter(thread_safe_sender()))->GetFilter());

  AddFilter((new ServiceWorkerContextMessageFilter())->GetFilter());

#if defined(USE_AURA) && !defined(TOOLKIT_QT)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseMusInRenderer)) {
    CreateRenderWidgetWindowTreeClientFactory(GetServiceManagerConnection());
  }
#endif

  // Must be called before RenderThreadStarted() below.
  StartServiceManagerConnection();

  GetContentClient()->renderer()->RenderThreadStarted();

  GetAssociatedInterfaceRegistry()->AddInterface(
      base::Bind(&RenderThreadImpl::OnRendererInterfaceRequest,
                 base::Unretained(this)));

  InitSkiaEventTracer();
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      skia::SkiaMemoryDumpProvider::GetInstance(), "Skia", nullptr);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

#if defined(ENABLE_IPC_FUZZER)
  if (command_line.HasSwitch(switches::kIpcDumpDirectory)) {
    base::FilePath dump_directory =
        command_line.GetSwitchValuePath(switches::kIpcDumpDirectory);
    IPC::ChannelProxy::OutgoingMessageFilter* filter =
        LoadExternalIPCDumper(dump_directory);
    GetChannel()->set_outgoing_message_filter(filter);
  }
#endif

  cc::SetClientNameForMetrics("Renderer");

  is_threaded_animation_enabled_ =
      !command_line.HasSwitch(cc::switches::kDisableThreadedAnimation);

  is_zero_copy_enabled_ = command_line.HasSwitch(switches::kEnableZeroCopy);
  is_partial_raster_enabled_ =
      !command_line.HasSwitch(switches::kDisablePartialRaster);
  is_gpu_memory_buffer_compositor_resources_enabled_ = command_line.HasSwitch(
      switches::kEnableGpuMemoryBufferCompositorResources);

#if defined(OS_MACOSX)
  base::ScopedCFTypeRef<CFStringRef> key(
      base::SysUTF8ToCFStringRef("NSScrollViewRubberbanding"));
  Boolean key_exists = false;
  Boolean value = CFPreferencesGetAppBooleanValue(
      key, kCFPreferencesCurrentApplication, &key_exists);
  is_elastic_overscroll_enabled_ = !key_exists || value;
#else
  is_elastic_overscroll_enabled_ = false;
#endif

  std::string image_texture_target_string =
      command_line.GetSwitchValueASCII(switches::kContentImageTextureTarget);
  buffer_to_texture_target_map_ =
      cc::StringToBufferToTextureTargetMap(image_texture_target_string);

  if (command_line.HasSwitch(switches::kDisableLCDText)) {
    is_lcd_text_enabled_ = false;
  } else if (command_line.HasSwitch(switches::kEnableLCDText)) {
    is_lcd_text_enabled_ = true;
  } else {
#if defined(OS_ANDROID)
    is_lcd_text_enabled_ = false;
#else
    is_lcd_text_enabled_ = true;
#endif
  }

  is_gpu_rasterization_enabled_ =
      command_line.HasSwitch(switches::kEnableGpuRasterization);
  is_gpu_rasterization_forced_ =
      command_line.HasSwitch(switches::kForceGpuRasterization);
  is_async_worker_context_enabled_ =
      command_line.HasSwitch(switches::kEnableGpuAsyncWorkerContext);

  if (command_line.HasSwitch(switches::kGpuRasterizationMSAASampleCount)) {
    std::string string_value = command_line.GetSwitchValueASCII(
        switches::kGpuRasterizationMSAASampleCount);
    bool parsed_msaa_sample_count =
        base::StringToInt(string_value, &gpu_rasterization_msaa_sample_count_);
    DCHECK(parsed_msaa_sample_count) << string_value;
    DCHECK_GE(gpu_rasterization_msaa_sample_count_, 0);
  } else {
    gpu_rasterization_msaa_sample_count_ = -1;
  }

  if (command_line.HasSwitch(switches::kDisableDistanceFieldText)) {
    is_distance_field_text_enabled_ = false;
  } else if (command_line.HasSwitch(switches::kEnableDistanceFieldText)) {
    is_distance_field_text_enabled_ = true;
  } else {
    is_distance_field_text_enabled_ = false;
  }

  // Note that under Linux, the media library will normally already have
  // been initialized by the Zygote before this instance became a Renderer.
  media::InitializeMediaLibrary();

#if defined(OS_ANDROID)
  if (!command_line.HasSwitch(switches::kDisableAcceleratedVideoDecode) &&
      media::MediaCodecUtil::IsMediaCodecAvailable()) {
    media::EnablePlatformDecoderSupport();
  }
#endif

  memory_pressure_listener_.reset(new base::MemoryPressureListener(
      base::Bind(&RenderThreadImpl::OnMemoryPressure, base::Unretained(this)),
      base::Bind(&RenderThreadImpl::OnSyncMemoryPressure,
                 base::Unretained(this))));

  if (base::FeatureList::IsEnabled(features::kMemoryCoordinator)) {
    // Currently it is not possible to enable both PurgeAndSuspend and
    // MemoryCoordinator at the same time.
    DCHECK(!base::FeatureList::IsEnabled(features::kPurgeAndSuspend));

    // Disable MemoryPressureListener when memory coordinator is enabled.
    base::MemoryPressureListener::SetNotificationsSuppressed(true);

    // TODO(bashi): Revisit how to manage the lifetime of
    // ChildMemoryCoordinatorImpl.
    // https://codereview.chromium.org/2094583002/#msg52
    mojom::MemoryCoordinatorHandlePtr parent_coordinator;
    GetRemoteInterfaces()->GetInterface(mojo::GetProxy(&parent_coordinator));
    memory_coordinator_ = CreateChildMemoryCoordinator(
        std::move(parent_coordinator), this);
  }

  int num_raster_threads = 0;
  std::string string_value =
      command_line.GetSwitchValueASCII(switches::kNumRasterThreads);
  bool parsed_num_raster_threads =
      base::StringToInt(string_value, &num_raster_threads);
  DCHECK(parsed_num_raster_threads) << string_value;
  DCHECK_GT(num_raster_threads, 0);

  // TODO(vmpstr): If the flag sticks, we should clean it up and always have
  // image decode tasks.
  are_image_decode_tasks_enabled_ = true;

  categorized_worker_pool_->Start(num_raster_threads);

  // TODO(boliu): In single process, browser main loop should set up the
  // discardable memory manager, and should skip this if kSingleProcess.
  // See crbug.com/503724.
  base::DiscardableMemoryAllocator::SetInstance(
      ChildThreadImpl::discardable_shared_memory_manager());

  GetContentClient()->renderer()->ExposeInterfacesToBrowser(
      GetInterfaceRegistry());

  GetInterfaceRegistry()->AddInterface(base::Bind(&CreateFrameFactory));
  GetInterfaceRegistry()->AddInterface(base::Bind(&CreateEmbeddedWorkerSetup));
  GetInterfaceRegistry()->AddInterface(
      base::Bind(&EmbeddedWorkerInstanceClientImpl::Create,
                 base::Unretained(embedded_worker_dispatcher_.get())));

  GetRemoteInterfaces()->GetInterface(
      mojo::GetProxy(&storage_partition_service_));

#if defined(OS_LINUX)
  ChildProcess::current()->SetIOThreadPriority(base::ThreadPriority::DISPLAY);
  ChildThreadImpl::current()->SetThreadPriority(
      categorized_worker_pool_->background_worker_thread_id(),
      base::ThreadPriority::BACKGROUND);
#endif

  record_purge_suspend_metric_closure_.Reset(base::Bind(
      &RenderThreadImpl::RecordPurgeAndSuspendMetrics, base::Unretained(this)));
  is_renderer_suspended_ = false;

  base::MemoryCoordinatorClientRegistry::GetInstance()->Register(this);
}

RenderThreadImpl::~RenderThreadImpl() {
}

void RenderThreadImpl::Shutdown() {
  for (auto& observer : observers_)
    observer.OnRenderProcessShutdown();

  if (memory_observer_) {
    message_loop()->RemoveTaskObserver(memory_observer_.get());
    memory_observer_.reset();
  }

  // Wait for all databases to be closed.
  if (blink_platform_impl_) {
    // Crash the process if they fail to close after a generous amount of time.
    bool all_closed = blink_platform_impl_->web_database_observer_impl()
        ->WaitForAllDatabasesToClose(base::TimeDelta::FromSeconds(60));
    CHECK(all_closed);
  }

  // Shutdown in reverse of the initialization order.
  if (devtools_agent_message_filter_.get()) {
    RemoveFilter(devtools_agent_message_filter_.get());
    devtools_agent_message_filter_ = nullptr;
  }

  RemoveFilter(audio_input_message_filter_.get());
  audio_input_message_filter_ = nullptr;

#if defined(ENABLE_WEBRTC)
  RTCPeerConnectionHandler::DestructAllHandlers();
  // |peer_connection_factory_| cannot be deleted until after the main message
  // loop has been destroyed.  This is because there may be pending tasks that
  // hold on to objects produced by the PC factory that depend on threads owned
  // by the PC factory.  Once those tasks have been freed, the factory can be
  // deleted.
#endif
  vc_manager_.reset();

  RemoveFilter(db_message_filter_.get());
  db_message_filter_ = nullptr;

  // Shutdown the file thread if it's running.
  if (file_thread_)
    file_thread_->Stop();

  if (compositor_message_filter_.get()) {
    RemoveFilter(compositor_message_filter_.get());
    compositor_message_filter_ = nullptr;
  }

#if defined(OS_ANDROID)
  if (sync_compositor_message_filter_) {
    RemoveFilter(sync_compositor_message_filter_.get());
    sync_compositor_message_filter_ = nullptr;
  }
  stream_texture_factory_ = nullptr;
#endif

  media_thread_.reset();

  if (blink_platform_impl_)
    blink_platform_impl_->SetCompositorThread(nullptr);

  compositor_thread_.reset();

  // AudioMessageFilter may be accessed on |media_thread_|, so shutdown after.
  RemoveFilter(audio_message_filter_.get());
  audio_message_filter_ = nullptr;

  categorized_worker_pool_->Shutdown();

  main_input_callback_.Cancel();
  input_handler_manager_.reset();
  if (input_event_filter_.get()) {
    RemoveFilter(input_event_filter_.get());
    input_event_filter_ = nullptr;
  }

  // RemoveEmbeddedWorkerRoute may be called while deleting
  // EmbeddedWorkerDispatcher. So it must be deleted before deleting
  // RenderThreadImpl.
  embedded_worker_dispatcher_.reset();

  // Ramp down IDB before we ramp down WebKit (and V8), since IDB classes might
  // hold pointers to V8 objects (e.g., via pending requests).
  main_thread_indexed_db_dispatcher_.reset();

  main_thread_compositor_task_runner_ = nullptr;

  gpu_factories_.clear();

  // Context providers must be released prior to destroying the GPU channel.
  shared_worker_context_provider_ = nullptr;
  shared_main_thread_contexts_ = nullptr;

  if (gpu_channel_.get())
    gpu_channel_->DestroyChannel();

  ChildThreadImpl::Shutdown();

  // Shut down the message loop (if provided when the RenderThreadImpl was
  // constructed) and the renderer scheduler before shutting down Blink. This
  // prevents a scenario where a pending task in the message loop accesses Blink
  // objects after Blink shuts down.
  renderer_scheduler_->SetRAILModeObserver(nullptr);
  renderer_scheduler_->Shutdown();
  if (main_message_loop_)
    base::RunLoop().RunUntilIdle();

  if (blink_platform_impl_) {
    blink_platform_impl_->Shutdown();
    // This must be at the very end of the shutdown sequence.
    // blink::shutdown() must be called after all strong references from
    // Chromium to Blink are cleared.
    blink::shutdown();
  }

  // Delay shutting down DiscardableSharedMemoryManager until blink::shutdown
  // is complete, because blink::shutdown destructs Blink Resources and they
  // may try to unlock their underlying discardable memory.
  ChildThreadImpl::ShutdownDiscardableSharedMemoryManager();

  // The message loop must be cleared after shutting down
  // the DiscardableSharedMemoryManager, which needs to send messages
  // to the browser process.
  main_message_loop_.reset();

  lazy_tls.Pointer()->Set(nullptr);

  base::MemoryCoordinatorClientRegistry::GetInstance()->Unregister(this);
}

bool RenderThreadImpl::Send(IPC::Message* msg) {
  // There are cases where we want to pump asynchronous messages while waiting
  // synchronously for the replies to the message to be sent here. However, this
  // may create an opportunity for re-entrancy into WebKit and other subsystems,
  // so we need to take care to disable callbacks, timers, and pending network
  // loads that could trigger such callbacks.
  bool pumping_events = false;
  if (msg->is_sync()) {
    if (msg->is_caller_pumping_messages()) {
      pumping_events = true;
    }
  }

  if (pumping_events) {
    renderer_scheduler_->SuspendTimerQueue();
    WebView::willEnterModalLoop();
  }

  bool rv = ChildThreadImpl::Send(msg);

  if (pumping_events) {
    WebView::didExitModalLoop();
    renderer_scheduler_->ResumeTimerQueue();
  }

  return rv;
}

IPC::SyncChannel* RenderThreadImpl::GetChannel() {
  return channel();
}

std::string RenderThreadImpl::GetLocale() {
  // The browser process should have passed the locale to the renderer via the
  // --lang command line flag.
  const base::CommandLine& parsed_command_line =
      *base::CommandLine::ForCurrentProcess();
  const std::string& lang =
      parsed_command_line.GetSwitchValueASCII(switches::kLang);
  DCHECK(!lang.empty());
  return lang;
}

IPC::SyncMessageFilter* RenderThreadImpl::GetSyncMessageFilter() {
  return sync_message_filter();
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetIOTaskRunner() {
  return ChildProcess::current()->io_task_runner();
}

void RenderThreadImpl::AddRoute(int32_t routing_id, IPC::Listener* listener) {
  ChildThreadImpl::GetRouter()->AddRoute(routing_id, listener);
  auto it = pending_frame_creates_.find(routing_id);
  if (it == pending_frame_creates_.end())
    return;

  RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(routing_id);
  if (!frame)
    return;

  scoped_refptr<PendingFrameCreate> create(it->second);
  frame->Bind(it->second->TakeFrameRequest(), it->second->TakeFrameHost());
  pending_frame_creates_.erase(it);
}

void RenderThreadImpl::RemoveRoute(int32_t routing_id) {
  ChildThreadImpl::GetRouter()->RemoveRoute(routing_id);
}

void RenderThreadImpl::AddEmbeddedWorkerRoute(int32_t routing_id,
                                              IPC::Listener* listener) {
  AddRoute(routing_id, listener);
  if (devtools_agent_message_filter_.get()) {
    devtools_agent_message_filter_->AddEmbeddedWorkerRouteOnMainThread(
        routing_id);
  }
}

void RenderThreadImpl::RemoveEmbeddedWorkerRoute(int32_t routing_id) {
  RemoveRoute(routing_id);
  if (devtools_agent_message_filter_.get()) {
    devtools_agent_message_filter_->RemoveEmbeddedWorkerRouteOnMainThread(
        routing_id);
  }
}

void RenderThreadImpl::RegisterPendingFrameCreate(
    int routing_id,
    mojom::FrameRequest frame_request,
    mojom::FrameHostPtr frame_host) {
  std::pair<PendingFrameCreateMap::iterator, bool> result =
      pending_frame_creates_.insert(std::make_pair(
          routing_id,
          make_scoped_refptr(new PendingFrameCreate(
              routing_id, std::move(frame_request), std::move(frame_host)))));
  CHECK(result.second) << "Inserting a duplicate item.";
}

mojom::StoragePartitionService* RenderThreadImpl::GetStoragePartitionService() {
  return storage_partition_service_.get();
}

int RenderThreadImpl::GenerateRoutingID() {
  int32_t routing_id = MSG_ROUTING_NONE;
  render_message_filter()->GenerateRoutingID(&routing_id);
  return routing_id;
}

void RenderThreadImpl::AddFilter(IPC::MessageFilter* filter) {
  channel()->AddFilter(filter);
}

void RenderThreadImpl::RemoveFilter(IPC::MessageFilter* filter) {
  channel()->RemoveFilter(filter);
}

void RenderThreadImpl::AddObserver(RenderThreadObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderThreadImpl::RemoveObserver(RenderThreadObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RenderThreadImpl::SetResourceDispatcherDelegate(
    ResourceDispatcherDelegate* delegate) {
  resource_dispatcher()->set_delegate(delegate);
}

void RenderThreadImpl::InitializeCompositorThread() {
  base::Thread::Options options;
#if defined(OS_ANDROID)
  options.priority = base::ThreadPriority::DISPLAY;
#endif
  compositor_thread_.reset(new WebThreadForCompositor(options));
  blink_platform_impl_->SetCompositorThread(compositor_thread_.get());
  compositor_task_runner_ = compositor_thread_->GetTaskRunner();
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&ThreadRestrictions::SetIOAllowed), false));
#if defined(OS_LINUX)
  ChildThreadImpl::current()->SetThreadPriority(compositor_thread_->threadId(),
                                                base::ThreadPriority::DISPLAY);
#endif

  SynchronousInputHandlerProxyClient* synchronous_input_handler_proxy_client =
      nullptr;
#if defined(OS_ANDROID)
  if (GetContentClient()->UsingSynchronousCompositing()) {
    sync_compositor_message_filter_ =
        new SynchronousCompositorFilter(compositor_task_runner_);
    AddFilter(sync_compositor_message_filter_.get());
    synchronous_input_handler_proxy_client =
        sync_compositor_message_filter_.get();
  }
#endif
  scoped_refptr<InputEventFilter> compositor_input_event_filter(
      new InputEventFilter(main_input_callback_.callback(),
                           main_thread_compositor_task_runner_,
                           compositor_task_runner_));
  InputHandlerManagerClient* input_handler_manager_client =
      compositor_input_event_filter.get();
  input_event_filter_ = compositor_input_event_filter;
  input_handler_manager_.reset(new InputHandlerManager(
      compositor_task_runner_, input_handler_manager_client,
      synchronous_input_handler_proxy_client, renderer_scheduler_.get()));
}

void RenderThreadImpl::InitializeWebKit(
    scoped_refptr<base::SingleThreadTaskRunner>& resource_task_queue) {
  DCHECK(!blink_platform_impl_);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

#ifdef ENABLE_VTUNE_JIT_INTERFACE
  if (command_line.HasSwitch(switches::kEnableVtune))
    gin::Debug::SetJitCodeEventHandler(vTune::GetVtuneCodeEventHandler());
#endif

  SetRuntimeFeaturesDefaultsAndUpdateFromArgs(command_line);
  GetContentClient()
      ->renderer()
      ->SetRuntimeFeaturesDefaultsBeforeBlinkInitialization();

  blink_platform_impl_.reset(new RendererBlinkPlatformImpl(
      renderer_scheduler_.get(),
      GetRemoteInterfaces()->GetWeakPtr()));
  blink::initialize(blink_platform_impl_.get());

  v8::Isolate* isolate = blink::mainThreadIsolate();
  isolate->SetCreateHistogramFunction(CreateHistogram);
  isolate->SetAddHistogramSampleFunction(AddHistogramSample);
  renderer_scheduler_->SetRAILModeObserver(this);

  main_thread_compositor_task_runner_ =
      renderer_scheduler_->CompositorTaskRunner();

  main_input_callback_.Reset(
      base::Bind(base::IgnoreResult(&RenderThreadImpl::OnMessageReceived),
                 base::Unretained(this)));

  scoped_refptr<base::SingleThreadTaskRunner> resource_task_queue2;
  if (resource_task_queue) {
    resource_task_queue2 = resource_task_queue;
  } else {
    resource_task_queue2 = renderer_scheduler_->LoadingTaskRunner();
  }
  // Add a filter that forces resource messages to be dispatched via a
  // particular task runner.
  scoped_refptr<ResourceSchedulingFilter> filter(
      new ResourceSchedulingFilter(
          resource_task_queue2, resource_dispatcher()));
  channel()->AddFilter(filter.get());
  resource_dispatcher()->SetResourceSchedulingFilter(filter);

  // The ChildResourceMessageFilter and the ResourceDispatcher need to use the
  // same queue to ensure tasks are executed in the expected order.
  child_resource_message_filter()->SetMainThreadTaskRunner(
      resource_task_queue2);
  resource_dispatcher()->SetMainThreadTaskRunner(resource_task_queue2);

  if (!command_line.HasSwitch(switches::kDisableThreadedCompositing) &&
      !command_line.HasSwitch(switches::kUseRemoteCompositing))
    InitializeCompositorThread();

  if (!input_event_filter_.get()) {
    // Always provide an input event filter implementation to ensure consistent
    // input event scheduling and prioritization.
    // TODO(jdduke): Merge InputEventFilter, InputHandlerManager and
    // MainThreadInputEventFilter, crbug.com/436057.
    input_event_filter_ = new MainThreadInputEventFilter(
        main_input_callback_.callback(), main_thread_compositor_task_runner_);
  }
  AddFilter(input_event_filter_.get());

  scoped_refptr<base::SingleThreadTaskRunner> compositor_impl_side_task_runner;
  if (compositor_task_runner_)
    compositor_impl_side_task_runner = compositor_task_runner_;
  else
    compositor_impl_side_task_runner = base::ThreadTaskRunnerHandle::Get();

  compositor_message_filter_ = new CompositorForwardingMessageFilter(
      compositor_impl_side_task_runner.get());
  AddFilter(compositor_message_filter_.get());

  RenderThreadImpl::RegisterSchemes();

  RenderMediaClient::Initialize();

  devtools_agent_message_filter_ = new DevToolsAgentFilter();
  AddFilter(devtools_agent_message_filter_.get());

  if (GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden()) {
    ScheduleIdleHandler(kLongIdleHandlerDelayMs);
  } else {
    // If we do not track widget visibility, then assume conservatively that
    // the isolate is in background. This reduces memory usage.
    isolate->IsolateInBackgroundNotification();
  }

  renderer_scheduler_->SetTimerQueueSuspensionWhenBackgroundedEnabled(
      GetContentClient()
          ->renderer()
          ->AllowTimerSuspensionWhenProcessBackgrounded());

  SkGraphics::SetResourceCacheSingleAllocationByteLimit(
      kImageCacheSingleAllocationByteLimit);

  // Hook up blink's codecs so skia can call them
  SkGraphics::SetImageGeneratorFromEncodedFactory(
      blink::WebImageGenerator::create);

  if (command_line.HasSwitch(switches::kMemoryMetrics)) {
    memory_observer_.reset(new MemoryObserver());
    message_loop()->AddTaskObserver(memory_observer_.get());
  }

  if (command_line.HasSwitch(switches::kExplicitlyAllowedPorts)) {
    std::string allowed_ports =
        command_line.GetSwitchValueASCII(switches::kExplicitlyAllowedPorts);
    net::SetExplicitlyAllowedPorts(allowed_ports);
  }
}

void RenderThreadImpl::RegisterSchemes() {
  // chrome:
  WebString chrome_scheme(WebString::fromASCII(kChromeUIScheme));
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(chrome_scheme);
  WebSecurityPolicy::registerURLSchemeAsNotAllowingJavascriptURLs(
      chrome_scheme);
  WebSecurityPolicy::registerURLSchemeAsSecure(chrome_scheme);
  WebSecurityPolicy::registerURLSchemeAsCORSEnabled(chrome_scheme);

  // chrome-devtools:
  WebString devtools_scheme(WebString::fromASCII(kChromeDevToolsScheme));
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(devtools_scheme);

  // view-source:
  WebString view_source_scheme(WebString::fromASCII(kViewSourceScheme));
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(view_source_scheme);
}

void RenderThreadImpl::RecordAction(const base::UserMetricsAction& action) {
  Send(new ViewHostMsg_UserMetricsRecordAction(action.str_));
}

void RenderThreadImpl::RecordComputedAction(const std::string& action) {
  Send(new ViewHostMsg_UserMetricsRecordAction(action));
}

std::unique_ptr<base::SharedMemory>
RenderThreadImpl::HostAllocateSharedMemoryBuffer(size_t size) {
  return ChildThreadImpl::AllocateSharedMemory(size, thread_safe_sender(),
                                               nullptr);
}

cc::SharedBitmapManager* RenderThreadImpl::GetSharedBitmapManager() {
  return shared_bitmap_manager();
}

void RenderThreadImpl::RegisterExtension(v8::Extension* extension) {
  WebScriptController::registerExtension(extension);
}

void RenderThreadImpl::ScheduleIdleHandler(int64_t initial_delay_ms) {
  idle_notification_delay_in_ms_ = initial_delay_ms;
  idle_timer_.Stop();
  idle_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(initial_delay_ms),
      this, &RenderThreadImpl::IdleHandler);
}

void RenderThreadImpl::IdleHandler() {
  bool run_in_foreground_tab = (widget_count_ > hidden_widget_count_) &&
                               GetContentClient()->renderer()->
                                   RunIdleHandlerWhenWidgetsHidden();
  if (run_in_foreground_tab) {
    if (idle_notifications_to_skip_ > 0) {
      --idle_notifications_to_skip_;
    } else {
      ReleaseFreeMemory();
    }
    ScheduleIdleHandler(kLongIdleHandlerDelayMs);
    return;
  }

  ReleaseFreeMemory();

  // Continue the idle timer if the webkit shared timer is not suspended or
  // something is left to do.
  bool continue_timer = !webkit_shared_timer_suspended_;

  // Schedule next invocation. When the tab is originally hidden, an invocation
  // is scheduled for kInitialIdleHandlerDelayMs in
  // RenderThreadImpl::WidgetHidden in order to race to a minimal heap.
  // After that, idle calls can be much less frequent, so run at a maximum of
  // once every kLongIdleHandlerDelayMs.
  // Dampen the delay using the algorithm (if delay is in seconds):
  //    delay = delay + 1 / (delay + 2)
  // Using floor(delay) has a dampening effect such as:
  //    30s, 30, 30, 31, 31, 31, 31, 32, 32, ...
  // If the delay is in milliseconds, the above formula is equivalent to:
  //    delay_ms / 1000 = delay_ms / 1000 + 1 / (delay_ms / 1000 + 2)
  // which is equivalent to
  //    delay_ms = delay_ms + 1000*1000 / (delay_ms + 2000).
  if (continue_timer) {
    ScheduleIdleHandler(
        std::max(kLongIdleHandlerDelayMs,
                 idle_notification_delay_in_ms_ +
                 1000000 / (idle_notification_delay_in_ms_ + 2000)));

  } else {
    idle_timer_.Stop();
  }

  for (auto& observer : observers_)
    observer.IdleNotification();
}

int64_t RenderThreadImpl::GetIdleNotificationDelayInMs() const {
  return idle_notification_delay_in_ms_;
}

void RenderThreadImpl::SetIdleNotificationDelayInMs(
    int64_t idle_notification_delay_in_ms) {
  idle_notification_delay_in_ms_ = idle_notification_delay_in_ms;
}

int RenderThreadImpl::PostTaskToAllWebWorkers(const base::Closure& closure) {
  return WorkerThreadRegistry::Instance()->PostTaskToAllThreads(closure);
}

bool RenderThreadImpl::ResolveProxy(const GURL& url, std::string* proxy_list) {
  bool result = false;
  Send(new ViewHostMsg_ResolveProxy(url, &result, proxy_list));
  return result;
}

void RenderThreadImpl::PostponeIdleNotification() {
  idle_notifications_to_skip_ = 2;
}

media::GpuVideoAcceleratorFactories* RenderThreadImpl::GetGpuFactories() {
  DCHECK(IsMainThread());

  if (!gpu_factories_.empty()) {
    scoped_refptr<ContextProviderCommandBuffer> shared_context_provider =
        gpu_factories_.back()->ContextProviderMainThread();
    if (shared_context_provider) {
      cc::ContextProvider::ScopedContextLock lock(
          shared_context_provider.get());
      if (lock.ContextGL()->GetGraphicsResetStatusKHR() == GL_NO_ERROR) {
        return gpu_factories_.back();
      } else {
        scoped_refptr<base::SingleThreadTaskRunner> media_task_runner =
            GetMediaThreadTaskRunner();
        media_task_runner->PostTask(
            FROM_HERE,
            base::Bind(
                base::IgnoreResult(
                    &RendererGpuVideoAcceleratorFactories::CheckContextLost),
                base::Unretained(gpu_factories_.back())));
      }
    }
  }

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
      EstablishGpuChannelSync();
  if (!gpu_channel_host)
    return nullptr;
  // This context is only used to create textures and mailbox them, so
  // use lower limits than the default.
  gpu::SharedMemoryLimits limits = gpu::SharedMemoryLimits::ForMailboxContext();
  bool support_locking = true;
  scoped_refptr<ContextProviderCommandBuffer> media_context_provider =
      CreateOffscreenContext(gpu_channel_host, limits, support_locking,
                             command_buffer_metrics::RENDER_WORKER_CONTEXT,
                             gpu::GPU_STREAM_DEFAULT,
                             gpu::GpuStreamPriority::NORMAL);
  if (!media_context_provider->BindToCurrentThread())
    return nullptr;

  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner =
      GetMediaThreadTaskRunner();
  const bool enable_video_accelerator =
      !cmd_line->HasSwitch(switches::kDisableAcceleratedVideoDecode);
  const bool enable_gpu_memory_buffer_video_frames =
#if defined(OS_MACOSX) || defined(OS_LINUX)
      !cmd_line->HasSwitch(switches::kDisableGpuMemoryBufferVideoFrames) &&
      !cmd_line->HasSwitch(switches::kDisableGpuCompositing) &&
      !gpu_channel_host->gpu_info().software_rendering;
#else
      cmd_line->HasSwitch(switches::kEnableGpuMemoryBufferVideoFrames);
#endif

  gpu_factories_.push_back(RendererGpuVideoAcceleratorFactories::Create(
      std::move(gpu_channel_host), base::ThreadTaskRunnerHandle::Get(),
      media_task_runner, std::move(media_context_provider),
      enable_gpu_memory_buffer_video_frames, buffer_to_texture_target_map_,
      enable_video_accelerator));
  return gpu_factories_.back();
}

scoped_refptr<ContextProviderCommandBuffer>
RenderThreadImpl::SharedMainThreadContextProvider() {
  DCHECK(IsMainThread());
  if (shared_main_thread_contexts_ &&
      shared_main_thread_contexts_->ContextGL()->GetGraphicsResetStatusKHR() ==
          GL_NO_ERROR)
    return shared_main_thread_contexts_;

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      EstablishGpuChannelSync());
  if (!gpu_channel_host) {
    shared_main_thread_contexts_ = nullptr;
    return nullptr;
  }

  bool support_locking = false;
  shared_main_thread_contexts_ = CreateOffscreenContext(
      std::move(gpu_channel_host), gpu::SharedMemoryLimits(), support_locking,
      command_buffer_metrics::RENDERER_MAINTHREAD_CONTEXT,
      gpu::GPU_STREAM_DEFAULT, gpu::GpuStreamPriority::NORMAL);
  if (!shared_main_thread_contexts_->BindToCurrentThread())
    shared_main_thread_contexts_ = nullptr;
  return shared_main_thread_contexts_;
}

#if defined(OS_ANDROID)

scoped_refptr<StreamTextureFactory> RenderThreadImpl::GetStreamTexureFactory() {
  DCHECK(IsMainThread());
  if (!stream_texture_factory_.get() ||
      stream_texture_factory_->ContextGL()->GetGraphicsResetStatusKHR() !=
          GL_NO_ERROR) {
    scoped_refptr<ContextProviderCommandBuffer> shared_context_provider =
        SharedMainThreadContextProvider();
    if (!shared_context_provider) {
      stream_texture_factory_ = nullptr;
      return nullptr;
    }
    DCHECK(shared_context_provider->GetCommandBufferProxy());
    DCHECK(shared_context_provider->GetCommandBufferProxy()->channel());
    stream_texture_factory_ =
        StreamTextureFactory::Create(std::move(shared_context_provider));
  }
  return stream_texture_factory_;
}

bool RenderThreadImpl::EnableStreamTextureCopy() {
  return sync_compositor_message_filter_.get();
}

#endif

AudioRendererMixerManager* RenderThreadImpl::GetAudioRendererMixerManager() {
  if (!audio_renderer_mixer_manager_) {
    audio_renderer_mixer_manager_ = AudioRendererMixerManager::Create();
  }

  return audio_renderer_mixer_manager_.get();
}

base::WaitableEvent* RenderThreadImpl::GetShutdownEvent() {
  return ChildProcess::current()->GetShutDownEvent();
}

int32_t RenderThreadImpl::GetClientId() {
  return client_id_;
}

void RenderThreadImpl::OnAssociatedInterfaceRequest(
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  associated_interfaces_.BindRequest(name, std::move(handle));
}

bool RenderThreadImpl::IsGpuRasterizationForced() {
  return is_gpu_rasterization_forced_;
}

bool RenderThreadImpl::IsGpuRasterizationEnabled() {
  return is_gpu_rasterization_enabled_;
}

bool RenderThreadImpl::IsAsyncWorkerContextEnabled() {
  return is_async_worker_context_enabled_;
}

int RenderThreadImpl::GetGpuRasterizationMSAASampleCount() {
  return gpu_rasterization_msaa_sample_count_;
}

bool RenderThreadImpl::IsLcdTextEnabled() {
  return is_lcd_text_enabled_;
}

bool RenderThreadImpl::IsDistanceFieldTextEnabled() {
  return is_distance_field_text_enabled_;
}

bool RenderThreadImpl::IsZeroCopyEnabled() {
  return is_zero_copy_enabled_;
}

bool RenderThreadImpl::IsPartialRasterEnabled() {
  return is_partial_raster_enabled_;
}

bool RenderThreadImpl::IsGpuMemoryBufferCompositorResourcesEnabled() {
  return is_gpu_memory_buffer_compositor_resources_enabled_;
}

bool RenderThreadImpl::IsElasticOverscrollEnabled() {
  return is_elastic_overscroll_enabled_;
}

const cc::BufferToTextureTargetMap&
RenderThreadImpl::GetBufferToTextureTargetMap() {
  return buffer_to_texture_target_map_;
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetCompositorMainThreadTaskRunner() {
  return main_thread_compositor_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetCompositorImplThreadTaskRunner() {
  return compositor_task_runner_;
}

gpu::GpuMemoryBufferManager* RenderThreadImpl::GetGpuMemoryBufferManager() {
#if defined(USE_AURA) && !defined(TOOLKIT_QT)
  if (gpu_service_)
    return gpu_service_->gpu_memory_buffer_manager();
#endif
  return gpu_memory_buffer_manager_.get();
}

blink::scheduler::RendererScheduler* RenderThreadImpl::GetRendererScheduler() {
  return renderer_scheduler_.get();
}

std::unique_ptr<cc::BeginFrameSource>
RenderThreadImpl::CreateExternalBeginFrameSource(int routing_id) {
  const base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(switches::kDisableGpuVsync)) {
    std::string display_vsync_string =
        cmd->GetSwitchValueASCII(switches::kDisableGpuVsync);
    if (display_vsync_string != "gpu") {
      // In disable gpu vsync mode, also let the renderer tick as fast as it
      // can.  The top level begin frame source will also be running as a back
      // to back begin frame source, but using a synthetic begin frame source
      // here reduces latency when in this mode (at least for frames
      // starting--it potentially increases it for input on the other hand.)
      base::SingleThreadTaskRunner* compositor_impl_side_task_runner =
          compositor_task_runner_ ? compositor_task_runner_.get()
                                  : base::ThreadTaskRunnerHandle::Get().get();
      return base::MakeUnique<cc::BackToBackBeginFrameSource>(
          base::MakeUnique<cc::DelayBasedTimeSource>(
              compositor_impl_side_task_runner));
    }
  }

  return base::MakeUnique<CompositorExternalBeginFrameSource>(
      compositor_message_filter_.get(), sync_message_filter(), routing_id);
}

cc::ImageSerializationProcessor*
RenderThreadImpl::GetImageSerializationProcessor() {
  return GetContentClient()->renderer()->GetImageSerializationProcessor();
}

cc::TaskGraphRunner* RenderThreadImpl::GetTaskGraphRunner() {
  return categorized_worker_pool_->GetTaskGraphRunner();
}

bool RenderThreadImpl::AreImageDecodeTasksEnabled() {
  return are_image_decode_tasks_enabled_;
}

bool RenderThreadImpl::IsThreadedAnimationEnabled() {
  return is_threaded_animation_enabled_;
}

void RenderThreadImpl::OnRAILModeChanged(v8::RAILMode rail_mode) {
  blink::mainThreadIsolate()->SetRAILMode(rail_mode);
  blink::setRAILModeOnWorkerThreadIsolates(rail_mode);
}

bool RenderThreadImpl::IsMainThread() {
  return !!current();
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetIOThreadTaskRunner() {
  return io_thread_task_runner_;
}

std::unique_ptr<base::SharedMemory> RenderThreadImpl::AllocateSharedMemory(
    size_t size) {
  return HostAllocateSharedMemoryBuffer(size);
}

void RenderThreadImpl::OnChannelError() {
  // In single-process mode, the renderer can't be restarted after shutdown.
  // So, if we get a channel error, crash the whole process right now to get a
  // more informative stack, since we will otherwise just crash later when we
  // try to restart it.
  CHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess));
  ChildThreadImpl::OnChannelError();
}

bool RenderThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  for (auto& observer : observers_) {
    if (observer.OnControlMessageReceived(msg))
      return true;
  }

  // Some messages are handled by delegates.
  if (appcache_dispatcher_->OnMessageReceived(msg) ||
      dom_storage_dispatcher_->OnMessageReceived(msg) ||
      embedded_worker_dispatcher_->OnMessageReceived(msg)) {
    return true;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderThreadImpl, msg)
    IPC_MESSAGE_HANDLER(WorkerProcessMsg_CreateWorker, OnCreateNewSharedWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderThreadImpl::OnProcessBackgrounded(bool backgrounded) {
  ChildThreadImpl::OnProcessBackgrounded(backgrounded);

  if (backgrounded) {
    renderer_scheduler_->OnRendererBackgrounded();
  } else {
    renderer_scheduler_->OnRendererForegrounded();
    // TODO(tasak): after enabling MemoryCoordinator, remove this Notify
    // and follow MemoryCoordinator's request.
    if (base::FeatureList::IsEnabled(features::kPurgeAndSuspend))
      base::MemoryCoordinatorClientRegistry::GetInstance()->Notify(
          base::MemoryState::NORMAL);

    record_purge_suspend_metric_closure_.Cancel();
    record_purge_suspend_metric_closure_.Reset(
        base::Bind(&RenderThreadImpl::RecordPurgeAndSuspendMetrics,
                   base::Unretained(this)));
    is_renderer_suspended_ = false;
  }
}

void RenderThreadImpl::OnProcessPurgeAndSuspend() {
  ChildThreadImpl::OnProcessPurgeAndSuspend();
  DCHECK(!is_renderer_suspended_);
  if (!RendererIsHidden())
    return;
  is_renderer_suspended_ = true;
  if (base::FeatureList::IsEnabled(features::kPurgeAndSuspend)) {
    // TODO(tasak): After enabling MemoryCoordinator, remove this Notify
    // and follow MemoryCoordinator's request.
    base::MemoryCoordinatorClientRegistry::GetInstance()->Notify(
        base::MemoryState::SUSPENDED);
    renderer_scheduler_->SuspendRenderer();
  }

  // Since purging is not a synchronous task (e.g. v8 GC, oilpan GC, ...),
  // we need to wait until the task is finished. So wait 15 seconds and
  // update purge+suspend UMA histogram.
  // TODO(tasak): use MemoryCoordinator's callback to report purge+suspend
  // UMA when MemoryCoordinator is available.
  GetRendererScheduler()->DefaultTaskRunner()->PostDelayedTask(
      FROM_HERE, record_purge_suspend_metric_closure_.callback(),
      base::TimeDelta::FromSeconds(15));
}

// TODO(tasak): Replace the following GetMallocUsage() with memory-infra
// when it is possible to run memory-infra without tracing.
#if defined(OS_WIN)
namespace {

static size_t GetMallocUsage() {
  DWORD number_of_heaps = ::GetProcessHeaps(0, NULL);
  if (number_of_heaps <= 0)
    return 0;

  size_t malloc_usage = 0;
  std::unique_ptr<HANDLE[]> heaps(new HANDLE[number_of_heaps]);
  ::GetProcessHeaps(number_of_heaps, heaps.get());
  for (size_t i = 0; i < number_of_heaps; i++) {
    PROCESS_HEAP_ENTRY heap_entry;
    ::HeapLock(heaps[i]);
    heap_entry.lpData = NULL;
    while (::HeapWalk(heaps[i], &heap_entry) != 0) {
      if (heap_entry.lpData == heaps.get())
        continue;
      if ((heap_entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) != 0)
        malloc_usage += heap_entry.cbData;
    }
    ::HeapUnlock(heaps[i]);
  }
  return malloc_usage;
}

}  // namespace
#elif defined(OS_MACOSX) || defined(OS_IOS)
namespace {

static size_t GetMallocUsage() {
  malloc_statistics_t stats = {0};
  malloc_zone_statistics(nullptr, &stats);
  return stats.size_in_use;
}

}  // namespace
#endif

// TODO(tasak): Once it is possible to use memory-infra without tracing,
// we should collect the metrics using memory-infra.
// TODO(tasak): We should also report a difference between the memory usages
// before and after purging by using memory-infra.
void RenderThreadImpl::RecordPurgeAndSuspendMetrics() const {
  // If this renderer is resumed, we should not update UMA.
  if (!is_renderer_suspended_)
    return;

  // TODO(tasak): Compare memory metrics between purge-enabled renderers and
  // purge-disabled renderers (A/B testing).
  blink::WebMemoryStatistics blink_stats = blink::WebMemoryStatistics::Get();
  UMA_HISTOGRAM_MEMORY_KB("PurgeAndSuspend.Memory.PartitionAllocKB",
                          blink_stats.partitionAllocTotalAllocatedBytes / 1024);
  UMA_HISTOGRAM_MEMORY_KB("PurgeAndSuspend.Memory.BlinkGCKB",
                          blink_stats.blinkGCTotalAllocatedBytes / 1024);
#if defined(OS_LINUX) || defined(OS_ANDROID)
  struct mallinfo minfo = mallinfo();
#if defined(USE_TCMALLOC)
  size_t malloc_usage = minfo.uordblks;
#else
  size_t malloc_usage = minfo.hblkhd + minfo.arena;
#endif
#else
  size_t malloc_usage = GetMallocUsage();
#endif
  UMA_HISTOGRAM_MEMORY_MB("PurgeAndSuspend.Memory.MallocMB",
                          malloc_usage / 1024 / 1024);

  discardable_memory::ClientDiscardableSharedMemoryManager::Statistics
      discardable_stats =
          ChildThreadImpl::discardable_shared_memory_manager()->GetStatistics();
  size_t discardable_usage =
      discardable_stats.total_size - discardable_stats.freelist_size;
  UMA_HISTOGRAM_MEMORY_KB("PurgeAndSuspend.Memory.DiscardableKB",
                          discardable_usage / 1024);

  size_t v8_usage = 0;
  if (v8::Isolate* isolate = blink::mainThreadIsolate()) {
    v8::HeapStatistics v8_heap_statistics;
    isolate->GetHeapStatistics(&v8_heap_statistics);
    v8_usage = v8_heap_statistics.total_heap_size();
  }
  // TODO(tasak): Currently only memory usage of mainThreadIsolate() is
  // reported. We should collect memory usages of all isolates using
  // memory-infra.
  UMA_HISTOGRAM_MEMORY_MB("PurgeAndSuspend.Memory.V8MainThreadIsolateMB",
                          v8_usage / 1024 / 1024);
  UMA_HISTOGRAM_MEMORY_MB("PurgeAndSuspend.Memory.TotalAllocatedMB",
                          (blink_stats.partitionAllocTotalAllocatedBytes +
                           blink_stats.blinkGCTotalAllocatedBytes +
                           malloc_usage + v8_usage + discardable_usage) /
                              1024 / 1024);
}

void RenderThreadImpl::OnProcessResume() {
  ChildThreadImpl::OnProcessResume();

  DCHECK(is_renderer_suspended_);
  is_renderer_suspended_ = false;
  if (base::FeatureList::IsEnabled(features::kPurgeAndSuspend)) {
    // TODO(tasak): after enabling MemoryCoordinator, remove this Notify
    // and follow MemoryCoordinator's request.
    base::MemoryCoordinatorClientRegistry::GetInstance()->Notify(
        base::MemoryState::NORMAL);
    renderer_scheduler_->ResumeRenderer();
  }
}

scoped_refptr<gpu::GpuChannelHost> RenderThreadImpl::EstablishGpuChannelSync() {
  TRACE_EVENT0("gpu", "RenderThreadImpl::EstablishGpuChannelSync");

  if (gpu_channel_) {
    // Do nothing if we already have a GPU channel or are already
    // establishing one.
    if (!gpu_channel_->IsLost())
      return gpu_channel_;

    // Recreate the channel if it has been lost.
    gpu_channel_->DestroyChannel();
    gpu_channel_ = nullptr;
  }

  if (!IsRunningInMash()) {
    int client_id = 0;
    IPC::ChannelHandle channel_handle;
    gpu::GPUInfo gpu_info;
    // Ask the browser for the channel name.
    if (!Send(new ChildProcessHostMsg_EstablishGpuChannel(
            &client_id, &channel_handle, &gpu_info)) ||
        !channel_handle.mojo_handle.is_valid()) {
      // Otherwise cancel the connection.
      return nullptr;
    }
    GetContentClient()->SetGpuInfo(gpu_info);

    // Cache some variables that are needed on the compositor thread for our
    // implementation of GpuChannelHostFactory.
    io_thread_task_runner_ = ChildProcess::current()->io_task_runner();

    gpu_channel_ =
        gpu::GpuChannelHost::Create(this, client_id, gpu_info, channel_handle,
                                    ChildProcess::current()->GetShutDownEvent(),
                                    GetGpuMemoryBufferManager());
  } else {
#if defined(USE_AURA) && !defined(TOOLKIT_QT)
    gpu_channel_ = gpu_service_->EstablishGpuChannelSync();
#else
    NOTREACHED();
#endif
  }
  return gpu_channel_;
}

std::unique_ptr<cc::CompositorFrameSink>
RenderThreadImpl::CreateCompositorFrameSink(
    bool use_software,
    int routing_id,
    scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue,
    const GURL& url) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisableGpuCompositing))
    use_software = true;

#if defined(USE_AURA) && !defined(TOOLKIT_QT)
  if (GetServiceManagerConnection() && !use_software &&
      command_line.HasSwitch(switches::kUseMusInRenderer)) {
    RenderWidgetMusConnection* connection =
        RenderWidgetMusConnection::GetOrCreate(routing_id);
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
        EstablishGpuChannelSync();
    return connection->CreateCompositorFrameSink(std::move(gpu_channel_host),
                                                 GetGpuMemoryBufferManager());
  }
#endif

  uint32_t compositor_frame_sink_id = g_next_compositor_frame_sink_id++;

  if (command_line.HasSwitch(switches::kEnableVulkan)) {
    scoped_refptr<cc::VulkanContextProvider> vulkan_context_provider =
        cc::VulkanInProcessContextProvider::Create();
    if (vulkan_context_provider) {
      DCHECK(!layout_test_mode());
      return base::MakeUnique<RendererCompositorFrameSink>(
          routing_id, compositor_frame_sink_id,
          CreateExternalBeginFrameSource(routing_id),
          std::move(vulkan_context_provider),
          std::move(frame_swap_message_queue));
    }
  }

  // Create a gpu process channel and verify we want to use GPU compositing
  // before creating any context providers.
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host;
  if (!use_software) {
    gpu_channel_host = EstablishGpuChannelSync();
    if (!gpu_channel_host) {
      // Cause the compositor to wait and try again.
      return nullptr;
    }
    // We may get a valid channel, but with a software renderer. In that case,
    // disable GPU compositing.
    if (gpu_channel_host->gpu_info().software_rendering)
      use_software = true;
  }

  if (use_software) {
    DCHECK(!layout_test_mode());
    return base::MakeUnique<RendererCompositorFrameSink>(
        routing_id, compositor_frame_sink_id,
        CreateExternalBeginFrameSource(routing_id), nullptr, nullptr, nullptr,
        shared_bitmap_manager(), std::move(frame_swap_message_queue));
  }

  scoped_refptr<ContextProviderCommandBuffer> worker_context_provider =
      SharedCompositorWorkerContextProvider();
  if (!worker_context_provider) {
    // Cause the compositor to wait and try again.
    return nullptr;
  }

  // The renderer compositor context doesn't do a lot of stuff, so we don't
  // expect it to need a lot of space for commands or transfer. Raster and
  // uploads happen on the worker context instead.
  gpu::SharedMemoryLimits limits = gpu::SharedMemoryLimits::ForMailboxContext();

  // This is for an offscreen context for the compositor. So the default
  // framebuffer doesn't need alpha, depth, stencil, antialiasing.
  gpu::gles2::ContextCreationAttribHelper attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;

  constexpr bool automatic_flushes = false;
  constexpr bool support_locking = false;

  // The compositor context shares resources with the worker context unless
  // the worker is async.
  ContextProviderCommandBuffer* share_context = worker_context_provider.get();
  if (IsAsyncWorkerContextEnabled())
    share_context = nullptr;

  scoped_refptr<ContextProviderCommandBuffer> context_provider(
      new ContextProviderCommandBuffer(
          gpu_channel_host, gpu::GPU_STREAM_DEFAULT,
          gpu::GpuStreamPriority::NORMAL, gpu::kNullSurfaceHandle, url,
          automatic_flushes, support_locking, limits, attributes, share_context,
          command_buffer_metrics::RENDER_COMPOSITOR_CONTEXT));

  if (layout_test_deps_) {
    return layout_test_deps_->CreateCompositorFrameSink(
        routing_id, std::move(gpu_channel_host), std::move(context_provider),
        std::move(worker_context_provider), GetGpuMemoryBufferManager(),
        this);
  }

#if defined(OS_ANDROID)
  if (sync_compositor_message_filter_) {
    return base::MakeUnique<SynchronousCompositorFrameSink>(
        std::move(context_provider), std::move(worker_context_provider),
        GetGpuMemoryBufferManager(), routing_id, compositor_frame_sink_id,
        CreateExternalBeginFrameSource(routing_id),
        sync_compositor_message_filter_.get(),
        std::move(frame_swap_message_queue));
  }
#endif
  return base::WrapUnique(new RendererCompositorFrameSink(
      routing_id, compositor_frame_sink_id,
      CreateExternalBeginFrameSource(routing_id), std::move(context_provider),
      std::move(worker_context_provider), GetGpuMemoryBufferManager(),
      nullptr, std::move(frame_swap_message_queue)));
}

AssociatedInterfaceRegistry*
RenderThreadImpl::GetAssociatedInterfaceRegistry() {
  return &associated_interfaces_;
}

std::unique_ptr<cc::SwapPromise>
RenderThreadImpl::RequestCopyOfOutputForLayoutTest(
    int32_t routing_id,
    std::unique_ptr<cc::CopyOutputRequest> request) {
  DCHECK(layout_test_deps_);
  return layout_test_deps_->RequestCopyOfOutput(routing_id, std::move(request));
}

blink::WebMediaStreamCenter* RenderThreadImpl::CreateMediaStreamCenter(
    blink::WebMediaStreamCenterClient* client) {
#if defined(ENABLE_WEBRTC)
  if (!media_stream_center_) {
    media_stream_center_ = GetContentClient()->renderer()
        ->OverrideCreateWebMediaStreamCenter(client);
    if (!media_stream_center_) {
      std::unique_ptr<MediaStreamCenter> media_stream_center(
          new MediaStreamCenter(client, GetPeerConnectionDependencyFactory()));
      media_stream_center_ = media_stream_center.release();
    }
  }
#endif
  return media_stream_center_;
}

#if defined(ENABLE_WEBRTC)
PeerConnectionDependencyFactory*
RenderThreadImpl::GetPeerConnectionDependencyFactory() {
  return peer_connection_factory_.get();
}
#endif

mojom::RenderFrameMessageFilter*
RenderThreadImpl::render_frame_message_filter() {
  if (!render_frame_message_filter_)
    GetChannel()->GetRemoteAssociatedInterface(&render_frame_message_filter_);
  return render_frame_message_filter_.get();
}

mojom::RenderMessageFilter* RenderThreadImpl::render_message_filter() {
  if (!render_message_filter_)
    GetChannel()->GetRemoteAssociatedInterface(&render_message_filter_);
  return render_message_filter_.get();
}

gpu::GpuChannelHost* RenderThreadImpl::GetGpuChannel() {
  if (!gpu_channel_)
    return nullptr;
  if (gpu_channel_->IsLost())
    return nullptr;
  return gpu_channel_.get();
}

void RenderThreadImpl::CreateView(mojom::CreateViewParamsPtr params) {
  CompositorDependencies* compositor_deps = this;
  // When bringing in render_view, also bring in webkit's glue and jsbindings.
  RenderViewImpl::Create(compositor_deps, *params, false);
}

void RenderThreadImpl::CreateFrame(mojom::CreateFrameParamsPtr params) {
  // Debug cases of https://crbug.com/626802.
  base::debug::SetCrashKeyValue("newframe_routing_id",
                                base::IntToString(params->routing_id));
  base::debug::SetCrashKeyValue("newframe_proxy_id",
                                base::IntToString(params->proxy_routing_id));
  base::debug::SetCrashKeyValue("newframe_opener_id",
                                base::IntToString(params->opener_routing_id));
  base::debug::SetCrashKeyValue("newframe_parent_id",
                                base::IntToString(params->parent_routing_id));
  base::debug::SetCrashKeyValue("newframe_widget_id",
                                base::IntToString(
                                    params->widget_params->routing_id));
  base::debug::SetCrashKeyValue("newframe_widget_hidden",
                                params->widget_params->hidden ? "yes" : "no");
  base::debug::SetCrashKeyValue("newframe_replicated_origin",
                                params->replication_state.origin.Serialize());
  base::debug::SetCrashKeyValue("newframe_oopifs_possible",
      SiteIsolationPolicy::AreCrossProcessFramesPossible() ? "yes" : "no");
  CompositorDependencies* compositor_deps = this;
  RenderFrameImpl::CreateFrame(
      params->routing_id, params->proxy_routing_id, params->opener_routing_id,
      params->parent_routing_id, params->previous_sibling_routing_id,
      params->replication_state, compositor_deps, *params->widget_params,
      params->frame_owner_properties);
}

void RenderThreadImpl::CreateFrameProxy(
    int32_t routing_id,
    int32_t render_view_routing_id,
    int32_t opener_routing_id,
    int32_t parent_routing_id,
    const FrameReplicationState& replicated_state) {
  // Debug cases of https://crbug.com/575245.
  base::debug::SetCrashKeyValue("newproxy_proxy_id",
                                base::IntToString(routing_id));
  base::debug::SetCrashKeyValue("newproxy_view_id",
                                base::IntToString(render_view_routing_id));
  base::debug::SetCrashKeyValue("newproxy_opener_id",
                                base::IntToString(opener_routing_id));
  base::debug::SetCrashKeyValue("newproxy_parent_id",
                                base::IntToString(parent_routing_id));
  RenderFrameProxy::CreateFrameProxy(routing_id, render_view_routing_id,
                                     opener_routing_id, parent_routing_id,
                                     replicated_state);
}

void RenderThreadImpl::OnNetworkConnectionChanged(
    net::NetworkChangeNotifier::ConnectionType type,
    double max_bandwidth_mbps) {
  bool online = type != net::NetworkChangeNotifier::CONNECTION_NONE;
  WebNetworkStateNotifier::setOnLine(online);
  for (auto& observer : observers_)
    observer.NetworkStateChanged(online);
  WebNetworkStateNotifier::setWebConnection(
      NetConnectionTypeToWebConnectionType(type), max_bandwidth_mbps);
}

void RenderThreadImpl::SetWebKitSharedTimersSuspended(bool suspend) {
#if defined(OS_ANDROID)
  if (suspend) {
    renderer_scheduler_->SuspendTimerQueue();
  } else {
    renderer_scheduler_->ResumeTimerQueue();
  }
  webkit_shared_timer_suspended_ = suspend;
#else
  NOTREACHED();
#endif
}

void RenderThreadImpl::UpdateScrollbarTheme(
    mojom::UpdateScrollbarThemeParamsPtr params) {
#if defined(OS_MACOSX)
  static_cast<WebScrollbarBehaviorImpl*>(
      blink_platform_impl_->scrollbarBehavior())
      ->set_jump_on_track_click(params->jump_on_track_click);

  blink::WebScrollbarTheme::updateScrollbarsWithNSDefaults(
      params->initial_button_delay, params->autoscroll_button_delay,
      params->preferred_scroller_style, params->redraw,
      params->button_placement);
#else
  NOTREACHED();
#endif
}

void RenderThreadImpl::OnSystemColorsChanged(
    int32_t aqua_color_variant,
    const std::string& highlight_text_color,
    const std::string& highlight_color) {
#if defined(OS_MACOSX)
  SystemColorsDidChange(aqua_color_variant, highlight_text_color,
                        highlight_color);
#else
  NOTREACHED();
#endif
}

void RenderThreadImpl::PurgePluginListCache(bool reload_pages) {
#if defined(ENABLE_PLUGINS)
  // The call below will cause a GetPlugins call with refresh=true, but at this
  // point we already know that the browser has refreshed its list, so disable
  // refresh temporarily to prevent each renderer process causing the list to be
  // regenerated.
  blink_platform_impl_->set_plugin_refresh_allowed(false);
  blink::resetPluginCache(reload_pages);
  blink_platform_impl_->set_plugin_refresh_allowed(true);

  for (auto& observer : observers_)
    observer.PluginListChanged();
#else
  NOTREACHED();
#endif
}

void RenderThreadImpl::OnCreateNewSharedWorker(
    const WorkerProcessMsg_CreateWorker_Params& params) {
  // EmbeddedSharedWorkerStub will self-destruct.
  new EmbeddedSharedWorkerStub(
      params.url, params.name, params.content_security_policy,
      params.security_policy_type, params.creation_address_space,
      params.pause_on_start, params.route_id);
}

void RenderThreadImpl::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  TRACE_EVENT0("memory","RenderThreadImpl::OnMemoryPressure");
  if (blink_platform_impl_) {
    blink::WebMemoryCoordinator::onMemoryPressure(
        static_cast<blink::WebMemoryPressureLevel>(memory_pressure_level));
  }
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    ReleaseFreeMemory();
    ClearMemory();
  }
}

void RenderThreadImpl::OnMemoryStateChange(base::MemoryState state) {
  // TODO(hajimehoshi): Adjust the size of this memory usage according to
  // |state|. RenderThreadImpl doesn't have a feature to limit memory usage at
  // present.
  if (blink_platform_impl_) {
    blink::WebMemoryCoordinator::onMemoryStateChange(
        static_cast<blink::MemoryState>(state));
  }
  switch (state) {
    case base::MemoryState::NORMAL:
      break;
    case base::MemoryState::THROTTLED:
      ReleaseFreeMemory();
      break;
    case base::MemoryState::SUSPENDED:
      ReleaseFreeMemory();
      ClearMemory();
      break;
    case base::MemoryState::UNKNOWN:
      NOTREACHED();
      break;
  }
}

void RenderThreadImpl::ClearMemory() {
  // Do not call into blink if it is not initialized.
  if (blink_platform_impl_) {
    // Purge Skia font cache, by setting it to 0 and then again to the
    // previous limit.
    size_t font_cache_limit = SkGraphics::SetFontCacheLimit(0);
    SkGraphics::SetFontCacheLimit(font_cache_limit);
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetFileThreadTaskRunner() {
  DCHECK(message_loop()->task_runner()->BelongsToCurrentThread());
  if (!file_thread_) {
    file_thread_.reset(new base::Thread("Renderer::FILE"));
    file_thread_->Start();
  }
  return file_thread_->task_runner();
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetMediaThreadTaskRunner() {
  DCHECK(message_loop()->task_runner()->BelongsToCurrentThread());
  if (!media_thread_) {
    media_thread_.reset(new base::Thread("Media"));
    media_thread_->Start();
  }
  return media_thread_->task_runner();
}

base::TaskRunner* RenderThreadImpl::GetWorkerTaskRunner() {
  return categorized_worker_pool_.get();
}

scoped_refptr<ContextProviderCommandBuffer>
RenderThreadImpl::SharedCompositorWorkerContextProvider() {
  DCHECK(IsMainThread());
  // Try to reuse existing shared worker context provider.
  if (shared_worker_context_provider_) {
    // Note: If context is lost, delete reference after releasing the lock.
    cc::ContextProvider::ScopedContextLock lock(
        shared_worker_context_provider_.get());
    if (shared_worker_context_provider_->ContextGL()
            ->GetGraphicsResetStatusKHR() == GL_NO_ERROR)
      return shared_worker_context_provider_;
  }

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      EstablishGpuChannelSync());
  if (!gpu_channel_host) {
    shared_worker_context_provider_ = nullptr;
    return shared_worker_context_provider_;
  }

  int32_t stream_id = gpu::GPU_STREAM_DEFAULT;
  gpu::GpuStreamPriority stream_priority = gpu::GpuStreamPriority::NORMAL;
  if (is_async_worker_context_enabled_) {
    stream_id = gpu_channel_host->GenerateStreamID();
    stream_priority = gpu::GpuStreamPriority::LOW;
  }

  bool support_locking = true;
  shared_worker_context_provider_ = CreateOffscreenContext(
      std::move(gpu_channel_host), gpu::SharedMemoryLimits(), support_locking,
      command_buffer_metrics::RENDER_WORKER_CONTEXT, stream_id,
      stream_priority);
  if (!shared_worker_context_provider_->BindToCurrentThread())
    shared_worker_context_provider_ = nullptr;
  return shared_worker_context_provider_;
}

void RenderThreadImpl::SampleGamepads(blink::WebGamepads* data) {
  if (blink_platform_impl_)
    blink_platform_impl_->sampleGamepads(*data);
}

bool RenderThreadImpl::RendererIsHidden() const {
  return widget_count_ > 0 && hidden_widget_count_ == widget_count_;
}

void RenderThreadImpl::WidgetCreated() {
  bool renderer_was_hidden = RendererIsHidden();
  widget_count_++;
  if (renderer_was_hidden)
    OnRendererVisible();
}

void RenderThreadImpl::WidgetDestroyed() {
  // TODO(rmcilroy): Remove the restriction that destroyed widgets must be
  // unhidden before WidgetDestroyed is called.
  DCHECK_GT(widget_count_, 0);
  DCHECK_GT(widget_count_, hidden_widget_count_);
  widget_count_--;
  if (RendererIsHidden())
    OnRendererHidden();
}

void RenderThreadImpl::WidgetHidden() {
  DCHECK_LT(hidden_widget_count_, widget_count_);
  hidden_widget_count_++;
  if (RendererIsHidden())
    OnRendererHidden();
}

void RenderThreadImpl::WidgetRestored() {
  bool renderer_was_hidden = RendererIsHidden();
  DCHECK_GT(hidden_widget_count_, 0);
  hidden_widget_count_--;
  if (renderer_was_hidden)
    OnRendererVisible();
}

void RenderThreadImpl::OnRendererHidden() {
  blink::mainThreadIsolate()->IsolateInBackgroundNotification();
  // TODO(rmcilroy): Remove IdleHandler and replace it with an IdleTask
  // scheduled by the RendererScheduler - http://crbug.com/469210.
  if (!GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden())
    return;
  ScheduleIdleHandler(kInitialIdleHandlerDelayMs);
}

void RenderThreadImpl::OnRendererVisible() {
  blink::mainThreadIsolate()->IsolateInForegroundNotification();
  if (!GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden())
    return;
  ScheduleIdleHandler(kLongIdleHandlerDelayMs);
}

void RenderThreadImpl::ReleaseFreeMemory() {
  base::allocator::ReleaseFreeMemory();
  discardable_shared_memory_manager()->ReleaseFreeMemory();

  if (blink_platform_impl_)
    blink::decommitFreeableMemory();
}

RenderThreadImpl::PendingFrameCreate::PendingFrameCreate(
    int routing_id,
    mojom::FrameRequest frame_request,
    mojom::FrameHostPtr frame_host)
    : routing_id_(routing_id),
      frame_request_(std::move(frame_request)),
      frame_host_(std::move(frame_host)) {
  // The RenderFrame may be deleted before the CreateFrame message is received.
  // In that case, the RenderFrameHost should cancel the create, which is
  // detected by setting an error handler on |frame_host_|.
  frame_host_.set_connection_error_handler(base::Bind(
      &RenderThreadImpl::PendingFrameCreate::OnConnectionError,
      base::Unretained(this)));
}

RenderThreadImpl::PendingFrameCreate::~PendingFrameCreate() {
}

void RenderThreadImpl::PendingFrameCreate::OnConnectionError() {
  size_t erased =
      RenderThreadImpl::current()->pending_frame_creates_.erase(routing_id_);
  DCHECK_EQ(1u, erased);
}

void RenderThreadImpl::OnSyncMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  if (!blink::mainThreadIsolate())
    return;

  v8::MemoryPressureLevel v8_memory_pressure_level =
      static_cast<v8::MemoryPressureLevel>(memory_pressure_level);

  // In order to reduce performance impact, translate critical level to
  // moderate level for foregroud renderer.
  if (!RendererIsHidden() &&
      v8_memory_pressure_level == v8::MemoryPressureLevel::kCritical)
    v8_memory_pressure_level = v8::MemoryPressureLevel::kModerate;

  blink::mainThreadIsolate()->MemoryPressureNotification(
      v8_memory_pressure_level);
  blink::MemoryPressureNotificationToWorkerThreadIsolates(
      v8_memory_pressure_level);
}

// Note that this would be called only when memory_coordinator is enabled.
// OnSyncMemoryPressure() is never called in that case.
void RenderThreadImpl::OnTrimMemoryImmediately() {
  if (blink::mainThreadIsolate()) {
    blink::mainThreadIsolate()->MemoryPressureNotification(
        v8::MemoryPressureLevel::kCritical);
    blink::MemoryPressureNotificationToWorkerThreadIsolates(
        v8::MemoryPressureLevel::kCritical);
  }
}

void RenderThreadImpl::OnRendererInterfaceRequest(
    mojom::RendererAssociatedRequest request) {
  DCHECK(!renderer_binding_.is_bound());
  renderer_binding_.Bind(std::move(request));
}

}  // namespace content

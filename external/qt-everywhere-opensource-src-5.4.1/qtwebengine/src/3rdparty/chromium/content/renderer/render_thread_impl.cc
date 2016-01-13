// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_thread_impl.h"

#include <algorithm>
#include <limits>
#include <map>
#include <vector>

#include "base/allocator/allocator_extension.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_table.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "cc/base/switches.h"
#include "cc/resources/raster_worker_pool.h"
#include "content/child/appcache/appcache_dispatcher.h"
#include "content/child/appcache/appcache_frontend_impl.h"
#include "content/child/child_histogram_message_filter.h"
#include "content/child/db_message_filter.h"
#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/indexed_db/indexed_db_message_filter.h"
#include "content/child/npapi/npobject_util.h"
#include "content/child/plugin_messages.h"
#include "content/child/resource_dispatcher.h"
#include "content/child/runtime_features.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/web_database_observer_impl.h"
#include "content/child/worker_task_runner.h"
#include "content/common/child_process_messages.h"
#include "content/common/content_constants_internal.h"
#include "content/common/database_messages.h"
#include "content/common/dom_storage/dom_storage_messages.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/mojo/mojo_service_names.h"
#include "content/common/resource_messages.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_process_observer.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/renderer/compositor_bindings/web_external_bitmap_impl.h"
#include "content/renderer/compositor_bindings/web_layer_impl.h"
#include "content/renderer/devtools/devtools_agent_filter.h"
#include "content/renderer/dom_storage/dom_storage_dispatcher.h"
#include "content/renderer/dom_storage/webstoragearea_impl.h"
#include "content/renderer/dom_storage/webstoragenamespace_impl.h"
#include "content/renderer/gamepad_shared_memory_reader.h"
#include "content/renderer/gpu/compositor_output_surface.h"
#include "content/renderer/gpu/gpu_benchmarking_extension.h"
#include "content/renderer/input/input_event_filter.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/media/aec_dump_message_filter.h"
#include "content/renderer/media/audio_input_message_filter.h"
#include "content/renderer/media/audio_message_filter.h"
#include "content/renderer/media/audio_renderer_mixer_manager.h"
#include "content/renderer/media/media_stream_center.h"
#include "content/renderer/media/midi_message_filter.h"
#include "content/renderer/media/peer_connection_tracker.h"
#include "content/renderer/media/renderer_gpu_video_accelerator_factories.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc_identity_service.h"
#include "content/renderer/net_info_helper.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "content/renderer/service_worker/embedded_worker_context_message_filter.h"
#include "content/renderer/service_worker/embedded_worker_dispatcher.h"
#include "content/renderer/shared_worker/embedded_shared_worker_stub.h"
#include "content/renderer/web_ui_setup_impl.h"
#include "grit/content_resources.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_forwarding_message_filter.h"
#include "ipc/ipc_platform_file.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/media.h"
#include "media/filters/gpu_video_accelerator_factories.h"
#include "mojo/common/common_type_converters.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "skia/ext/event_tracer_impl.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebColorName.h"
#include "third_party/WebKit/public/web/WebDatabase.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebImageCache.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebNetworkStateNotifier.h"
#include "third_party/WebKit/public/web/WebPopupMenu.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebScriptController.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_switches.h"
#include "v8/include/v8.h"

#if defined(OS_ANDROID)
#include <cpu-features.h>
#include "content/renderer/android/synchronous_compositor_factory.h"
#include "content/renderer/media/android/renderer_demuxer_android.h"
#endif

#if defined(OS_MACOSX)
#include "content/renderer/webscrollbarbehavior_impl_mac.h"
#endif

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#include <objbase.h>
#else
// TODO(port)
#include "content/child/npapi/np_channel_base.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "content/renderer/npapi/plugin_channel_host.h"
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

namespace content {

namespace {

const int64 kInitialIdleHandlerDelayMs = 1000;
const int64 kShortIdleHandlerDelayMs = 1000;
const int64 kLongIdleHandlerDelayMs = 30*1000;
const int kIdleCPUUsageThresholdInPercents = 3;
const int kMinRasterThreads = 1;
const int kMaxRasterThreads = 64;

// Maximum allocation size allowed for image scaling filters that
// require pre-scaling. Skia will fallback to a filter that doesn't
// require pre-scaling if the default filter would require an
// allocation that exceeds this limit.
const size_t kImageCacheSingleAllocationByteLimit = 64 * 1024 * 1024;

// Keep the global RenderThreadImpl in a TLS slot so it is impossible to access
// incorrectly from the wrong thread.
base::LazyInstance<base::ThreadLocalPointer<RenderThreadImpl> >
    lazy_tls = LAZY_INSTANCE_INITIALIZER;

class RenderViewZoomer : public RenderViewVisitor {
 public:
  RenderViewZoomer(const std::string& scheme,
                   const std::string& host,
                   double zoom_level) : scheme_(scheme),
                                        host_(host),
                                        zoom_level_(zoom_level) {
  }

  virtual bool Visit(RenderView* render_view) OVERRIDE {
    WebView* webview = render_view->GetWebView();
    WebDocument document = webview->mainFrame()->document();

    // Don't set zoom level for full-page plugin since they don't use the same
    // zoom settings.
    if (document.isPluginDocument())
      return true;
    GURL url(document.url());
    // Empty scheme works as wildcard that matches any scheme,
    if ((net::GetHostOrSpecFromURL(url) == host_) &&
        (scheme_.empty() || scheme_ == url.scheme()) &&
        !static_cast<RenderViewImpl*>(render_view)
             ->uses_temporary_zoom_level()) {
      webview->hidePopups();
      webview->setZoomLevel(zoom_level_);
    }
    return true;
  }

 private:
  const std::string scheme_;
  const std::string host_;
  const double zoom_level_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewZoomer);
};

std::string HostToCustomHistogramSuffix(const std::string& host) {
  if (host == "mail.google.com")
    return ".gmail";
  if (host == "docs.google.com" || host == "drive.google.com")
    return ".docs";
  if (host == "plus.google.com")
    return ".plus";
  return std::string();
}

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

scoped_ptr<base::SharedMemory> AllocateSharedMemoryFunction(size_t size) {
  return RenderThreadImpl::Get()->HostAllocateSharedMemoryBuffer(size);
}

void EnableBlinkPlatformLogChannels(const std::string& channels) {
  if (channels.empty())
    return;
  base::StringTokenizer t(channels, ", ");
  while (t.GetNext())
    blink::enableLogChannel(t.token().c_str());
}

void NotifyTimezoneChangeOnThisThread() {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (!isolate)
    return;
  v8::Date::DateTimeConfigurationChangeNotification(isolate);
}

}  // namespace

RenderThreadImpl::HistogramCustomizer::HistogramCustomizer() {
  custom_histograms_.insert("V8.MemoryExternalFragmentationTotal");
  custom_histograms_.insert("V8.MemoryHeapSampleTotalCommitted");
  custom_histograms_.insert("V8.MemoryHeapSampleTotalUsed");
}

RenderThreadImpl::HistogramCustomizer::~HistogramCustomizer() {}

void RenderThreadImpl::HistogramCustomizer::RenderViewNavigatedToHost(
    const std::string& host, size_t view_count) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
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

RenderThreadImpl* RenderThreadImpl::current() {
  return lazy_tls.Pointer()->Get();
}

// When we run plugins in process, we actually run them on the render thread,
// which means that we need to make the render thread pump UI events.
RenderThreadImpl::RenderThreadImpl() {
  Init();
}

RenderThreadImpl::RenderThreadImpl(const std::string& channel_name)
    : ChildThread(channel_name) {
  Init();
}

void RenderThreadImpl::Init() {
  TRACE_EVENT_BEGIN_ETW("RenderThreadImpl::Init", 0, "");

  base::debug::TraceLog::GetInstance()->SetThreadSortIndex(
      base::PlatformThread::CurrentId(),
      kTraceEventRendererMainThreadSortIndex);

#if (defined(OS_MACOSX) || defined(OS_ANDROID)) && !defined(TOOLKIT_QT)
  // On Mac and Android, the select popups are rendered by the browser.
  blink::WebView::setUseExternalPopupMenus(true);
#endif

  lazy_tls.Pointer()->Set(this);

  // Register this object as the main thread.
  ChildProcess::current()->set_main_thread(this);

  // In single process the single process is all there is.
  suspend_webkit_shared_timer_ = true;
  notify_webkit_of_modal_loop_ = true;
  webkit_shared_timer_suspended_ = false;
  widget_count_ = 0;
  hidden_widget_count_ = 0;
  idle_notification_delay_in_ms_ = kInitialIdleHandlerDelayMs;
  idle_notifications_to_skip_ = 0;
  layout_test_mode_ = false;

  appcache_dispatcher_.reset(
      new AppCacheDispatcher(Get(), new AppCacheFrontendImpl()));
  dom_storage_dispatcher_.reset(new DomStorageDispatcher());
  main_thread_indexed_db_dispatcher_.reset(new IndexedDBDispatcher(
      thread_safe_sender()));
  embedded_worker_dispatcher_.reset(new EmbeddedWorkerDispatcher());

  media_stream_center_ = NULL;

  db_message_filter_ = new DBMessageFilter();
  AddFilter(db_message_filter_.get());

  vc_manager_.reset(new VideoCaptureImplManager());
  AddFilter(vc_manager_->video_capture_message_filter());

#if defined(ENABLE_WEBRTC)
  peer_connection_tracker_.reset(new PeerConnectionTracker());
  AddObserver(peer_connection_tracker_.get());

  p2p_socket_dispatcher_ =
      new P2PSocketDispatcher(GetIOMessageLoopProxy().get());
  AddFilter(p2p_socket_dispatcher_.get());

  webrtc_identity_service_.reset(new WebRTCIdentityService());

  aec_dump_message_filter_ =
      new AecDumpMessageFilter(GetIOMessageLoopProxy(),
                               message_loop()->message_loop_proxy());
  AddFilter(aec_dump_message_filter_.get());

  peer_connection_factory_.reset(new PeerConnectionDependencyFactory(
      p2p_socket_dispatcher_.get()));
#endif  // defined(ENABLE_WEBRTC)

  audio_input_message_filter_ =
      new AudioInputMessageFilter(GetIOMessageLoopProxy());
  AddFilter(audio_input_message_filter_.get());

  audio_message_filter_ = new AudioMessageFilter(GetIOMessageLoopProxy());
  AddFilter(audio_message_filter_.get());

  midi_message_filter_ = new MidiMessageFilter(GetIOMessageLoopProxy());
  AddFilter(midi_message_filter_.get());

  AddFilter((new IndexedDBMessageFilter(thread_safe_sender()))->GetFilter());

  AddFilter((new EmbeddedWorkerContextMessageFilter())->GetFilter());

  GetContentClient()->renderer()->RenderThreadStarted();

  InitSkiaEventTracer();

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(cc::switches::kEnableGpuBenchmarking))
      RegisterExtension(GpuBenchmarkingExtension::Get());

  is_impl_side_painting_enabled_ =
      command_line.HasSwitch(switches::kEnableImplSidePainting);
  WebLayerImpl::SetImplSidePaintingEnabled(is_impl_side_painting_enabled_);

  is_zero_copy_enabled_ = command_line.HasSwitch(switches::kEnableZeroCopy) &&
                          !command_line.HasSwitch(switches::kDisableZeroCopy);

  is_one_copy_enabled_ = command_line.HasSwitch(switches::kEnableOneCopy);

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

  if (command_line.HasSwitch(switches::kDisableDistanceFieldText)) {
    is_distance_field_text_enabled_ = false;
  } else if (command_line.HasSwitch(switches::kEnableDistanceFieldText)) {
    is_distance_field_text_enabled_ = true;
  } else {
    is_distance_field_text_enabled_ = false;
  }

  is_low_res_tiling_enabled_ = true;
  if (command_line.HasSwitch(switches::kDisableLowResTiling) &&
      !command_line.HasSwitch(switches::kEnableLowResTiling)) {
    is_low_res_tiling_enabled_ = false;
  }

  // Note that under Linux, the media library will normally already have
  // been initialized by the Zygote before this instance became a Renderer.
  base::FilePath media_path;
  PathService::Get(DIR_MEDIA_LIBS, &media_path);
  if (!media_path.empty())
    media::InitializeMediaLibrary(media_path);

  memory_pressure_listener_.reset(new base::MemoryPressureListener(
      base::Bind(&RenderThreadImpl::OnMemoryPressure, base::Unretained(this))));

  std::vector<base::DiscardableMemoryType> supported_types;
  base::DiscardableMemory::GetSupportedTypes(&supported_types);
  DCHECK(!supported_types.empty());

  // The default preferred type is always the first one in list.
  base::DiscardableMemoryType type = supported_types[0];

  if (command_line.HasSwitch(switches::kUseDiscardableMemory)) {
    std::string requested_type_name = command_line.GetSwitchValueASCII(
        switches::kUseDiscardableMemory);
    base::DiscardableMemoryType requested_type =
        base::DiscardableMemory::GetNamedType(requested_type_name);
    if (std::find(supported_types.begin(),
                  supported_types.end(),
                  requested_type) != supported_types.end()) {
      type = requested_type;
    } else {
      LOG(ERROR) << "Requested discardable memory type is not supported.";
    }
  }

  base::DiscardableMemory::SetPreferredType(type);

  // Allow discardable memory implementations to register memory pressure
  // listeners.
  base::DiscardableMemory::RegisterMemoryPressureListeners();

  // AllocateGpuMemoryBuffer must be used exclusively on one thread but
  // it doesn't have to be the same thread RenderThreadImpl is created on.
  allocate_gpu_memory_buffer_thread_checker_.DetachFromThread();

  if (command_line.HasSwitch(switches::kNumRasterThreads)) {
    int num_raster_threads;
    std::string string_value =
        command_line.GetSwitchValueASCII(switches::kNumRasterThreads);
    if (base::StringToInt(string_value, &num_raster_threads) &&
        num_raster_threads >= kMinRasterThreads &&
        num_raster_threads <= kMaxRasterThreads) {
      cc::RasterWorkerPool::SetNumRasterThreads(num_raster_threads);
    } else {
      LOG(WARNING) << "Failed to parse switch " <<
                      switches::kNumRasterThreads  << ": " << string_value;
    }
  }

  TRACE_EVENT_END_ETW("RenderThreadImpl::Init", 0, "");
}

RenderThreadImpl::~RenderThreadImpl() {
}

void RenderThreadImpl::Shutdown() {
  FOR_EACH_OBSERVER(
      RenderProcessObserver, observers_, OnRenderProcessShutdown());

  ChildThread::Shutdown();

  // Wait for all databases to be closed.
  if (webkit_platform_support_) {
    // WaitForAllDatabasesToClose might run a nested message loop. To avoid
    // processing timer events while we're already in the process of shutting
    // down blink, put a ScopePageLoadDeferrer on the stack.
    WebView::willEnterModalLoop();
    webkit_platform_support_->web_database_observer_impl()->
        WaitForAllDatabasesToClose();
    WebView::didExitModalLoop();
  }

  // Shutdown in reverse of the initialization order.
  if (devtools_agent_message_filter_.get()) {
    RemoveFilter(devtools_agent_message_filter_.get());
    devtools_agent_message_filter_ = NULL;
  }

  RemoveFilter(audio_input_message_filter_.get());
  audio_input_message_filter_ = NULL;

  RemoveFilter(audio_message_filter_.get());
  audio_message_filter_ = NULL;

#if defined(ENABLE_WEBRTC)
  RTCPeerConnectionHandler::DestructAllHandlers();

  peer_connection_factory_.reset();
#endif
  RemoveFilter(vc_manager_->video_capture_message_filter());
  vc_manager_.reset();

  RemoveFilter(db_message_filter_.get());
  db_message_filter_ = NULL;

  // Shutdown the file thread if it's running.
  if (file_thread_)
    file_thread_->Stop();

  if (compositor_output_surface_filter_.get()) {
    RemoveFilter(compositor_output_surface_filter_.get());
    compositor_output_surface_filter_ = NULL;
  }

  media_thread_.reset();
  compositor_thread_.reset();
  input_handler_manager_.reset();
  if (input_event_filter_.get()) {
    RemoveFilter(input_event_filter_.get());
    input_event_filter_ = NULL;
  }

  // RemoveEmbeddedWorkerRoute may be called while deleting
  // EmbeddedWorkerDispatcher. So it must be deleted before deleting
  // RenderThreadImpl.
  embedded_worker_dispatcher_.reset();

  // Ramp down IDB before we ramp down WebKit (and V8), since IDB classes might
  // hold pointers to V8 objects (e.g., via pending requests).
  main_thread_indexed_db_dispatcher_.reset();

  if (webkit_platform_support_)
    blink::shutdown();

  lazy_tls.Pointer()->Set(NULL);

  // TODO(port)
#if defined(OS_WIN)
  // Clean up plugin channels before this thread goes away.
  NPChannelBase::CleanupChannels();
#endif
}

bool RenderThreadImpl::Send(IPC::Message* msg) {
  // Certain synchronous messages cannot always be processed synchronously by
  // the browser, e.g., putting up UI and waiting for the user. This could cause
  // a complete hang of Chrome if a windowed plug-in is trying to communicate
  // with the renderer thread since the browser's UI thread could be stuck
  // (within a Windows API call) trying to synchronously communicate with the
  // plug-in.  The remedy is to pump messages on this thread while the browser
  // is processing this request. This creates an opportunity for re-entrancy
  // into WebKit, so we need to take care to disable callbacks, timers, and
  // pending network loads that could trigger such callbacks.
  bool pumping_events = false;
  if (msg->is_sync()) {
    if (msg->is_caller_pumping_messages()) {
      pumping_events = true;
    }
  }

  bool suspend_webkit_shared_timer = true;  // default value
  std::swap(suspend_webkit_shared_timer, suspend_webkit_shared_timer_);

  bool notify_webkit_of_modal_loop = true;  // default value
  std::swap(notify_webkit_of_modal_loop, notify_webkit_of_modal_loop_);

#if defined(ENABLE_PLUGINS)
  int render_view_id = MSG_ROUTING_NONE;
#endif

  if (pumping_events) {
    if (suspend_webkit_shared_timer)
      webkit_platform_support_->SuspendSharedTimer();

    if (notify_webkit_of_modal_loop)
      WebView::willEnterModalLoop();
#if defined(ENABLE_PLUGINS)
    RenderViewImpl* render_view =
        RenderViewImpl::FromRoutingID(msg->routing_id());
    if (render_view) {
      render_view_id = msg->routing_id();
      PluginChannelHost::Broadcast(
          new PluginMsg_SignalModalDialogEvent(render_view_id));
    }
#endif
  }

  bool rv = ChildThread::Send(msg);

  if (pumping_events) {
#if defined(ENABLE_PLUGINS)
    if (render_view_id != MSG_ROUTING_NONE) {
      PluginChannelHost::Broadcast(
          new PluginMsg_ResetModalDialogEvent(render_view_id));
    }
#endif

    if (notify_webkit_of_modal_loop)
      WebView::didExitModalLoop();

    if (suspend_webkit_shared_timer)
      webkit_platform_support_->ResumeSharedTimer();
  }

  return rv;
}

base::MessageLoop* RenderThreadImpl::GetMessageLoop() {
  return message_loop();
}

IPC::SyncChannel* RenderThreadImpl::GetChannel() {
  return channel();
}

std::string RenderThreadImpl::GetLocale() {
  // The browser process should have passed the locale to the renderer via the
  // --lang command line flag.
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  const std::string& lang =
      parsed_command_line.GetSwitchValueASCII(switches::kLang);
  DCHECK(!lang.empty());
  return lang;
}

IPC::SyncMessageFilter* RenderThreadImpl::GetSyncMessageFilter() {
  return sync_message_filter();
}

scoped_refptr<base::MessageLoopProxy>
    RenderThreadImpl::GetIOMessageLoopProxy() {
  return ChildProcess::current()->io_message_loop_proxy();
}

void RenderThreadImpl::AddRoute(int32 routing_id, IPC::Listener* listener) {
  ChildThread::GetRouter()->AddRoute(routing_id, listener);
}

void RenderThreadImpl::RemoveRoute(int32 routing_id) {
  ChildThread::GetRouter()->RemoveRoute(routing_id);
}

void RenderThreadImpl::AddEmbeddedWorkerRoute(int32 routing_id,
                                              IPC::Listener* listener) {
  AddRoute(routing_id, listener);
  if (devtools_agent_message_filter_.get()) {
    devtools_agent_message_filter_->AddEmbeddedWorkerRouteOnMainThread(
        routing_id);
  }
}

void RenderThreadImpl::RemoveEmbeddedWorkerRoute(int32 routing_id) {
  RemoveRoute(routing_id);
  if (devtools_agent_message_filter_.get()) {
    devtools_agent_message_filter_->RemoveEmbeddedWorkerRouteOnMainThread(
        routing_id);
  }
}

int RenderThreadImpl::GenerateRoutingID() {
  int routing_id = MSG_ROUTING_NONE;
  Send(new ViewHostMsg_GenerateRoutingID(&routing_id));
  return routing_id;
}

void RenderThreadImpl::AddFilter(IPC::MessageFilter* filter) {
  channel()->AddFilter(filter);
}

void RenderThreadImpl::RemoveFilter(IPC::MessageFilter* filter) {
  channel()->RemoveFilter(filter);
}

void RenderThreadImpl::AddObserver(RenderProcessObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderThreadImpl::RemoveObserver(RenderProcessObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RenderThreadImpl::SetResourceDispatcherDelegate(
    ResourceDispatcherDelegate* delegate) {
  resource_dispatcher()->set_delegate(delegate);
}

void RenderThreadImpl::EnsureWebKitInitialized() {
  if (webkit_platform_support_)
    return;

  webkit_platform_support_.reset(new RendererWebKitPlatformSupportImpl);
  blink::initialize(webkit_platform_support_.get());

  v8::Isolate* isolate = blink::mainThreadIsolate();

  isolate->SetCounterFunction(base::StatsTable::FindLocation);
  isolate->SetCreateHistogramFunction(CreateHistogram);
  isolate->SetAddHistogramSampleFunction(AddHistogramSample);

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  bool enable = command_line.HasSwitch(switches::kEnableThreadedCompositing);
  if (enable) {
#if defined(OS_ANDROID)
    if (SynchronousCompositorFactory* factory =
        SynchronousCompositorFactory::GetInstance())
      compositor_message_loop_proxy_ =
          factory->GetCompositorMessageLoop();
#endif
    if (!compositor_message_loop_proxy_.get()) {
      compositor_thread_.reset(new base::Thread("Compositor"));
      compositor_thread_->Start();
#if defined(OS_ANDROID)
      compositor_thread_->SetPriority(base::kThreadPriority_Display);
#endif
      compositor_message_loop_proxy_ =
          compositor_thread_->message_loop_proxy();
      compositor_message_loop_proxy_->PostTask(
          FROM_HERE,
          base::Bind(base::IgnoreResult(&ThreadRestrictions::SetIOAllowed),
                     false));
    }

    InputHandlerManagerClient* input_handler_manager_client = NULL;
#if defined(OS_ANDROID)
    if (SynchronousCompositorFactory* factory =
        SynchronousCompositorFactory::GetInstance()) {
      input_handler_manager_client = factory->GetInputHandlerManagerClient();
    }
#endif
    if (!input_handler_manager_client) {
      input_event_filter_ =
          new InputEventFilter(this, compositor_message_loop_proxy_);
      AddFilter(input_event_filter_.get());
      input_handler_manager_client = input_event_filter_.get();
    }
    input_handler_manager_.reset(
        new InputHandlerManager(compositor_message_loop_proxy_,
                                input_handler_manager_client));
  }

  scoped_refptr<base::MessageLoopProxy> output_surface_loop;
  if (enable)
    output_surface_loop = compositor_message_loop_proxy_;
  else
    output_surface_loop = base::MessageLoopProxy::current();

  compositor_output_surface_filter_ =
      CompositorOutputSurface::CreateFilter(output_surface_loop.get());
  AddFilter(compositor_output_surface_filter_.get());

  gamepad_shared_memory_reader_.reset(
      new GamepadSharedMemoryReader(webkit_platform_support_.get()));
  AddObserver(gamepad_shared_memory_reader_.get());

  RenderThreadImpl::RegisterSchemes();

  EnableBlinkPlatformLogChannels(
      command_line.GetSwitchValueASCII(switches::kBlinkPlatformLogChannels));

  SetRuntimeFeaturesDefaultsAndUpdateFromArgs(command_line);

  if (!media::IsMediaLibraryInitialized()) {
    WebRuntimeFeatures::enableMediaPlayer(false);
    WebRuntimeFeatures::enableWebAudio(false);
  }

  FOR_EACH_OBSERVER(RenderProcessObserver, observers_, WebKitInitialized());

  devtools_agent_message_filter_ = new DevToolsAgentFilter();
  AddFilter(devtools_agent_message_filter_.get());

  if (GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden())
    ScheduleIdleHandler(kLongIdleHandlerDelayMs);

  SetSharedMemoryAllocationFunction(AllocateSharedMemoryFunction);

  // Limit use of the scaled image cache to when deferred image decoding is
  // enabled.
  if (!command_line.HasSwitch(switches::kEnableDeferredImageDecoding) &&
      !is_impl_side_painting_enabled_)
    SkGraphics::SetImageCacheByteLimit(0u);

  SkGraphics::SetImageCacheSingleAllocationByteLimit(
      kImageCacheSingleAllocationByteLimit);
}

void RenderThreadImpl::RegisterSchemes() {
  // swappedout: pages should not be accessible, and should also
  // be treated as empty documents that can commit synchronously.
  WebString swappedout_scheme(base::ASCIIToUTF16(kSwappedOutScheme));
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(swappedout_scheme);
  WebSecurityPolicy::registerURLSchemeAsEmptyDocument(swappedout_scheme);
}

void RenderThreadImpl::NotifyTimezoneChange() {
  NotifyTimezoneChangeOnThisThread();
  RenderThread::Get()->PostTaskToAllWebWorkers(
      base::Bind(&NotifyTimezoneChangeOnThisThread));
}

void RenderThreadImpl::RecordAction(const base::UserMetricsAction& action) {
  Send(new ViewHostMsg_UserMetricsRecordAction(action.str_));
}

void RenderThreadImpl::RecordComputedAction(const std::string& action) {
  Send(new ViewHostMsg_UserMetricsRecordAction(action));
}

scoped_ptr<base::SharedMemory>
    RenderThreadImpl::HostAllocateSharedMemoryBuffer(size_t size) {
  if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
    return scoped_ptr<base::SharedMemory>();

  base::SharedMemoryHandle handle;
  bool success;
  IPC::Message* message =
      new ChildProcessHostMsg_SyncAllocateSharedMemory(size, &handle);

  // Allow calling this from the compositor thread.
  if (base::MessageLoop::current() == message_loop())
    success = ChildThread::Send(message);
  else
    success = sync_message_filter()->Send(message);

  if (!success)
    return scoped_ptr<base::SharedMemory>();

  if (!base::SharedMemory::IsHandleValid(handle))
    return scoped_ptr<base::SharedMemory>();

  return scoped_ptr<base::SharedMemory>(new base::SharedMemory(handle, false));
}

void RenderThreadImpl::RegisterExtension(v8::Extension* extension) {
  WebScriptController::registerExtension(extension);
}

void RenderThreadImpl::ScheduleIdleHandler(int64 initial_delay_ms) {
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
    IdleHandlerInForegroundTab();
    return;
  }

  base::allocator::ReleaseFreeMemory();

  // Continue the idle timer if the webkit shared timer is not suspended or
  // something is left to do.
  bool continue_timer = !webkit_shared_timer_suspended_;

  if (!v8::V8::IdleNotification()) {
    continue_timer = true;
  }
  if (!base::DiscardableMemory::ReduceMemoryUsage()) {
    continue_timer = true;
  }

  // Schedule next invocation.
  // Dampen the delay using the algorithm (if delay is in seconds):
  //    delay = delay + 1 / (delay + 2)
  // Using floor(delay) has a dampening effect such as:
  //    1s, 1, 1, 2, 2, 2, 2, 3, 3, ...
  // If the delay is in milliseconds, the above formula is equivalent to:
  //    delay_ms / 1000 = delay_ms / 1000 + 1 / (delay_ms / 1000 + 2)
  // which is equivalent to
  //    delay_ms = delay_ms + 1000*1000 / (delay_ms + 2000).
  // Note that idle_notification_delay_in_ms_ would be reset to
  // kInitialIdleHandlerDelayMs in RenderThreadImpl::WidgetHidden.
  if (continue_timer) {
    ScheduleIdleHandler(idle_notification_delay_in_ms_ +
                        1000000 / (idle_notification_delay_in_ms_ + 2000));

  } else {
    idle_timer_.Stop();
  }

  FOR_EACH_OBSERVER(RenderProcessObserver, observers_, IdleNotification());
}

void RenderThreadImpl::IdleHandlerInForegroundTab() {
  // Increase the delay in the same way as in IdleHandler,
  // but make it periodic by reseting it once it is too big.
  int64 new_delay_ms = idle_notification_delay_in_ms_ +
                       1000000 / (idle_notification_delay_in_ms_ + 2000);
  if (new_delay_ms >= kLongIdleHandlerDelayMs)
    new_delay_ms = kShortIdleHandlerDelayMs;

  if (idle_notifications_to_skip_ > 0) {
    idle_notifications_to_skip_--;
  } else  {
    int cpu_usage = 0;
    Send(new ViewHostMsg_GetCPUUsage(&cpu_usage));
    // Idle notification hint roughly specifies the expected duration of the
    // idle pause. We set it proportional to the idle timer delay.
    int idle_hint = static_cast<int>(new_delay_ms / 10);
    if (cpu_usage < kIdleCPUUsageThresholdInPercents) {
      base::allocator::ReleaseFreeMemory();

      bool finished_idle_work = true;
      if (!v8::V8::IdleNotification(idle_hint))
        finished_idle_work = false;
      if (!base::DiscardableMemory::ReduceMemoryUsage())
        finished_idle_work = false;

      // V8 finished collecting garbage and discardable memory system has no
      // more idle work left.
      if (finished_idle_work)
        new_delay_ms = kLongIdleHandlerDelayMs;
    }
  }
  ScheduleIdleHandler(new_delay_ms);
}

int64 RenderThreadImpl::GetIdleNotificationDelayInMs() const {
  return idle_notification_delay_in_ms_;
}

void RenderThreadImpl::SetIdleNotificationDelayInMs(
    int64 idle_notification_delay_in_ms) {
  idle_notification_delay_in_ms_ = idle_notification_delay_in_ms;
}

void RenderThreadImpl::UpdateHistograms(int sequence_number) {
  child_histogram_message_filter()->SendHistograms(sequence_number);
}

int RenderThreadImpl::PostTaskToAllWebWorkers(const base::Closure& closure) {
  return WorkerTaskRunner::Instance()->PostTaskToAllThreads(closure);
}

bool RenderThreadImpl::ResolveProxy(const GURL& url, std::string* proxy_list) {
  bool result = false;
  Send(new ViewHostMsg_ResolveProxy(url, &result, proxy_list));
  return result;
}

void RenderThreadImpl::PostponeIdleNotification() {
  idle_notifications_to_skip_ = 2;
}

scoped_refptr<media::GpuVideoAcceleratorFactories>
RenderThreadImpl::GetGpuFactories() {
  DCHECK(IsMainThread());

  scoped_refptr<GpuChannelHost> gpu_channel_host = GetGpuChannel();
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  scoped_refptr<media::GpuVideoAcceleratorFactories> gpu_factories;
  scoped_refptr<base::MessageLoopProxy> media_loop_proxy =
      GetMediaThreadMessageLoopProxy();
  if (!cmd_line->HasSwitch(switches::kDisableAcceleratedVideoDecode)) {
    if (!gpu_va_context_provider_ ||
        gpu_va_context_provider_->DestroyedOnMainThread()) {
      if (!gpu_channel_host) {
        gpu_channel_host = EstablishGpuChannelSync(
            CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE);
      }
      blink::WebGraphicsContext3D::Attributes attributes;
      bool lose_context_when_out_of_memory = false;
      gpu_va_context_provider_ = ContextProviderCommandBuffer::Create(
          make_scoped_ptr(
              WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
                  gpu_channel_host.get(),
                  attributes,
                  lose_context_when_out_of_memory,
                  GURL("chrome://gpu/RenderThreadImpl::GetGpuVDAContext3D"),
                  WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits(),
                  NULL)),
          "GPU-VideoAccelerator-Offscreen");
    }
  }
  if (gpu_va_context_provider_) {
    gpu_factories = RendererGpuVideoAcceleratorFactories::Create(
        gpu_channel_host, media_loop_proxy, gpu_va_context_provider_);
  }
  return gpu_factories;
}

scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
RenderThreadImpl::CreateOffscreenContext3d() {
  blink::WebGraphicsContext3D::Attributes attributes;
  attributes.shareResources = true;
  attributes.depth = false;
  attributes.stencil = false;
  attributes.antialias = false;
  attributes.noAutomaticFlushes = true;
  bool lose_context_when_out_of_memory = true;

  scoped_refptr<GpuChannelHost> gpu_channel_host(EstablishGpuChannelSync(
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE));
  return make_scoped_ptr(
      WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
          gpu_channel_host.get(),
          attributes,
          lose_context_when_out_of_memory,
          GURL("chrome://gpu/RenderThreadImpl::CreateOffscreenContext3d"),
          WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits(),
          NULL));
}

scoped_refptr<webkit::gpu::ContextProviderWebContext>
RenderThreadImpl::SharedMainThreadContextProvider() {
  DCHECK(IsMainThread());
#if defined(OS_ANDROID)
  if (SynchronousCompositorFactory* factory =
      SynchronousCompositorFactory::GetInstance())
    return factory->GetSharedOffscreenContextProviderForMainThread();
#endif

  if (!shared_main_thread_contexts_ ||
      shared_main_thread_contexts_->DestroyedOnMainThread()) {
    shared_main_thread_contexts_ = ContextProviderCommandBuffer::Create(
        CreateOffscreenContext3d(), "Offscreen-MainThread");
  }
  if (shared_main_thread_contexts_ &&
      !shared_main_thread_contexts_->BindToCurrentThread())
    shared_main_thread_contexts_ = NULL;
  return shared_main_thread_contexts_;
}

AudioRendererMixerManager* RenderThreadImpl::GetAudioRendererMixerManager() {
  if (!audio_renderer_mixer_manager_) {
    audio_renderer_mixer_manager_.reset(new AudioRendererMixerManager(
        GetAudioHardwareConfig()));
  }

  return audio_renderer_mixer_manager_.get();
}

media::AudioHardwareConfig* RenderThreadImpl::GetAudioHardwareConfig() {
  if (!audio_hardware_config_) {
    media::AudioParameters input_params;
    media::AudioParameters output_params;
    Send(new ViewHostMsg_GetAudioHardwareConfig(
        &input_params, &output_params));

    audio_hardware_config_.reset(new media::AudioHardwareConfig(
        input_params, output_params));
    audio_message_filter_->SetAudioHardwareConfig(audio_hardware_config_.get());
  }

  return audio_hardware_config_.get();
}

base::WaitableEvent* RenderThreadImpl::GetShutdownEvent() {
  return ChildProcess::current()->GetShutDownEvent();
}

#if defined(OS_WIN)
void RenderThreadImpl::PreCacheFontCharacters(const LOGFONT& log_font,
                                              const base::string16& str) {
  Send(new ViewHostMsg_PreCacheFontCharacters(log_font, str));
}

void RenderThreadImpl::PreCacheFont(const LOGFONT& log_font) {
  Send(new ChildProcessHostMsg_PreCacheFont(log_font));
}

void RenderThreadImpl::ReleaseCachedFonts() {
  Send(new ChildProcessHostMsg_ReleaseCachedFonts());
}

#endif  // OS_WIN

bool RenderThreadImpl::IsMainThread() {
  return !!current();
}

base::MessageLoop* RenderThreadImpl::GetMainLoop() {
  return message_loop();
}

scoped_refptr<base::MessageLoopProxy> RenderThreadImpl::GetIOLoopProxy() {
  return io_message_loop_proxy_;
}

scoped_ptr<base::SharedMemory> RenderThreadImpl::AllocateSharedMemory(
    size_t size) {
  return scoped_ptr<base::SharedMemory>(
      HostAllocateSharedMemoryBuffer(size));
}

CreateCommandBufferResult RenderThreadImpl::CreateViewCommandBuffer(
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params,
      int32 route_id) {
  TRACE_EVENT1("gpu",
               "RenderThreadImpl::CreateViewCommandBuffer",
               "surface_id",
               surface_id);

  CreateCommandBufferResult result = CREATE_COMMAND_BUFFER_FAILED;
  IPC::Message* message = new GpuHostMsg_CreateViewCommandBuffer(
      surface_id,
      init_params,
      route_id,
      &result);

  // Allow calling this from the compositor thread.
  thread_safe_sender()->Send(message);

  return result;
}

void RenderThreadImpl::CreateImage(
    gfx::PluginWindowHandle window,
    int32 image_id,
    const CreateImageCallback& callback) {
  NOTREACHED();
}

void RenderThreadImpl::DeleteImage(int32 image_id, int32 sync_point) {
  NOTREACHED();
}

scoped_ptr<gfx::GpuMemoryBuffer> RenderThreadImpl::AllocateGpuMemoryBuffer(
    size_t width,
    size_t height,
    unsigned internalformat,
    unsigned usage) {
  DCHECK(allocate_gpu_memory_buffer_thread_checker_.CalledOnValidThread());

  if (!GpuMemoryBufferImpl::IsFormatValid(internalformat))
    return scoped_ptr<gfx::GpuMemoryBuffer>();

  gfx::GpuMemoryBufferHandle handle;
  bool success;
  IPC::Message* message = new ChildProcessHostMsg_SyncAllocateGpuMemoryBuffer(
      width, height, internalformat, usage, &handle);

  // Allow calling this from the compositor thread.
  if (base::MessageLoop::current() == message_loop())
    success = ChildThread::Send(message);
  else
    success = sync_message_filter()->Send(message);

  if (!success)
    return scoped_ptr<gfx::GpuMemoryBuffer>();

  return GpuMemoryBufferImpl::CreateFromHandle(
             handle, gfx::Size(width, height), internalformat)
      .PassAs<gfx::GpuMemoryBuffer>();
}

void RenderThreadImpl::ConnectToService(
    const mojo::String& service_url,
    const mojo::String& service_name,
    mojo::ScopedMessagePipeHandle message_pipe,
    const mojo::String& requestor_url) {
  // TODO(darin): Invent some kind of registration system to use here.
  if (service_url.To<base::StringPiece>() == kRendererService_WebUISetup) {
    WebUISetupImpl::Bind(message_pipe.Pass());
  } else {
    NOTREACHED() << "Unknown service name";
  }
}

void RenderThreadImpl::DoNotSuspendWebKitSharedTimer() {
  suspend_webkit_shared_timer_ = false;
}

void RenderThreadImpl::DoNotNotifyWebKitOfModalLoop() {
  notify_webkit_of_modal_loop_ = false;
}

void RenderThreadImpl::OnSetZoomLevelForCurrentURL(const std::string& scheme,
                                                   const std::string& host,
                                                   double zoom_level) {
  RenderViewZoomer zoomer(scheme, host, zoom_level);
  RenderView::ForEach(&zoomer);
}

bool RenderThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  ObserverListBase<RenderProcessObserver>::Iterator it(observers_);
  RenderProcessObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    if (observer->OnControlMessageReceived(msg))
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
    IPC_MESSAGE_HANDLER(ViewMsg_SetZoomLevelForCurrentURL,
                        OnSetZoomLevelForCurrentURL)
    // TODO(port): removed from render_messages_internal.h;
    // is there a new non-windows message I should add here?
    IPC_MESSAGE_HANDLER(ViewMsg_New, OnCreateNewView)
    IPC_MESSAGE_HANDLER(ViewMsg_PurgePluginListCache, OnPurgePluginListCache)
    IPC_MESSAGE_HANDLER(ViewMsg_NetworkTypeChanged, OnNetworkTypeChanged)
    IPC_MESSAGE_HANDLER(ViewMsg_TempCrashWithData, OnTempCrashWithData)
    IPC_MESSAGE_HANDLER(WorkerProcessMsg_CreateWorker, OnCreateNewSharedWorker)
    IPC_MESSAGE_HANDLER(ViewMsg_TimezoneChange, OnUpdateTimezone)
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ViewMsg_SetWebKitSharedTimersSuspended,
                        OnSetWebKitSharedTimersSuspended)
#endif
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateScrollbarTheme, OnUpdateScrollbarTheme)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderThreadImpl::OnCreateNewView(const ViewMsg_New_Params& params) {
  EnsureWebKitInitialized();
  // When bringing in render_view, also bring in webkit's glue and jsbindings.
  RenderViewImpl::Create(params.opener_route_id,
                         params.window_was_created_with_opener,
                         params.renderer_preferences,
                         params.web_preferences,
                         params.view_id,
                         params.main_frame_routing_id,
                         params.surface_id,
                         params.session_storage_namespace_id,
                         params.frame_name,
                         false,
                         params.swapped_out,
                         params.proxy_routing_id,
                         params.hidden,
                         params.never_visible,
                         params.next_page_id,
                         params.screen_info,
                         params.accessibility_mode);
}

GpuChannelHost* RenderThreadImpl::EstablishGpuChannelSync(
    CauseForGpuLaunch cause_for_gpu_launch) {
  TRACE_EVENT0("gpu", "RenderThreadImpl::EstablishGpuChannelSync");

  if (gpu_channel_.get()) {
    // Do nothing if we already have a GPU channel or are already
    // establishing one.
    if (!gpu_channel_->IsLost())
      return gpu_channel_.get();

    // Recreate the channel if it has been lost.
    gpu_channel_ = NULL;
  }

  // Ask the browser for the channel name.
  int client_id = 0;
  IPC::ChannelHandle channel_handle;
  gpu::GPUInfo gpu_info;
  if (!Send(new GpuHostMsg_EstablishGpuChannel(cause_for_gpu_launch,
                                               &client_id,
                                               &channel_handle,
                                               &gpu_info)) ||
#if defined(OS_POSIX)
      channel_handle.socket.fd == -1 ||
#endif
      channel_handle.name.empty()) {
    // Otherwise cancel the connection.
    return NULL;
  }

  GetContentClient()->SetGpuInfo(gpu_info);

  // Cache some variables that are needed on the compositor thread for our
  // implementation of GpuChannelHostFactory.
  io_message_loop_proxy_ = ChildProcess::current()->io_message_loop_proxy();

  gpu_channel_ = GpuChannelHost::Create(
      this, gpu_info, channel_handle,
      ChildProcess::current()->GetShutDownEvent());
  return gpu_channel_.get();
}

blink::WebMediaStreamCenter* RenderThreadImpl::CreateMediaStreamCenter(
    blink::WebMediaStreamCenterClient* client) {
#if defined(OS_ANDROID)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableWebRTC))
    return NULL;
#endif

#if defined(ENABLE_WEBRTC)
  if (!media_stream_center_) {
    media_stream_center_ = GetContentClient()->renderer()
        ->OverrideCreateWebMediaStreamCenter(client);
    if (!media_stream_center_) {
      scoped_ptr<MediaStreamCenter> media_stream_center(
          new MediaStreamCenter(client, GetPeerConnectionDependencyFactory()));
      AddObserver(media_stream_center.get());
      media_stream_center_ = media_stream_center.release();
    }
  }
#endif
  return media_stream_center_;
}

PeerConnectionDependencyFactory*
RenderThreadImpl::GetPeerConnectionDependencyFactory() {
  return peer_connection_factory_.get();
}

GpuChannelHost* RenderThreadImpl::GetGpuChannel() {
  if (!gpu_channel_.get())
    return NULL;

  if (gpu_channel_->IsLost())
    return NULL;

  return gpu_channel_.get();
}

void RenderThreadImpl::OnPurgePluginListCache(bool reload_pages) {
  EnsureWebKitInitialized();
  // The call below will cause a GetPlugins call with refresh=true, but at this
  // point we already know that the browser has refreshed its list, so disable
  // refresh temporarily to prevent each renderer process causing the list to be
  // regenerated.
  webkit_platform_support_->set_plugin_refresh_allowed(false);
  blink::resetPluginCache(reload_pages);
  webkit_platform_support_->set_plugin_refresh_allowed(true);

  FOR_EACH_OBSERVER(RenderProcessObserver, observers_, PluginListChanged());
}

void RenderThreadImpl::OnNetworkTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  EnsureWebKitInitialized();
  bool online = type != net::NetworkChangeNotifier::CONNECTION_NONE;
  WebNetworkStateNotifier::setOnLine(online);
  FOR_EACH_OBSERVER(
      RenderProcessObserver, observers_, NetworkStateChanged(online));
  WebNetworkStateNotifier::setWebConnectionType(
      NetConnectionTypeToWebConnectionType(type));
}

void RenderThreadImpl::OnTempCrashWithData(const GURL& data) {
  GetContentClient()->SetActiveURL(data);
  CHECK(false);
}

void RenderThreadImpl::OnUpdateTimezone() {
  NotifyTimezoneChange();
}


#if defined(OS_ANDROID)
void RenderThreadImpl::OnSetWebKitSharedTimersSuspended(bool suspend) {
  if (suspend_webkit_shared_timer_) {
    EnsureWebKitInitialized();
    if (suspend) {
      webkit_platform_support_->SuspendSharedTimer();
    } else {
      webkit_platform_support_->ResumeSharedTimer();
    }
    webkit_shared_timer_suspended_ = suspend;
  }
}
#endif

#if defined(OS_MACOSX)
void RenderThreadImpl::OnUpdateScrollbarTheme(
    float initial_button_delay,
    float autoscroll_button_delay,
    bool jump_on_track_click,
    blink::ScrollerStyle preferred_scroller_style,
    bool redraw) {
  EnsureWebKitInitialized();
  static_cast<WebScrollbarBehaviorImpl*>(
      webkit_platform_support_->scrollbarBehavior())->set_jump_on_track_click(
          jump_on_track_click);
  blink::WebScrollbarTheme::updateScrollbars(initial_button_delay,
                                             autoscroll_button_delay,
                                             preferred_scroller_style,
                                             redraw);
}
#endif

void RenderThreadImpl::OnCreateNewSharedWorker(
    const WorkerProcessMsg_CreateWorker_Params& params) {
  // EmbeddedSharedWorkerStub will self-destruct.
  new EmbeddedSharedWorkerStub(params.url,
                               params.name,
                               params.content_security_policy,
                               params.security_policy_type,
                               params.pause_on_start,
                               params.route_id);
}

void RenderThreadImpl::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  base::allocator::ReleaseFreeMemory();

  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_CRITICAL) {
    // Trigger full v8 garbage collection on critical memory notification.
    v8::V8::LowMemoryNotification();

    if (webkit_platform_support_) {
      // Clear the image cache. Do not call into blink if it is not initialized.
      blink::WebImageCache::clear();
    }

    // Purge Skia font cache, by setting it to 0 and then again to the previous
    // limit.
    size_t font_cache_limit = SkGraphics::SetFontCacheLimit(0);
    SkGraphics::SetFontCacheLimit(font_cache_limit);
  } else {
    // Otherwise trigger a couple of v8 GCs using IdleNotification.
    if (!v8::V8::IdleNotification())
      v8::V8::IdleNotification();
  }
}

scoped_refptr<base::MessageLoopProxy>
RenderThreadImpl::GetFileThreadMessageLoopProxy() {
  DCHECK(message_loop() == base::MessageLoop::current());
  if (!file_thread_) {
    file_thread_.reset(new base::Thread("Renderer::FILE"));
    file_thread_->Start();
  }
  return file_thread_->message_loop_proxy();
}

scoped_refptr<base::MessageLoopProxy>
RenderThreadImpl::GetMediaThreadMessageLoopProxy() {
  DCHECK(message_loop() == base::MessageLoop::current());
  if (!media_thread_) {
    media_thread_.reset(new base::Thread("Media"));
    media_thread_->Start();

#if defined(OS_ANDROID)
    renderer_demuxer_ = new RendererDemuxerAndroid();
    AddFilter(renderer_demuxer_.get());
#endif
  }
  return media_thread_->message_loop_proxy();
}

void RenderThreadImpl::SetFlingCurveParameters(
    const std::vector<float>& new_touchpad,
    const std::vector<float>& new_touchscreen) {
  webkit_platform_support_->SetFlingCurveParameters(new_touchpad,
                                                    new_touchscreen);

}

void RenderThreadImpl::SampleGamepads(blink::WebGamepads* data) {
  gamepad_shared_memory_reader_->SampleGamepads(*data);
}

void RenderThreadImpl::SetGamepadListener(blink::WebGamepadListener* listener) {
  gamepad_shared_memory_reader_->SetGamepadListener(listener);
}

void RenderThreadImpl::WidgetCreated() {
  widget_count_++;
}

void RenderThreadImpl::WidgetDestroyed() {
  widget_count_--;
}

void RenderThreadImpl::WidgetHidden() {
  DCHECK_LT(hidden_widget_count_, widget_count_);
  hidden_widget_count_++;

  if (widget_count_ && hidden_widget_count_ == widget_count_) {
#if !defined(SYSTEM_NATIVELY_SIGNALS_MEMORY_PRESSURE)
    // TODO(vollick): Remove this this heavy-handed approach once we're polling
    // the real system memory pressure.
    base::MemoryPressureListener::NotifyMemoryPressure(
        base::MemoryPressureListener::MEMORY_PRESSURE_MODERATE);
#endif
    if (GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden())
      ScheduleIdleHandler(kInitialIdleHandlerDelayMs);
  }
}

void RenderThreadImpl::WidgetRestored() {
  DCHECK_GT(hidden_widget_count_, 0);
  hidden_widget_count_--;

  if (!GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden()) {
    return;
  }

  ScheduleIdleHandler(kLongIdleHandlerDelayMs);
}

}  // namespace content

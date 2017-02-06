// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_THREAD_IMPL_H_
#define CONTENT_RENDERER_RENDER_THREAD_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/user_metrics_action.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "content/child/child_thread_impl.h"
#include "content/common/content_export.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_replication_state.h"
#include "content/common/gpu_process_launch_causes.h"
#include "content/common/storage_partition_service.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/gpu/compositor_dependencies.h"
#include "content/renderer/layout_test_dependencies.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "net/base/network_change_notifier.h"
#include "third_party/WebKit/public/platform/WebConnectionType.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_MACOSX)
#include "third_party/WebKit/public/web/mac/WebScrollbarTheme.h"
#endif

class GrContext;
class SkBitmap;
struct FrameMsg_NewFrame_Params;
struct ViewMsg_New_Params;
struct ViewMsg_UpdateScrollbarTheme_Params;
struct WorkerProcessMsg_CreateWorker_Params;

namespace blink {
class WebGamepads;
class WebMediaStreamCenter;
class WebMediaStreamCenterClient;
}

namespace base {
class SingleThreadTaskRunner;
class Thread;
}

namespace cc {
class ContextProvider;
class ImageSerializationProcessor;
class OutputSurface;
class TaskGraphRunner;
}

namespace gpu {
class GpuChannelHost;
}

namespace IPC {
class MessageFilter;
}

namespace media {
class AudioHardwareConfig;
class GpuVideoAcceleratorFactories;
}

namespace scheduler {
class WebThreadBase;
}

namespace v8 {
class Extension;
}

namespace content {

class AppCacheDispatcher;
class AecDumpMessageFilter;
class AudioInputMessageFilter;
class AudioMessageFilter;
class AudioRendererMixerManager;
class BlobMessageFilter;
class BrowserPluginManager;
class CacheStorageDispatcher;
class CompositorForwardingMessageFilter;
class ContextProviderCommandBuffer;
class DBMessageFilter;
class DevToolsAgentFilter;
class DomStorageDispatcher;
class EmbeddedWorkerDispatcher;
class FrameSwapMessageQueue;
class IndexedDBDispatcher;
class InputHandlerManager;
class MediaStreamCenter;
class MemoryObserver;
class MidiMessageFilter;
class NetInfoDispatcher;
class P2PSocketDispatcher;
class PeerConnectionDependencyFactory;
class PeerConnectionTracker;
class CategorizedWorkerPool;
class RenderThreadObserver;
class RendererBlinkPlatformImpl;
class RendererDemuxerAndroid;
class RendererGpuVideoAcceleratorFactories;
class ResourceDispatchThrottler;
class V8SamplingProfiler;
class VideoCaptureImplManager;
class WebRTCIdentityService;

#if defined(OS_ANDROID)
class StreamTextureFactory;
class SynchronousCompositorFilter;
#endif

#if defined(COMPILER_MSVC)
// See explanation for other RenderViewHostImpl which is the same issue.
#pragma warning(push)
#pragma warning(disable: 4250)
#endif

// The RenderThreadImpl class represents a background thread where RenderView
// instances live.  The RenderThread supports an API that is used by its
// consumer to talk indirectly to the RenderViews and supporting objects.
// Likewise, it provides an API for the RenderViews to talk back to the main
// process (i.e., their corresponding WebContentsImpl).
//
// Most of the communication occurs in the form of IPC messages.  They are
// routed to the RenderThread according to the routing IDs of the messages.
// The routing IDs correspond to RenderView instances.
class CONTENT_EXPORT RenderThreadImpl
    : public RenderThread,
      public ChildThreadImpl,
      public gpu::GpuChannelHostFactory,
      public scheduler::RendererScheduler::RAILModeObserver,
      NON_EXPORTED_BASE(public CompositorDependencies) {
 public:
  static RenderThreadImpl* Create(const InProcessChildThreadParams& params);
  static RenderThreadImpl* Create(
      std::unique_ptr<base::MessageLoop> main_message_loop,
      std::unique_ptr<scheduler::RendererScheduler> renderer_scheduler);
  static RenderThreadImpl* current();

  ~RenderThreadImpl() override;
  void Shutdown() override;

  // When initializing WebKit, ensure that any schemes needed for the content
  // module are registered properly.  Static to allow sharing with tests.
  static void RegisterSchemes();

  // Notify V8 that the date/time configuration of the system might have
  // changed.
  static void NotifyTimezoneChange();

  // RenderThread implementation:
  bool Send(IPC::Message* msg) override;
  IPC::SyncChannel* GetChannel() override;
  std::string GetLocale() override;
  IPC::SyncMessageFilter* GetSyncMessageFilter() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetIOMessageLoopProxy() override;
  void AddRoute(int32_t routing_id, IPC::Listener* listener) override;
  void RemoveRoute(int32_t routing_id) override;
  int GenerateRoutingID() override;
  void AddFilter(IPC::MessageFilter* filter) override;
  void RemoveFilter(IPC::MessageFilter* filter) override;
  void AddObserver(RenderThreadObserver* observer) override;
  void RemoveObserver(RenderThreadObserver* observer) override;
  void SetResourceDispatcherDelegate(
      ResourceDispatcherDelegate* delegate) override;
  std::unique_ptr<base::SharedMemory> HostAllocateSharedMemoryBuffer(
      size_t buffer_size) override;
  cc::SharedBitmapManager* GetSharedBitmapManager() override;
  void RegisterExtension(v8::Extension* extension) override;
  void ScheduleIdleHandler(int64_t initial_delay_ms) override;
  void IdleHandler() override;
  int64_t GetIdleNotificationDelayInMs() const override;
  void SetIdleNotificationDelayInMs(
      int64_t idle_notification_delay_in_ms) override;
  void UpdateHistograms(int sequence_number) override;
  int PostTaskToAllWebWorkers(const base::Closure& closure) override;
  bool ResolveProxy(const GURL& url, std::string* proxy_list) override;
  base::WaitableEvent* GetShutdownEvent() override;

  // CompositorDependencies implementation.
  bool IsGpuRasterizationForced() override;
  bool IsGpuRasterizationEnabled() override;
  bool IsAsyncWorkerContextEnabled() override;
  int GetGpuRasterizationMSAASampleCount() override;
  bool IsLcdTextEnabled() override;
  bool IsDistanceFieldTextEnabled() override;
  bool IsZeroCopyEnabled() override;
  bool IsPartialRasterEnabled() override;
  bool IsGpuMemoryBufferCompositorResourcesEnabled() override;
  bool IsElasticOverscrollEnabled() override;
  std::vector<unsigned> GetImageTextureTargets() override;
  scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorMainThreadTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorImplThreadTaskRunner() override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  scheduler::RendererScheduler* GetRendererScheduler() override;
  std::unique_ptr<cc::BeginFrameSource> CreateExternalBeginFrameSource(
      int routing_id) override;
  cc::ImageSerializationProcessor* GetImageSerializationProcessor() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  bool AreImageDecodeTasksEnabled() override;
  bool IsThreadedAnimationEnabled() override;

  // scheduler::RendererScheduler::RAILModeObserver implementation.
  void OnRAILModeChanged(v8::RAILMode rail_mode) override;

  // Synchronously establish a channel to the GPU plugin if not previously
  // established or if it has been lost (for example if the GPU plugin crashed).
  // If there is a pending asynchronous request, it will be completed by the
  // time this routine returns.
  scoped_refptr<gpu::GpuChannelHost> EstablishGpuChannelSync(CauseForGpuLaunch);

  std::unique_ptr<cc::OutputSurface> CreateCompositorOutputSurface(
      bool use_software,
      int routing_id,
      scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue,
      const GURL& url);

  // True if we are running layout tests. This currently disables forwarding
  // various status messages to the console, skips network error pages, and
  // short circuits size update and focus events.
  bool layout_test_mode() const { return !!layout_test_deps_; }
  void set_layout_test_dependencies(
      std::unique_ptr<LayoutTestDependencies> deps) {
    layout_test_deps_ = std::move(deps);
  }

  RendererBlinkPlatformImpl* blink_platform_impl() const {
    DCHECK(blink_platform_impl_);
    return blink_platform_impl_.get();
  }

  CompositorForwardingMessageFilter* compositor_message_filter() const {
    return compositor_message_filter_.get();
  }

  InputHandlerManager* input_handler_manager() const {
    return input_handler_manager_.get();
  }

  // Will be null if threaded compositing has not been enabled.
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner() const {
    return compositor_task_runner_;
  }

  AppCacheDispatcher* appcache_dispatcher() const {
    return appcache_dispatcher_.get();
  }

  DomStorageDispatcher* dom_storage_dispatcher() const {
    return dom_storage_dispatcher_.get();
  }

  EmbeddedWorkerDispatcher* embedded_worker_dispatcher() const {
    return embedded_worker_dispatcher_.get();
  }

  AudioInputMessageFilter* audio_input_message_filter() {
    return audio_input_message_filter_.get();
  }

  AudioMessageFilter* audio_message_filter() {
    return audio_message_filter_.get();
  }

  MidiMessageFilter* midi_message_filter() {
    return midi_message_filter_.get();
  }

#if defined(OS_ANDROID)
  RendererDemuxerAndroid* renderer_demuxer() {
    return renderer_demuxer_.get();
  }

  SynchronousCompositorFilter* sync_compositor_message_filter() {
    return sync_compositor_message_filter_.get();
  }

  scoped_refptr<StreamTextureFactory> GetStreamTexureFactory();
  bool EnableStreamTextureCopy();
#endif

  // Creates the embedder implementation of WebMediaStreamCenter.
  // The resulting object is owned by WebKit and deleted by WebKit at tear-down.
  blink::WebMediaStreamCenter* CreateMediaStreamCenter(
      blink::WebMediaStreamCenterClient* client);

  BrowserPluginManager* browser_plugin_manager() const {
    return browser_plugin_manager_.get();
  }

#if defined(ENABLE_WEBRTC)
  // Returns a factory used for creating RTC PeerConnection objects.
  PeerConnectionDependencyFactory* GetPeerConnectionDependencyFactory();

  PeerConnectionTracker* peer_connection_tracker() {
    return peer_connection_tracker_.get();
  }

  // Current P2PSocketDispatcher. Set to NULL if P2P API is disabled.
  P2PSocketDispatcher* p2p_socket_dispatcher() {
    return p2p_socket_dispatcher_.get();
  }
#endif

  VideoCaptureImplManager* video_capture_impl_manager() const {
    return vc_manager_.get();
  }

  // Get the GPU channel. Returns NULL if the channel is not established or
  // has been lost.
  gpu::GpuChannelHost* GetGpuChannel();

  // Returns a SingleThreadTaskRunner instance corresponding to the message loop
  // of the thread on which file operations should be run. Must be called
  // on the renderer's main thread.
  scoped_refptr<base::SingleThreadTaskRunner> GetFileThreadMessageLoopProxy();

  // Returns a SingleThreadTaskRunner instance corresponding to the message loop
  // of the thread on which media operations should be run. Must be called
  // on the renderer's main thread.
  scoped_refptr<base::SingleThreadTaskRunner> GetMediaThreadTaskRunner();

  // A TaskRunner instance that runs tasks on the raster worker pool.
  base::TaskRunner* GetWorkerTaskRunner();

  // Returns a worker context provider that will be bound on the compositor
  // thread.
  scoped_refptr<ContextProviderCommandBuffer>
  SharedCompositorWorkerContextProvider();

  // Causes the idle handler to skip sending idle notifications
  // on the two next scheduled calls, so idle notifications are
  // not sent for at least one notification delay.
  void PostponeIdleNotification();

  media::GpuVideoAcceleratorFactories* GetGpuFactories();

  scoped_refptr<ContextProviderCommandBuffer> SharedMainThreadContextProvider();

  // AudioRendererMixerManager instance which manages renderer side mixer
  // instances shared based on configured audio parameters.  Lazily created on
  // first call.
  AudioRendererMixerManager* GetAudioRendererMixerManager();

  // AudioHardwareConfig contains audio hardware configuration for
  // renderer side clients.  Creation requires a synchronous IPC call so it is
  // lazily created on the first call.
  media::AudioHardwareConfig* GetAudioHardwareConfig();

#if defined(OS_WIN)
  void PreCacheFontCharacters(const LOGFONT& log_font,
                              const base::string16& str);
#endif

#if defined(ENABLE_WEBRTC)
  WebRTCIdentityService* get_webrtc_identity_service() {
    return webrtc_identity_service_.get();
  }
#endif

  // For producing custom V8 histograms. Custom histograms are produced if all
  // RenderViews share the same host, and the host is in the pre-specified set
  // of hosts we want to produce custom diagrams for. The name for a custom
  // diagram is the name of the corresponding generic diagram plus a
  // host-specific suffix.
  class CONTENT_EXPORT HistogramCustomizer {
   public:
    HistogramCustomizer();
    ~HistogramCustomizer();

    // Called when a top frame of a RenderView navigates. This function updates
    // RenderThreadImpl's information about whether all RenderViews are
    // displaying a page from the same host. |host| is the host where a
    // RenderView navigated, and |view_count| is the number of RenderViews in
    // this process.
    void RenderViewNavigatedToHost(const std::string& host, size_t view_count);

    // Used for customizing some histograms if all RenderViews share the same
    // host. Returns the current custom histogram name to use for
    // |histogram_name|, or |histogram_name| if it shouldn't be customized.
    std::string ConvertToCustomHistogramName(const char* histogram_name) const;

   private:
    FRIEND_TEST_ALL_PREFIXES(RenderThreadImplUnittest,
                             IdentifyAlexaTop10NonGoogleSite);
    friend class RenderThreadImplUnittest;

    // Converts a host name to a suffix for histograms
    std::string HostToCustomHistogramSuffix(const std::string& host);

    // Helper function to identify a certain set of top pages
    bool IsAlexaTop10NonGoogleSite(const std::string& host);

    // Used for updating the information on which is the common host which all
    // RenderView's share (if any). If there is no common host, this function is
    // called with an empty string.
    void SetCommonHost(const std::string& host);

    // The current common host of the RenderViews; empty string if there is no
    // common host.
    std::string common_host_;
    // The corresponding suffix.
    std::string common_host_histogram_suffix_;
    // Set of histograms for which we want to produce a custom histogram if
    // possible.
    std::set<std::string> custom_histograms_;

    DISALLOW_COPY_AND_ASSIGN(HistogramCustomizer);
  };

  HistogramCustomizer* histogram_customizer() {
    return &histogram_customizer_;
  }

  // Retrieve current gamepad data.
  void SampleGamepads(blink::WebGamepads* data);

  // Called by a RenderWidget when it is created or destroyed. This
  // allows the process to know when there are no visible widgets.
  void WidgetCreated();
  // Note: A widget must not be hidden when it is destroyed - ensure that
  // WidgetRestored is called before WidgetDestroyed for any hidden widget.
  void WidgetDestroyed();
  void WidgetHidden();
  void WidgetRestored();

  void AddEmbeddedWorkerRoute(int32_t routing_id, IPC::Listener* listener);
  void RemoveEmbeddedWorkerRoute(int32_t routing_id);

  void RegisterPendingFrameCreate(int routing_id,
                                  mojom::FrameRequest frame,
                                  mojom::FrameHostPtr host);

  mojom::StoragePartitionService* GetStoragePartitionService();

 protected:
  RenderThreadImpl(
      const InProcessChildThreadParams& params,
      std::unique_ptr<scheduler::RendererScheduler> scheduler,
      scoped_refptr<base::SingleThreadTaskRunner>& resource_task_queue);
  RenderThreadImpl(std::unique_ptr<base::MessageLoop> main_message_loop,
                   std::unique_ptr<scheduler::RendererScheduler> scheduler);

 private:
  // IPC::Listener
  void OnChannelError() override;

  // ChildThread
  bool OnControlMessageReceived(const IPC::Message& msg) override;
  void OnProcessBackgrounded(bool backgrounded) override;
  void OnProcessPurgeAndSuspend() override;
  void RecordAction(const base::UserMetricsAction& action) override;
  void RecordComputedAction(const std::string& action) override;

  // GpuChannelHostFactory implementation:
  bool IsMainThread() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner() override;
  std::unique_ptr<base::SharedMemory> AllocateSharedMemory(
      size_t size) override;

  void Init(scoped_refptr<base::SingleThreadTaskRunner>& resource_task_queue);

  void InitializeCompositorThread();

  void InitializeWebKit(
      scoped_refptr<base::SingleThreadTaskRunner>& resource_task_queue);

  void OnCreateNewFrame(FrameMsg_NewFrame_Params params);
  void OnCreateNewFrameProxy(int routing_id,
                             int render_view_routing_id,
                             int opener_routing_id,
                             int parent_routing_id,
                             const FrameReplicationState& replicated_state);
  void OnCreateNewView(const ViewMsg_New_Params& params);
  void OnTransferBitmap(const SkBitmap& bitmap, int resource_id);
#if defined(ENABLE_PLUGINS)
  void OnPurgePluginListCache(bool reload_pages);
#endif
  void OnNetworkConnectionChanged(
      net::NetworkChangeNotifier::ConnectionType type,
      double max_bandwidth_mbps);
  void OnGetAccessibilityTree();
  void OnUpdateTimezone(const std::string& zoneId);
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);
#if defined(OS_ANDROID)
  void OnSetWebKitSharedTimersSuspended(bool suspend);
#endif
#if defined(OS_MACOSX)
  void OnUpdateScrollbarTheme(
      const ViewMsg_UpdateScrollbarTheme_Params& params);
  void OnSystemColorsChanged(int aqua_color_variant,
                             const std::string& highlight_text_color,
                             const std::string& highlight_color);
#endif
  void OnCreateNewSharedWorker(
      const WorkerProcessMsg_CreateWorker_Params& params);
  bool RendererIsHidden() const;
  void OnRendererHidden();
  void OnRendererVisible();

  void ReleaseFreeMemory();

  void OnSyncMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // These objects live solely on the render thread.
  std::unique_ptr<AppCacheDispatcher> appcache_dispatcher_;
  std::unique_ptr<DomStorageDispatcher> dom_storage_dispatcher_;
  std::unique_ptr<IndexedDBDispatcher> main_thread_indexed_db_dispatcher_;
  std::unique_ptr<scheduler::RendererScheduler> renderer_scheduler_;
  std::unique_ptr<RendererBlinkPlatformImpl> blink_platform_impl_;
  std::unique_ptr<ResourceDispatchThrottler> resource_dispatch_throttler_;
  std::unique_ptr<CacheStorageDispatcher> main_thread_cache_storage_dispatcher_;
  std::unique_ptr<EmbeddedWorkerDispatcher> embedded_worker_dispatcher_;

  // Used on the render thread and deleted by WebKit at shutdown.
  blink::WebMediaStreamCenter* media_stream_center_;

  // Used on the renderer and IPC threads.
  scoped_refptr<BlobMessageFilter> blob_message_filter_;
  scoped_refptr<DBMessageFilter> db_message_filter_;
  scoped_refptr<AudioInputMessageFilter> audio_input_message_filter_;
  scoped_refptr<AudioMessageFilter> audio_message_filter_;
  scoped_refptr<MidiMessageFilter> midi_message_filter_;
#if defined(OS_ANDROID)
  scoped_refptr<RendererDemuxerAndroid> renderer_demuxer_;
#endif
  scoped_refptr<DevToolsAgentFilter> devtools_agent_message_filter_;
  std::unique_ptr<V8SamplingProfiler> v8_sampling_profiler_;

  std::unique_ptr<BrowserPluginManager> browser_plugin_manager_;

#if defined(ENABLE_WEBRTC)
  std::unique_ptr<PeerConnectionDependencyFactory> peer_connection_factory_;

  // This is used to communicate to the browser process the status
  // of all the peer connections created in the renderer.
  std::unique_ptr<PeerConnectionTracker> peer_connection_tracker_;

  // Dispatches all P2P sockets.
  scoped_refptr<P2PSocketDispatcher> p2p_socket_dispatcher_;
#endif

  // Used on the render thread.
  std::unique_ptr<VideoCaptureImplManager> vc_manager_;

  // Used for communicating registering AEC dump consumers with the browser and
  // receving AEC dump file handles when AEC dump is enabled. An AEC dump is
  // diagnostic audio data for WebRTC stored locally when enabled by the user in
  // chrome://webrtc-internals.
  scoped_refptr<AecDumpMessageFilter> aec_dump_message_filter_;

  // The count of RenderWidgets running through this thread.
  int widget_count_;

  // The count of hidden RenderWidgets running through this thread.
  int hidden_widget_count_;

  // The current value of the idle notification timer delay.
  int64_t idle_notification_delay_in_ms_;

  // The number of idle handler calls that skip sending idle notifications.
  int idle_notifications_to_skip_;

  bool webkit_shared_timer_suspended_;

  // Used to control layout test specific behavior.
  std::unique_ptr<LayoutTestDependencies> layout_test_deps_;

  // Timer that periodically calls IdleHandler.
  base::RepeatingTimer idle_timer_;

  // The channel from the renderer process to the GPU process.
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_;

  // Cache of variables that are needed on the compositor thread by
  // GpuChannelHostFactory methods.
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;

  // The message loop of the renderer main thread.
  // This message loop should be destructed before the RenderThreadImpl
  // shuts down Blink.
  std::unique_ptr<base::MessageLoop> main_message_loop_;

  // A lazily initiated thread on which file operations are run.
  std::unique_ptr<base::Thread> file_thread_;

  // May be null if overridden by ContentRendererClient.
  std::unique_ptr<scheduler::WebThreadBase> compositor_thread_;

  // Utility class to provide GPU functionalities to media.
  // TODO(dcastagna): This should be just one scoped_ptr once
  // http://crbug.com/580386 is fixed.
  // NOTE(dcastagna): At worst this accumulates a few bytes per context lost.
  ScopedVector<content::RendererGpuVideoAcceleratorFactories> gpu_factories_;

  // Thread for running multimedia operations (e.g., video decoding).
  std::unique_ptr<base::Thread> media_thread_;

  // Will point to appropriate task runner after initialization,
  // regardless of whether |compositor_thread_| is overriden.
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

  // Pool of workers used for raster operations (e.g., tile rasterization).
  scoped_refptr<CategorizedWorkerPool> categorized_worker_pool_;

  base::CancelableCallback<void(const IPC::Message&)> main_input_callback_;
  scoped_refptr<IPC::MessageFilter> input_event_filter_;
  std::unique_ptr<InputHandlerManager> input_handler_manager_;
  scoped_refptr<CompositorForwardingMessageFilter> compositor_message_filter_;

#if defined(OS_ANDROID)
  scoped_refptr<SynchronousCompositorFilter> sync_compositor_message_filter_;
  scoped_refptr<StreamTextureFactory> stream_texture_factory_;
#endif

  scoped_refptr<ContextProviderCommandBuffer> shared_main_thread_contexts_;

  base::ObserverList<RenderThreadObserver> observers_;

  scoped_refptr<ContextProviderCommandBuffer> shared_worker_context_provider_;

  std::unique_ptr<AudioRendererMixerManager> audio_renderer_mixer_manager_;
  std::unique_ptr<media::AudioHardwareConfig> audio_hardware_config_;

  HistogramCustomizer histogram_customizer_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

#if defined(ENABLE_WEBRTC)
  std::unique_ptr<WebRTCIdentityService> webrtc_identity_service_;
#endif

  std::unique_ptr<MemoryObserver> memory_observer_;

  scoped_refptr<base::SingleThreadTaskRunner>
      main_thread_compositor_task_runner_;

  // Compositor settings.
  bool is_gpu_rasterization_enabled_;
  bool is_gpu_rasterization_forced_;
  bool is_async_worker_context_enabled_;
  int gpu_rasterization_msaa_sample_count_;
  bool is_lcd_text_enabled_;
  bool is_distance_field_text_enabled_;
  bool is_zero_copy_enabled_;
  bool is_gpu_memory_buffer_compositor_resources_enabled_;
  bool is_partial_raster_enabled_;
  bool is_elastic_overscroll_enabled_;
  std::vector<unsigned> use_image_texture_targets_;
  std::vector<unsigned> use_video_frame_image_texture_targets_;
  bool are_image_decode_tasks_enabled_;
  bool is_threaded_animation_enabled_;

  class PendingFrameCreate : public base::RefCounted<PendingFrameCreate> {
   public:
     PendingFrameCreate(int routing_id,
                        mojom::FrameRequest frame_request,
                        mojom::FrameHostPtr frame_host);

    mojom::FrameRequest TakeFrameRequest() { return std::move(frame_request_); }
    mojom::FrameHostPtr TakeFrameHost() {
      frame_host_.set_connection_error_handler(base::Closure());
      return std::move(frame_host_);
    }

   private:
    friend class base::RefCounted<PendingFrameCreate>;

    ~PendingFrameCreate();

    // Mojo error handler.
    void OnConnectionError();

    int routing_id_;
    mojom::FrameRequest frame_request_;
    mojom::FrameHostPtr frame_host_;
  };

  using PendingFrameCreateMap =
      std::map<int, scoped_refptr<PendingFrameCreate>>;
  PendingFrameCreateMap pending_frame_creates_;

  mojom::StoragePartitionServicePtr storage_partition_service_;

  bool is_renderer_suspended_;

  DISALLOW_COPY_AND_ASSIGN(RenderThreadImpl);
};

#if defined(COMPILER_MSVC)
#pragma warning(pop)
#endif

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_THREAD_IMPL_H_

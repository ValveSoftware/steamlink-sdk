// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <queue>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "content/browser/bluetooth/bluetooth_adapter_factory_wrapper.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/power_monitor_message_broadcaster.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_platform_file.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gpu_switching_observer.h"

namespace base {
class CommandLine;
class MessageLoop;
}

namespace gfx {
class Size;
}

namespace IPC {
class ChannelMojoHost;
}

namespace content {
class AudioInputRendererHost;
class AudioRendererHost;
class BrowserCdmManager;
class BrowserDemuxerAndroid;
class InProcessChildThreadParams;
class MessagePortMessageFilter;
class MojoChildConnection;
class NotificationMessageFilter;
#if defined(ENABLE_WEBRTC)
class P2PSocketDispatcherHost;
#endif
class PermissionServiceContext;
class PeerConnectionTrackerHost;
class RendererMainThread;
class RenderWidgetHelper;
class RenderWidgetHost;
class RenderWidgetHostImpl;
class RenderWidgetHostViewFrameSubscriber;
class StoragePartition;
class StoragePartitionImpl;

namespace mojom {
class StoragePartitionService;
}

typedef base::Thread* (*RendererMainThreadFactoryFunction)(
    const InProcessChildThreadParams& params);

// Implements a concrete RenderProcessHost for the browser process for talking
// to actual renderer processes (as opposed to mocks).
//
// Represents the browser side of the browser <--> renderer communication
// channel. There will be one RenderProcessHost per renderer process.
//
// This object is refcounted so that it can release its resources when all
// hosts using it go away.
//
// This object communicates back and forth with the RenderProcess object
// running in the renderer process. Each RenderProcessHost and RenderProcess
// keeps a list of RenderView (renderer) and WebContentsImpl (browser) which
// are correlated with IDs. This way, the Views and the corresponding ViewHosts
// communicate through the two process objects.
//
// A RenderProcessHost is also associated with one and only one
// StoragePartition.  This allows us to implement strong storage isolation
// because all the IPCs from the RenderViews (renderer) will only ever be able
// to access the partition they are assigned to.
class CONTENT_EXPORT RenderProcessHostImpl
    : public RenderProcessHost,
      public ChildProcessLauncher::Client,
      public ui::GpuSwitchingObserver {
 public:
  RenderProcessHostImpl(BrowserContext* browser_context,
                        StoragePartitionImpl* storage_partition_impl,
                        bool is_for_guests_only);
  ~RenderProcessHostImpl() override;

  // RenderProcessHost implementation (public portion).
  void EnableSendQueue() override;
  bool Init() override;
  int GetNextRoutingID() override;
  void AddRoute(int32_t routing_id, IPC::Listener* listener) override;
  void RemoveRoute(int32_t routing_id) override;
  void AddObserver(RenderProcessHostObserver* observer) override;
  void RemoveObserver(RenderProcessHostObserver* observer) override;
  void ShutdownForBadMessage() override;
  void WidgetRestored() override;
  void WidgetHidden() override;
  int VisibleWidgetCount() const override;
  void AudioStateChanged() override;
  bool IsForGuestsOnly() const override;
  StoragePartition* GetStoragePartition() const override;
  bool Shutdown(int exit_code, bool wait) override;
  bool FastShutdownIfPossible() override;
  base::ProcessHandle GetHandle() const override;
  bool IsReady() const override;
  BrowserContext* GetBrowserContext() const override;
  bool InSameStoragePartition(StoragePartition* partition) const override;
  int GetID() const override;
  bool HasConnection() const override;
  void SetIgnoreInputEvents(bool ignore_input_events) override;
  bool IgnoreInputEvents() const override;
  void Cleanup() override;
  void AddPendingView() override;
  void RemovePendingView() override;
  void SetSuddenTerminationAllowed(bool enabled) override;
  bool SuddenTerminationAllowed() const override;
  IPC::ChannelProxy* GetChannel() override;
  void AddFilter(BrowserMessageFilter* filter) override;
  bool FastShutdownForPageCount(size_t count) override;
  bool FastShutdownStarted() const override;
  base::TimeDelta GetChildProcessIdleTime() const override;
  void FilterURL(bool empty_allowed, GURL* url) override;
#if defined(ENABLE_WEBRTC)
  void EnableAudioDebugRecordings(const base::FilePath& file) override;
  void DisableAudioDebugRecordings() override;
  void EnableEventLogRecordings(const base::FilePath& file) override;
  void DisableEventLogRecordings() override;
  void SetWebRtcLogMessageCallback(
      base::Callback<void(const std::string&)> callback) override;
  void ClearWebRtcLogMessageCallback() override;
  WebRtcStopRtpDumpCallback StartRtpDump(
      bool incoming,
      bool outgoing,
      const WebRtcRtpPacketCallback& packet_callback) override;
#endif
  void ResumeDeferredNavigation(const GlobalRequestID& request_id) override;
  void NotifyTimezoneChange(const std::string& timezone) override;
  shell::InterfaceRegistry* GetInterfaceRegistry() override;
  shell::InterfaceProvider* GetRemoteInterfaces() override;
  shell::Connection* GetChildConnection() override;
  std::unique_ptr<base::SharedPersistentMemoryAllocator> TakeMetricsAllocator()
      override;
  const base::TimeTicks& GetInitTimeForNavigationMetrics() const override;
#if defined(ENABLE_BROWSER_CDMS)
  scoped_refptr<media::MediaKeys> GetCdm(int render_frame_id,
                                         int cdm_id) const override;
#endif
  bool IsProcessBackgrounded() const override;
  void IncrementWorkerRefCount() override;
  void DecrementWorkerRefCount() override;
  void PurgeAndSuspend() override;

  // IPC::Sender via RenderProcessHost.
  bool Send(IPC::Message* msg) override;

  // IPC::Listener via RenderProcessHost.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;
  void OnBadMessageReceived(const IPC::Message& message) override;

  // ChildProcessLauncher::Client implementation.
  void OnProcessLaunched() override;
  void OnProcessLaunchFailed(int error_code) override;

  scoped_refptr<AudioRendererHost> audio_renderer_host() const;

  // Call this function when it is evident that the child process is actively
  // performing some operation, for example if we just received an IPC message.
  void mark_child_process_activity_time() {
    child_process_activity_time_ = base::TimeTicks::Now();
  }

  // Used to extend the lifetime of the sessions until the render view
  // in the renderer is fully closed. This is static because its also called
  // with mock hosts as input in test cases.
  static void ReleaseOnCloseACK(
      RenderProcessHost* host,
      const SessionStorageNamespaceMap& sessions,
      int view_route_id);

  // Register/unregister the host identified by the host id in the global host
  // list.
  static void RegisterHost(int host_id, RenderProcessHost* host);
  static void UnregisterHost(int host_id);

  // Implementation of FilterURL below that can be shared with the mock class.
  static void FilterURL(RenderProcessHost* rph, bool empty_allowed, GURL* url);

  // Returns true if |host| is suitable for launching a new view with |site_url|
  // in the given |browser_context|.
  static bool IsSuitableHost(RenderProcessHost* host,
                             BrowserContext* browser_context,
                             const GURL& site_url);

  // Returns an existing RenderProcessHost for |url| in |browser_context|,
  // if one exists.  Otherwise a new RenderProcessHost should be created and
  // registered using RegisterProcessHostForSite().
  // This should only be used for process-per-site mode, which can be enabled
  // globally with a command line flag or per-site, as determined by
  // SiteInstanceImpl::ShouldUseProcessPerSite.
  static RenderProcessHost* GetProcessHostForSite(
      BrowserContext* browser_context,
      const GURL& url);

  // Registers the given |process| to be used for any instance of |url|
  // within |browser_context|.
  // This should only be used for process-per-site mode, which can be enabled
  // globally with a command line flag or per-site, as determined by
  // SiteInstanceImpl::ShouldUseProcessPerSite.
  static void RegisterProcessHostForSite(
      BrowserContext* browser_context,
      RenderProcessHost* process,
      const GURL& url);

  static base::MessageLoop* GetInProcessRendererThreadForTesting();

  // This forces a renderer that is running "in process" to shut down.
  static void ShutDownInProcessRenderer();

  static void RegisterRendererMainThreadFactory(
      RendererMainThreadFactoryFunction create);

#if defined(OS_ANDROID)
  const scoped_refptr<BrowserDemuxerAndroid>& browser_demuxer_android() {
    return browser_demuxer_android_;
  }
#endif

  MessagePortMessageFilter* message_port_message_filter() const {
    return message_port_message_filter_.get();
  }

  NotificationMessageFilter* notification_message_filter() const {
    return notification_message_filter_.get();
  }

  void set_is_for_guests_only_for_testing(bool is_for_guests_only) {
    is_for_guests_only_ = is_for_guests_only;
  }

  void GetAudioOutputControllers(
      const GetAudioOutputControllersCallback& callback) const override;

  BluetoothAdapterFactoryWrapper* GetBluetoothAdapterFactoryWrapper();

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
  // Launch the zygote early in the browser startup.
  static void EarlyZygoteLaunch();
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)

  void RecomputeAndUpdateWebKitPreferences();

 protected:
  // A proxy for our IPC::Channel that lives on the IO thread.
  std::unique_ptr<IPC::ChannelProxy> channel_;

  // True if fast shutdown has been performed on this RPH.
  bool fast_shutdown_started_;

  // True if we've posted a DeleteTask and will be deleted soon.
  bool deleting_soon_;

#ifndef NDEBUG
  // True if this object has deleted itself.
  bool is_self_deleted_;
#endif

  // The count of currently swapped out but pending RenderViews.  We have
  // started to swap these in, so the renderer process should not exit if
  // this count is non-zero.
  int32_t pending_views_;

 private:
  friend class ChildProcessLauncherBrowserTest_ChildSpawnFail_Test;
  friend class VisitRelayingRenderProcessHost;

  std::unique_ptr<IPC::ChannelProxy> CreateChannelProxy(
      const std::string& channel_id);

  // Creates and adds the IO thread message filters.
  void CreateMessageFilters();

  // Registers Mojo interfaces to be exposed to the renderer.
  void RegisterMojoInterfaces();

  void CreateStoragePartitionService(
      mojo::InterfaceRequest<mojom::StoragePartitionService> request);

  // Control message handlers.
  void OnShutdownRequest();
  void SuddenTerminationChanged(bool enabled);
  void OnUserMetricsRecordAction(const std::string& action);
  void OnCloseACK(int old_route_id);

  // Generates a command line to be used to spawn a renderer and appends the
  // results to |*command_line|.
  void AppendRendererCommandLine(base::CommandLine* command_line) const;

  // Copies applicable command line switches from the given |browser_cmd| line
  // flags to the output |renderer_cmd| line flags. Not all switches will be
  // copied over.
  void PropagateBrowserCommandLineToRenderer(
      const base::CommandLine& browser_cmd,
      base::CommandLine* renderer_cmd) const;

  // Inspects the current object state and sets/removes background priority if
  // appropriate. Should be called after any of the involved data members
  // change.
  void UpdateProcessPriority();

  // Creates a PersistentMemoryAllocator and shares it with the renderer
  // process for it to store histograms from that process. The allocator is
  // available for extraction by a SubprocesMetricsProvider in order to
  // report those histograms to UMA.
  void CreateSharedRendererHistogramAllocator();

  // Handle termination of our process.
  void ProcessDied(bool already_dead, RendererClosedDetails* known_details);

  // GpuSwitchingObserver implementation.
  void OnGpuSwitched() override;

#if defined(ENABLE_WEBRTC)
  void OnRegisterAecDumpConsumer(int id);
  void OnRegisterEventLogConsumer(int id);
  void OnUnregisterAecDumpConsumer(int id);
  void OnUnregisterEventLogConsumer(int id);
  void RegisterAecDumpConsumerOnUIThread(int id);
  void RegisterEventLogConsumerOnUIThread(int id);
  void UnregisterAecDumpConsumerOnUIThread(int id);
  void UnregisterEventLogConsumerOnUIThread(int id);
  void EnableAecDumpForId(const base::FilePath& file, int id);
  void EnableEventLogForId(const base::FilePath& file, int id);
  // Sends |file_for_transit| to the render process.
  void SendAecDumpFileToRenderer(int id,
                                 IPC::PlatformFileForTransit file_for_transit);
  void SendEventLogFileToRenderer(int id,
                                  IPC::PlatformFileForTransit file_for_transit);
  void SendDisableAecDumpToRenderer();
  void SendDisableEventLogToRenderer();
  base::FilePath GetAecDumpFilePathWithExtensions(const base::FilePath& file);
  base::FilePath GetEventLogFilePathWithExtensions(const base::FilePath& file);
#endif

  static void OnMojoError(
      base::WeakPtr<RenderProcessHostImpl> process,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const std::string& error);

  std::string child_token_;

  std::unique_ptr<MojoChildConnection> mojo_child_connection_;
  shell::mojom::ShellClientPtr test_shell_client_;

  // The registered IPC listener objects. When this list is empty, we should
  // delete ourselves.
  IDMap<IPC::Listener> listeners_;

  // The count of currently visible widgets.  Since the host can be a container
  // for multiple widgets, it uses this count to determine when it should be
  // backgrounded.
  int32_t visible_widgets_;

  // Whether this process currently has backgrounded priority. Tracked so that
  // UpdateProcessPriority() can avoid redundantly setting the priority.
  bool is_process_backgrounded_;

  // Used to allow a RenderWidgetHost to intercept various messages on the
  // IO thread.
  scoped_refptr<RenderWidgetHelper> widget_helper_;

  // The filter for MessagePort messages coming from the renderer.
  scoped_refptr<MessagePortMessageFilter> message_port_message_filter_;

  // The filter for Web Notification messages coming from the renderer. Holds a
  // closure per notification that must be freed when the notification closes.
  scoped_refptr<NotificationMessageFilter> notification_message_filter_;

  // Used in single-process mode.
  std::unique_ptr<base::Thread> in_process_renderer_;

  // True after Init() has been called. We can't just check channel_ because we
  // also reset that in the case of process termination.
  bool is_initialized_;

  // PlzNavigate
  // Stores the time at which the first call to Init happened.
  base::TimeTicks init_time_;

  // Used to launch and terminate the process without blocking the UI thread.
  std::unique_ptr<ChildProcessLauncher> child_process_launcher_;

  // Messages we queue while waiting for the process handle.  We queue them here
  // instead of in the channel so that we ensure they're sent after init related
  // messages that are sent once the process handle is available.  This is
  // because the queued messages may have dependencies on the init messages.
  std::queue<IPC::Message*> queued_messages_;

  // The globally-unique identifier for this RPH.
  const int id_;

  // A secondary ID used by the Mojo shell to distinguish different incarnations
  // of the same RPH from each other. Unlike |id_| this is not globally unique,
  // but it is guaranteed to change every time Init() is called.
  int instance_id_ = 1;

  BrowserContext* browser_context_;

  // Owned by |browser_context_|.
  StoragePartitionImpl* storage_partition_impl_;

  // The observers watching our lifetime.
  base::ObserverList<RenderProcessHostObserver> observers_;

  // True if the process can be shut down suddenly.  If this is true, then we're
  // sure that all the RenderViews in the process can be shutdown suddenly.  If
  // it's false, then specific RenderViews might still be allowed to be shutdown
  // suddenly by checking their SuddenTerminationAllowed() flag.  This can occur
  // if one WebContents has an unload event listener but another WebContents in
  // the same process doesn't.
  bool sudden_termination_allowed_;

  // Set to true if we shouldn't send input events.  We actually do the
  // filtering for this at the render widget level.
  bool ignore_input_events_;

  // Records the last time we regarded the child process active.
  base::TimeTicks child_process_activity_time_;

  // Indicates whether this RenderProcessHost is exclusively hosting guest
  // RenderFrames.
  bool is_for_guests_only_;

  // Forwards messages between WebRTCInternals in the browser process
  // and PeerConnectionTracker in the renderer process.
  scoped_refptr<PeerConnectionTrackerHost> peer_connection_tracker_host_;

  // Prevents the class from being added as a GpuDataManagerImpl observer more
  // than once.
  bool gpu_observer_registered_;

  // Set if a call to Cleanup is required once the RenderProcessHostImpl is no
  // longer within the RenderProcessHostObserver::RenderProcessExited callbacks.
  bool delayed_cleanup_needed_;

  // Indicates whether RenderProcessHostImpl is currently iterating and calling
  // through RenderProcessHostObserver::RenderProcessExited.
  bool within_process_died_observer_;

  // Forwards power state messages to the renderer process.
  PowerMonitorMessageBroadcaster power_monitor_broadcaster_;

  scoped_refptr<AudioRendererHost> audio_renderer_host_;

  scoped_refptr<AudioInputRendererHost> audio_input_renderer_host_;

  BluetoothAdapterFactoryWrapper bluetooth_adapter_factory_wrapper_;

#if defined(OS_ANDROID)
  scoped_refptr<BrowserDemuxerAndroid> browser_demuxer_android_;
#endif

#if defined(ENABLE_WEBRTC)
  scoped_refptr<P2PSocketDispatcherHost> p2p_socket_dispatcher_host_;

  // Must be accessed on UI thread.
  std::vector<int> aec_dump_consumers_;

  WebRtcStopRtpDumpCallback stop_rtp_dump_callback_;
#endif

  int worker_ref_count_;

  // Records the time when the process starts surviving for workers for UMA.
  base::TimeTicks survive_for_worker_start_time_;

  // Records the maximum # of workers simultaneously hosted in this process
  // for UMA.
  int max_worker_count_;

  // Context shared for each mojom::PermissionService instance created for this
  // RPH.
  std::unique_ptr<PermissionServiceContext> permission_service_context_;

  // The memory allocator, if any, in which the renderer will write its metrics.
  std::unique_ptr<base::SharedPersistentMemoryAllocator> metrics_allocator_;

  bool channel_connected_;
  bool sent_render_process_ready_;

#if defined(OS_ANDROID)
  // UI thread is the source of sync IPCs and all shutdown signals.
  // Therefore a proper shutdown event to unblock the UI thread is not
  // possible without massive refactoring shutdown code.
  // Luckily Android never performs a clean shutdown. So explicitly
  // ignore this problem.
  base::WaitableEvent never_signaled_;
#endif

  std::string mojo_channel_token_;
  mojo::ScopedMessagePipeHandle in_process_renderer_handle_;

  base::WeakPtrFactory<RenderProcessHostImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_IMPL_H_

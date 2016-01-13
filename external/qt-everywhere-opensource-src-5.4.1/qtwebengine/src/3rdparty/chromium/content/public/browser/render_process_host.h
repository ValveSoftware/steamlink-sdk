// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_H_

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_sender.h"
#include "ui/gfx/native_widget_types.h"

class GURL;
struct ViewMsg_SwapOut_Params;

namespace base {
class TimeDelta;
}

namespace content {
class BrowserContext;
class BrowserMessageFilter;
class RenderProcessHostObserver;
class RenderWidgetHost;
class StoragePartition;
struct GlobalRequestID;

// Interface that represents the browser side of the browser <-> renderer
// communication channel. There will generally be one RenderProcessHost per
// renderer process.
class CONTENT_EXPORT RenderProcessHost : public IPC::Sender,
                                         public IPC::Listener,
                                         public base::SupportsUserData {
 public:
  typedef IDMap<RenderProcessHost>::iterator iterator;

  // Details for RENDERER_PROCESS_CLOSED notifications.
  struct RendererClosedDetails {
    RendererClosedDetails(base::ProcessHandle handle,
                          base::TerminationStatus status,
                          int exit_code) {
      this->handle = handle;
      this->status = status;
      this->exit_code = exit_code;
    }
    base::ProcessHandle handle;
    base::TerminationStatus status;
    int exit_code;
  };

  // General functions ---------------------------------------------------------

  virtual ~RenderProcessHost() {}

  // Initialize the new renderer process, returning true on success. This must
  // be called once before the object can be used, but can be called after
  // that with no effect. Therefore, if the caller isn't sure about whether
  // the process has been created, it should just call Init().
  virtual bool Init() = 0;

  // Gets the next available routing id.
  virtual int GetNextRoutingID() = 0;

  // These methods add or remove listener for a specific message routing ID.
  // Used for refcounting, each holder of this object must AddRoute and
  // RemoveRoute. This object should be allocated on the heap; when no
  // listeners own it any more, it will delete itself.
  virtual void AddRoute(int32 routing_id, IPC::Listener* listener) = 0;
  virtual void RemoveRoute(int32 routing_id) = 0;

  // Add and remove observers for lifecycle events. The order in which
  // notifications are sent to observers is undefined. Observers must be sure to
  // remove the observer before they go away.
  virtual void AddObserver(RenderProcessHostObserver* observer) = 0;
  virtual void RemoveObserver(RenderProcessHostObserver* observer) = 0;

  // Called to wait for the next UpdateRect message for the specified render
  // widget.  Returns true if successful, and the msg out-param will contain a
  // copy of the received UpdateRect message.
  virtual bool WaitForBackingStoreMsg(int render_widget_id,
                                      const base::TimeDelta& max_delay,
                                      IPC::Message* msg) = 0;

  // Called when a received message cannot be decoded.
  virtual void ReceivedBadMessage() = 0;

  // Track the count of visible widgets. Called by listeners to register and
  // unregister visibility.
  virtual void WidgetRestored() = 0;
  virtual void WidgetHidden() = 0;
  virtual int VisibleWidgetCount() const = 0;

  // Indicates whether the current RenderProcessHost is associated with an
  // isolated guest renderer process. Not all guest renderers are created equal.
  // A guest, as indicated by BrowserPluginGuest::IsGuest, may coexist with
  // other non-guest renderers in the same process if IsIsolatedGuest is false.
  virtual bool IsIsolatedGuest() const = 0;

  // Returns the storage partition associated with this process.
  //
  // TODO(nasko): Remove this function from the public API once
  // URLRequestContextGetter's creation is moved into StoragePartition.
  // http://crbug.com/158595
  virtual StoragePartition* GetStoragePartition() const = 0;

  // Try to shutdown the associated renderer process as fast as possible.
  // If this renderer has any RenderViews with unload handlers, then this
  // function does nothing.  The current implementation uses TerminateProcess.
  // Returns True if it was able to do fast shutdown.
  virtual bool FastShutdownIfPossible() = 0;

  // Returns true if fast shutdown was started for the renderer.
  virtual bool FastShutdownStarted() const = 0;

  // Dump the child process' handle table before shutting down.
  virtual void DumpHandles() = 0;

  // Returns the process object associated with the child process.  In certain
  // tests or single-process mode, this will actually represent the current
  // process.
  //
  // NOTE: this is not necessarily valid immediately after calling Init, as
  // Init starts the process asynchronously.  It's guaranteed to be valid after
  // the first IPC arrives.
  virtual base::ProcessHandle GetHandle() const = 0;

  // Returns the user browser context associated with this renderer process.
  virtual content::BrowserContext* GetBrowserContext() const = 0;

  // Returns whether this process is using the same StoragePartition as
  // |partition|.
  virtual bool InSameStoragePartition(StoragePartition* partition) const = 0;

  // Returns the unique ID for this child process host. This can be used later
  // in a call to FromID() to get back to this object (this is used to avoid
  // sending non-threadsafe pointers to other threads).
  //
  // This ID will be unique across all child process hosts, including workers,
  // plugins, etc.
  //
  // This will never return ChildProcessHost::kInvalidUniqueID.
  virtual int GetID() const = 0;

  // Returns true iff channel_ has been set to non-NULL. Use this for checking
  // if there is connection or not. Virtual for mocking out for tests.
  virtual bool HasConnection() const = 0;

  // Call this to allow queueing of IPC messages that are sent before the
  // process is launched.
  virtual void EnableSendQueue() = 0;

  // Returns the renderer channel.
  virtual IPC::ChannelProxy* GetChannel() = 0;

  // Adds a message filter to the IPC channel.
  virtual void AddFilter(BrowserMessageFilter* filter) = 0;

  // Try to shutdown the associated render process as fast as possible
  virtual bool FastShutdownForPageCount(size_t count) = 0;

  // TODO(ananta)
  // Revisit whether the virtual functions declared from here on need to be
  // part of the interface.
  virtual void SetIgnoreInputEvents(bool ignore_input_events) = 0;
  virtual bool IgnoreInputEvents() const = 0;

  // Schedules the host for deletion and removes it from the all_hosts list.
  virtual void Cleanup() = 0;

  // Track the count of pending views that are being swapped back in.  Called
  // by listeners to register and unregister pending views to prevent the
  // process from exiting.
  virtual void AddPendingView() = 0;
  virtual void RemovePendingView() = 0;

  // Sets a flag indicating that the process can be abnormally terminated.
  virtual void SetSuddenTerminationAllowed(bool allowed) = 0;
  // Returns true if the process can be abnormally terminated.
  virtual bool SuddenTerminationAllowed() const = 0;

  // Returns how long the child has been idle. The definition of idle
  // depends on when a derived class calls mark_child_process_activity_time().
  // This is a rough indicator and its resolution should not be better than
  // 10 milliseconds.
  virtual base::TimeDelta GetChildProcessIdleTime() const = 0;

  // Called to resume the requests for a view created through window.open that
  // were initially blocked.
  virtual void ResumeRequestsForView(int route_id) = 0;

  // Checks that the given renderer can request |url|, if not it sets it to
  // about:blank.
  // |empty_allowed| must be set to false for navigations for security reasons.
  virtual void FilterURL(bool empty_allowed, GURL* url) = 0;

#if defined(ENABLE_WEBRTC)
  virtual void EnableAecDump(const base::FilePath& file) = 0;
  virtual void DisableAecDump() = 0;

  // When set, |callback| receives log messages regarding, for example, media
  // devices (webcams, mics, etc) that were initially requested in the render
  // process associated with this RenderProcessHost.
  virtual void SetWebRtcLogMessageCallback(
      base::Callback<void(const std::string&)> callback) = 0;

  typedef base::Callback<void(scoped_ptr<uint8[]> packet_header,
                              size_t header_length,
                              size_t packet_length,
                              bool incoming)> WebRtcRtpPacketCallback;

  typedef base::Callback<void(bool incoming, bool outgoing)>
      WebRtcStopRtpDumpCallback;

  // Starts passing RTP packets to |packet_callback| and returns the callback
  // used to stop dumping.
  virtual WebRtcStopRtpDumpCallback StartRtpDump(
      bool incoming,
      bool outgoing,
      const WebRtcRtpPacketCallback& packet_callback) = 0;
#endif

  // Tells the ResourceDispatcherHost to resume a deferred navigation without
  // transferring it to a new renderer process.
  virtual void ResumeDeferredNavigation(const GlobalRequestID& request_id) = 0;

  // Notifies the renderer that the timezone configuration of the system might
  // have changed.
  virtual void NotifyTimezoneChange() = 0;

  // Static management functions -----------------------------------------------

  // Flag to run the renderer in process.  This is primarily
  // for debugging purposes.  When running "in process", the
  // browser maintains a single RenderProcessHost which communicates
  // to a RenderProcess which is instantiated in the same process
  // with the Browser.  All IPC between the Browser and the
  // Renderer is the same, it's just not crossing a process boundary.

  static bool run_renderer_in_process();

  // This also calls out to ContentBrowserClient::GetApplicationLocale and
  // modifies the current process' command line.
  static void SetRunRendererInProcess(bool value);

  // Allows iteration over all the RenderProcessHosts in the browser. Note
  // that each host may not be active, and therefore may have NULL channels.
  static iterator AllHostsIterator();

  // Returns the RenderProcessHost given its ID.  Returns NULL if the ID does
  // not correspond to a live RenderProcessHost.
  static RenderProcessHost* FromID(int render_process_id);

  // Returns whether the process-per-site model is in use (globally or just for
  // the current site), in which case we should ensure there is only one
  // RenderProcessHost per site for the entire browser context.
  static bool ShouldUseProcessPerSite(content::BrowserContext* browser_context,
                                      const GURL& url);

  // Returns true if the caller should attempt to use an existing
  // RenderProcessHost rather than creating a new one.
  static bool ShouldTryToUseExistingProcessHost(
      content::BrowserContext* browser_context, const GURL& site_url);

  // Get an existing RenderProcessHost associated with the given browser
  // context, if possible.  The renderer process is chosen randomly from
  // suitable renderers that share the same context and type (determined by the
  // site url).
  // Returns NULL if no suitable renderer process is available, in which case
  // the caller is free to create a new renderer.
  static RenderProcessHost* GetExistingProcessHost(
      content::BrowserContext* browser_context, const GURL& site_url);

  // Overrides the default heuristic for limiting the max renderer process
  // count.  This is useful for unit testing process limit behaviors.  It is
  // also used to allow a command line parameter to configure the max number of
  // renderer processes and should only be called once during startup.
  // A value of zero means to use the default heuristic.
  static void SetMaxRendererProcessCount(size_t count);

  // Returns the current max number of renderer processes used by the content
  // module.
  static size_t GetMaxRendererProcessCount();
};

}  // namespace content.

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_H_

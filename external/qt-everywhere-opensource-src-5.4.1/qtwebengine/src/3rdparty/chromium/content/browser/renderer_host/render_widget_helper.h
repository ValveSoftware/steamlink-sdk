// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HELPER_H_

#include <deque>
#include <map>

#include "base/atomic_sequence_num.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/common/window_container_type.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/surface/transport_dib.h"

namespace IPC {
class Message;
}

namespace base {
class TimeDelta;
}

struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;
struct ViewHostMsg_CreateWindow_Params;
struct ViewMsg_SwapOut_Params;

namespace content {
class GpuProcessHost;
class ResourceDispatcherHostImpl;
class SessionStorageNamespace;

// Instantiated per RenderProcessHost to provide various optimizations on
// behalf of a RenderWidgetHost.  This class bridges between the IO thread
// where the RenderProcessHost's MessageFilter lives and the UI thread where
// the RenderWidgetHost lives.
//
//
// OPTIMIZED RESIZE
//
//   RenderWidgetHelper is used to implement optimized resize.  When the
//   RenderWidgetHost is resized, it sends a Resize message to its RenderWidget
//   counterpart in the renderer process.  In response to the Resize message,
//   the RenderWidget generates a new BackingStore and sends an UpdateRect
//   message (or BuffersSwapped via the GPU process in the case of accelerated
//   compositing), and it sets the IS_RESIZE_ACK flag in the UpdateRect message
//   to true.  In the accelerated case, an UpdateRect is still sent from the
//   renderer to the browser with acks and plugin moves even though the GPU
//   BackingStore was sent earlier in the BuffersSwapped message. "BackingStore
//   message" is used throughout this code and documentation to mean either a
//   software UpdateRect or GPU BuffersSwapped message.
//
//   Back in the browser process, when the RenderProcessHost's MessageFilter
//   sees an UpdateRect message (or when the GpuProcessHost sees a
//   BuffersSwapped message), it directs it to the RenderWidgetHelper by calling
//   the DidReceiveBackingStoreMsg method. That method stores the data for the
//   message in a map, where it can be directly accessed by the RenderWidgetHost
//   on the UI thread during a call to RenderWidgetHost's GetBackingStore
//   method.
//
//   When the RenderWidgetHost's GetBackingStore method is called, it first
//   checks to see if it is waiting for a resize ack.  If it is, then it calls
//   the RenderWidgetHelper's WaitForBackingStoreMsg to check if there is
//   already a resulting BackingStore message (or to wait a short amount of time
//   for one to arrive).  The main goal of this mechanism is to short-cut the
//   usual way in which IPC messages are proxied over to the UI thread via
//   InvokeLater. This approach is necessary since window resize is followed up
//   immediately by a request to repaint the window.
//
//
// OPTIMIZED TAB SWITCHING
//
//   When a RenderWidgetHost is in a background tab, it is flagged as hidden.
//   This causes the corresponding RenderWidget to stop sending BackingStore
//   messages. The RenderWidgetHost also discards its backingstore when it is
//   hidden, which helps free up memory.  As a result, when a RenderWidgetHost
//   is restored, it can be momentarily be without a backingstore.  (Restoring
//   a RenderWidgetHost results in a WasShown message being sent to the
//   RenderWidget, which triggers a full BackingStore message.)  This can lead
//   to an observed rendering glitch as the WebContentsImpl will just have to
//   fill white overtop the RenderWidgetHost until the RenderWidgetHost
//   receives a BackingStore message to refresh its backingstore.
//
//   To avoid this 'white flash', the RenderWidgetHost again makes use of the
//   RenderWidgetHelper's WaitForBackingStoreMsg method.  When the
//   RenderWidgetHost's GetBackingStore method is called, it will call
//   WaitForBackingStoreMsg if it has no backingstore.
//
// TRANSPORT DIB CREATION
//
//   On some platforms (currently the Mac) the renderer cannot create transport
//   DIBs because of sandbox limitations. Thus, it has to make synchronous IPCs
//   to the browser for them. Since these requests are synchronous, they cannot
//   terminate on the UI thread. Thus, in this case, this object performs the
//   allocation and maintains the set of allocated transport DIBs which the
//   renderers can refer to.
//
class RenderWidgetHelper
    : public base::RefCountedThreadSafe<RenderWidgetHelper,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  RenderWidgetHelper();

  void Init(int render_process_id,
            ResourceDispatcherHostImpl* resource_dispatcher_host);

  // Gets the next available routing id.  This is thread safe.
  int GetNextRoutingID();

  // IO THREAD ONLY -----------------------------------------------------------

  // Lookup the RenderWidgetHelper from the render_process_host_id. Returns NULL
  // if not found. NOTE: The raw pointer is for temporary use only. To retain,
  // store in a scoped_refptr.
  static RenderWidgetHelper* FromProcessHostID(int render_process_host_id);

  // UI THREAD ONLY -----------------------------------------------------------

  // These three functions provide the backend implementation of the
  // corresponding functions in RenderProcessHost. See those declarations
  // for documentation.
  void ResumeDeferredNavigation(const GlobalRequestID& request_id);
  bool WaitForBackingStoreMsg(int render_widget_id,
                              const base::TimeDelta& max_delay,
                              IPC::Message* msg);
  // Called to resume the requests for a view after it's ready. The view was
  // created by CreateNewWindow which initially blocked the requests.
  void ResumeRequestsForView(int route_id);

  // IO THREAD ONLY -----------------------------------------------------------

  // Called on the IO thread when a BackingStore message is received.
  void DidReceiveBackingStoreMsg(const IPC::Message& msg);

  void CreateNewWindow(
      const ViewHostMsg_CreateWindow_Params& params,
      bool no_javascript_access,
      base::ProcessHandle render_process,
      int* route_id,
      int* main_frame_route_id,
      int* surface_id,
      SessionStorageNamespace* session_storage_namespace);
  void CreateNewWidget(int opener_id,
                       blink::WebPopupType popup_type,
                       int* route_id,
                       int* surface_id);
  void CreateNewFullscreenWidget(int opener_id, int* route_id, int* surface_id);

#if defined(OS_POSIX)
  // Called on the IO thread to handle the allocation of a TransportDIB.  If
  // |cache_in_browser| is |true|, then a copy of the shmem is kept by the
  // browser, and it is the caller's repsonsibility to call
  // FreeTransportDIB().  In all cases, the caller is responsible for deleting
  // the resulting TransportDIB.
  void AllocTransportDIB(uint32 size,
                         bool cache_in_browser,
                         TransportDIB::Handle* result);

  // Called on the IO thread to handle the freeing of a transport DIB
  void FreeTransportDIB(TransportDIB::Id dib_id);
#endif

#if defined(OS_MACOSX)
  static void OnNativeSurfaceBuffersSwappedOnIOThread(
      GpuProcessHost* gpu_process_host,
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params);
#endif

 private:
  // A class used to proxy a paint message.  PaintMsgProxy objects are created
  // on the IO thread and destroyed on the UI thread.
  class BackingStoreMsgProxy;
  friend class BackingStoreMsgProxy;
  friend class base::RefCountedThreadSafe<RenderWidgetHelper>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<RenderWidgetHelper>;

  typedef std::deque<BackingStoreMsgProxy*> BackingStoreMsgProxyQueue;
  // Map from render_widget_id to a queue of live PaintMsgProxy instances.
  typedef base::hash_map<int, BackingStoreMsgProxyQueue >
      BackingStoreMsgProxyMap;

  ~RenderWidgetHelper();

  // Called on the UI thread to discard a paint message.
  void OnDiscardBackingStoreMsg(BackingStoreMsgProxy* proxy);

  // Called on the UI thread to dispatch a paint message if necessary.
  void OnDispatchBackingStoreMsg(BackingStoreMsgProxy* proxy);

  // Called on the UI thread to finish creating a window.
  void OnCreateWindowOnUI(
      const ViewHostMsg_CreateWindow_Params& params,
      int route_id,
      int main_frame_route_id,
      SessionStorageNamespace* session_storage_namespace);

  // Called on the IO thread after a window was created on the UI thread.
  void OnResumeRequestsForView(int route_id);

  // Called on the UI thread to finish creating a widget.
  void OnCreateWidgetOnUI(int opener_id,
                          int route_id,
                          blink::WebPopupType popup_type);

  // Called on the UI thread to create a fullscreen widget.
  void OnCreateFullscreenWidgetOnUI(int opener_id, int route_id);

  // Called on the IO thread to resume a paused navigation in the network
  // stack without transferring it to a new renderer process.
  void OnResumeDeferredNavigation(const GlobalRequestID& request_id);

#if defined(OS_POSIX)
  // Called on destruction to release all allocated transport DIBs
  void ClearAllocatedDIBs();

  // On POSIX we keep file descriptors to all the allocated DIBs around until
  // the renderer frees them.
  base::Lock allocated_dibs_lock_;
  std::map<TransportDIB::Id, int> allocated_dibs_;
#endif

  // A map of live paint messages.  Must hold pending_paints_lock_ to access.
  // The BackingStoreMsgProxy objects are not owned by this map. (See
  // BackingStoreMsgProxy for details about how the lifetime of instances are
  // managed.)
  BackingStoreMsgProxyMap pending_paints_;
  base::Lock pending_paints_lock_;

  int render_process_id_;

  // Event used to implement WaitForBackingStoreMsg.
  base::WaitableEvent event_;

  // The next routing id to use.
  base::AtomicSequenceNumber next_routing_id_;

  ResourceDispatcherHostImpl* resource_dispatcher_host_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HELPER_H_

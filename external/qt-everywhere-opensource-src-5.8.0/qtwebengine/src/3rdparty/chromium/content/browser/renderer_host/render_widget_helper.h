// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HELPER_H_

#include <stdint.h>

#include <map>

#include "base/atomic_sequence_num.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/common/window_container_type.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "ui/gfx/native_widget_types.h"

namespace IPC {
class Message;
}

namespace base {
class TimeDelta;
}

struct ViewHostMsg_CreateWindow_Params;

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

  // These two functions provide the backend implementation of the
  // corresponding functions in RenderProcessHost. See those declarations
  // for documentation.
  void ResumeDeferredNavigation(const GlobalRequestID& request_id);

  // IO THREAD ONLY -----------------------------------------------------------

  void CreateNewWindow(const ViewHostMsg_CreateWindow_Params& params,
                       bool no_javascript_access,
                       base::ProcessHandle render_process,
                       int32_t* route_id,
                       int32_t* main_frame_route_id,
                       int32_t* main_frame_widget_route_id,
                       SessionStorageNamespace* session_storage_namespace);
  void CreateNewWidget(int opener_id,
                       blink::WebPopupType popup_type,
                       int* route_id);
  void CreateNewFullscreenWidget(int opener_id, int* route_id);

 private:
  friend class base::RefCountedThreadSafe<RenderWidgetHelper>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<RenderWidgetHelper>;

  ~RenderWidgetHelper();

  // Called on the UI thread to finish creating a window.
  void OnCreateWindowOnUI(const ViewHostMsg_CreateWindow_Params& params,
                          int32_t route_id,
                          int32_t main_frame_route_id,
                          int32_t main_frame_widget_route_id,
                          SessionStorageNamespace* session_storage_namespace);

  // Called on the UI thread to finish creating a widget.
  void OnCreateWidgetOnUI(int32_t opener_id,
                          int32_t route_id,
                          blink::WebPopupType popup_type);

  // Called on the UI thread to create a fullscreen widget.
  void OnCreateFullscreenWidgetOnUI(int32_t opener_id, int32_t route_id);

  // Called on the IO thread to resume a paused navigation in the network
  // stack without transferring it to a new renderer process.
  void OnResumeDeferredNavigation(const GlobalRequestID& request_id);

  // Called on the IO thread to resume a navigation paused immediately after
  // receiving response headers.
  void OnResumeResponseDeferredAtStart(const GlobalRequestID& request_id);

  int render_process_id_;

  // The next routing id to use.
  base::AtomicSequenceNumber next_routing_id_;

  ResourceDispatcherHostImpl* resource_dispatcher_host_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HELPER_H_

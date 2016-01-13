// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/common/view_messages.h"

namespace content {
namespace {

typedef std::map<int, RenderWidgetHelper*> WidgetHelperMap;
base::LazyInstance<WidgetHelperMap> g_widget_helpers =
    LAZY_INSTANCE_INITIALIZER;

void AddWidgetHelper(int render_process_id,
                     const scoped_refptr<RenderWidgetHelper>& widget_helper) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // We don't care if RenderWidgetHelpers overwrite an existing process_id. Just
  // want this to be up to date.
  g_widget_helpers.Get()[render_process_id] = widget_helper.get();
}

}  // namespace

// A helper used with DidReceiveBackingStoreMsg that we hold a pointer to in
// pending_paints_.
class RenderWidgetHelper::BackingStoreMsgProxy {
 public:
  BackingStoreMsgProxy(RenderWidgetHelper* h, const IPC::Message& m);
  ~BackingStoreMsgProxy();
  void Run();
  void Cancel() { cancelled_ = true; }

  const IPC::Message& message() const { return message_; }

 private:
  scoped_refptr<RenderWidgetHelper> helper_;
  IPC::Message message_;
  bool cancelled_;  // If true, then the message will not be dispatched.

  DISALLOW_COPY_AND_ASSIGN(BackingStoreMsgProxy);
};

RenderWidgetHelper::BackingStoreMsgProxy::BackingStoreMsgProxy(
    RenderWidgetHelper* h, const IPC::Message& m)
    : helper_(h),
      message_(m),
      cancelled_(false) {
}

RenderWidgetHelper::BackingStoreMsgProxy::~BackingStoreMsgProxy() {
  // If the paint message was never dispatched, then we need to let the
  // helper know that we are going away.
  if (!cancelled_ && helper_.get())
    helper_->OnDiscardBackingStoreMsg(this);
}

void RenderWidgetHelper::BackingStoreMsgProxy::Run() {
  if (!cancelled_) {
    helper_->OnDispatchBackingStoreMsg(this);
    helper_ = NULL;
  }
}

RenderWidgetHelper::RenderWidgetHelper()
    : render_process_id_(-1),
#if defined(OS_WIN)
      event_(CreateEvent(NULL, FALSE /* auto-reset */, FALSE, NULL)),
#elif defined(OS_POSIX)
      event_(false /* auto-reset */, false),
#endif
      resource_dispatcher_host_(NULL) {
}

RenderWidgetHelper::~RenderWidgetHelper() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Delete this RWH from the map if it is found.
  WidgetHelperMap& widget_map = g_widget_helpers.Get();
  WidgetHelperMap::iterator it = widget_map.find(render_process_id_);
  if (it != widget_map.end() && it->second == this)
    widget_map.erase(it);

  // The elements of pending_paints_ each hold an owning reference back to this
  // object, so we should not be destroyed unless pending_paints_ is empty!
  DCHECK(pending_paints_.empty());

#if defined(OS_POSIX) && !defined(OS_ANDROID)
  ClearAllocatedDIBs();
#endif
}

void RenderWidgetHelper::Init(
    int render_process_id,
    ResourceDispatcherHostImpl* resource_dispatcher_host) {
  render_process_id_ = render_process_id;
  resource_dispatcher_host_ = resource_dispatcher_host;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AddWidgetHelper,
                 render_process_id_, make_scoped_refptr(this)));
}

int RenderWidgetHelper::GetNextRoutingID() {
  return next_routing_id_.GetNext() + 1;
}

// static
RenderWidgetHelper* RenderWidgetHelper::FromProcessHostID(
    int render_process_host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  WidgetHelperMap::const_iterator ci = g_widget_helpers.Get().find(
      render_process_host_id);
  return (ci == g_widget_helpers.Get().end())? NULL : ci->second;
}

void RenderWidgetHelper::ResumeDeferredNavigation(
    const GlobalRequestID& request_id) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RenderWidgetHelper::OnResumeDeferredNavigation,
                 this,
                 request_id));
}

bool RenderWidgetHelper::WaitForBackingStoreMsg(
    int render_widget_id, const base::TimeDelta& max_delay, IPC::Message* msg) {
  base::TimeTicks time_start = base::TimeTicks::Now();

  for (;;) {
    BackingStoreMsgProxy* proxy = NULL;
    {
      base::AutoLock lock(pending_paints_lock_);

      BackingStoreMsgProxyMap::iterator it =
          pending_paints_.find(render_widget_id);
      if (it != pending_paints_.end()) {
        BackingStoreMsgProxyQueue &queue = it->second;
        DCHECK(!queue.empty());
        proxy = queue.front();

        // Flag the proxy as cancelled so that when it is run as a task it will
        // do nothing.
        proxy->Cancel();

        queue.pop_front();
        if (queue.empty())
          pending_paints_.erase(it);
      }
    }

    if (proxy) {
      *msg = proxy->message();
      DCHECK(msg->routing_id() == render_widget_id);
      return true;
    }

    // Calculate the maximum amount of time that we are willing to sleep.
    base::TimeDelta max_sleep_time =
        max_delay - (base::TimeTicks::Now() - time_start);
    if (max_sleep_time <= base::TimeDelta::FromMilliseconds(0))
      break;

    base::ThreadRestrictions::ScopedAllowWait allow_wait;
    event_.TimedWait(max_sleep_time);
  }

  return false;
}

void RenderWidgetHelper::ResumeRequestsForView(int route_id) {
  // We only need to resume blocked requests if we used a valid route_id.
  // See CreateNewWindow.
  if (route_id != MSG_ROUTING_NONE) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&RenderWidgetHelper::OnResumeRequestsForView,
            this, route_id));
  }
}

void RenderWidgetHelper::DidReceiveBackingStoreMsg(const IPC::Message& msg) {
  int render_widget_id = msg.routing_id();

  BackingStoreMsgProxy* proxy = new BackingStoreMsgProxy(this, msg);
  {
    base::AutoLock lock(pending_paints_lock_);

    pending_paints_[render_widget_id].push_back(proxy);
  }

  // Notify anyone waiting on the UI thread that there is a new entry in the
  // proxy map.  If they don't find the entry they are looking for, then they
  // will just continue waiting.
  event_.Signal();

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&BackingStoreMsgProxy::Run, base::Owned(proxy)));
}

void RenderWidgetHelper::OnDiscardBackingStoreMsg(BackingStoreMsgProxy* proxy) {
  const IPC::Message& msg = proxy->message();

  // Remove the proxy from the map now that we are going to handle it normally.
  {
    base::AutoLock lock(pending_paints_lock_);

    BackingStoreMsgProxyMap::iterator it =
        pending_paints_.find(msg.routing_id());
    DCHECK(it != pending_paints_.end());
    BackingStoreMsgProxyQueue &queue = it->second;
    DCHECK(queue.front() == proxy);

    queue.pop_front();
    if (queue.empty())
      pending_paints_.erase(it);
  }
}

void RenderWidgetHelper::OnDispatchBackingStoreMsg(
    BackingStoreMsgProxy* proxy) {
  OnDiscardBackingStoreMsg(proxy);

  // It is reasonable for the host to no longer exist.
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id_);
  if (host)
    host->OnMessageReceived(proxy->message());
}

void RenderWidgetHelper::OnResumeDeferredNavigation(
    const GlobalRequestID& request_id) {
  resource_dispatcher_host_->ResumeDeferredNavigation(request_id);
}

void RenderWidgetHelper::CreateNewWindow(
    const ViewHostMsg_CreateWindow_Params& params,
    bool no_javascript_access,
    base::ProcessHandle render_process,
    int* route_id,
    int* main_frame_route_id,
    int* surface_id,
    SessionStorageNamespace* session_storage_namespace) {
  if (params.opener_suppressed || no_javascript_access) {
    // If the opener is supppressed or script access is disallowed, we should
    // open the window in a new BrowsingInstance, and thus a new process. That
    // means the current renderer process will not be able to route messages to
    // it. Because of this, we will immediately show and navigate the window
    // in OnCreateWindowOnUI, using the params provided here.
    *route_id = MSG_ROUTING_NONE;
    *main_frame_route_id = MSG_ROUTING_NONE;
    *surface_id = 0;
  } else {
    *route_id = GetNextRoutingID();
    *main_frame_route_id = GetNextRoutingID();
    *surface_id = GpuSurfaceTracker::Get()->AddSurfaceForRenderer(
        render_process_id_, *route_id);
    // Block resource requests until the view is created, since the HWND might
    // be needed if a response ends up creating a plugin.
    resource_dispatcher_host_->BlockRequestsForRoute(
        render_process_id_, *route_id);
    resource_dispatcher_host_->BlockRequestsForRoute(
        render_process_id_, *main_frame_route_id);
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RenderWidgetHelper::OnCreateWindowOnUI,
                 this, params, *route_id, *main_frame_route_id,
                 make_scoped_refptr(session_storage_namespace)));
}

void RenderWidgetHelper::OnCreateWindowOnUI(
    const ViewHostMsg_CreateWindow_Params& params,
    int route_id,
    int main_frame_route_id,
    SessionStorageNamespace* session_storage_namespace) {
  RenderViewHostImpl* host =
      RenderViewHostImpl::FromID(render_process_id_, params.opener_id);
  if (host)
    host->CreateNewWindow(route_id, main_frame_route_id, params,
        session_storage_namespace);
}

void RenderWidgetHelper::OnResumeRequestsForView(int route_id) {
  resource_dispatcher_host_->ResumeBlockedRequestsForRoute(
      render_process_id_, route_id);
}

void RenderWidgetHelper::CreateNewWidget(int opener_id,
                                         blink::WebPopupType popup_type,
                                         int* route_id,
                                         int* surface_id) {
  *route_id = GetNextRoutingID();
  *surface_id = GpuSurfaceTracker::Get()->AddSurfaceForRenderer(
      render_process_id_, *route_id);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &RenderWidgetHelper::OnCreateWidgetOnUI, this, opener_id, *route_id,
          popup_type));
}

void RenderWidgetHelper::CreateNewFullscreenWidget(int opener_id,
                                                   int* route_id,
                                                   int* surface_id) {
  *route_id = GetNextRoutingID();
  *surface_id = GpuSurfaceTracker::Get()->AddSurfaceForRenderer(
      render_process_id_, *route_id);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &RenderWidgetHelper::OnCreateFullscreenWidgetOnUI, this,
          opener_id, *route_id));
}

void RenderWidgetHelper::OnCreateWidgetOnUI(
    int opener_id, int route_id, blink::WebPopupType popup_type) {
  RenderViewHostImpl* host = RenderViewHostImpl::FromID(
      render_process_id_, opener_id);
  if (host)
    host->CreateNewWidget(route_id, popup_type);
}

void RenderWidgetHelper::OnCreateFullscreenWidgetOnUI(int opener_id,
                                                      int route_id) {
  RenderViewHostImpl* host = RenderViewHostImpl::FromID(
      render_process_id_, opener_id);
  if (host)
    host->CreateNewFullscreenWidget(route_id);
}

#if defined(OS_POSIX) && !defined(OS_ANDROID)
void RenderWidgetHelper::AllocTransportDIB(uint32 size,
                                           bool cache_in_browser,
                                           TransportDIB::Handle* result) {
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());
  if (!shared_memory->CreateAnonymous(size)) {
    result->fd = -1;
    result->auto_close = false;
    return;
  }

  shared_memory->GiveToProcess(0 /* pid, not needed */, result);

  if (cache_in_browser) {
    // Keep a copy of the file descriptor around
    base::AutoLock locked(allocated_dibs_lock_);
    allocated_dibs_[shared_memory->id()] = dup(result->fd);
  }
}

void RenderWidgetHelper::FreeTransportDIB(TransportDIB::Id dib_id) {
  base::AutoLock locked(allocated_dibs_lock_);

  const std::map<TransportDIB::Id, int>::iterator
    i = allocated_dibs_.find(dib_id);

  if (i != allocated_dibs_.end()) {
    if (IGNORE_EINTR(close(i->second)) < 0)
      PLOG(ERROR) << "close";
    allocated_dibs_.erase(i);
  } else {
    DLOG(WARNING) << "Renderer asked us to free unknown transport DIB";
  }
}

void RenderWidgetHelper::ClearAllocatedDIBs() {
  for (std::map<TransportDIB::Id, int>::iterator
       i = allocated_dibs_.begin(); i != allocated_dibs_.end(); ++i) {
    if (IGNORE_EINTR(close(i->second)) < 0)
      PLOG(ERROR) << "close: " << i->first;
  }

  allocated_dibs_.clear();
}
#endif

}  // namespace content

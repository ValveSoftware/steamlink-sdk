// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/web_service_worker_impl.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/web_service_worker_provider_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProxy.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"

using blink::WebMessagePortChannel;
using blink::WebMessagePortChannelArray;
using blink::WebMessagePortChannelClient;
using blink::WebSecurityOrigin;
using blink::WebString;

namespace content {

namespace {

class HandleImpl : public blink::WebServiceWorker::Handle {
 public:
  explicit HandleImpl(const scoped_refptr<WebServiceWorkerImpl>& worker)
      : worker_(worker) {}
  ~HandleImpl() override {}

  blink::WebServiceWorker* serviceWorker() override { return worker_.get(); }

 private:
  scoped_refptr<WebServiceWorkerImpl> worker_;

  DISALLOW_COPY_AND_ASSIGN(HandleImpl);
};

void SendPostMessageToWorkerOnMainThread(
    ThreadSafeSender* thread_safe_sender,
    int handle_id,
    int provider_id,
    const base::string16& message,
    const url::Origin& source_origin,
    std::unique_ptr<WebMessagePortChannelArray> channels) {
  thread_safe_sender->Send(new ServiceWorkerHostMsg_PostMessageToWorker(
      handle_id, provider_id, message, source_origin,
      WebMessagePortChannelImpl::ExtractMessagePortIDs(std::move(channels))));
}

}  // namespace

WebServiceWorkerImpl::WebServiceWorkerImpl(
    std::unique_ptr<ServiceWorkerHandleReference> handle_ref,
    ThreadSafeSender* thread_safe_sender)
    : handle_ref_(std::move(handle_ref)),
      state_(handle_ref_->state()),
      thread_safe_sender_(thread_safe_sender),
      proxy_(nullptr) {
  DCHECK_NE(kInvalidServiceWorkerHandleId, handle_ref_->handle_id());
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  dispatcher->AddServiceWorker(handle_ref_->handle_id(), this);
}

void WebServiceWorkerImpl::OnStateChanged(
    blink::WebServiceWorkerState new_state) {
  state_ = new_state;

  // TODO(nhiroki): This is a quick fix for http://crbug.com/507110
  DCHECK(proxy_);
  if (proxy_)
    proxy_->dispatchStateChangeEvent();
}

void WebServiceWorkerImpl::setProxy(blink::WebServiceWorkerProxy* proxy) {
  proxy_ = proxy;
}

blink::WebServiceWorkerProxy* WebServiceWorkerImpl::proxy() {
  return proxy_;
}

blink::WebURL WebServiceWorkerImpl::url() const {
  return handle_ref_->url();
}

blink::WebServiceWorkerState WebServiceWorkerImpl::state() const {
  return state_;
}

void WebServiceWorkerImpl::postMessage(
    blink::WebServiceWorkerProvider* provider,
    const WebString& message,
    const WebSecurityOrigin& source_origin,
    WebMessagePortChannelArray* channels) {
  WebServiceWorkerProviderImpl* provider_impl =
      static_cast<WebServiceWorkerProviderImpl*>(provider);
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);

  // This may send channels for MessagePorts, and all internal book-keeping
  // messages for MessagePort (e.g. QueueMessages) are sent from main thread
  // (with thread hopping), so we need to do the same thread hopping here not
  // to overtake those messages.
  dispatcher->main_thread_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SendPostMessageToWorkerOnMainThread,
                 base::RetainedRef(thread_safe_sender_),
                 handle_ref_->handle_id(), provider_impl->provider_id(),
                 // We cast WebString to string16 before crossing
                 // threads for thread-safety.
                 static_cast<base::string16>(message),
                 url::Origin(source_origin),
                 base::Passed(base::WrapUnique(channels))));
}

void WebServiceWorkerImpl::terminate() {
  thread_safe_sender_->Send(
      new ServiceWorkerHostMsg_TerminateWorker(handle_ref_->handle_id()));
}

// static
std::unique_ptr<blink::WebServiceWorker::Handle>
WebServiceWorkerImpl::CreateHandle(
    const scoped_refptr<WebServiceWorkerImpl>& worker) {
  if (!worker)
    return nullptr;
  return base::WrapUnique(new HandleImpl(worker));
}

WebServiceWorkerImpl::~WebServiceWorkerImpl() {
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  if (dispatcher)
    dispatcher->RemoveServiceWorker(handle_ref_->handle_id());
}

}  // namespace content

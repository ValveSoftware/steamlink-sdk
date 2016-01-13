// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_
#define CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannel.h"
#include "third_party/WebKit/public/platform/WebServiceWorker.h"
#include "third_party/WebKit/public/web/WebFrame.h"

namespace blink {
class WebServiceWorkerProxy;
}

namespace content {

class ServiceWorkerHandleReference;
struct ServiceWorkerObjectInfo;
class ThreadSafeSender;

// Each instance corresponds to one ServiceWorker object in JS context, and
// is held by ServiceWorker object in blink's c++ layer, e.g. created one
// per navigator.serviceWorker.current or per successful
// navigator.serviceWorker.register call.
//
// Each instance holds one ServiceWorkerHandleReference so that
// corresponding ServiceWorkerHandle doesn't go away in the browser process
// while the ServiceWorker object is alive.
class WebServiceWorkerImpl
    : NON_EXPORTED_BASE(public blink::WebServiceWorker) {
 public:
  WebServiceWorkerImpl(scoped_ptr<ServiceWorkerHandleReference> handle_ref,
                       ThreadSafeSender* thread_safe_sender);
  virtual ~WebServiceWorkerImpl();

  // Notifies that the service worker's state changed. This function may queue
  // the state change for later processing, if the proxy is not yet ready to
  // handle state changes.
  void OnStateChanged(blink::WebServiceWorkerState new_state);

  virtual void setProxy(blink::WebServiceWorkerProxy* proxy);
  virtual blink::WebServiceWorkerProxy* proxy();
  virtual void proxyReadyChanged();
  virtual blink::WebURL scope() const;
  virtual blink::WebURL url() const;
  virtual blink::WebServiceWorkerState state() const;
  virtual void postMessage(const blink::WebString& message,
                           blink::WebMessagePortChannelArray* channels);

 private:
  // Commits the new state internally and notifies the proxy of the change.
  void ChangeState(blink::WebServiceWorkerState new_state);

  scoped_ptr<ServiceWorkerHandleReference> handle_ref_;
  blink::WebServiceWorkerState state_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  blink::WebServiceWorkerProxy* proxy_;
  std::vector<blink::WebServiceWorkerState> queued_states_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_

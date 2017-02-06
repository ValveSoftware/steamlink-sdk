// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_
#define CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"

namespace blink {
class WebServiceWorkerRegistrationProxy;
}

namespace content {

class ServiceWorkerRegistrationHandleReference;
class ThreadSafeSender;
class WebServiceWorkerImpl;
struct ServiceWorkerObjectInfo;

// Each instance corresponds to one ServiceWorkerRegistration object in JS
// context, and is held by ServiceWorkerRegistration object in Blink's C++ layer
// via WebServiceWorkerRegistration::Handle.
//
// Each instance holds one ServiceWorkerRegistrationHandleReference so that
// corresponding ServiceWorkerRegistrationHandle doesn't go away in the browser
// process while the ServiceWorkerRegistration object is alive.
class CONTENT_EXPORT WebServiceWorkerRegistrationImpl
    : NON_EXPORTED_BASE(public blink::WebServiceWorkerRegistration),
      public base::RefCounted<WebServiceWorkerRegistrationImpl> {
 public:
  explicit WebServiceWorkerRegistrationImpl(
      std::unique_ptr<ServiceWorkerRegistrationHandleReference> handle_ref);

  void SetInstalling(const scoped_refptr<WebServiceWorkerImpl>& service_worker);
  void SetWaiting(const scoped_refptr<WebServiceWorkerImpl>& service_worker);
  void SetActive(const scoped_refptr<WebServiceWorkerImpl>& service_worker);

  void OnUpdateFound();

  // blink::WebServiceWorkerRegistration overrides.
  void setProxy(blink::WebServiceWorkerRegistrationProxy* proxy) override;
  blink::WebServiceWorkerRegistrationProxy* proxy() override;
  blink::WebURL scope() const override;
  void update(blink::WebServiceWorkerProvider* provider,
              WebServiceWorkerUpdateCallbacks* callbacks) override;
  void unregister(blink::WebServiceWorkerProvider* provider,
                  WebServiceWorkerUnregistrationCallbacks* callbacks) override;

  int64_t registration_id() const;

  using WebServiceWorkerRegistrationHandle =
      blink::WebServiceWorkerRegistration::Handle;

  // Creates WebServiceWorkerRegistrationHandle object that owns a reference to
  // the given WebServiceWorkerRegistrationImpl object.
  static std::unique_ptr<WebServiceWorkerRegistrationHandle> CreateHandle(
      const scoped_refptr<WebServiceWorkerRegistrationImpl>& registration);

  // Same with CreateHandle(), but returns a raw pointer to the handle w/ its
  // ownership instead. The caller must manage the ownership. This function must
  // be used only for passing the handle to Blink API that does not support
  // blink::WebPassOwnPtr.
  static WebServiceWorkerRegistrationHandle* CreateLeakyHandle(
      const scoped_refptr<WebServiceWorkerRegistrationImpl>& registration);

 private:
  friend class base::RefCounted<WebServiceWorkerRegistrationImpl>;
  ~WebServiceWorkerRegistrationImpl() override;

  enum QueuedTaskType {
    INSTALLING,
    WAITING,
    ACTIVE,
    UPDATE_FOUND,
  };

  struct QueuedTask {
    QueuedTask(QueuedTaskType type,
               const scoped_refptr<WebServiceWorkerImpl>& worker);
    QueuedTask(const QueuedTask& other);
    ~QueuedTask();
    QueuedTaskType type;
    scoped_refptr<WebServiceWorkerImpl> worker;
  };

  void RunQueuedTasks();

  std::unique_ptr<ServiceWorkerRegistrationHandleReference> handle_ref_;
  blink::WebServiceWorkerRegistrationProxy* proxy_;

  std::vector<QueuedTask> queued_tasks_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerRegistrationImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_IMPL_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_PERMISSIONS_PERMISSION_DISPATCHER_THREAD_PROXY_H_
#define CONTENT_CHILD_PERMISSIONS_PERMISSION_DISPATCHER_THREAD_PROXY_H_

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/child/permissions/permission_observers_registry.h"
#include "content/public/child/worker_thread.h"
#include "third_party/WebKit/public/platform/modules/permissions/WebPermissionClient.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

class PermissionDispatcher;

// PermissionDispatcherThreadProxy is a a proxy to the PermissionDispatcher for
// callers running on a different thread than the main thread. There is one
// instance of that class per thread.
class PermissionDispatcherThreadProxy : public blink::WebPermissionClient,
                                        public PermissionObserversRegistry,
                                        public WorkerThread::Observer {
 public:
  static PermissionDispatcherThreadProxy* GetThreadInstance(
      base::SingleThreadTaskRunner* main_thread_task_runner,
      PermissionDispatcher* permissions_dispatcher);

  // blink::WebPermissionClient implementation.
  void queryPermission(blink::WebPermissionType type,
                       const blink::WebURL& origin,
                       blink::WebPermissionCallback* callback) override;
  void requestPermission(blink::WebPermissionType type,
                         const blink::WebURL& origin,
                         blink::WebPermissionCallback* callback) override;
  void requestPermissions(
      const blink::WebVector<blink::WebPermissionType>& types,
      const blink::WebURL& origin,
      blink::WebPermissionsCallback* callback) override;
  void revokePermission(blink::WebPermissionType type,
                        const blink::WebURL& origin,
                        blink::WebPermissionCallback* callback) override;
  void startListening(blink::WebPermissionType type,
                      const blink::WebURL& origin,
                      blink::WebPermissionObserver* observer) override;
  void stopListening(blink::WebPermissionObserver* observer) override;

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

 private:
  PermissionDispatcherThreadProxy(
      base::SingleThreadTaskRunner* main_thread_task_runner,
      PermissionDispatcher* permissions_dispatcher);

  // Callback when an observed permission changes.
  void OnPermissionChanged(blink::WebPermissionType type,
                           const std::string& origin,
                           blink::WebPermissionObserver* observer,
                           blink::WebPermissionStatus status);

  ~PermissionDispatcherThreadProxy() override;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  PermissionDispatcher* permission_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(PermissionDispatcherThreadProxy);
};

}  // namespace content

#endif // CONTENT_CHILD_PERMISSIONS_PERMISSION_DISPATCHER_THREAD_PROXY_H_

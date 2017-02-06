// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/permissions/permission_dispatcher_thread_proxy.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_local.h"
#include "content/child/permissions/permission_dispatcher.h"
#include "content/public/child/worker_thread.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/modules/permissions/WebPermissionObserver.h"

using base::LazyInstance;
using base::ThreadLocalPointer;

namespace content {

namespace {

LazyInstance<ThreadLocalPointer<PermissionDispatcherThreadProxy>>::Leaky
    g_permission_dispatcher_tls = LAZY_INSTANCE_INITIALIZER;

}  // anonymous namespace

PermissionDispatcherThreadProxy*
PermissionDispatcherThreadProxy::GetThreadInstance(
    base::SingleThreadTaskRunner* main_thread_task_runner,
    PermissionDispatcher* permission_dispatcher) {
  if (g_permission_dispatcher_tls.Pointer()->Get())
    return g_permission_dispatcher_tls.Pointer()->Get();

  PermissionDispatcherThreadProxy* instance =
      new PermissionDispatcherThreadProxy(main_thread_task_runner,
                                        permission_dispatcher);
  DCHECK(WorkerThread::GetCurrentId());
  WorkerThread::AddObserver(instance);
  return instance;
}

PermissionDispatcherThreadProxy::PermissionDispatcherThreadProxy(
    base::SingleThreadTaskRunner* main_thread_task_runner,
    PermissionDispatcher* permission_dispatcher)
  : main_thread_task_runner_(main_thread_task_runner),
    permission_dispatcher_(permission_dispatcher) {
  g_permission_dispatcher_tls.Pointer()->Set(this);
}

PermissionDispatcherThreadProxy::~PermissionDispatcherThreadProxy() {
  g_permission_dispatcher_tls.Pointer()->Set(nullptr);
}

void PermissionDispatcherThreadProxy::queryPermission(
    blink::WebPermissionType type,
    const blink::WebURL& origin,
    blink::WebPermissionCallback* callback) {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PermissionDispatcher::QueryPermissionForWorker,
                            base::Unretained(permission_dispatcher_), type,
                            origin.string().utf8(), base::Unretained(callback),
                            WorkerThread::GetCurrentId()));
}

void PermissionDispatcherThreadProxy::requestPermission(
    blink::WebPermissionType type,
    const blink::WebURL& origin,
    blink::WebPermissionCallback* callback) {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PermissionDispatcher::RequestPermissionForWorker,
                            base::Unretained(permission_dispatcher_), type,
                            origin.string().utf8(), base::Unretained(callback),
                            WorkerThread::GetCurrentId()));
}

void PermissionDispatcherThreadProxy::requestPermissions(
    const blink::WebVector<blink::WebPermissionType>& types,
    const blink::WebURL& origin,
    blink::WebPermissionsCallback* callback) {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PermissionDispatcher::RequestPermissionsForWorker,
                            base::Unretained(permission_dispatcher_), types,
                            origin.string().utf8(), base::Unretained(callback),
                            WorkerThread::GetCurrentId()));
}

void PermissionDispatcherThreadProxy::revokePermission(
    blink::WebPermissionType type,
    const blink::WebURL& origin,
    blink::WebPermissionCallback* callback) {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PermissionDispatcher::RevokePermissionForWorker,
                            base::Unretained(permission_dispatcher_), type,
                            origin.string().utf8(), base::Unretained(callback),
                            WorkerThread::GetCurrentId()));
}

void PermissionDispatcherThreadProxy::startListening(
    blink::WebPermissionType type,
    const blink::WebURL& origin,
    blink::WebPermissionObserver* observer) {
  if (!PermissionDispatcher::IsObservable(type))
    return;

  RegisterObserver(observer);

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &PermissionDispatcher::StartListeningForWorker,
          base::Unretained(permission_dispatcher_), type,
          origin.string().utf8(), WorkerThread::GetCurrentId(),
          base::Bind(&PermissionDispatcherThreadProxy::OnPermissionChanged,
                     base::Unretained(this), type, origin.string().utf8(),
                     base::Unretained(observer))));
}

void PermissionDispatcherThreadProxy::stopListening(
    blink::WebPermissionObserver* observer) {
  UnregisterObserver(observer);
}

void PermissionDispatcherThreadProxy::OnPermissionChanged(
    blink::WebPermissionType type,
    const std::string& origin,
    blink::WebPermissionObserver* observer,
    blink::WebPermissionStatus status) {
  if (!IsObserverRegistered(observer))
    return;

  observer->permissionChanged(type, status);

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &PermissionDispatcher::GetNextPermissionChangeForWorker,
          base::Unretained(permission_dispatcher_), type, origin, status,
          WorkerThread::GetCurrentId(),
          base::Bind(&PermissionDispatcherThreadProxy::OnPermissionChanged,
                     base::Unretained(this), type, origin,
                     base::Unretained(observer))));
}

void PermissionDispatcherThreadProxy::WillStopCurrentWorkerThread() {
  delete this;
}

}  // namespace content

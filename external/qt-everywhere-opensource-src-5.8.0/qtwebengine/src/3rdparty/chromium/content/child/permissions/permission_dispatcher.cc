// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/permissions/permission_dispatcher.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "content/public/child/worker_thread.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/modules/permissions/WebPermissionObserver.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"

using blink::WebPermissionObserver;
using blink::mojom::PermissionName;
using blink::mojom::PermissionStatus;

namespace content {

namespace {

PermissionName GetPermissionName(blink::WebPermissionType type) {
  switch (type) {
    case blink::WebPermissionTypeGeolocation:
      return PermissionName::GEOLOCATION;
    case blink::WebPermissionTypeNotifications:
      return PermissionName::NOTIFICATIONS;
    case blink::WebPermissionTypePushNotifications:
      return PermissionName::PUSH_NOTIFICATIONS;
    case blink::WebPermissionTypeMidiSysEx:
      return PermissionName::MIDI_SYSEX;
    case blink::WebPermissionTypeDurableStorage:
      return PermissionName::DURABLE_STORAGE;
    case blink::WebPermissionTypeMidi:
      return PermissionName::MIDI;
    case blink::WebPermissionTypeBackgroundSync:
      return PermissionName::BACKGROUND_SYNC;
    default:
      // The default statement is only there to prevent compilation failures if
      // WebPermissionType enum gets extended.
      NOTREACHED();
      return PermissionName::GEOLOCATION;
  }
}

PermissionStatus GetPermissionStatus(blink::WebPermissionStatus status) {
  switch (status) {
    case blink::WebPermissionStatusGranted:
      return PermissionStatus::GRANTED;
    case blink::WebPermissionStatusDenied:
      return PermissionStatus::DENIED;
    case blink::WebPermissionStatusPrompt:
      return PermissionStatus::ASK;
  }

  NOTREACHED();
  return PermissionStatus::DENIED;
}

blink::WebPermissionStatus GetWebPermissionStatus(PermissionStatus status) {
  switch (status) {
    case PermissionStatus::GRANTED:
      return blink::WebPermissionStatusGranted;
    case PermissionStatus::DENIED:
      return blink::WebPermissionStatusDenied;
    case PermissionStatus::ASK:
      return blink::WebPermissionStatusPrompt;
  }

  NOTREACHED();
  return blink::WebPermissionStatusDenied;
}

const int kNoWorkerThread = 0;

}  // anonymous namespace

// static
bool PermissionDispatcher::IsObservable(blink::WebPermissionType type) {
  return type == blink::WebPermissionTypeGeolocation ||
         type == blink::WebPermissionTypeNotifications ||
         type == blink::WebPermissionTypePushNotifications ||
         type == blink::WebPermissionTypeMidiSysEx ||
         type == blink::WebPermissionTypeMidi ||
         type == blink::WebPermissionTypeBackgroundSync;
}

PermissionDispatcher::PermissionDispatcher(
    shell::InterfaceProvider* remote_interfaces)
    : remote_interfaces_(remote_interfaces) {
}

PermissionDispatcher::~PermissionDispatcher() {
}

void PermissionDispatcher::queryPermission(
    blink::WebPermissionType type,
    const blink::WebURL& origin,
    blink::WebPermissionCallback* callback) {
  QueryPermissionInternal(
      type, origin.string().utf8(), callback, kNoWorkerThread);
}

void PermissionDispatcher::requestPermission(
    blink::WebPermissionType type,
    const blink::WebURL& origin,
    blink::WebPermissionCallback* callback) {
  RequestPermissionInternal(
      type, origin.string().utf8(), callback, kNoWorkerThread);
}

void PermissionDispatcher::requestPermissions(
    const blink::WebVector<blink::WebPermissionType>& types,
    const blink::WebURL& origin,
    blink::WebPermissionsCallback* callback) {
  RequestPermissionsInternal(
      types, origin.string().utf8(), callback, kNoWorkerThread);
}

void PermissionDispatcher::revokePermission(
    blink::WebPermissionType type,
    const blink::WebURL& origin,
    blink::WebPermissionCallback* callback) {
  RevokePermissionInternal(
      type, origin.string().utf8(), callback, kNoWorkerThread);
}

void PermissionDispatcher::startListening(
    blink::WebPermissionType type,
    const blink::WebURL& origin,
    WebPermissionObserver* observer) {
  if (!IsObservable(type))
    return;

  RegisterObserver(observer);

  GetNextPermissionChange(type, origin.string().utf8(), observer,
                          // We initialize with an arbitrary value because the
                          // mojo service wants a value. Worst case, the
                          // observer will get notified about a non-change which
                          // should be a no-op. After the first notification,
                          // GetNextPermissionChange will be called with the
                          // latest known value.
                          PermissionStatus::ASK);
}

void PermissionDispatcher::stopListening(WebPermissionObserver* observer) {
  UnregisterObserver(observer);
}

void PermissionDispatcher::QueryPermissionForWorker(
    blink::WebPermissionType type,
    const std::string& origin,
    blink::WebPermissionCallback* callback,
    int worker_thread_id) {
  QueryPermissionInternal(type, origin, callback, worker_thread_id);
}

void PermissionDispatcher::RequestPermissionForWorker(
    blink::WebPermissionType type,
    const std::string& origin,
    blink::WebPermissionCallback* callback,
    int worker_thread_id) {
  RequestPermissionInternal(type, origin, callback, worker_thread_id);
}

void PermissionDispatcher::RequestPermissionsForWorker(
    const blink::WebVector<blink::WebPermissionType>& types,
    const std::string& origin,
    blink::WebPermissionsCallback* callback,
    int worker_thread_id) {
  RequestPermissionsInternal(types, origin, callback, worker_thread_id);
}

void PermissionDispatcher::RevokePermissionForWorker(
    blink::WebPermissionType type,
    const std::string& origin,
    blink::WebPermissionCallback* callback,
    int worker_thread_id) {
  RevokePermissionInternal(type, origin, callback, worker_thread_id);
}

void PermissionDispatcher::StartListeningForWorker(
    blink::WebPermissionType type,
    const std::string& origin,
    int worker_thread_id,
    const base::Callback<void(blink::WebPermissionStatus)>& callback) {
  GetPermissionServicePtr()->GetNextPermissionChange(
      GetPermissionName(type), origin,
      // We initialize with an arbitrary value because the mojo service wants a
      // value. Worst case, the observer will get notified about a non-change
      // which should be a no-op. After the first notification,
      // GetNextPermissionChange will be called with the latest known value.
      PermissionStatus::ASK,
      base::Bind(&PermissionDispatcher::OnPermissionChangedForWorker,
                 base::Unretained(this), worker_thread_id, callback));
}

void PermissionDispatcher::GetNextPermissionChangeForWorker(
    blink::WebPermissionType type,
    const std::string& origin,
    blink::WebPermissionStatus status,
    int worker_thread_id,
    const base::Callback<void(blink::WebPermissionStatus)>& callback) {
  GetPermissionServicePtr()->GetNextPermissionChange(
      GetPermissionName(type),
      origin,
      GetPermissionStatus(status),
      base::Bind(&PermissionDispatcher::OnPermissionChangedForWorker,
                 base::Unretained(this),
                 worker_thread_id,
                 callback));
}

// static
void PermissionDispatcher::RunPermissionCallbackOnWorkerThread(
    std::unique_ptr<blink::WebPermissionCallback> callback,
    blink::WebPermissionStatus status) {
  callback->onSuccess(status);
}

void PermissionDispatcher::RunPermissionsCallbackOnWorkerThread(
    std::unique_ptr<blink::WebPermissionsCallback> callback,
    std::unique_ptr<blink::WebVector<blink::WebPermissionStatus>> statuses) {
  callback->onSuccess(std::move(statuses));
}

blink::mojom::PermissionService*
PermissionDispatcher::GetPermissionServicePtr() {
  if (!permission_service_.get()) {
    remote_interfaces_->GetInterface(mojo::GetProxy(&permission_service_));
  }
  return permission_service_.get();
}

void PermissionDispatcher::QueryPermissionInternal(
    blink::WebPermissionType type,
    const std::string& origin,
    blink::WebPermissionCallback* callback,
    int worker_thread_id) {
  // We need to save the |callback| in an ScopedPtrHashMap so if |this| gets
  // deleted, the callback will not leak. In the case of |this| gets deleted,
  // the |permission_service_| pipe will be destroyed too so OnQueryPermission
  // will not be called.
  uintptr_t callback_key = reinterpret_cast<uintptr_t>(callback);
  permission_callbacks_.add(
      callback_key, std::unique_ptr<blink::WebPermissionCallback>(callback));

  GetPermissionServicePtr()->HasPermission(
      GetPermissionName(type),
      origin,
      base::Bind(&PermissionDispatcher::OnPermissionResponse,
                 base::Unretained(this),
                 worker_thread_id,
                 callback_key));
}

void PermissionDispatcher::RequestPermissionInternal(
    blink::WebPermissionType type,
    const std::string& origin,
    blink::WebPermissionCallback* callback,
    int worker_thread_id) {
  // We need to save the |callback| in an ScopedPtrHashMap so if |this| gets
  // deleted, the callback will not leak. In the case of |this| gets deleted,
  // the |permission_service_| pipe will be destroyed too so OnQueryPermission
  // will not be called.
  uintptr_t callback_key = reinterpret_cast<uintptr_t>(callback);
  permission_callbacks_.add(
      callback_key, std::unique_ptr<blink::WebPermissionCallback>(callback));

  GetPermissionServicePtr()->RequestPermission(
      GetPermissionName(type),
      origin,
      blink::WebUserGestureIndicator::isProcessingUserGesture(),
      base::Bind(&PermissionDispatcher::OnPermissionResponse,
                 base::Unretained(this),
                 worker_thread_id,
                 callback_key));
}

void PermissionDispatcher::RequestPermissionsInternal(
    const blink::WebVector<blink::WebPermissionType>& types,
    const std::string& origin,
    blink::WebPermissionsCallback* callback,
    int worker_thread_id) {
  // We need to save the |callback| in an ScopedVector so if |this| gets
  // deleted, the callback will not leak. In the case of |this| gets deleted,
  // the |permission_service_| pipe will be destroyed too so OnQueryPermission
  // will not be called.
  uintptr_t callback_key = reinterpret_cast<uintptr_t>(callback);
  permissions_callbacks_.add(
      callback_key, std::unique_ptr<blink::WebPermissionsCallback>(callback));

  mojo::Array<PermissionName> names(types.size());
  for (size_t i = 0; i < types.size(); ++i)
    names[i] = GetPermissionName(types[i]);

  GetPermissionServicePtr()->RequestPermissions(
      std::move(names), origin,
      blink::WebUserGestureIndicator::isProcessingUserGesture(),
      base::Bind(&PermissionDispatcher::OnRequestPermissionsResponse,
                 base::Unretained(this), worker_thread_id, callback_key));
}

void PermissionDispatcher::RevokePermissionInternal(
    blink::WebPermissionType type,
    const std::string& origin,
    blink::WebPermissionCallback* callback,
    int worker_thread_id) {
  // We need to save the |callback| in an ScopedPtrHashMap so if |this| gets
  // deleted, the callback will not leak. In the case of |this| gets deleted,
  // the |permission_service_| pipe will be destroyed too so OnQueryPermission
  // will not be called.
  uintptr_t callback_key = reinterpret_cast<uintptr_t>(callback);
  permission_callbacks_.add(
      callback_key, std::unique_ptr<blink::WebPermissionCallback>(callback));

  GetPermissionServicePtr()->RevokePermission(
      GetPermissionName(type),
      origin,
      base::Bind(&PermissionDispatcher::OnPermissionResponse,
                 base::Unretained(this),
                 worker_thread_id,
                 callback_key));
}

void PermissionDispatcher::OnPermissionResponse(int worker_thread_id,
                                                uintptr_t callback_key,
                                                PermissionStatus result) {
  std::unique_ptr<blink::WebPermissionCallback> callback =
      permission_callbacks_.take_and_erase(callback_key);
  blink::WebPermissionStatus status = GetWebPermissionStatus(result);

  if (worker_thread_id != kNoWorkerThread) {
    // If the worker is no longer running, ::PostTask() will return false and
    // gracefully fail, destroying the callback too.
    WorkerThread::PostTask(
        worker_thread_id,
        base::Bind(&PermissionDispatcher::RunPermissionCallbackOnWorkerThread,
                   base::Passed(&callback), status));
    return;
  }

  callback->onSuccess(status);
}

void PermissionDispatcher::OnRequestPermissionsResponse(
    int worker_thread_id,
    uintptr_t callback_key,
    mojo::Array<PermissionStatus> result) {
  std::unique_ptr<blink::WebPermissionsCallback> callback =
      permissions_callbacks_.take_and_erase(callback_key);
  std::unique_ptr<blink::WebVector<blink::WebPermissionStatus>> statuses(
      new blink::WebVector<blink::WebPermissionStatus>(result.size()));

  for (size_t i = 0; i < result.size(); i++)
    statuses->operator[](i) = GetWebPermissionStatus(result[i]);

  if (worker_thread_id != kNoWorkerThread) {
    // If the worker is no longer running, ::PostTask() will return false and
    // gracefully fail, destroying the callback too.
    WorkerThread::PostTask(
        worker_thread_id,
        base::Bind(&PermissionDispatcher::RunPermissionsCallbackOnWorkerThread,
                   base::Passed(&callback), base::Passed(&statuses)));
    return;
  }

  callback->onSuccess(std::move(statuses));
}

void PermissionDispatcher::OnPermissionChanged(blink::WebPermissionType type,
                                               const std::string& origin,
                                               WebPermissionObserver* observer,
                                               PermissionStatus status) {
  if (!IsObserverRegistered(observer))
    return;

  observer->permissionChanged(type, GetWebPermissionStatus(status));

  GetNextPermissionChange(type, origin, observer, status);
}

void PermissionDispatcher::OnPermissionChangedForWorker(
    int worker_thread_id,
    const base::Callback<void(blink::WebPermissionStatus)>& callback,
    PermissionStatus status) {
  DCHECK(worker_thread_id != kNoWorkerThread);

  WorkerThread::PostTask(worker_thread_id,
                         base::Bind(callback, GetWebPermissionStatus(status)));
}

void PermissionDispatcher::GetNextPermissionChange(
    blink::WebPermissionType type,
    const std::string& origin,
    WebPermissionObserver* observer,
    PermissionStatus current_status) {
  GetPermissionServicePtr()->GetNextPermissionChange(
      GetPermissionName(type),
      origin,
      current_status,
      base::Bind(&PermissionDispatcher::OnPermissionChanged,
                 base::Unretained(this),
                 type,
                 origin,
                 base::Unretained(observer)));
}

}  // namespace content

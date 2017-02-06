// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_permission_dispatcher.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/bind_to_current_loop.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "url/gurl.h"

namespace {

using Type = media::MediaPermission::Type;

blink::mojom::PermissionName MediaPermissionTypeToPermissionName(Type type) {
  switch (type) {
    case Type::PROTECTED_MEDIA_IDENTIFIER:
      return blink::mojom::PermissionName::PROTECTED_MEDIA_IDENTIFIER;
    case Type::AUDIO_CAPTURE:
      return blink::mojom::PermissionName::AUDIO_CAPTURE;
    case Type::VIDEO_CAPTURE:
      return blink::mojom::PermissionName::VIDEO_CAPTURE;
  }
  NOTREACHED();
  return blink::mojom::PermissionName::PROTECTED_MEDIA_IDENTIFIER;
}

}  // namespace

namespace content {

MediaPermissionDispatcher::MediaPermissionDispatcher(
    const ConnectToServiceCB& connect_to_service_cb)
    : connect_to_service_cb_(connect_to_service_cb),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      next_request_id_(0),
      weak_factory_(this) {
  DCHECK(!connect_to_service_cb_.is_null());
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

MediaPermissionDispatcher::~MediaPermissionDispatcher() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  // Fire all pending callbacks with |false|.
  for (auto& request : requests_)
    request.second.Run(false);
}

void MediaPermissionDispatcher::HasPermission(
    Type type,
    const GURL& security_origin,
    const PermissionStatusCB& permission_status_cb) {
  if (!task_runner_->RunsTasksOnCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&MediaPermissionDispatcher::HasPermission,
                              weak_ptr_, type, security_origin,
                              media::BindToCurrentLoop(permission_status_cb)));
    return;
  }

  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (!permission_service_)
    connect_to_service_cb_.Run(mojo::GetProxy(&permission_service_));

  int request_id = RegisterCallback(permission_status_cb);
  DVLOG(2) << __FUNCTION__ << ": request ID " << request_id;

  permission_service_->HasPermission(
      MediaPermissionTypeToPermissionName(type), security_origin.spec(),
      base::Bind(&MediaPermissionDispatcher::OnPermissionStatus, weak_ptr_,
                 request_id));
}

void MediaPermissionDispatcher::RequestPermission(
    Type type,
    const GURL& security_origin,
    const PermissionStatusCB& permission_status_cb) {
  if (!task_runner_->RunsTasksOnCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&MediaPermissionDispatcher::RequestPermission,
                              weak_ptr_, type, security_origin,
                              media::BindToCurrentLoop(permission_status_cb)));
    return;
  }

  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (!permission_service_)
    connect_to_service_cb_.Run(mojo::GetProxy(&permission_service_));

  int request_id = RegisterCallback(permission_status_cb);
  DVLOG(2) << __FUNCTION__ << ": request ID " << request_id;

  permission_service_->RequestPermission(
      MediaPermissionTypeToPermissionName(type), security_origin.spec(),
      blink::WebUserGestureIndicator::isProcessingUserGesture(),
      base::Bind(&MediaPermissionDispatcher::OnPermissionStatus, weak_ptr_,
                 request_id));
}

uint32_t MediaPermissionDispatcher::RegisterCallback(
    const PermissionStatusCB& permission_status_cb) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  uint32_t request_id = next_request_id_++;
  DCHECK(!requests_.count(request_id));
  requests_[request_id] = permission_status_cb;

  return request_id;
}

void MediaPermissionDispatcher::OnPermissionStatus(
    uint32_t request_id,
    blink::mojom::PermissionStatus status) {
  DVLOG(2) << __FUNCTION__ << ": (" << request_id << ", " << status << ")";
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  RequestMap::iterator iter = requests_.find(request_id);
  DCHECK(iter != requests_.end()) << "Request not found.";

  PermissionStatusCB permission_status_cb = iter->second;
  requests_.erase(iter);

  permission_status_cb.Run(status == blink::mojom::PermissionStatus::GRANTED);
}

}  // namespace content

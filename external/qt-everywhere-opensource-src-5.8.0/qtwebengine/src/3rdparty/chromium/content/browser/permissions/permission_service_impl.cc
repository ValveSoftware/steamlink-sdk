// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_service_impl.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/permission_type.h"

using blink::mojom::PermissionName;
using blink::mojom::PermissionStatus;

namespace content {

namespace {

PermissionType PermissionNameToPermissionType(PermissionName name) {
  switch(name) {
    case PermissionName::GEOLOCATION:
      return PermissionType::GEOLOCATION;
    case PermissionName::NOTIFICATIONS:
      return PermissionType::NOTIFICATIONS;
    case PermissionName::PUSH_NOTIFICATIONS:
      return PermissionType::PUSH_MESSAGING;
    case PermissionName::MIDI:
      return PermissionType::MIDI;
    case PermissionName::MIDI_SYSEX:
      return PermissionType::MIDI_SYSEX;
    case PermissionName::PROTECTED_MEDIA_IDENTIFIER:
      return PermissionType::PROTECTED_MEDIA_IDENTIFIER;
    case PermissionName::DURABLE_STORAGE:
      return PermissionType::DURABLE_STORAGE;
    case PermissionName::AUDIO_CAPTURE:
      return PermissionType::AUDIO_CAPTURE;
    case PermissionName::VIDEO_CAPTURE:
      return PermissionType::VIDEO_CAPTURE;
    case PermissionName::BACKGROUND_SYNC:
      return PermissionType::BACKGROUND_SYNC;
  }

  NOTREACHED();
  return PermissionType::NUM;
}

// This function allows the usage of the the multiple request map
// with single requests.
void PermissionRequestResponseCallbackWrapper(
    const base::Callback<void(PermissionStatus)>& callback,
    mojo::Array<PermissionStatus> vector) {
  DCHECK_EQ(vector.size(), 1ul);
  callback.Run(vector[0]);
}

} // anonymous namespace

PermissionServiceImpl::PendingRequest::PendingRequest(
    const RequestPermissionsCallback& callback,
    int request_count)
    : callback(callback),
      request_count(request_count) {
}

PermissionServiceImpl::PendingRequest::~PendingRequest() {
  if (callback.is_null())
    return;

  mojo::Array<PermissionStatus> result =
      mojo::Array<PermissionStatus>::New(request_count);
  for (int i = 0; i < request_count; ++i)
    result[i] = PermissionStatus::DENIED;
  callback.Run(std::move(result));
}

PermissionServiceImpl::PendingSubscription::PendingSubscription(
    PermissionType permission,
    const GURL& origin,
    const PermissionStatusCallback& callback)
    : id(-1),
      permission(permission),
      origin(origin),
      callback(callback) {
}

PermissionServiceImpl::PendingSubscription::~PendingSubscription() {
  if (!callback.is_null())
    callback.Run(PermissionStatus::ASK);
}

PermissionServiceImpl::PermissionServiceImpl(
    PermissionServiceContext* context,
    mojo::InterfaceRequest<blink::mojom::PermissionService> request)
    : context_(context),
      binding_(this, std::move(request)),
      weak_factory_(this) {
  binding_.set_connection_error_handler(
      base::Bind(&PermissionServiceImpl::OnConnectionError,
                 base::Unretained(this)));
}

PermissionServiceImpl::~PermissionServiceImpl() {
  DCHECK(pending_requests_.IsEmpty());
}

void PermissionServiceImpl::OnConnectionError() {
  CancelPendingOperations();
  context_->ServiceHadConnectionError(this);
  // After that call, |this| will be deleted.
}

void PermissionServiceImpl::RequestPermission(
    PermissionName permission,
    const mojo::String& origin,
    bool user_gesture,
    const PermissionStatusCallback& callback) {
  // This condition is valid if the call is coming from a ChildThread instead of
  // a RenderFrame. Some consumers of the service run in Workers and some in
  // Frames. In the context of a Worker, it is not possible to show a
  // permission prompt because there is no tab. In the context of a Frame, we
  // can. Even if the call comes from a context where it is not possible to show
  // any UI, we want to still return something relevant so the current
  // permission status is returned.
  BrowserContext* browser_context = context_->GetBrowserContext();
  DCHECK(browser_context);
  if (!context_->render_frame_host() ||
      !browser_context->GetPermissionManager()) {
    callback.Run(GetPermissionStatusFromName(permission, GURL(origin.get())));
    return;
  }

  int pending_request_id = pending_requests_.Add(new PendingRequest(
      base::Bind(&PermissionRequestResponseCallbackWrapper, callback), 1));
  int id = browser_context->GetPermissionManager()->RequestPermission(
      PermissionNameToPermissionType(permission),
      context_->render_frame_host(),
      GURL(origin.get()),
      base::Bind(&PermissionServiceImpl::OnRequestPermissionResponse,
                 weak_factory_.GetWeakPtr(),
                 pending_request_id));

  // Check if the request still exists. It might have been removed by the
  // callback if it was run synchronously.
  PendingRequest* pending_request = pending_requests_.Lookup(
      pending_request_id);
  if (!pending_request)
      return;
  pending_request->id = id;
}

void PermissionServiceImpl::OnRequestPermissionResponse(
    int pending_request_id,
    PermissionStatus status) {
  OnRequestPermissionsResponse(pending_request_id,
                               std::vector<PermissionStatus>(1, status));
}

void PermissionServiceImpl::RequestPermissions(
    mojo::Array<PermissionName> permissions,
    const mojo::String& origin,
    bool user_gesture,
    const RequestPermissionsCallback& callback) {
  if (permissions.is_null()) {
    callback.Run(mojo::Array<PermissionStatus>());
    return;
  }

  // This condition is valid if the call is coming from a ChildThread instead of
  // a RenderFrame. Some consumers of the service run in Workers and some in
  // Frames. In the context of a Worker, it is not possible to show a
  // permission prompt because there is no tab. In the context of a Frame, we
  // can. Even if the call comes from a context where it is not possible to show
  // any UI, we want to still return something relevant so the current
  // permission status is returned for each permission.
  BrowserContext* browser_context = context_->GetBrowserContext();
  DCHECK(browser_context);
  if (!context_->render_frame_host() ||
      !browser_context->GetPermissionManager()) {
    mojo::Array<PermissionStatus> result(permissions.size());
    for (size_t i = 0; i < permissions.size(); ++i) {
      result[i] =
          GetPermissionStatusFromName(permissions[i], GURL(origin.get()));
    }
    callback.Run(std::move(result));
    return;
  }

  std::vector<PermissionType> types(permissions.size());
  for (size_t i = 0; i < types.size(); ++i)
    types[i] = PermissionNameToPermissionType(permissions[i]);

  int pending_request_id = pending_requests_.Add(
      new PendingRequest(callback, permissions.size()));
  int id = browser_context->GetPermissionManager()->RequestPermissions(
      types,
      context_->render_frame_host(),
      GURL(origin.get()),
      base::Bind(&PermissionServiceImpl::OnRequestPermissionsResponse,
                 weak_factory_.GetWeakPtr(),
                 pending_request_id));

  // Check if the request still exists. It may have been removed by the
  // the response callback.
  PendingRequest* pending_request = pending_requests_.Lookup(
      pending_request_id);
  if (!pending_request)
      return;
  pending_request->id = id;
}

void PermissionServiceImpl::OnRequestPermissionsResponse(
    int pending_request_id,
    const std::vector<PermissionStatus>& result) {
  PendingRequest* request = pending_requests_.Lookup(pending_request_id);
  RequestPermissionsCallback callback(request->callback);
  request->callback.Reset();
  pending_requests_.Remove(pending_request_id);
  callback.Run(mojo::Array<PermissionStatus>::From(result));
}

void PermissionServiceImpl::CancelPendingOperations() {
  DCHECK(context_->GetBrowserContext());

  PermissionManager* permission_manager =
      context_->GetBrowserContext()->GetPermissionManager();
  if (!permission_manager)
    return;

  // Cancel pending requests.
  for (RequestsMap::Iterator<PendingRequest> it(&pending_requests_);
       !it.IsAtEnd(); it.Advance()) {
    permission_manager->CancelPermissionRequest(
        it.GetCurrentValue()->id);
  }
  pending_requests_.Clear();

  // Cancel pending subscriptions.
  for (SubscriptionsMap::Iterator<PendingSubscription>
          it(&pending_subscriptions_); !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->callback.Run(GetPermissionStatusFromType(
        it.GetCurrentValue()->permission, it.GetCurrentValue()->origin));
    it.GetCurrentValue()->callback.Reset();
    permission_manager->UnsubscribePermissionStatusChange(
        it.GetCurrentValue()->id);
  }
  pending_subscriptions_.Clear();
}

void PermissionServiceImpl::HasPermission(
    PermissionName permission,
    const mojo::String& origin,
    const PermissionStatusCallback& callback) {
  callback.Run(GetPermissionStatusFromName(permission, GURL(origin.get())));
}

void PermissionServiceImpl::RevokePermission(
    PermissionName permission,
    const mojo::String& origin,
    const PermissionStatusCallback& callback) {
  GURL origin_url(origin.get());
  PermissionType permission_type = PermissionNameToPermissionType(permission);
  PermissionStatus status =
      GetPermissionStatusFromType(permission_type, origin_url);

  // Resetting the permission should only be possible if the permission is
  // already granted.
  if (status != PermissionStatus::GRANTED) {
    callback.Run(status);
    return;
  }

  ResetPermissionStatus(permission_type, origin_url);

  callback.Run(GetPermissionStatusFromType(permission_type, origin_url));
}

void PermissionServiceImpl::GetNextPermissionChange(
    PermissionName permission,
    const mojo::String& mojo_origin,
    PermissionStatus last_known_status,
    const PermissionStatusCallback& callback) {
  GURL origin(mojo_origin.get());
  PermissionStatus current_status =
      GetPermissionStatusFromName(permission, origin);
  if (current_status != last_known_status) {
    callback.Run(current_status);
    return;
  }

  BrowserContext* browser_context = context_->GetBrowserContext();
  DCHECK(browser_context);
  if (!browser_context->GetPermissionManager()) {
    callback.Run(current_status);
    return;
  }

  PermissionType permission_type = PermissionNameToPermissionType(permission);

  // We need to pass the id of PendingSubscription in pending_subscriptions_
  // to the callback but SubscribePermissionStatusChange() will also return an
  // id which is different.
  PendingSubscription* subscription =
      new PendingSubscription(permission_type, origin, callback);
  int pending_subscription_id = pending_subscriptions_.Add(subscription);

  GURL embedding_origin = context_->GetEmbeddingOrigin();
  subscription->id =
      browser_context->GetPermissionManager()->SubscribePermissionStatusChange(
          permission_type,
          origin,
          // If the embedding_origin is empty, we,ll use the |origin| instead.
          embedding_origin.is_empty() ? origin : embedding_origin,
          base::Bind(&PermissionServiceImpl::OnPermissionStatusChanged,
                     weak_factory_.GetWeakPtr(),
                     pending_subscription_id));
}

PermissionStatus PermissionServiceImpl::GetPermissionStatusFromName(
    PermissionName permission,
    const GURL& origin) {
  return GetPermissionStatusFromType(PermissionNameToPermissionType(permission),
                                     origin);
}

PermissionStatus PermissionServiceImpl::GetPermissionStatusFromType(
    PermissionType type,
    const GURL& origin) {
  BrowserContext* browser_context = context_->GetBrowserContext();
  DCHECK(browser_context);
  if (!browser_context->GetPermissionManager())
    return PermissionStatus::DENIED;

  // If the embedding_origin is empty we'll use |origin| instead.
  GURL embedding_origin = context_->GetEmbeddingOrigin();
  return browser_context->GetPermissionManager()->GetPermissionStatus(
      type, origin, embedding_origin.is_empty() ? origin : embedding_origin);
}

void PermissionServiceImpl::ResetPermissionStatus(PermissionType type,
                                                  const GURL& origin) {
  BrowserContext* browser_context = context_->GetBrowserContext();
  DCHECK(browser_context);
  if (!browser_context->GetPermissionManager())
    return;

  // If the embedding_origin is empty we'll use |origin| instead.
  GURL embedding_origin = context_->GetEmbeddingOrigin();
  browser_context->GetPermissionManager()->ResetPermission(
      type, origin, embedding_origin.is_empty() ? origin : embedding_origin);
}

void PermissionServiceImpl::OnPermissionStatusChanged(
    int pending_subscription_id,
    PermissionStatus status) {
  PendingSubscription* subscription =
      pending_subscriptions_.Lookup(pending_subscription_id);

  BrowserContext* browser_context = context_->GetBrowserContext();
  DCHECK(browser_context);
  if (browser_context->GetPermissionManager()) {
    browser_context->GetPermissionManager()->UnsubscribePermissionStatusChange(
        subscription->id);
  }

  PermissionStatusCallback callback = subscription->callback;

  subscription->callback.Reset();
  pending_subscriptions_.Remove(pending_subscription_id);

  callback.Run(status);
}

}  // namespace content

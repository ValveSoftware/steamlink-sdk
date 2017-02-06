// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_IMPL_H_

#include "base/callback.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/permissions/permission_service_context.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission.mojom.h"

namespace content {

enum class PermissionType;

// Implements the PermissionService Mojo interface.
// This service can be created from a RenderFrameHost or a RenderProcessHost.
// It is owned by a PermissionServiceContext.
// It receives at PermissionServiceContext instance when created which allows it
// to have some information about the current context. That enables the service
// to know whether it can show UI and have knowledge of the associated
// WebContents for example.
class PermissionServiceImpl : public blink::mojom::PermissionService {
 public:
  ~PermissionServiceImpl() override;

  // Clear pending operations currently run by the service. This will be called
  // by PermissionServiceContext when it will need the service to clear its
  // state for example, if the frame changes.
  void CancelPendingOperations();

 protected:
  friend PermissionServiceContext;

  PermissionServiceImpl(
      PermissionServiceContext* context,
      mojo::InterfaceRequest<blink::mojom::PermissionService> request);

 private:
  using PermissionStatusCallback =
      base::Callback<void(blink::mojom::PermissionStatus)>;

  struct PendingRequest {
    PendingRequest(const RequestPermissionsCallback& callback,
                   int request_count);
    ~PendingRequest();

    // Request ID received from the PermissionManager.
    int id;
    RequestPermissionsCallback callback;
    int request_count;
  };
  using RequestsMap = IDMap<PendingRequest, IDMapOwnPointer>;

  struct PendingSubscription {
    PendingSubscription(PermissionType permission, const GURL& origin,
                        const PermissionStatusCallback& callback);
    ~PendingSubscription();

    // Subscription ID received from the PermissionManager.
    int id;
    PermissionType permission;
    GURL origin;
    PermissionStatusCallback callback;
  };
  using SubscriptionsMap = IDMap<PendingSubscription, IDMapOwnPointer>;

  // blink::mojom::PermissionService.
  void HasPermission(blink::mojom::PermissionName permission,
                     const mojo::String& origin,
                     const PermissionStatusCallback& callback) override;
  void RequestPermission(blink::mojom::PermissionName permission,
                         const mojo::String& origin,
                         bool user_gesture,
                         const PermissionStatusCallback& callback) override;
  void RequestPermissions(mojo::Array<blink::mojom::PermissionName> permissions,
                          const mojo::String& origin,
                          bool user_gesture,
                          const RequestPermissionsCallback& callback) override;
  void RevokePermission(blink::mojom::PermissionName permission,
                        const mojo::String& origin,
                        const PermissionStatusCallback& callback) override;
  void GetNextPermissionChange(
      blink::mojom::PermissionName permission,
      const mojo::String& origin,
      blink::mojom::PermissionStatus last_known_status,
      const PermissionStatusCallback& callback) override;

  void OnConnectionError();

  void OnRequestPermissionResponse(int pending_request_id,
                                   blink::mojom::PermissionStatus status);
  void OnRequestPermissionsResponse(
      int pending_request_id,
      const std::vector<blink::mojom::PermissionStatus>& result);

  blink::mojom::PermissionStatus GetPermissionStatusFromName(
      blink::mojom::PermissionName permission,
      const GURL& origin);
  blink::mojom::PermissionStatus GetPermissionStatusFromType(
      PermissionType type,
      const GURL& origin);
  void ResetPermissionStatus(PermissionType type, const GURL& origin);

  void OnPermissionStatusChanged(int pending_subscription_id,
                                 blink::mojom::PermissionStatus status);

  RequestsMap pending_requests_;
  SubscriptionsMap pending_subscriptions_;
  // context_ owns |this|.
  PermissionServiceContext* context_;
  mojo::Binding<blink::mojom::PermissionService> binding_;
  base::WeakPtrFactory<PermissionServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PermissionServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_IMPL_H_

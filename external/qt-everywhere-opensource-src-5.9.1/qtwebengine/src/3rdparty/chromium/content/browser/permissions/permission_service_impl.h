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
#include "url/origin.h"

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
  PermissionServiceImpl(
      PermissionServiceContext* context,
      mojo::InterfaceRequest<blink::mojom::PermissionService> request);
  ~PermissionServiceImpl() override;

  // Clear pending operations currently run by the service. This will be called
  // by PermissionServiceContext when it will need the service to clear its
  // state for example, if the frame changes.
  void CancelPendingOperations();

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

  // blink::mojom::PermissionService.
  void HasPermission(blink::mojom::PermissionDescriptorPtr permission,
                     const url::Origin& origin,
                     const PermissionStatusCallback& callback) override;
  void RequestPermission(blink::mojom::PermissionDescriptorPtr permission,
                         const url::Origin& origin,
                         bool user_gesture,
                         const PermissionStatusCallback& callback) override;
  void RequestPermissions(
      std::vector<blink::mojom::PermissionDescriptorPtr> permissions,
      const url::Origin& origin,
      bool user_gesture,
      const RequestPermissionsCallback& callback) override;
  void RevokePermission(blink::mojom::PermissionDescriptorPtr permission,
                        const url::Origin& origin,
                        const PermissionStatusCallback& callback) override;
  void AddPermissionObserver(
      blink::mojom::PermissionDescriptorPtr permission,
      const url::Origin& origin,
      blink::mojom::PermissionStatus last_known_status,
      blink::mojom::PermissionObserverPtr observer) override;

  void OnConnectionError();

  void OnRequestPermissionResponse(int pending_request_id,
                                   blink::mojom::PermissionStatus status);
  void OnRequestPermissionsResponse(
      int pending_request_id,
      const std::vector<blink::mojom::PermissionStatus>& result);

  blink::mojom::PermissionStatus GetPermissionStatus(
      const blink::mojom::PermissionDescriptorPtr& permission,
      const url::Origin& origin);
  blink::mojom::PermissionStatus GetPermissionStatusFromType(
      PermissionType type,
      const url::Origin& origin);
  void ResetPermissionStatus(PermissionType type, const url::Origin& origin);

  RequestsMap pending_requests_;
  // context_ owns |this|.
  PermissionServiceContext* context_;
  mojo::Binding<blink::mojom::PermissionService> binding_;
  base::WeakPtrFactory<PermissionServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PermissionServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_IMPL_H_

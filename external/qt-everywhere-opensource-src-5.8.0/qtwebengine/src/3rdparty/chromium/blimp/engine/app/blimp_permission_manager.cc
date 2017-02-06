// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_permission_manager.h"

#include <vector>

#include "base/callback.h"
#include "content/public/browser/permission_type.h"

namespace blimp {
namespace engine {

BlimpPermissionManager::BlimpPermissionManager()
    : content::PermissionManager() {}

BlimpPermissionManager::~BlimpPermissionManager() {}

int BlimpPermissionManager::RequestPermission(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& origin,
    const base::Callback<void(blink::mojom::PermissionStatus)>& callback) {
  callback.Run(blink::mojom::PermissionStatus::DENIED);
  return kNoPendingOperation;
}

int BlimpPermissionManager::RequestPermissions(
    const std::vector<content::PermissionType>& permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const base::Callback<
        void(const std::vector<blink::mojom::PermissionStatus>&)>& callback) {
  callback.Run(std::vector<blink::mojom::PermissionStatus>(
      permission.size(), blink::mojom::PermissionStatus::DENIED));
  return kNoPendingOperation;
}

void BlimpPermissionManager::CancelPermissionRequest(int request_id) {}

void BlimpPermissionManager::ResetPermission(content::PermissionType permission,
                                             const GURL& requesting_origin,
                                             const GURL& embedding_origin) {}

blink::mojom::PermissionStatus BlimpPermissionManager::GetPermissionStatus(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return blink::mojom::PermissionStatus::DENIED;
}

void BlimpPermissionManager::RegisterPermissionUsage(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {}

int BlimpPermissionManager::SubscribePermissionStatusChange(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::Callback<void(blink::mojom::PermissionStatus)>& callback) {
  return -1;
}

void BlimpPermissionManager::UnsubscribePermissionStatusChange(
    int subscription_id) {}

}  // namespace engine
}  // namespace blimp

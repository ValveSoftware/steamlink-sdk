// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_STORAGE_QUOTA_PARAMS_H_
#define CONTENT_PUBLIC_COMMON_STORAGE_QUOTA_PARAMS_H_

#include "content/common/content_export.h"
#include "ipc/ipc_message.h"
#include "url/gurl.h"
#include "webkit/common/quota/quota_types.h"

namespace content {

// Parameters from the renderer to the browser process on a
// RequestStorageQuota call.
struct CONTENT_EXPORT StorageQuotaParams {
  StorageQuotaParams()
      : render_view_id(MSG_ROUTING_NONE),
        request_id(-1),
        storage_type(quota::kStorageTypeTemporary),
        requested_size(0),
        user_gesture(false) {}

  int render_view_id;
  int request_id;
  GURL origin_url;
  quota::StorageType storage_type;
  uint64 requested_size;

  // Request was made in the context of a user gesture.
  bool user_gesture;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_STORAGE_QUOTA_PARAMS_H_

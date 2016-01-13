// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include "base/basictypes.h"
#include "content/public/common/storage_quota_params.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"
#include "webkit/common/quota/quota_types.h"

#define IPC_MESSAGE_START QuotaMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(quota::StorageType, quota::kStorageTypeLast)
IPC_ENUM_TRAITS(quota::QuotaStatusCode)

IPC_STRUCT_TRAITS_BEGIN(content::StorageQuotaParams)
  IPC_STRUCT_TRAITS_MEMBER(render_view_id)
  IPC_STRUCT_TRAITS_MEMBER(request_id)
  IPC_STRUCT_TRAITS_MEMBER(origin_url)
  IPC_STRUCT_TRAITS_MEMBER(storage_type)
  IPC_STRUCT_TRAITS_MEMBER(requested_size)
  IPC_STRUCT_TRAITS_MEMBER(user_gesture)
IPC_STRUCT_TRAITS_END()

// Quota messages sent from the browser to the child process.

IPC_MESSAGE_CONTROL3(QuotaMsg_DidGrantStorageQuota,
                     int /* request_id */,
                     int64 /* current_usage */,
                     int64 /* granted_quota */)

IPC_MESSAGE_CONTROL3(QuotaMsg_DidQueryStorageUsageAndQuota,
                     int /* request_id */,
                     int64 /* current_usage */,
                     int64 /* current_quota */)

IPC_MESSAGE_CONTROL2(QuotaMsg_DidFail,
                     int /* request_id */,
                     quota::QuotaStatusCode /* error */)

// Quota messages sent from the child process to the browser.

IPC_MESSAGE_CONTROL3(QuotaHostMsg_QueryStorageUsageAndQuota,
                     int /* request_id */,
                     GURL /* origin_url */,
                     quota::StorageType /* type */)

IPC_MESSAGE_CONTROL1(QuotaHostMsg_RequestStorageQuota,
                     content::StorageQuotaParams)

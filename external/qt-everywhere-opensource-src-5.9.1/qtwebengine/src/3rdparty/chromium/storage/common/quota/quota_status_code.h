// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_QUOTA_QUOTA_STATUS_CODE_H_
#define STORAGE_COMMON_QUOTA_QUOTA_STATUS_CODE_H_

#include "storage/common/storage_common_export.h"
#include "third_party/WebKit/public/platform/WebStorageQuotaError.h"

namespace storage {

enum QuotaStatusCode {
  kQuotaStatusOk = 0,
  kQuotaErrorNotSupported = blink::WebStorageQuotaErrorNotSupported,
  kQuotaErrorInvalidModification =
      blink::WebStorageQuotaErrorInvalidModification,
  kQuotaErrorInvalidAccess = blink::WebStorageQuotaErrorInvalidAccess,
  kQuotaErrorAbort = blink::WebStorageQuotaErrorAbort,
  kQuotaStatusUnknown = -1,

  kQuotaStatusLast = kQuotaErrorAbort,
};

STORAGE_COMMON_EXPORT const char* QuotaStatusCodeToString(
    QuotaStatusCode status);

}  // namespace storage

#endif  // STORAGE_COMMON_QUOTA_QUOTA_STATUS_CODE_H_

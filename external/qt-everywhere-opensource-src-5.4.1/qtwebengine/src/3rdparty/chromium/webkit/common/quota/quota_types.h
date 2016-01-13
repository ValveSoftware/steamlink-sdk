// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_QUOTA_QUOTA_TYPES_H_
#define WEBKIT_COMMON_QUOTA_QUOTA_TYPES_H_

#include "webkit/common/quota/quota_status_code.h"

namespace quota {

enum StorageType {
  kStorageTypeTemporary,
  kStorageTypePersistent,
  kStorageTypeSyncable,
  kStorageTypeQuotaNotManaged,
  kStorageTypeUnknown,
  kStorageTypeLast = kStorageTypeUnknown
};

enum QuotaLimitType {
  kQuotaLimitTypeUnknown,
  kQuotaLimitTypeLimited,
  kQuotaLimitTypeUnlimited,
};

}  // namespace quota

#endif  // WEBKIT_COMMON_QUOTA_QUOTA_TYPES_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "storage/common/quota/quota_status_code.h"

namespace storage {

const char* QuotaStatusCodeToString(QuotaStatusCode status) {
  switch (status) {
    case kQuotaStatusOk:
      return "OK.";
    case kQuotaErrorNotSupported:
      return "Operation not supported.";
    case kQuotaErrorInvalidModification:
      return "Invalid modification.";
    case kQuotaErrorInvalidAccess:
      return "Invalid access.";
    case kQuotaErrorAbort:
      return "Quota operation aborted.";
    case kQuotaStatusUnknown:
      return "Unknown error.";
  }
  NOTREACHED();
  return "Unknown error.";
}

}  // namespace storage

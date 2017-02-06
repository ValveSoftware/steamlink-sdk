// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/content/browser/download_constants_utils.h"

#include "base/logging.h"
#include "components/history/core/browser/download_constants.h"

namespace history {

content::DownloadItem::DownloadState ToContentDownloadState(
    DownloadState state) {
  switch (state) {
    case DownloadState::IN_PROGRESS:
      return content::DownloadItem::IN_PROGRESS;
    case DownloadState::COMPLETE:
      return content::DownloadItem::COMPLETE;
    case DownloadState::CANCELLED:
      return content::DownloadItem::CANCELLED;
    case DownloadState::INTERRUPTED:
      return content::DownloadItem::INTERRUPTED;
    case DownloadState::INVALID:
    case DownloadState::BUG_140687:
      NOTREACHED();
      return content::DownloadItem::MAX_DOWNLOAD_STATE;
  }
  NOTREACHED();
  return content::DownloadItem::MAX_DOWNLOAD_STATE;
}

DownloadState ToHistoryDownloadState(
    content::DownloadItem::DownloadState state) {
  switch (state) {
    case content::DownloadItem::IN_PROGRESS:
      return DownloadState::IN_PROGRESS;
    case content::DownloadItem::COMPLETE:
      return DownloadState::COMPLETE;
    case content::DownloadItem::CANCELLED:
      return DownloadState::CANCELLED;
    case content::DownloadItem::INTERRUPTED:
      return DownloadState::INTERRUPTED;
    case content::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
      return DownloadState::INVALID;
  }
  NOTREACHED();
  return DownloadState::INVALID;
}

content::DownloadDangerType ToContentDownloadDangerType(
    DownloadDangerType danger_type) {
  switch (danger_type) {
    case DownloadDangerType::NOT_DANGEROUS:
      return content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
    case DownloadDangerType::DANGEROUS_FILE:
      return content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
    case DownloadDangerType::DANGEROUS_URL:
      return content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL;
    case DownloadDangerType::DANGEROUS_CONTENT:
      return content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT;
    case DownloadDangerType::MAYBE_DANGEROUS_CONTENT:
      return content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT;
    case DownloadDangerType::UNCOMMON_CONTENT:
      return content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT;
    case DownloadDangerType::USER_VALIDATED:
      return content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED;
    case DownloadDangerType::DANGEROUS_HOST:
      return content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST;
    case DownloadDangerType::POTENTIALLY_UNWANTED:
      return content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED;
    case DownloadDangerType::INVALID:
      NOTREACHED();
      return content::DOWNLOAD_DANGER_TYPE_MAX;
  }
  NOTREACHED();
  return content::DOWNLOAD_DANGER_TYPE_MAX;
}

DownloadDangerType ToHistoryDownloadDangerType(
    content::DownloadDangerType danger_type) {
  switch (danger_type) {
    case content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
      return DownloadDangerType::NOT_DANGEROUS;
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return DownloadDangerType::DANGEROUS_FILE;
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return DownloadDangerType::DANGEROUS_URL;
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return DownloadDangerType::DANGEROUS_CONTENT;
    case content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
      return DownloadDangerType::MAYBE_DANGEROUS_CONTENT;
    case content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return DownloadDangerType::UNCOMMON_CONTENT;
    case content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
      return DownloadDangerType::USER_VALIDATED;
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return DownloadDangerType::DANGEROUS_HOST;
    case content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return DownloadDangerType::POTENTIALLY_UNWANTED;
    default:
      NOTREACHED();
      return DownloadDangerType::INVALID;
  }
}

content::DownloadInterruptReason ToContentDownloadInterruptReason(
    DownloadInterruptReason interrupt_reason) {
  return static_cast<content::DownloadInterruptReason>(interrupt_reason);
}

DownloadInterruptReason ToHistoryDownloadInterruptReason(
    content::DownloadInterruptReason interrupt_reason) {
  return static_cast<DownloadInterruptReason>(interrupt_reason);
}

uint32_t ToContentDownloadId(DownloadId id) {
  DCHECK_NE(id, kInvalidDownloadId);
  return static_cast<uint32_t>(id);
}

DownloadId ToHistoryDownloadId(uint32_t id) {
  DCHECK_NE(id, content::DownloadItem::kInvalidId);
  return static_cast<DownloadId>(id);
}

}  // namespace history

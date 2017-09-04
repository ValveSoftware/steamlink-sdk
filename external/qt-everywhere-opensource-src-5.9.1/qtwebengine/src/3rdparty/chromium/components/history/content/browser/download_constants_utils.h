// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CONTENT_BROWSER_DOWNLOAD_CONSTANTS_UTILS_H_
#define COMPONENTS_HISTORY_CONTENT_BROWSER_DOWNLOAD_CONSTANTS_UTILS_H_

#include <stdint.h>

#include <string>

#include "components/history/core/browser/download_types.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"

namespace history {

// Utility functions to convert between content::DownloadItem::DownloadState
// enumeration and history::DownloadState constants.
content::DownloadItem::DownloadState ToContentDownloadState(
    DownloadState state);
DownloadState ToHistoryDownloadState(
    content::DownloadItem::DownloadState state);

// Utility functions to convert between content::DownloadDangerType enumeration
// and history::DownloadDangerType constants.
content::DownloadDangerType ToContentDownloadDangerType(
    DownloadDangerType danger_type);
DownloadDangerType ToHistoryDownloadDangerType(
    content::DownloadDangerType danger_type);

// Utility functions to convert between content::DownloadInterruptReason
// enumeration and history::DownloadInterruptReason type (value have no
// meaning in history, but have a different type to avoid bugs due to
// implicit conversions).
content::DownloadInterruptReason ToContentDownloadInterruptReason(
    DownloadInterruptReason interrupt_reason);
DownloadInterruptReason ToHistoryDownloadInterruptReason(
    content::DownloadInterruptReason interrupt_reason);

// Utility functions to convert between content download id values and
// history::DownloadId type (value have no meaning in history, except
// for kInvalidDownloadId).
uint32_t ToContentDownloadId(DownloadId id);
DownloadId ToHistoryDownloadId(uint32_t id);
}  // namespace history

#endif  // COMPONENTS_HISTORY_CONTENT_BROWSER_DOWNLOAD_CONSTANTS_UTILS_H_

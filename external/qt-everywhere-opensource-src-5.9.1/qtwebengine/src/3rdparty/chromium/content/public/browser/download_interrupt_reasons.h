// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_INTERRUPT_REASONS_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_INTERRUPT_REASONS_H_

#include <string>

#include "content/common/content_export.h"

namespace content {

enum DownloadInterruptReason {
  DOWNLOAD_INTERRUPT_REASON_NONE = 0,

#define INTERRUPT_REASON(name, value)  DOWNLOAD_INTERRUPT_REASON_##name = value,

#include "content/public/browser/download_interrupt_reason_values.h"

#undef INTERRUPT_REASON
};

std::string CONTENT_EXPORT DownloadInterruptReasonToString(
    DownloadInterruptReason error);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_INTERRUPT_REASONS_H_

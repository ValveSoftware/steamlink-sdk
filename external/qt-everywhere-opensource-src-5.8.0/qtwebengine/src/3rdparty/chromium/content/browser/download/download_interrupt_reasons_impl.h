// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_INTERRUPT_REASONS_IMPL_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_INTERRUPT_REASONS_IMPL_H_

#include "base/files/file.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "net/base/net_errors.h"

namespace content {

enum DownloadInterruptSource {
  DOWNLOAD_INTERRUPT_FROM_DISK,
  DOWNLOAD_INTERRUPT_FROM_NETWORK,
  DOWNLOAD_INTERRUPT_FROM_SERVER,
  DOWNLOAD_INTERRUPT_FROM_USER,
  DOWNLOAD_INTERRUPT_FROM_CRASH
};

// Safe to call from any thread.
DownloadInterruptReason CONTENT_EXPORT ConvertNetErrorToInterruptReason(
    net::Error file_error, DownloadInterruptSource source);

// Safe to call from any thread.
DownloadInterruptReason CONTENT_EXPORT ConvertFileErrorToInterruptReason(
    base::File::Error file_error);

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_INTERRUPT_REASONS_IMPL_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_DESTINATION_OBSERVER_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_DESTINATION_OBSERVER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "content/public/browser/download_interrupt_reasons.h"
#include "crypto/secure_hash.h"

namespace content {

// Class that receives asynchronous events from a DownloadDestination about
// downloading progress and completion.  These should report status when the
// data arrives at its final location; i.e. DestinationUpdate should be
// called after the destination is finished with whatever operation it
// is doing on the data described by |bytes_so_far| and DestinationCompleted
// should only be called once that is true for all data.
//
// All methods are invoked on the UI thread.
//
// Note that this interface does not deal with cross-thread lifetime issues.
class DownloadDestinationObserver {
 public:
  virtual void DestinationUpdate(int64_t bytes_so_far,
                                 int64_t bytes_per_sec) = 0;

  virtual void DestinationError(
      DownloadInterruptReason reason,
      int64_t bytes_so_far,
      std::unique_ptr<crypto::SecureHash> hash_state) = 0;

  virtual void DestinationCompleted(
      int64_t total_bytes,
      std::unique_ptr<crypto::SecureHash> hash_state) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_DESTINATION_OBSERVER_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_DESTINATION_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_DESTINATION_OBSERVER_H_

#include <string>

#include "content/public/browser/download_interrupt_reasons.h"

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
  virtual void DestinationUpdate(int64 bytes_so_far,
                                 int64 bytes_per_sec,
                                 const std::string& hash_state) = 0;

  virtual void DestinationError(DownloadInterruptReason reason) = 0;

  virtual void DestinationCompleted(const std::string& final_hash) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_DESTINATION_OBSERVER_H_

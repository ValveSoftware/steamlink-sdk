// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_H_

#include <string>

#include "base/callback.h"

namespace offline_pages {

class SavePageRequest;

// Interface of a class responsible for constructing an offline page given
// a request with a URL.
class Offliner {
 public:
  // Status of processing an offline page request.
  // WARNING: You must update histograms.xml to match any changes made to
  // this enum (ie, OfflinePagesBackgroundOfflinerRequestStatus histogram enum).
  // Also update related switch code in RequestCoordinatorEventLogger.
  enum RequestStatus {
    // No status determined/reported yet. Interim status, not sent in callback.
    UNKNOWN = 0,
    // Page loaded but not (yet) saved. Interim status, not sent in callback.
    LOADED = 1,
    // Offline page snapshot saved.
    SAVED = 2,
    // RequestCoordinator canceled request.
    REQUEST_COORDINATOR_CANCELED = 3,
    // Prerendering was canceled.
    PRERENDERING_CANCELED = 4,
    // Prerendering failed to load page.
    PRERENDERING_FAILED = 5,
    // Failed to save loaded page.
    SAVE_FAILED = 6,
    // Foreground transition canceled request.
    FOREGROUND_CANCELED = 7,
    // RequestCoordinator canceled request attempt per time limit.
    REQUEST_COORDINATOR_TIMED_OUT = 8,
    // The loader did not accept/start the request.
    PRERENDERING_NOT_STARTED = 9,
    // Prerendering failed with hard error so should not retry the request.
    PRERENDERING_FAILED_NO_RETRY = 10,
    // NOTE: insert new values above this line and update histogram enum too.
    STATUS_COUNT
  };

  // Reports the completion status of a request.
  // TODO(dougarnett): consider passing back a request id instead of request.
  typedef base::Callback<void(const SavePageRequest&, RequestStatus)>
      CompletionCallback;

  Offliner() {}
  virtual ~Offliner() {}

  // Processes |request| to load and save an offline page.
  // Returns whether the request was accepted or not. |callback| is guaranteed
  // to be called if the request was accepted and |Cancel()| is not called.
  virtual bool LoadAndSave(
      const SavePageRequest& request,
      const CompletionCallback& callback) = 0;

  // Clears the currently processing request, if any, and skips running its
  // CompletionCallback.
  virtual void Cancel() = 0;

  // TODO(dougarnett): add policy support methods.
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_H_

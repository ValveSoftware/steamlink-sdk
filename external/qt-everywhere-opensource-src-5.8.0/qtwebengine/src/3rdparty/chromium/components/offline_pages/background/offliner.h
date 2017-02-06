// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_H_

#include "base/callback.h"

namespace offline_pages {

class SavePageRequest;

// Interface of a class responsible for constructing an offline page given
// a request with a URL.
class Offliner {
 public:
  // Status of processing an offline page request.
  enum class RequestStatus {
    UNKNOWN,      // No status determined/reported yet.
    LOADED,       // Page loaded but not (yet) saved.
    SAVED,        // Offline page snapshot saved.
    CANCELED,     // Request was canceled.
    FAILED,       // Failed to load page.
    FAILED_SAVE,  // Failed to save loaded page.
    // TODO(dougarnett): Define a retry-able failure status.
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

  // Clears the currently processing request, if any.
  virtual void Cancel() = 0;

  // TODO(dougarnett): add policy support methods.
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_H_

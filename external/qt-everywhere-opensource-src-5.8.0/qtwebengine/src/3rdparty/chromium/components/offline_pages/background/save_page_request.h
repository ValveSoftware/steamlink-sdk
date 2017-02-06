// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_SAVE_PAGE_REQUEST_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_SAVE_PAGE_REQUEST_H_

#include <stdint.h>

#include "base/time/time.h"
#include "components/offline_pages/offline_page_item.h"
#include "url/gurl.h"

namespace offline_pages {

// Class representing a request to save page.
class SavePageRequest {
 public:
  enum class Status {
    NOT_READY,  // Component requested a page be saved, but not until
                // |activation_time_|.
    PENDING,    // Page request is pending, and coordinator can attempt it
                // when it gets a chance to.
    STARTED,    // The request is currently being processed.
    FAILED,     // Page request failed many times and will no longer be
                // retried.
    EXPIRED,    // Save page request expired without being fulfilled.
  };

  SavePageRequest(int64_t request_id,
                  const GURL& url,
                  const ClientId& client_id,
                  const base::Time& creation_time);
  SavePageRequest(int64_t request_id,
                  const GURL& url,
                  const ClientId& client_id,
                  const base::Time& creation_time,
                  const base::Time& activation_time);
  SavePageRequest(const SavePageRequest& other);
  ~SavePageRequest();

  // Status of this request.
  Status GetStatus(const base::Time& now) const;

  // Updates the |last_attempt_time_| and increments |attempt_count_|.
  void MarkAttemptStarted(const base::Time& start_time);

  // Marks attempt as completed and clears |last_attempt_time_|.
  void MarkAttemptCompleted();

  int64_t request_id() const { return request_id_; }

  const GURL& url() const { return url_; }

  const ClientId& client_id() const { return client_id_; }

  const base::Time& creation_time() const { return creation_time_; }

  const base::Time& activation_time() const { return activation_time_; }

  int64_t attempt_count() const { return attempt_count_; }
  void set_attempt_count(int64_t attempt_count) {
    attempt_count_ = attempt_count;
  }

  const base::Time& last_attempt_time() const { return last_attempt_time_; }
  void set_last_attempt_time(const base::Time& last_attempt_time) {
    last_attempt_time_ = last_attempt_time;
  }

 private:
  // ID of this request.
  int64_t request_id_;

  // Online URL of a page to be offlined.
  GURL url_;

  // Client ID related to the request. Contains namespace and ID assigned by the
  // requester.
  ClientId client_id_;

  // Time when this request was created. (Alternative 2).
  base::Time creation_time_;

  // Time when this request will become active.
  base::Time activation_time_;

  // Number of attempts made to get the page.
  int attempt_count_;

  // Timestamp of the last request starting.
  base::Time last_attempt_time_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_SAVE_PAGE_REQUEST_H_

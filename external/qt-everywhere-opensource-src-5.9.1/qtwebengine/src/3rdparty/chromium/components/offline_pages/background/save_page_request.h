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
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offlinepages
  enum class RequestState {
    AVAILABLE = 0,     // Request can be scheduled when preconditions are met.
    PAUSED = 1,        // Request is not available until it is unpaused.
    OFFLINING = 2,     // Request is actively offlining.
  };

  SavePageRequest(int64_t request_id,
                  const GURL& url,
                  const ClientId& client_id,
                  const base::Time& creation_time,
                  const bool user_requested);
  SavePageRequest(int64_t request_id,
                  const GURL& url,
                  const ClientId& client_id,
                  const base::Time& creation_time,
                  const base::Time& activation_time,
                  const bool user_requested);
  SavePageRequest(const SavePageRequest& other);
  ~SavePageRequest();

  bool operator==(const SavePageRequest& other) const;

  // Updates the |last_attempt_time_| and increments |attempt_count_|.
  void MarkAttemptStarted(const base::Time& start_time);

  // Marks attempt as completed and clears |last_attempt_time_|.
  void MarkAttemptCompleted();

  // Marks attempt as aborted. This will change the state of an OFFLINING
  // request to be AVAILABLE.  It will not change the state of a PAUSED request
  void MarkAttemptAborted();

  // Mark the attempt as paused.  It is not available for future prerendering
  // until it has been explicitly unpaused.
  void MarkAttemptPaused();

  int64_t request_id() const { return request_id_; }

  const GURL& url() const { return url_; }

  const ClientId& client_id() const { return client_id_; }

  RequestState request_state() const { return state_; }
  void set_request_state(RequestState new_state) { state_ = new_state; }

  const base::Time& creation_time() const { return creation_time_; }

  const base::Time& activation_time() const { return activation_time_; }

  int64_t started_attempt_count() const { return started_attempt_count_; }
  void set_started_attempt_count(int64_t started_attempt_count) {
    started_attempt_count_ = started_attempt_count;
  }

  int64_t completed_attempt_count() const { return completed_attempt_count_; }
  void set_completed_attempt_count(int64_t completed_attempt_count) {
    completed_attempt_count_ = completed_attempt_count;
  }

  const base::Time& last_attempt_time() const { return last_attempt_time_; }
  void set_last_attempt_time(const base::Time& last_attempt_time) {
    last_attempt_time_ = last_attempt_time;
  }

  bool user_requested() const { return user_requested_; }

  void set_user_requested(bool user_requested) {
    user_requested_ = user_requested;
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

  // Number of attempts started to get the page.  This may be different than the
  // number of attempts completed because we could crash.
  int started_attempt_count_;

  // Number of attempts we actually completed to get the page.
  int completed_attempt_count_;

  // Timestamp of the last request starting.
  base::Time last_attempt_time_;

  // Whether the user specifically requested this page (as opposed to a client
  // like AGSA or Now.)
  bool user_requested_;

  // The current state of this request
  RequestState state_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_SAVE_PAGE_REQUEST_H_

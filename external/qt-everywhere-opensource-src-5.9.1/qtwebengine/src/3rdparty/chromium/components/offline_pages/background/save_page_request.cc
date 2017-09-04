// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/save_page_request.h"

namespace offline_pages {

SavePageRequest::SavePageRequest(int64_t request_id,
                                 const GURL& url,
                                 const ClientId& client_id,
                                 const base::Time& creation_time,
                                 const bool user_requested)
    : request_id_(request_id),
      url_(url),
      client_id_(client_id),
      creation_time_(creation_time),
      activation_time_(creation_time),
      started_attempt_count_(0),
      completed_attempt_count_(0),
      user_requested_(user_requested),
      state_(RequestState::AVAILABLE) {}

SavePageRequest::SavePageRequest(int64_t request_id,
                                 const GURL& url,
                                 const ClientId& client_id,
                                 const base::Time& creation_time,
                                 const base::Time& activation_time,
                                 const bool user_requested)
    : request_id_(request_id),
      url_(url),
      client_id_(client_id),
      creation_time_(creation_time),
      activation_time_(activation_time),
      started_attempt_count_(0),
      completed_attempt_count_(0),
      user_requested_(user_requested),
      state_(RequestState::AVAILABLE) {}

SavePageRequest::SavePageRequest(const SavePageRequest& other)
    : request_id_(other.request_id_),
      url_(other.url_),
      client_id_(other.client_id_),
      creation_time_(other.creation_time_),
      activation_time_(other.activation_time_),
      started_attempt_count_(other.started_attempt_count_),
      completed_attempt_count_(other.completed_attempt_count_),
      last_attempt_time_(other.last_attempt_time_),
      user_requested_(other.user_requested_),
      state_(other.state_) {}

SavePageRequest::~SavePageRequest() {}

bool SavePageRequest::operator==(const SavePageRequest& other) const {
  return request_id_ == other.request_id_ &&
         url_ == other.url_ &&
         client_id_ == other.client_id_ &&
         creation_time_ == other.creation_time_ &&
         activation_time_ == other.activation_time_ &&
         started_attempt_count_ == other.started_attempt_count_ &&
         completed_attempt_count_ == other.completed_attempt_count_ &&
         last_attempt_time_ == other.last_attempt_time_ &&
         state_ == other.state_;
}

void SavePageRequest::MarkAttemptStarted(const base::Time& start_time) {
  DCHECK_LE(activation_time_, start_time);
  // TODO(fgorski): As part of introducing policy in GetStatus, we can make a
  // check here to ensure we only start tasks in status pending, and bail out in
  // other cases.
  last_attempt_time_ = start_time;
  ++started_attempt_count_;
  state_ = RequestState::OFFLINING;
}

void SavePageRequest::MarkAttemptCompleted() {
  ++completed_attempt_count_;
  state_ = RequestState::AVAILABLE;
}

void SavePageRequest::MarkAttemptAborted() {
  DCHECK_GT(started_attempt_count_, 0);
  // We intentinally do not increment the completed_attempt_count_, since this
  // was killed before it completed, so we could use the phone or browser for
  // other things.
  if (state_ == RequestState::OFFLINING)
    state_ = RequestState::AVAILABLE;
}

void SavePageRequest::MarkAttemptPaused() {
  state_ = RequestState::PAUSED;
}

}  // namespace offline_pages

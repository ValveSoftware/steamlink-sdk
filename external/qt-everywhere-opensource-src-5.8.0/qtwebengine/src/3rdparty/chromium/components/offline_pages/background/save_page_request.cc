// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/save_page_request.h"

namespace offline_pages {

SavePageRequest::SavePageRequest(int64_t request_id,
                                 const GURL& url,
                                 const ClientId& client_id,
                                 const base::Time& creation_time)
    : request_id_(request_id),
      url_(url),
      client_id_(client_id),
      creation_time_(creation_time),
      activation_time_(creation_time),
      attempt_count_(0) {}

SavePageRequest::SavePageRequest(int64_t request_id,
                                 const GURL& url,
                                 const ClientId& client_id,
                                 const base::Time& creation_time,
                                 const base::Time& activation_time)
    : request_id_(request_id),
      url_(url),
      client_id_(client_id),
      creation_time_(creation_time),
      activation_time_(activation_time),
      attempt_count_(0) {}

SavePageRequest::SavePageRequest(const SavePageRequest& other)
    : request_id_(other.request_id_),
      url_(other.url_),
      client_id_(other.client_id_),
      creation_time_(other.creation_time_),
      activation_time_(other.activation_time_),
      attempt_count_(other.attempt_count_),
      last_attempt_time_(other.last_attempt_time_) {}

SavePageRequest::~SavePageRequest() {}

// TODO(fgorski): Introduce policy parameter, once policy is available.
SavePageRequest::Status SavePageRequest::GetStatus(
    const base::Time& now) const {
  if (now < activation_time_)
    return Status::NOT_READY;

  // TODO(fgorski): enable check once policy available.
  // if (attempt_count_ >= policy.max_attempt_count)
  // return Status::FAILED;

  // TODO(fgorski): enable check once policy available.
  // if (activation_time_+ policy.page_expiration_interval < now)
  // return Status::EXPIRED;

  if (creation_time_ < last_attempt_time_)
    return Status::STARTED;

  return Status::PENDING;
}

void SavePageRequest::MarkAttemptStarted(const base::Time& start_time) {
  DCHECK_LE(activation_time_, start_time);
  // TODO(fgorski): As part of introducing policy in GetStatus, we can make a
  // check here to ensure we only start tasks in status pending, and bail out in
  // other cases.
  last_attempt_time_ = start_time;
  ++attempt_count_;
}

void SavePageRequest::MarkAttemptCompleted() {
  last_attempt_time_ = base::Time();
}

}  // namespace offline_pages

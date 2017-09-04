// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/save_page_request.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const int64_t kRequestId = 42;
const GURL kUrl("http://example.com");
const ClientId kClientId("bookmark", "1234");
const bool kUserRequested = true;
}  // namespace

class SavePageRequestTest : public testing::Test {
 public:
  ~SavePageRequestTest() override;
};

SavePageRequestTest::~SavePageRequestTest() {}

TEST_F(SavePageRequestTest, CreatePendingReqeust) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(
      kRequestId, kUrl, kClientId, creation_time, kUserRequested);
  ASSERT_EQ(kRequestId, request.request_id());
  ASSERT_EQ(kUrl, request.url());
  ASSERT_EQ(kClientId, request.client_id());
  ASSERT_EQ(creation_time, request.creation_time());
  ASSERT_EQ(creation_time, request.activation_time());
  ASSERT_EQ(base::Time(), request.last_attempt_time());
  ASSERT_EQ(0, request.completed_attempt_count());
  ASSERT_EQ(SavePageRequest::RequestState::AVAILABLE, request.request_state());
  ASSERT_EQ(0, request.started_attempt_count());
  ASSERT_EQ(0, request.completed_attempt_count());
}

TEST_F(SavePageRequestTest, StartAndCompleteRequest) {
  base::Time creation_time = base::Time::Now();
  base::Time activation_time = creation_time + base::TimeDelta::FromHours(6);
  SavePageRequest request(kRequestId, kUrl, kClientId, creation_time,
                          activation_time, kUserRequested);

  base::Time start_time = activation_time + base::TimeDelta::FromHours(3);
  request.MarkAttemptStarted(start_time);

  // Most things don't change about the request.
  ASSERT_EQ(kRequestId, request.request_id());
  ASSERT_EQ(kUrl, request.url());
  ASSERT_EQ(kClientId, request.client_id());
  ASSERT_EQ(creation_time, request.creation_time());
  ASSERT_EQ(activation_time, request.activation_time());

  // Attempt time, attempt count and status will though.
  ASSERT_EQ(start_time, request.last_attempt_time());
  ASSERT_EQ(1, request.started_attempt_count());
  ASSERT_EQ(SavePageRequest::RequestState::OFFLINING,
            request.request_state());

  request.MarkAttemptCompleted();

  // Again, most things don't change about the request.
  ASSERT_EQ(kRequestId, request.request_id());
  ASSERT_EQ(kUrl, request.url());
  ASSERT_EQ(kClientId, request.client_id());
  ASSERT_EQ(creation_time, request.creation_time());
  ASSERT_EQ(activation_time, request.activation_time());

  // Last attempt time and status are updated.
  ASSERT_EQ(1, request.completed_attempt_count());
  ASSERT_EQ(SavePageRequest::RequestState::AVAILABLE, request.request_state());
}

TEST_F(SavePageRequestTest, StartAndAbortRequest) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kUrl, kClientId, creation_time,
                          kUserRequested);

  base::Time start_time = creation_time + base::TimeDelta::FromHours(3);
  request.MarkAttemptStarted(start_time);

  // Most things don't change about the request.
  ASSERT_EQ(kRequestId, request.request_id());
  ASSERT_EQ(kUrl, request.url());
  ASSERT_EQ(kClientId, request.client_id());
  ASSERT_EQ(creation_time, request.creation_time());

  // Attempt time and attempt count will though.
  ASSERT_EQ(start_time, request.last_attempt_time());
  ASSERT_EQ(1, request.started_attempt_count());
  ASSERT_EQ(SavePageRequest::RequestState::OFFLINING,
            request.request_state());

  request.MarkAttemptAborted();

  // Again, most things don't change about the request.
  ASSERT_EQ(kRequestId, request.request_id());
  ASSERT_EQ(kUrl, request.url());
  ASSERT_EQ(kClientId, request.client_id());
  ASSERT_EQ(creation_time, request.creation_time());

  // Last attempt time is updated and completed attempt count did not rise.
  ASSERT_EQ(0, request.completed_attempt_count());
  ASSERT_EQ(SavePageRequest::RequestState::AVAILABLE, request.request_state());
}

}  // namespace offline_pages

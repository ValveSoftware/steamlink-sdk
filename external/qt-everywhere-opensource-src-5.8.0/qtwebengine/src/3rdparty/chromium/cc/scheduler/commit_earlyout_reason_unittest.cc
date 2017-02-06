// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/commit_earlyout_reason.h"

#include "cc/proto/commit_earlyout_reason.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

CommitEarlyOutReason SerializeAndDeserializeReason(
    CommitEarlyOutReason reason) {
  proto::CommitEarlyOutReason proto;
  CommitEarlyOutReasonToProtobuf(reason, &proto);
  return CommitEarlyOutReasonFromProtobuf(proto);
}

TEST(CommitEarlyOutReasonUnittest, SerializeCommitEarlyOutReason) {
  EXPECT_EQ(CommitEarlyOutReason::ABORTED_OUTPUT_SURFACE_LOST,
            SerializeAndDeserializeReason(
                CommitEarlyOutReason::ABORTED_OUTPUT_SURFACE_LOST));

  EXPECT_EQ(
      CommitEarlyOutReason::ABORTED_NOT_VISIBLE,
      SerializeAndDeserializeReason(CommitEarlyOutReason::ABORTED_NOT_VISIBLE));

  EXPECT_EQ(CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT,
            SerializeAndDeserializeReason(
                CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT));

  EXPECT_EQ(
      CommitEarlyOutReason::FINISHED_NO_UPDATES,
      SerializeAndDeserializeReason(CommitEarlyOutReason::FINISHED_NO_UPDATES));
}

}  // namespace
}  // namespace cc

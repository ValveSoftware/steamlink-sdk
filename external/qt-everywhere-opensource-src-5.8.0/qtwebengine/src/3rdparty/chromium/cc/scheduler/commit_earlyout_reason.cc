// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/commit_earlyout_reason.h"

#include "cc/proto/commit_earlyout_reason.pb.h"

namespace cc {

CommitEarlyOutReason CommitEarlyOutReasonFromProtobuf(
    const proto::CommitEarlyOutReason& proto) {
  switch (proto.reason()) {
    case proto::CommitEarlyOutReason::ABORTED_OUTPUT_SURFACE_LOST:
      return CommitEarlyOutReason::ABORTED_OUTPUT_SURFACE_LOST;
    case proto::CommitEarlyOutReason::ABORTED_NOT_VISIBLE:
      return CommitEarlyOutReason::ABORTED_NOT_VISIBLE;
    case proto::CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT:
      return CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT;
    case proto::CommitEarlyOutReason::FINISHED_NO_UPDATES:
      return CommitEarlyOutReason::FINISHED_NO_UPDATES;
  }
  NOTREACHED();
  return CommitEarlyOutReason::ABORTED_OUTPUT_SURFACE_LOST;
}

void CommitEarlyOutReasonToProtobuf(CommitEarlyOutReason reason,
                                    proto::CommitEarlyOutReason* proto) {
  switch (reason) {
    case CommitEarlyOutReason::ABORTED_OUTPUT_SURFACE_LOST:
      proto->set_reason(
          proto::CommitEarlyOutReason::ABORTED_OUTPUT_SURFACE_LOST);
      return;
    case CommitEarlyOutReason::ABORTED_NOT_VISIBLE:
      proto->set_reason(proto::CommitEarlyOutReason::ABORTED_NOT_VISIBLE);
      return;
    case CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT:
      proto->set_reason(proto::CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT);
      return;
    case CommitEarlyOutReason::FINISHED_NO_UPDATES:
      proto->set_reason(proto::CommitEarlyOutReason::FINISHED_NO_UPDATES);
      return;
  }
  NOTREACHED();
}

}  // namespace cc

// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_RENDERER_FAKE_PAGE_TIMING_METRICS_IPC_SENDER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_RENDERER_FAKE_PAGE_TIMING_METRICS_IPC_SENDER_H_

#include <vector>

#include "components/page_load_metrics/common/page_load_timing.h"
#include "ipc/ipc_sender.h"

namespace IPC {
class Message;
}

namespace page_load_metrics {

// IPC::Sender implementation for use in tests. Allows for setting and verifying
// basic expectations when sending PageLoadTiming IPCs. By default,
// FakePageTimingMetricsIPCSender will verify that expected and actual
// PageLoadTimings match on each invocation to ExpectPageLoadTiming() and
// Send(), as well as in the destructor. Tests can force additional validations
// by calling VerifyExpectedTimings.
//
// Expected PageLoadTimings are specified via ExpectPageLoadTiming, and actual
// PageLoadTimings are dispatched through Send(). When Send() is called, we
// verify that the actual PageLoadTimings dipatched through Send() match the
// expected PageLoadTimings provided via ExpectPageLoadTiming.
//
// Normally, gmock would be used in place of this class, but gmock is not
// compatible with structures that use aligned memory, and PageLoadTiming will
// soon use base::Optional which uses aligned memory, so we're forced to roll
// our own implementation here.  See
// https://groups.google.com/forum/#!topic/googletestframework/W-Hud3j_c6I for
// more details.
class FakePageTimingMetricsIPCSender : public IPC::Sender {
 public:
  FakePageTimingMetricsIPCSender();
  ~FakePageTimingMetricsIPCSender() override;

  // Implementation of IPC::Sender. PageLoadMetricsMsg_TimingUpdated IPCs that
  // send updated PageLoadTimings should be dispatched through this method. This
  // method will verify that all PageLoadTiming update IPCs dispatched so far
  // match with the expected PageLoadTimings passed to ExpectPageLoadTiming.
  bool Send(IPC::Message* message) override;

  // PageLoadTimings that are expected to be sent through Send() should be
  // passed to ExpectPageLoadTiming.
  void ExpectPageLoadTiming(const PageLoadTiming& timing);

  // Forces verification that actual timings sent through Send match
  // expected timings provided via ExpectPageLoadTiming.
  void VerifyExpectedTimings() const;

  const std::vector<PageLoadTiming>& expected_timings() const {
    return expected_timings_;
  }
  const std::vector<PageLoadTiming>& actual_timings() const {
    return actual_timings_;
  }

 private:
  void OnTimingUpdated(const PageLoadTiming& timing, PageLoadMetadata metadata);

  std::vector<PageLoadTiming> expected_timings_;
  std::vector<PageLoadTiming> actual_timings_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_RENDERER_FAKE_PAGE_TIMING_METRICS_IPC_SENDER_H_

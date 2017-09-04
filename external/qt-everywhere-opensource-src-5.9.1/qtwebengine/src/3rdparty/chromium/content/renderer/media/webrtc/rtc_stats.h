// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_RTC_STATS_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_RTC_STATS_H_

#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/platform/WebRTCStats.h"
#include "third_party/webrtc/api/stats/rtcstats.h"
#include "third_party/webrtc/api/stats/rtcstatsreport.h"

namespace content {

class RTCStatsReport : public blink::WebRTCStatsReport {
 public:
  RTCStatsReport(
      const scoped_refptr<const webrtc::RTCStatsReport>& stats_report);
  ~RTCStatsReport() override;
  std::unique_ptr<blink::WebRTCStatsReport> copyHandle() const override;

  std::unique_ptr<blink::WebRTCStats> getStats(
      blink::WebString id) const override;
  std::unique_ptr<blink::WebRTCStats> next() override;

 private:
  const scoped_refptr<const webrtc::RTCStatsReport> stats_report_;
  webrtc::RTCStatsReport::ConstIterator it_;
  const webrtc::RTCStatsReport::ConstIterator end_;
};

class RTCStats : public blink::WebRTCStats {
 public:
  RTCStats(const scoped_refptr<const webrtc::RTCStatsReport>& stats_owner,
           const webrtc::RTCStats* stats);
  ~RTCStats() override;

  blink::WebString id() const override;
  blink::WebString type() const override;
  double timestamp() const override;

  size_t membersCount() const override;
  std::unique_ptr<blink::WebRTCStatsMember> getMember(size_t i) const override;

 private:
  // Reference to keep the report that owns |stats_| alive.
  const scoped_refptr<const webrtc::RTCStatsReport> stats_owner_;
  // Pointer to a stats object that is owned by |stats_owner_|.
  const webrtc::RTCStats* const stats_;
  // Members of the |stats_| object, equivalent to |stats_->Members()|.
  const std::vector<const webrtc::RTCStatsMemberInterface*> stats_members_;
};

class RTCStatsMember : public blink::WebRTCStatsMember {
 public:
  RTCStatsMember(const scoped_refptr<const webrtc::RTCStatsReport>& stats_owner,
                 const webrtc::RTCStatsMemberInterface* member);
  ~RTCStatsMember() override;

  blink::WebString name() const override;
  blink::WebRTCStatsMemberType type() const override;
  bool isDefined() const override;

  bool valueBool() const override;
  int32_t valueInt32() const override;
  uint32_t valueUint32() const override;
  int64_t valueInt64() const override;
  uint64_t valueUint64() const override;
  double valueDouble() const override;
  blink::WebString valueString() const override;
  blink::WebVector<int> valueSequenceBool() const override;
  blink::WebVector<int32_t> valueSequenceInt32() const override;
  blink::WebVector<uint32_t> valueSequenceUint32() const override;
  blink::WebVector<int64_t> valueSequenceInt64() const override;
  blink::WebVector<uint64_t> valueSequenceUint64() const override;
  blink::WebVector<double> valueSequenceDouble() const override;
  blink::WebVector<blink::WebString> valueSequenceString() const override;

 private:
  // Reference to keep the report that owns |member_|'s stats object alive.
  const scoped_refptr<const webrtc::RTCStatsReport> stats_owner_;
  // Pointer to member of a stats object that is owned by |stats_owner_|.
  const webrtc::RTCStatsMemberInterface* const member_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_RTC_STATS_H_

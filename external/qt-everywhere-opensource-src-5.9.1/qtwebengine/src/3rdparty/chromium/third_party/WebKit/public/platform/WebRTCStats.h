// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRTCStats_h
#define WebRTCStats_h

#include "WebCommon.h"
#include "WebString.h"
#include "WebVector.h"

#include <memory>
#include <string>
#include <vector>

namespace blink {

class WebRTCStats;
class WebRTCStatsMember;

enum WebRTCStatsMemberType {
  WebRTCStatsMemberTypeBool,    // bool
  WebRTCStatsMemberTypeInt32,   // int32_t
  WebRTCStatsMemberTypeUint32,  // uint32_t
  WebRTCStatsMemberTypeInt64,   // int64_t
  WebRTCStatsMemberTypeUint64,  // uint64_t
  WebRTCStatsMemberTypeDouble,  // double
  WebRTCStatsMemberTypeString,  // WebString

  WebRTCStatsMemberTypeSequenceBool,    // WebVector<int>
  WebRTCStatsMemberTypeSequenceInt32,   // WebVector<int32_t>
  WebRTCStatsMemberTypeSequenceUint32,  // WebVector<uint32_t>
  WebRTCStatsMemberTypeSequenceInt64,   // WebVector<int64_t>
  WebRTCStatsMemberTypeSequenceUint64,  // WebVector<uint64_t>
  WebRTCStatsMemberTypeSequenceDouble,  // WebVector<double>
  WebRTCStatsMemberTypeSequenceString,  // WebVector<WebString>
};

class WebRTCStatsReport {
 public:
  virtual ~WebRTCStatsReport() {}
  // Creates a new report object that is a handle to the same underlying stats
  // report (the stats are not copied). The new report's iterator is reset,
  // useful when needing multiple iterators.
  virtual std::unique_ptr<WebRTCStatsReport> copyHandle() const = 0;

  // Gets stats object by |id|, or null if no stats with that |id| exists.
  virtual std::unique_ptr<WebRTCStats> getStats(WebString id) const = 0;
  // The next stats object, or null if the end has been reached.
  virtual std::unique_ptr<WebRTCStats> next() = 0;
};

class WebRTCStats {
 public:
  virtual ~WebRTCStats() {}

  virtual WebString id() const = 0;
  virtual WebString type() const = 0;
  virtual double timestamp() const = 0;

  virtual size_t membersCount() const = 0;
  virtual std::unique_ptr<WebRTCStatsMember> getMember(size_t) const = 0;
};

class WebRTCStatsMember {
 public:
  virtual ~WebRTCStatsMember() {}

  virtual WebString name() const = 0;
  virtual WebRTCStatsMemberType type() const = 0;
  virtual bool isDefined() const = 0;

  // Value getters. No conversion is performed; the function must match the
  // member's |type|.
  virtual bool valueBool() const = 0;
  virtual int32_t valueInt32() const = 0;
  virtual uint32_t valueUint32() const = 0;
  virtual int64_t valueInt64() const = 0;
  virtual uint64_t valueUint64() const = 0;
  virtual double valueDouble() const = 0;
  virtual WebString valueString() const = 0;
  // |WebVector<int> because |WebVector| is incompatible with |bool|.
  virtual WebVector<int> valueSequenceBool() const = 0;
  virtual WebVector<int32_t> valueSequenceInt32() const = 0;
  virtual WebVector<uint32_t> valueSequenceUint32() const = 0;
  virtual WebVector<int64_t> valueSequenceInt64() const = 0;
  virtual WebVector<uint64_t> valueSequenceUint64() const = 0;
  virtual WebVector<double> valueSequenceDouble() const = 0;
  virtual WebVector<WebString> valueSequenceString() const = 0;
};

class WebRTCStatsReportCallback {
 public:
  virtual ~WebRTCStatsReportCallback() {}

  virtual void OnStatsDelivered(std::unique_ptr<WebRTCStatsReport>) = 0;
};

}  // namespace blink

#endif  // WebRTCStats_h

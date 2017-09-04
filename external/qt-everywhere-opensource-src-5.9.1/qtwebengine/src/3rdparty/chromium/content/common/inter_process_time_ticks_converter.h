// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INTER_PROCESS_TIME_TICKS_CONVERTER_H_
#define CONTENT_COMMON_INTER_PROCESS_TIME_TICKS_CONVERTER_H_

#include <stdint.h>

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

class LocalTimeDelta;
class LocalTimeTicks;
class RemoteTimeDelta;
class RemoteTimeTicks;

// On Windows, TimeTicks are not always consistent between processes as
// indicated by |TimeTicks::IsConsistentAcrossProcesses()|. Often, the values on
// one process have a static offset relative to another. Occasionally, these
// offsets shift while running.
//
// To combat this, any TimeTicks values sent from the remote process to the
// local process must be tweaked in order to appear monotonic.
//
// In order to properly tweak ticks, we need 4 reference points:
//
// - |local_lower_bound|:  A known point, recorded on the local process, that
//                         occurs before any remote values that will be
//                         converted.
// - |remote_lower_bound|: The equivalent point on the remote process. This
//                         should be recorded immediately after
//                         |local_lower_bound|.
// - |local_upper_bound|:  A known point, recorded on the local process, that
//                         occurs after any remote values that will be
//                         converted.
// - |remote_upper_bound|: The equivalent point on the remote process. This
//                         should be recorded immediately before
//                         |local_upper_bound|.
//
// Once these bounds are determined, values within the remote process's range
// can be converted to the local process's range. The values are converted as
// follows:
//
// 1. If the remote's range exceeds the local's range, it is scaled to fit.
//    Any values converted will have the same scale factor applied.
//
// 2. The remote's range is shifted so that it is centered within the
//    local's range. Any values converted will be shifted the same amount.
class CONTENT_EXPORT InterProcessTimeTicksConverter {
 public:
  InterProcessTimeTicksConverter(const LocalTimeTicks& local_lower_bound,
                                 const LocalTimeTicks& local_upper_bound,
                                 const RemoteTimeTicks& remote_lower_bound,
                                 const RemoteTimeTicks& remote_upper_bound);

  // Returns the value within the local's bounds that correlates to
  // |remote_ms|.
  LocalTimeTicks ToLocalTimeTicks(const RemoteTimeTicks& remote_ms) const;

  // Returns the equivalent delta after applying remote-to-local scaling to
  // |remote_delta|.
  LocalTimeDelta ToLocalTimeDelta(const RemoteTimeDelta& remote_delta) const;

  // Returns true iff the TimeTicks are converted by adding a constant, without
  // scaling. This is the case whenever the remote timespan is smaller than the
  // local timespan, which should be the majority of cases due to IPC overhead.
  bool IsSkewAdditiveForMetrics() const;

  // Returns the (remote time) - (local time) difference estimated by the
  // converter. This is the constant that is subtracted from remote TimeTicks to
  // get local TimeTicks when no scaling is applied.
  base::TimeDelta GetSkewForMetrics() const;

 private:
  int64_t Convert(int64_t value) const;

  // The local time which |remote_lower_bound_| is mapped to.
  int64_t local_base_time_;

  int64_t numerator_;
  int64_t denominator_;

  int64_t remote_lower_bound_;
  int64_t remote_upper_bound_;
};

class CONTENT_EXPORT LocalTimeDelta {
 public:
  int ToInt32() const { return value_; }

 private:
  friend class InterProcessTimeTicksConverter;
  friend class LocalTimeTicks;

  LocalTimeDelta(int value) : value_(value) {}

  int value_;
};

class CONTENT_EXPORT LocalTimeTicks {
 public:
  static LocalTimeTicks FromTimeTicks(const base::TimeTicks& value) {
    return LocalTimeTicks(value.ToInternalValue());
  }

  base::TimeTicks ToTimeTicks() {
    return base::TimeTicks::FromInternalValue(value_);
  }

  LocalTimeTicks operator+(const LocalTimeDelta& delta) {
    return LocalTimeTicks(value_ + delta.value_);
  }

 private:
  friend class InterProcessTimeTicksConverter;

  LocalTimeTicks(int64_t value) : value_(value) {}

  int64_t value_;
};

class CONTENT_EXPORT RemoteTimeDelta {
 public:
  static RemoteTimeDelta FromRawDelta(int delta) {
    return RemoteTimeDelta(delta);
  }

 private:
  friend class InterProcessTimeTicksConverter;
  friend class RemoteTimeTicks;

  RemoteTimeDelta(int value) : value_(value) {}

  int value_;
};

class CONTENT_EXPORT RemoteTimeTicks {
 public:
  static RemoteTimeTicks FromTimeTicks(const base::TimeTicks& ticks) {
    return RemoteTimeTicks(ticks.ToInternalValue());
  }

  RemoteTimeDelta operator-(const RemoteTimeTicks& rhs) const {
    return RemoteTimeDelta(value_ - rhs.value_);
  }

 private:
  friend class InterProcessTimeTicksConverter;

  RemoteTimeTicks(int64_t value) : value_(value) {}

  int64_t value_;
};

}  // namespace content

#endif  // CONTENT_COMMON_INTER_PROCESS_TIME_TICKS_CONVERTER_H_

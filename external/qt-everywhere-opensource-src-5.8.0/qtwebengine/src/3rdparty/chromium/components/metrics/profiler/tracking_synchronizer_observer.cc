// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/profiler/tracking_synchronizer_observer.h"

namespace metrics {

ProfilerDataAttributes::ProfilerDataAttributes(
    int profiling_phase,
    base::ProcessId process_id,
    ProfilerEventProto::TrackedObject::ProcessType process_type,
    base::TimeTicks phase_start,
    base::TimeTicks phase_finish)
    : profiling_phase(profiling_phase),
      process_id(process_id),
      process_type(process_type),
      phase_start(phase_start),
      phase_finish(phase_finish) {}

}  // namespace metrics

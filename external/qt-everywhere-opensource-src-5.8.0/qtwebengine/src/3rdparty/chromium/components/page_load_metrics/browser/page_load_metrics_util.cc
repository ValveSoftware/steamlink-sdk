// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_util.h"

#include <algorithm>

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_timing.h"

namespace page_load_metrics {

bool WasStartedInForegroundEventInForeground(base::TimeDelta event,
                                             const PageLoadExtraInfo& info) {
  return info.started_in_foreground && !event.is_zero() &&
         (!info.first_background_time ||
          event <= info.first_background_time.value());
}

// TODO (shivanisha) The above function is used for TimeDeltas coming over IPC.
// Merge these two when page_load_metrics only handles Optional TimeDelta
// values in the browser process.
bool WasStartedInForegroundOptionalEventInForeground(
    const base::Optional<base::TimeDelta>& event,
    const PageLoadExtraInfo& info) {
  return info.started_in_foreground && event &&
         (!info.first_background_time ||
          event.value() <= info.first_background_time.value());
}

bool WasParseInForeground(base::TimeDelta parse_start,
                          base::TimeDelta parse_stop,
                          const PageLoadExtraInfo& info) {
  if (parse_start.is_zero()) {
    return false;
  }
  const bool incomplete_parse_in_foreground = parse_stop.is_zero() &&
                                              info.started_in_foreground &&
                                              !info.first_background_time;

  return incomplete_parse_in_foreground ||
         WasStartedInForegroundEventInForeground(parse_stop, info);
}
}  // namespace page_load_metrics

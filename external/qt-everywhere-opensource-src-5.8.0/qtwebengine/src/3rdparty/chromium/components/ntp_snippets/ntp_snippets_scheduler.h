// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_SCHEDULER_H_
#define COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_SCHEDULER_H_

#include "base/macros.h"
#include "base/time/time.h"

namespace ntp_snippets {

// Interface to schedule the periodic fetching of snippets.
class NTPSnippetsScheduler {
 public:
  // Schedule periodic fetching of snippets, with different period depending on
  // network and charging state, and also set up a delay after which the periods
  // may change. The concrete implementation should call
  // NTPSnippetsService::FetchSnippets once per period, and
  // NTPSnippetsService::RescheduleFetching at |reschedule_time|.
  // Any of the values can be zero to indicate that the corresponding task
  // should not be scheduled.
  virtual bool Schedule(base::TimeDelta period_wifi_charging,
                        base::TimeDelta period_wifi,
                        base::TimeDelta period_fallback,
                        base::Time reschedule_time) = 0;

  // Cancel any scheduled tasks.
  virtual bool Unschedule() = 0;

 protected:
  NTPSnippetsScheduler() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsScheduler);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_SCHEDULER_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace page_load_metrics {

PageLoadExtraInfo::PageLoadExtraInfo(
    const base::Optional<base::TimeDelta>& first_background_time,
    const base::Optional<base::TimeDelta>& first_foreground_time,
    bool started_in_foreground,
    const GURL& committed_url,
    const base::Optional<base::TimeDelta>& time_to_commit,
    UserAbortType abort_type,
    const base::Optional<base::TimeDelta>& time_to_abort,
    const PageLoadMetadata& metadata)
    : first_background_time(first_background_time),
      first_foreground_time(first_foreground_time),
      started_in_foreground(started_in_foreground),
      committed_url(committed_url),
      time_to_commit(time_to_commit),
      abort_type(abort_type),
      time_to_abort(time_to_abort),
      metadata(metadata) {}

PageLoadExtraInfo::PageLoadExtraInfo(const PageLoadExtraInfo& other) = default;

PageLoadExtraInfo::~PageLoadExtraInfo() {}
}  // namespace page_load_metrics

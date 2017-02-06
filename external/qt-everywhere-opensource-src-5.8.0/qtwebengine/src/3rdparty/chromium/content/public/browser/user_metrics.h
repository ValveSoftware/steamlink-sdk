// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_USER_METRICS_H_
#define CONTENT_PUBLIC_BROWSER_USER_METRICS_H_

#include <string>

#include "base/callback.h"
#include "base/metrics/user_metrics_action.h"
#include "content/common/content_export.h"

namespace content {

// TODO(beaudoin): Get rid of these methods now that the base:: version does
// thread hopping. Tracked in crbug.com/601483.
// Wrappers around functions defined in base/metrics/user_metrics.h, refer to
// that header for full documentation. These wrappers can be called from any
// thread (they will post back to the UI thread to do the recording).

CONTENT_EXPORT void RecordAction(const base::UserMetricsAction& action);
CONTENT_EXPORT void RecordComputedAction(const std::string& action);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_USER_METRICS_H_

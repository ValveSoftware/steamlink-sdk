// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_METRICS_CAST_METRICS_TEST_HELPER_H_
#define CHROMECAST_BASE_METRICS_CAST_METRICS_TEST_HELPER_H_

namespace chromecast {
namespace metrics {

// Creates and initializes a metrics helper stub for the current process, in
// which all operations are no-ops. May be safely called multiple times per
// process.
void InitializeMetricsHelperForTesting();

}  // namespace metrics
}  // namespace chromecast

#endif  // CHROMECAST_BASE_METRICS_CAST_METRICS_TEST_HELPER_H_

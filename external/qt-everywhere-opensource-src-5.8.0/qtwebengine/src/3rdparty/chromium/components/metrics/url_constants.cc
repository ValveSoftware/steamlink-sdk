// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/url_constants.h"

#include "build/build_config.h"

namespace metrics {

#if defined(OS_ANDROID)
const char kDefaultMetricsServerUrl[] =
    "https://clientservices.googleapis.com/uma/v2";
#else
const char kDefaultMetricsServerUrl[] = "https://clients4.google.com/uma/v2";
#endif

const char kDefaultMetricsMimeType[] = "application/vnd.chrome.uma";

} // namespace metrics


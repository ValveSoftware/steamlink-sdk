// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/switches.h"

namespace tracing {

// Specifies if the |StatsCollectionController| needs to be bound in html pages.
// This binding happens on per-frame basis and hence can potentially be a
// performance bottleneck. One should only enable it when running a test that
// needs to access the provided statistics.
const char kEnableStatsCollectionBindings[] =
    "enable-stats-collection-bindings";

const char kTraceStartup[] = "trace-startup";

#ifdef NDEBUG
const char kEarlyTracing[] = "early-tracing";
#endif

}  // namespace tracing

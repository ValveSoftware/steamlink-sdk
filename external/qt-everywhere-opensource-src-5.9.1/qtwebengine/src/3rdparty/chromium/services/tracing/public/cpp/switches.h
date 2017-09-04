// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_SWITCHES_H_
#define SERVICES_TRACING_PUBLIC_CPP_SWITCHES_H_

namespace tracing {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kEnableStatsCollectionBindings[];

extern const char kTraceStartup[];

#ifdef NDEBUG
// In release builds, specifying this flag will force reporting of tracing
// before the main Application is initialized.
extern const char kEarlyTracing[];
#endif

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_SWITCHES_H_

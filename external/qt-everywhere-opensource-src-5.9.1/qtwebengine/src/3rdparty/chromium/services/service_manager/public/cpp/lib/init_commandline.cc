// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

namespace service_manager {

extern int g_service_runner_argc;
extern const char* const* g_service_runner_argv;

}  // namespace service_manager

#if !defined(OS_WIN)
extern "C" {
__attribute__((visibility("default"))) void InitCommandLineArgs(
    int argc, const char* const* argv) {
  service_manager::g_service_runner_argc = argc;
  service_manager::g_service_runner_argv = argv;
}
}
#endif

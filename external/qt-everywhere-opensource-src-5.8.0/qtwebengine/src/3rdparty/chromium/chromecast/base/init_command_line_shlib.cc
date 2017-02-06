// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/init_command_line_shlib.h"

#include "base/command_line.h"
#include "base/logging.h"

namespace chromecast {


void InitCommandLineShlib(const std::vector<std::string>& argv) {
  if (base::CommandLine::InitializedForCurrentProcess())
    return;

  base::CommandLine::Init(0, nullptr);
  base::CommandLine::ForCurrentProcess()->InitFromArgv(argv);

  logging::InitLogging(logging::LoggingSettings());
}

}  // namespace chromecast

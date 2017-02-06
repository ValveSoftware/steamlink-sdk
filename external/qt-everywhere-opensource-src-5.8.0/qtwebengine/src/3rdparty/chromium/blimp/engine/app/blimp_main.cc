// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "blimp/engine/app/blimp_content_main_delegate.h"
#include "blimp/engine/app/blimp_engine_config.h"
#include "content/public/app/content_main.h"


int main(int argc, const char** argv) {
  base::CommandLine::Init(argc, argv);
  blimp::engine::SetCommandLineDefaults(base::CommandLine::ForCurrentProcess());

  blimp::engine::BlimpContentMainDelegate delegate;
  content::ContentMainParams params(&delegate);
  params.argc = argc;
  params.argv = argv;
  return content::ContentMain(params);
}

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/embedder.h"
#include "services/shell/background/background_shell_main.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

class MusGpuTestSuite : public base::TestSuite {
 public:
  MusGpuTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 private:
  void Initialize() override {
    base::TestSuite::Initialize();
#if defined(USE_OZONE)
    ui::OzonePlatform::InitializeForGPU();
#endif
  }
};

int MasterProcessMain(int argc, char** argv) {
  MusGpuTestSuite test_suite(argc, argv);
  mojo::edk::Init();
  return base::LaunchUnitTests(
      argc, argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
}

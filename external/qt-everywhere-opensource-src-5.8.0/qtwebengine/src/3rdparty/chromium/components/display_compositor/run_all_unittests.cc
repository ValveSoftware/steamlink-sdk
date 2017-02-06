// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "components/display_compositor/display_compositor_test_suite.h"

int main(int argc, char** argv) {
  display_compositor::DisplayCompositorTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv,
      base::Bind(&display_compositor::DisplayCompositorTestSuite::Run,
                 base::Unretained(&test_suite)));
}

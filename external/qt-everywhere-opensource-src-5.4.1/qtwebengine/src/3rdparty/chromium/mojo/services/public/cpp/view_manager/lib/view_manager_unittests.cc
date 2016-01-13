// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "mojo/services/public/cpp/view_manager/lib/view_manager_test_suite.h"

int main(int argc, char** argv) {
  mojo::view_manager::ViewManagerTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&TestSuite::Run, base::Unretained(&test_suite)));
}

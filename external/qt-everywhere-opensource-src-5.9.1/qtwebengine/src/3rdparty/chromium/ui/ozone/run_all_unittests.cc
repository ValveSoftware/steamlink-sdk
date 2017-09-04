// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"

namespace {

class OzoneTestSuite : public base::TestSuite {
 public:
  OzoneTestSuite(int argc, char** argv);

 protected:
  // base::TestSuite:
  void Initialize() override;
  void Shutdown() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(OzoneTestSuite);
};

OzoneTestSuite::OzoneTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

void OzoneTestSuite::Initialize() {
  base::TestSuite::Initialize();
}

void OzoneTestSuite::Shutdown() {
  base::TestSuite::Shutdown();
}

}  // namespace

int main(int argc, char** argv) {
  OzoneTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(argc,
                               argv,
                               base::Bind(&OzoneTestSuite::Run,
                                          base::Unretained(&test_suite)));
}

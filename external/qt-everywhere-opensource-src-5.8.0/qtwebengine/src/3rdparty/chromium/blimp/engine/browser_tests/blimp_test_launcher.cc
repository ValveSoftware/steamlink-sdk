// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/sys_info.h"
#include "blimp/engine/app/test_content_main_delegate.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/test_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace {

class BlimpTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  BlimpTestLauncherDelegate() {}
  ~BlimpTestLauncherDelegate() override {}

  // content::TestLauncherDelegate implementation.
  int RunTestSuite(int argc, char** argv) override {
    return base::TestSuite(argc, argv).Run();
  }

  bool AdjustChildProcessCommandLine(
      base::CommandLine* command_line,
      const base::FilePath& temp_data_dir) override {
    return true;
  }

  content::ContentMainDelegate* CreateContentMainDelegate() override {
    return new engine::TestContentMainDelegate();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpTestLauncherDelegate);
};

}  // namespace
}  // namespace blimp

int main(int argc, char** argv) {
  int default_jobs = std::max(1, base::SysInfo::NumberOfProcessors() / 2);
  blimp::BlimpTestLauncherDelegate launcher_delegate;
  return LaunchTests(&launcher_delegate, default_jobs, argc, argv);
}

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "gin/modules/console.h"
#include "gin/modules/module_registry.h"
#include "gin/modules/timer.h"
#include "gin/test/file_runner.h"
#include "gin/test/gtest.h"
#include "mojo/bindings/js/core.h"
#include "mojo/common/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace js {
namespace {

class TestRunnerDelegate : public gin::FileRunnerDelegate {
 public:
  TestRunnerDelegate() {
    AddBuiltinModule(gin::Console::kModuleName, gin::Console::GetModule);
    AddBuiltinModule(Core::kModuleName, Core::GetModule);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestRunnerDelegate);
};

void RunTest(std::string test, bool run_until_idle) {
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("mojo")
             .AppendASCII("bindings")
             .AppendASCII("js")
             .AppendASCII(test);
  TestRunnerDelegate delegate;
  gin::RunTestFromFile(path, &delegate, run_until_idle);
}

// TODO(abarth): Should we autogenerate these stubs from GYP?
TEST(JSTest, core) {
  RunTest("core_unittests.js", true);
}

TEST(JSTest, codec) {
  // TODO(yzshen): Remove this check once isolated tests are supported on the
  // Chromium waterfall. (http://crbug.com/351214)
  const base::FilePath test_file_path(
      test::GetFilePathForJSResource(
          "mojo/public/interfaces/bindings/tests/sample_service.mojom"));
  if (!base::PathExists(test_file_path)) {
    LOG(WARNING) << "Mojom binding files don't exist. Skipping the test.";
    return;
  }

  RunTest("codec_unittests.js", true);
}

}  // namespace
}  // namespace js
}  // namespace mojo

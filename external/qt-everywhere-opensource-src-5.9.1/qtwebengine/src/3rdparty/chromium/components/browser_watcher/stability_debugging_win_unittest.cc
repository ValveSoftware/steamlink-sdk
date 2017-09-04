// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/stability_debugging_win.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process/process.h"
#include "base/test/multiprocess_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace browser_watcher {

class StabilityDebuggingWinMultiProcTest : public base::MultiProcessTest {};

MULTIPROCESS_TEST_MAIN(DummyProcess) {
  return 0;
}

TEST_F(StabilityDebuggingWinMultiProcTest, GetStabilityFileForProcessTest) {
  const base::FilePath empty_path;

  // Get the path for the current process.
  base::FilePath stability_path;
  ASSERT_TRUE(GetStabilityFileForProcess(base::Process::Current(), empty_path,
                                         &stability_path));

  // Ensure requesting a second time produces the same.
  base::FilePath stability_path_two;
  ASSERT_TRUE(GetStabilityFileForProcess(base::Process::Current(), empty_path,
                                         &stability_path_two));
  EXPECT_EQ(stability_path, stability_path_two);

  // Ensure a different process has a different stability path.
  base::FilePath stability_path_other;
  ASSERT_TRUE(GetStabilityFileForProcess(SpawnChild("DummyProcess"), empty_path,
                                         &stability_path_other));
  EXPECT_NE(stability_path, stability_path_other);
}

TEST(StabilityDebuggingWinTest,
     GetStabilityFilePatternMatchesGetStabilityFileForProcessResult) {
  // GetStabilityFileForProcess file names must match GetStabilityFilePattern
  // according to
  // FileEnumerator's algorithm. We test this by writing out some files and
  // validating what is matched.

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath user_data_dir = temp_dir.path();

  // Create the stability directory.
  base::FilePath stability_dir = GetStabilityDir(user_data_dir);
  ASSERT_TRUE(base::CreateDirectory(stability_dir));

  // Write a stability file.
  base::FilePath stability_file;
  ASSERT_TRUE(GetStabilityFileForProcess(base::Process::Current(),
                                         user_data_dir, &stability_file));
  {
    base::ScopedFILE file(base::OpenFile(stability_file, "w"));
    ASSERT_TRUE(file.get());
  }

  // Write a file that shouldn't match.
  base::FilePath non_matching_file =
      stability_dir.AppendASCII("non_matching.foo");
  {
    base::ScopedFILE file(base::OpenFile(non_matching_file, "w"));
    ASSERT_TRUE(file.get());
  }

  // Validate only the stability file matches.
  base::FileEnumerator enumerator(stability_dir, false /* recursive */,
                                  base::FileEnumerator::FILES,
                                  GetStabilityFilePattern());
  ASSERT_EQ(stability_file, enumerator.Next());
  ASSERT_TRUE(enumerator.Next().empty());
}

}  // namespace browser_watcher

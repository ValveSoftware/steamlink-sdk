// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <utility>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/file_util_icu.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "net/base/directory_lister.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

class ListerDelegate : public DirectoryLister::DirectoryListerDelegate {
 public:
  ListerDelegate(bool recursive,
                 bool quit_loop_after_each_file)
      : error_(-1),
        recursive_(recursive),
        quit_loop_after_each_file_(quit_loop_after_each_file) {
  }

  virtual void OnListFile(
      const DirectoryLister::DirectoryListerData& data) OVERRIDE {
    file_list_.push_back(data.info);
    paths_.push_back(data.path);
    if (quit_loop_after_each_file_)
      base::MessageLoop::current()->Quit();
  }

  virtual void OnListDone(int error) OVERRIDE {
    error_ = error;
    base::MessageLoop::current()->Quit();
    if (recursive_)
      CheckRecursiveSort();
    else
      CheckSort();
  }

  void CheckRecursiveSort() {
    // Check that we got files in the right order.
    if (!file_list_.empty()) {
      for (size_t previous = 0, current = 1;
           current < file_list_.size();
           previous++, current++) {
        EXPECT_TRUE(file_util::LocaleAwareCompareFilenames(
            paths_[previous], paths_[current]));
      }
    }
  }

  void CheckSort() {
    // Check that we got files in the right order.
    if (!file_list_.empty()) {
      for (size_t previous = 0, current = 1;
           current < file_list_.size();
           previous++, current++) {
        // Directories should come before files.
        if (file_list_[previous].IsDirectory() &&
            !file_list_[current].IsDirectory()) {
          continue;
        }
        EXPECT_NE(FILE_PATH_LITERAL(".."),
                  file_list_[current].GetName().BaseName().value());
        EXPECT_EQ(file_list_[previous].IsDirectory(),
                  file_list_[current].IsDirectory());
        EXPECT_TRUE(file_util::LocaleAwareCompareFilenames(
            file_list_[previous].GetName(),
            file_list_[current].GetName()));
      }
    }
  }

  int error() const { return error_; }

  int num_files() const { return file_list_.size(); }

 private:
  int error_;
  bool recursive_;
  bool quit_loop_after_each_file_;
  std::vector<base::FileEnumerator::FileInfo> file_list_;
  std::vector<base::FilePath> paths_;
};

class DirectoryListerTest : public PlatformTest {
 public:

  virtual void SetUp() OVERRIDE {
    const int kMaxDepth = 3;
    const int kBranchingFactor = 4;
    const int kFilesPerDirectory = 5;

    // Randomly create a directory structure of depth 3 in a temporary root
    // directory.
    std::list<std::pair<base::FilePath, int> > directories;
    ASSERT_TRUE(temp_root_dir_.CreateUniqueTempDir());
    directories.push_back(std::make_pair(temp_root_dir_.path(), 0));
    while (!directories.empty()) {
      std::pair<base::FilePath, int> dir_data = directories.front();
      directories.pop_front();
      for (int i = 0; i < kFilesPerDirectory; i++) {
        std::string file_name = base::StringPrintf("file_id_%d", i);
        base::FilePath file_path = dir_data.first.AppendASCII(file_name);
        base::File file(file_path,
                        base::File::FLAG_CREATE | base::File::FLAG_WRITE);
        ASSERT_TRUE(file.IsValid());
      }
      if (dir_data.second < kMaxDepth - 1) {
        for (int i = 0; i < kBranchingFactor; i++) {
          std::string dir_name = base::StringPrintf("child_dir_%d", i);
          base::FilePath dir_path = dir_data.first.AppendASCII(dir_name);
          ASSERT_TRUE(base::CreateDirectory(dir_path));
          directories.push_back(std::make_pair(dir_path, dir_data.second + 1));
        }
      }
    }
    PlatformTest::SetUp();
  }

  const base::FilePath& root_path() const {
    return temp_root_dir_.path();
  }

 private:
  base::ScopedTempDir temp_root_dir_;
};

TEST_F(DirectoryListerTest, BigDirTest) {
  ListerDelegate delegate(false, false);
  DirectoryLister lister(root_path(), &delegate);
  lister.Start();

  base::MessageLoop::current()->Run();

  EXPECT_EQ(OK, delegate.error());
}

TEST_F(DirectoryListerTest, BigDirRecursiveTest) {
  ListerDelegate delegate(true, false);
  DirectoryLister lister(root_path(), true, DirectoryLister::FULL_PATH,
                         &delegate);
  lister.Start();

  base::MessageLoop::current()->Run();

  EXPECT_EQ(OK, delegate.error());
}

TEST_F(DirectoryListerTest, CancelTest) {
  ListerDelegate delegate(false, true);
  DirectoryLister lister(root_path(), &delegate);
  lister.Start();

  base::MessageLoop::current()->Run();

  int num_files = delegate.num_files();

  lister.Cancel();

  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(num_files, delegate.num_files());
}

TEST_F(DirectoryListerTest, EmptyDirTest) {
  base::ScopedTempDir tempDir;
  EXPECT_TRUE(tempDir.CreateUniqueTempDir());

  bool kRecursive = false;
  bool kQuitLoopAfterEachFile = false;
  ListerDelegate delegate(kRecursive, kQuitLoopAfterEachFile);
  DirectoryLister lister(tempDir.path(), &delegate);
  lister.Start();

  base::MessageLoop::current()->Run();

  // Contains only the parent directory ("..")
  EXPECT_EQ(1, delegate.num_files());
  EXPECT_EQ(OK, delegate.error());
}

}  // namespace net

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"

#include "base/bind.h"
#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void Increment(int* value) {
  (*value)++;
}

TEST(BindHelpersTest, TestScopedClosureRunnerExitScope) {
  int run_count = 0;
  {
    base::ScopedClosureRunner runner(base::Bind(&Increment, &run_count));
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(1, run_count);
}

TEST(BindHelpersTest, TestScopedClosureRunnerRelease) {
  int run_count = 0;
  base::Closure c;
  {
    base::ScopedClosureRunner runner(base::Bind(&Increment, &run_count));
    c = runner.Release();
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(0, run_count);
  c.Run();
  EXPECT_EQ(1, run_count);
}

TEST(BindHelpersTest, TestScopedClosureRunnerReset) {
  int run_count_1 = 0;
  int run_count_2 = 0;
  {
    base::ScopedClosureRunner runner;
    runner.Reset(base::Bind(&Increment, &run_count_1));
    runner.Reset(base::Bind(&Increment, &run_count_2));
    EXPECT_EQ(1, run_count_1);
    EXPECT_EQ(0, run_count_2);
  }
  EXPECT_EQ(1, run_count_2);

  int run_count_3 = 0;
  {
    base::ScopedClosureRunner runner(base::Bind(&Increment, &run_count_3));
    EXPECT_EQ(0, run_count_3);
    runner.Reset();
    EXPECT_EQ(1, run_count_3);
  }
  EXPECT_EQ(1, run_count_3);
}

TEST(BindHelpersTest, TestScopedClosureRunnerMoveConstructor) {
  int run_count = 0;
  {
    std::unique_ptr<base::ScopedClosureRunner> runner(
        new base::ScopedClosureRunner(base::Bind(&Increment, &run_count)));
    base::ScopedClosureRunner runner2(std::move(*runner));
    runner.reset();
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(1, run_count);
}

TEST(BindHelpersTest, TestScopedClosureRunnerMoveAssignment) {
  int run_count = 0;
  {
    base::ScopedClosureRunner runner;
    {
      base::ScopedClosureRunner runner2(base::Bind(&Increment, &run_count));
      runner = std::move(runner2);
    }
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(1, run_count);
}

TEST(BindHelpersTest, TestScopedClosureRunnerRunOnReplace) {
  int run_count1 = 0;
  int run_count2 = 0;
  {
    base::ScopedClosureRunner runner1(base::Bind(&Increment, &run_count1));
    {
      base::ScopedClosureRunner runner2(base::Bind(&Increment, &run_count2));
      runner1 = std::move(runner2);
      EXPECT_EQ(1, run_count1);
      EXPECT_EQ(0, run_count2);
    }
    EXPECT_EQ(1, run_count1);
    EXPECT_EQ(0, run_count2);
  }
  EXPECT_EQ(1, run_count1);
  EXPECT_EQ(1, run_count2);
}

}  // namespace

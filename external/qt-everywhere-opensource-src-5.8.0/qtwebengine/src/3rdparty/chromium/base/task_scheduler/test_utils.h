// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_TEST_UTILS_H_
#define BASE_TASK_SCHEDULER_TEST_UTILS_H_

#include "base/logging.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

// Death tests misbehave on Android.
#if DCHECK_IS_ON() && defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)
#define EXPECT_DCHECK_DEATH(statement, regex) EXPECT_DEATH(statement, regex)
#else
#define EXPECT_DCHECK_DEATH(statement, regex)
#endif

#endif  // BASE_TASK_SCHEDULER_TEST_UTILS_H_

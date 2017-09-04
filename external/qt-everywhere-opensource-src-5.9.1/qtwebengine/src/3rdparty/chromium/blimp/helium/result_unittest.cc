// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/result.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace helium {
namespace {

class ResultTest : public testing::Test {
 public:
  ResultTest() {}
  ~ResultTest() override {}
};

TEST_F(ResultTest, ResultToStringWorks) {
  // The exhaustive list of errors need not be specified here, but enough are
  // specified that we can verify that the switch/case mapping works as
  // intended.
  EXPECT_STREQ("SUCCESS", ResultToString(Result::SUCCESS));
  EXPECT_STREQ("ERR_DISCONNECTED", ResultToString(Result::ERR_DISCONNECTED));
}

}  // namespace
}  // namespace helium
}  // namespace blimp

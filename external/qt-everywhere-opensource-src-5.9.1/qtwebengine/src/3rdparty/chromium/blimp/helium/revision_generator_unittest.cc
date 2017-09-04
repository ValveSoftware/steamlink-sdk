// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "blimp/helium/helium_test.h"
#include "blimp/helium/revision_generator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace helium {
namespace {

class RevisionGeneratorTest : public HeliumTest {};

TEST_F(RevisionGeneratorTest, CheckCurrentDoesntIncrease) {
  RevisionGenerator gen;
  EXPECT_EQ(0UL, gen.current());
}

TEST_F(RevisionGeneratorTest, MonotonicallyIncreasing) {
  RevisionGenerator gen;
  EXPECT_EQ(1UL, gen.GetNextRevision());
  EXPECT_EQ(2UL, gen.GetNextRevision());
  EXPECT_EQ(2UL, gen.current());
}

TEST_F(RevisionGeneratorTest, GetNextRevisionCall) {
  EXPECT_EQ(1UL, GetNextRevision());
  EXPECT_EQ(2UL, GetNextRevision());
  EXPECT_EQ(2UL, RevisionGenerator::GetInstance()->current());
}

}  // namespace
}  // namespace helium
}  // namespace blimp

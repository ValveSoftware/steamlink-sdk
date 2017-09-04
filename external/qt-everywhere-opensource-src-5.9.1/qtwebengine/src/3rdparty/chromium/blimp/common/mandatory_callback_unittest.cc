// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/mandatory_callback.h"

#include <memory>

#include "base/bind.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace {

class MandatoryCallbackTest : public testing::Test {
 public:
  class ResultCollector {
   public:
    MOCK_METHOD2(MarkInvokedTwoArgs, void(int, float));
    MOCK_METHOD1(MarkInvoked, void(int));
  };

  MandatoryCallbackTest() {}
  ~MandatoryCallbackTest() override {}

 protected:
  testing::StrictMock<ResultCollector> result_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MandatoryCallbackTest);
};

#if !DCHECK_IS_ON()

TEST_F(MandatoryCallbackTest, NoEffectForReleaseBuilds) {
  auto mandatory_cb = CreateMandatoryCallback(base::Bind(&base::DoNothing));

  // Test should not crash when |mandatory_cb| is leaked.
}

#else

TEST_F(MandatoryCallbackTest, PassesWhenDestroyedAfterRun) {
  EXPECT_CALL(result_, MarkInvoked(3));
  base::Callback<void(int)> cb =
      base::Bind(&MandatoryCallbackTest::ResultCollector::MarkInvoked,
                 base::Unretained(&result_));
  MandatoryCallback<void(int)> mandatory_cb = CreateMandatoryCallback(cb);
  mandatory_cb.Run(3);
}

TEST_F(MandatoryCallbackTest, FailsWhenDestroyedBeforeRun) {
  base::Callback<void(int)> cb =
      base::Bind(&MandatoryCallbackTest::ResultCollector::MarkInvoked,
                 base::Unretained(&result_));
  ASSERT_DCHECK_DEATH(auto mandatory_cb = CreateMandatoryCallback(cb));
}

TEST_F(MandatoryCallbackTest, FailsOnMultipleInvocations) {
  EXPECT_CALL(result_, MarkInvoked(3));
  base::Callback<void(int)> cb =
      base::Bind(&MandatoryCallbackTest::ResultCollector::MarkInvoked,
                 base::Unretained(&result_));
  auto mandatory_cb = CreateMandatoryCallback(cb);
  mandatory_cb.Run(3);
  EXPECT_DCHECK_DEATH(mandatory_cb.Run(3));
}

TEST_F(MandatoryCallbackTest, FailsOnMoveThenMultipleCall) {
  EXPECT_CALL(result_, MarkInvoked(3));
  base::Callback<void(int)> cb =
      base::Bind(&MandatoryCallbackTest::ResultCollector::MarkInvoked,
                 base::Unretained(&result_));
  auto mandatory_cb = CreateMandatoryCallback(cb);
  auto moved = std::move(mandatory_cb);
  moved.Run(3);
  EXPECT_DCHECK_DEATH(mandatory_cb.Run(3));
}

// Allows death checks to copy-and-discard as a one-liner statement,
// which is necessary for isolating the moved variable inside the forked death
// check process.
template <typename CallbackType>
void MoveAndToss(CallbackType&& cb) {
  auto copy = std::move(cb);
}

TEST_F(MandatoryCallbackTest, FailsOnMoveThenDrop) {
  base::Callback<void(int)> cb =
      base::Bind(&MandatoryCallbackTest::ResultCollector::MarkInvoked,
                 base::Unretained(&result_));
  ASSERT_DCHECK_DEATH(MoveAndToss(CreateMandatoryCallback(cb)));
}

#endif

}  // namespace
}  // namespace blimp

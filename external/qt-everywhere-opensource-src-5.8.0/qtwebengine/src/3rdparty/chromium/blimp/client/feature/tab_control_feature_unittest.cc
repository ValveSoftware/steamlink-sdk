// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/tab_control_feature.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/tab_control.pb.h"
#include "blimp/net/test_common.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

using testing::_;

namespace blimp {
namespace client {

MATCHER_P3(EqualsSizeMessage, width, height, dp_to_px, "") {
  return arg.tab_control().tab_control_case() == TabControlMessage::kSize &&
         arg.tab_control().size().width() == width &&
         arg.tab_control().size().height() == height &&
         arg.tab_control().size().device_pixel_ratio() == dp_to_px;
}

class TabControlFeatureTest : public testing::Test {
 public:
  TabControlFeatureTest() : out_processor_(nullptr) {}

  void SetUp() override {
    out_processor_ = new MockBlimpMessageProcessor();
    feature_.set_outgoing_message_processor(base::WrapUnique(out_processor_));
  }

 protected:
  // This is a raw pointer to a class that is owned by the ControlFeature.
  MockBlimpMessageProcessor* out_processor_;

  TabControlFeature feature_;
};

TEST_F(TabControlFeatureTest, CreatesCorrectSizeMessage) {
  uint64_t width = 10;
  uint64_t height = 15;
  float dp_to_px = 1.23f;

  EXPECT_CALL(
      *out_processor_,
      MockableProcessMessage(EqualsSizeMessage(width, height, dp_to_px), _))
      .Times(1);
  feature_.SetSizeAndScale(gfx::Size(width, height), dp_to_px);
}

TEST_F(TabControlFeatureTest, NoDuplicateSizeMessage) {
  uint64_t width = 10;
  uint64_t height = 15;
  float dp_to_px = 1.23f;

  EXPECT_CALL(
      *out_processor_,
      MockableProcessMessage(EqualsSizeMessage(width, height, dp_to_px), _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(
      *out_processor_,
      MockableProcessMessage(EqualsSizeMessage(width, height, dp_to_px + 1), _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*out_processor_,
              MockableProcessMessage(
                  EqualsSizeMessage(width + 1, height, dp_to_px + 1), _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*out_processor_,
              MockableProcessMessage(
                  EqualsSizeMessage(width + 1, height + 1, dp_to_px + 1), _))
      .Times(1)
      .RetiresOnSaturation();

  feature_.SetSizeAndScale(gfx::Size(width, height), dp_to_px);
  feature_.SetSizeAndScale(gfx::Size(width, height), dp_to_px);
  feature_.SetSizeAndScale(gfx::Size(width, height), dp_to_px + 1);
  feature_.SetSizeAndScale(gfx::Size(width + 1, height), dp_to_px + 1);
  feature_.SetSizeAndScale(gfx::Size(width + 1, height + 1), dp_to_px + 1);
}

}  // namespace client
}  // namespace blimp

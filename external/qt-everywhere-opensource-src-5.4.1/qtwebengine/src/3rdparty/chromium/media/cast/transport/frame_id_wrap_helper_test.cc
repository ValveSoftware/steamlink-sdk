// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include "media/cast/transport/cast_transport_defines.h"

namespace media {
namespace cast {
namespace transport {

class FrameIdWrapHelperTest : public ::testing::Test {
 protected:
  FrameIdWrapHelperTest() {}
  virtual ~FrameIdWrapHelperTest() {}

  FrameIdWrapHelper frame_id_wrap_helper_;

  DISALLOW_COPY_AND_ASSIGN(FrameIdWrapHelperTest);
};

TEST_F(FrameIdWrapHelperTest, FirstFrame) {
  EXPECT_EQ(kStartFrameId, frame_id_wrap_helper_.MapTo32bitsFrameId(255u));
}

TEST_F(FrameIdWrapHelperTest, Rollover) {
  uint32 new_frame_id = 0u;
  for (int i = 0; i <= 256; ++i) {
    new_frame_id =
        frame_id_wrap_helper_.MapTo32bitsFrameId(static_cast<uint8>(i));
  }
  EXPECT_EQ(256u, new_frame_id);
}

TEST_F(FrameIdWrapHelperTest, OutOfOrder) {
  uint32 new_frame_id = 0u;
  for (int i = 0; i < 255; ++i) {
    new_frame_id =
        frame_id_wrap_helper_.MapTo32bitsFrameId(static_cast<uint8>(i));
  }
  EXPECT_EQ(254u, new_frame_id);
  new_frame_id = frame_id_wrap_helper_.MapTo32bitsFrameId(0u);
  EXPECT_EQ(256u, new_frame_id);
  new_frame_id = frame_id_wrap_helper_.MapTo32bitsFrameId(255u);
  EXPECT_EQ(255u, new_frame_id);
  new_frame_id = frame_id_wrap_helper_.MapTo32bitsFrameId(1u);
  EXPECT_EQ(257u, new_frame_id);
}

}  // namespace transport
}  // namespace cast
}  // namespace media

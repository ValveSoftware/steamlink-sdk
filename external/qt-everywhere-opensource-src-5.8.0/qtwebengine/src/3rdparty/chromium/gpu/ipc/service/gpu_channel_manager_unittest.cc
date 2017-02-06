// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"

namespace gpu {

class GpuChannelManagerTest : public GpuChannelTestCommon {
 public:
  GpuChannelManagerTest() : GpuChannelTestCommon() {}
  ~GpuChannelManagerTest() override {}
};

TEST_F(GpuChannelManagerTest, EstablishChannel) {
  int32_t kClientId = 1;
  uint64_t kClientTracingId = 1;

  ASSERT_TRUE(channel_manager());

  IPC::ChannelHandle channel_handle = channel_manager()->EstablishChannel(
      kClientId, kClientTracingId, false /* preempts */,
      false /* allow_view_command_buffers */,
      false /* allow_real_time_streams */);
  EXPECT_NE("", channel_handle.name);

  GpuChannel* channel = channel_manager()->LookupChannel(kClientId);
  ASSERT_TRUE(channel);
  EXPECT_EQ(channel_handle.name, channel->channel_id());
}

}  // namespace gpu

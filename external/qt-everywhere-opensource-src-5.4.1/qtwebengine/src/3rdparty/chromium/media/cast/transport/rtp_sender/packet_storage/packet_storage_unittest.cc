// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/rtp_sender/packet_storage/packet_storage.h"

#include <stdint.h>

#include <vector>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {
namespace transport {

static size_t kStoredFrames = 10;

// Generate |number_of_frames| and store into |*storage|.
// First frame has 1 packet, second frame has 2 packets, etc.
static void StoreFrames(size_t number_of_frames,
                        uint32 first_frame_id,
                        PacketStorage* storage) {
  const base::TimeTicks kTicks;
  const int kSsrc = 1;
  for (size_t i = 0; i < number_of_frames; ++i) {
    SendPacketVector packets;
    // First frame has 1 packet, second frame has 2 packets, etc.
    const size_t kNumberOfPackets = i + 1;
    for (size_t j = 0; j < kNumberOfPackets; ++j) {
      Packet test_packet(1, 0);
      packets.push_back(
          std::make_pair(
              PacedPacketSender::MakePacketKey(kTicks, kSsrc, j),
              new base::RefCountedData<Packet>(test_packet)));
    }
    storage->StoreFrame(first_frame_id, packets);
    ++first_frame_id;
  }
}

TEST(PacketStorageTest, NumberOfStoredFrames) {
  PacketStorage storage(kStoredFrames);

  uint32 frame_id = 0;
  frame_id = ~frame_id; // The maximum value of uint32.
  StoreFrames(200, frame_id, &storage);
  EXPECT_EQ(kStoredFrames, storage.GetNumberOfStoredFrames());
}

TEST(PacketStorageTest, GetFrameWrapAround8bits) {
  PacketStorage storage(kStoredFrames);

  const uint32 kFirstFrameId = 250;
  StoreFrames(kStoredFrames, kFirstFrameId, &storage);
  EXPECT_EQ(kStoredFrames, storage.GetNumberOfStoredFrames());

  // Expect we get the correct frames by looking at the number of
  // packets.
  uint32 frame_id = kFirstFrameId;
  for (size_t i = 0; i < kStoredFrames; ++i) {
    ASSERT_TRUE(storage.GetFrame8(frame_id));
    EXPECT_EQ(i + 1, storage.GetFrame8(frame_id)->size());
    ++frame_id;
  }
}

TEST(PacketStorageTest, GetFrameWrapAround32bits) {
  PacketStorage storage(kStoredFrames);

  // First frame ID is close to the maximum value of uint32.
  uint32 first_frame_id = 0xffffffff - 5;
  StoreFrames(kStoredFrames, first_frame_id, &storage);
  EXPECT_EQ(kStoredFrames, storage.GetNumberOfStoredFrames());

  // Expect we get the correct frames by looking at the number of
  // packets.
  uint32 frame_id = first_frame_id;
  for (size_t i = 0; i < kStoredFrames; ++i) {
    ASSERT_TRUE(storage.GetFrame8(frame_id));
    EXPECT_EQ(i + 1, storage.GetFrame8(frame_id)->size());
    ++frame_id;
  }
}

TEST(PacketStorageTest, GetFrameTooOld) {
  PacketStorage storage(kStoredFrames);

  // First frame ID is close to the maximum value of uint32.
  uint32 first_frame_id = 0xffffffff - 5;

  // Store two times the capacity.
  StoreFrames(2 * kStoredFrames, first_frame_id, &storage);
  EXPECT_EQ(kStoredFrames, storage.GetNumberOfStoredFrames());

  uint32 frame_id = first_frame_id;
  // Old frames are evicted.
  for (size_t i = 0; i < kStoredFrames; ++i) {
    EXPECT_FALSE(storage.GetFrame8(frame_id));
    ++frame_id;
  }
  // Check recent frames are there.
  for (size_t i = 0; i < kStoredFrames; ++i) {
    ASSERT_TRUE(storage.GetFrame8(frame_id));
    EXPECT_EQ(kStoredFrames + i + 1,
              storage.GetFrame8(frame_id)->size());
    ++frame_id;
  }
}

}  // namespace transport
}  // namespace cast
}  // namespace media

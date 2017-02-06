// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "media/base/android/access_unit_queue.h"
#include "testing/gtest/include/gtest/gtest.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

namespace media {

class AccessUnitQueueTest : public testing::Test {
 public:
  AccessUnitQueueTest() {}
  ~AccessUnitQueueTest() override {}

 protected:
  enum UnitType { kNormal = 0, kKeyFrame, kEOS, kConfig };
  struct AUDescriptor {
    UnitType unit_type;
    std::string data;
  };

  DemuxerData CreateDemuxerData(const AUDescriptor* descr, int descr_length);
};

DemuxerData AccessUnitQueueTest::CreateDemuxerData(const AUDescriptor* descr,
                                                   int descr_length) {
  DemuxerData result;
  result.type = DemuxerStream::AUDIO;  // assign a valid type

  for (int i = 0; i < descr_length; ++i) {
    result.access_units.push_back(AccessUnit());
    AccessUnit& au = result.access_units.back();

    if (descr[i].unit_type == kConfig) {
      au.status = DemuxerStream::kConfigChanged;
      result.demuxer_configs.push_back(DemuxerConfigs());
      // ignore data
      continue;
    }

    au.status = DemuxerStream::kOk;

    if (descr[i].unit_type == kEOS) {
      au.is_end_of_stream = true;
      // ignore data
      continue;
    }

    au.data = std::vector<uint8_t>(descr[i].data.begin(), descr[i].data.end());

    if (descr[i].unit_type == kKeyFrame)
      au.is_key_frame = true;
  }
  return result;
}

#define VERIFY_FIRST_BYTE(expected, info)          \
  do {                                             \
    EXPECT_NE(nullptr, info.front_unit);           \
    EXPECT_TRUE(info.front_unit->data.size() > 0); \
    EXPECT_EQ(expected, info.front_unit->data[0]); \
  } while (0)

TEST_F(AccessUnitQueueTest, InitializedEmpty) {
  AccessUnitQueue au_queue;
  AccessUnitQueue::Info info = au_queue.GetInfo();

  EXPECT_EQ(0, info.length);
  EXPECT_FALSE(info.has_eos);
  EXPECT_EQ(nullptr, info.front_unit);
  EXPECT_EQ(nullptr, info.configs);
}

TEST_F(AccessUnitQueueTest, RewindToLastKeyFrameEmptyQueue) {
  AccessUnitQueue au_queue;
  EXPECT_FALSE(au_queue.RewindToLastKeyFrame());
}

TEST_F(AccessUnitQueueTest, PushAndAdvance) {
  AUDescriptor chunk1[] = {{kNormal, "0"},
                           {kNormal, "1"},
                           {kNormal, "2"},
                           {kNormal, "3"},
                           {kNormal, "4"},
                           {kNormal, "5"}};
  AUDescriptor chunk2[] = {{kNormal, "6"},
                           {kNormal, "7"},
                           {kNormal, "8"}};

  int total_size = ARRAY_SIZE(chunk1) + ARRAY_SIZE(chunk2);

  AccessUnitQueue au_queue;
  au_queue.PushBack(CreateDemuxerData(chunk1, ARRAY_SIZE(chunk1)));
  au_queue.PushBack(CreateDemuxerData(chunk2, ARRAY_SIZE(chunk2)));

  AccessUnitQueue::Info info;
  for (int i = 0; i < total_size; ++i) {
    info = au_queue.GetInfo();

    EXPECT_FALSE(info.has_eos);
    EXPECT_EQ(total_size - i, info.length);
    EXPECT_EQ(nullptr, info.configs);

    ASSERT_NE(nullptr, info.front_unit);
    EXPECT_TRUE(info.front_unit->data.size() > 0);
    EXPECT_EQ('0' + i, info.front_unit->data[0]);

    au_queue.Advance();
  }

  // After we advanced past the last AU, GetInfo() should report starvation.
  info = au_queue.GetInfo();

  EXPECT_EQ(0, info.length);
  EXPECT_FALSE(info.has_eos);
  EXPECT_EQ(nullptr, info.front_unit);
  EXPECT_EQ(nullptr, info.configs);
}

TEST_F(AccessUnitQueueTest, ChunksDoNotLeak) {
  AUDescriptor chunk[] = {
      {kNormal, "0"}, {kNormal, "1"}, {kNormal, "2"}, {kNormal, "3"}};

  AccessUnitQueue au_queue;

  // Verify that the old chunks get deleted (we rely on NumChunksForTesting()).
  // First, run the loop with default history size, which is zero chunks.
  for (size_t i = 0; i < 100; ++i) {
    au_queue.PushBack(CreateDemuxerData(chunk, ARRAY_SIZE(chunk)));
    for (size_t j = 0; j < ARRAY_SIZE(chunk); ++j)
      au_queue.Advance();

    EXPECT_EQ(0U, au_queue.NumChunksForTesting());
  }

  // Change the history size and run again.
  au_queue.SetHistorySizeForTesting(5);

  for (size_t i = 0; i < 100; ++i) {
    au_queue.PushBack(CreateDemuxerData(chunk, ARRAY_SIZE(chunk)));
    for (size_t j = 0; j < ARRAY_SIZE(chunk); ++j)
      au_queue.Advance();

    if (i < 4)
      EXPECT_EQ(i + 1, au_queue.NumChunksForTesting());
    else
      EXPECT_EQ(5U, au_queue.NumChunksForTesting());
  }
}

TEST_F(AccessUnitQueueTest, PushAfterStarvation) {
  // Two chunks
  AUDescriptor chunk[][4] = {
      {{kNormal, "0"}, {kNormal, "1"}, {kNormal, "2"}, {kNormal, "3"}},
      {{kNormal, "4"}, {kNormal, "5"}, {kNormal, "6"}, {kNormal, "7"}}};

  AccessUnitQueue au_queue;

  // Push the first chunk.
  au_queue.PushBack(CreateDemuxerData(chunk[0], ARRAY_SIZE(chunk[0])));

  // Advance past the end of queue.
  for (size_t i = 0; i < ARRAY_SIZE(chunk[0]); ++i)
    au_queue.Advance();

  // An extra Advance() should not change anything.
  au_queue.Advance();

  // Push the second chunk
  au_queue.PushBack(CreateDemuxerData(chunk[1], ARRAY_SIZE(chunk[1])));

  // Verify that we get the next access unit.
  AccessUnitQueue::Info info = au_queue.GetInfo();
  VERIFY_FIRST_BYTE('4', info);
}

TEST_F(AccessUnitQueueTest, HasEOS) {
  // Two chunks
  AUDescriptor chunk[][4] = {
      {{kNormal, "0"}, {kNormal, "1"}, {kNormal, "2"}, {kNormal, "3"}},
      {{kNormal, "4"}, {kNormal, "5"}, {kNormal, "6"}, {kEOS, "7"}}};

  AccessUnitQueue au_queue;
  au_queue.PushBack(CreateDemuxerData(chunk[0], ARRAY_SIZE(chunk[0])));
  au_queue.PushBack(CreateDemuxerData(chunk[1], ARRAY_SIZE(chunk[1])));

  // Verify that after EOS has been pushed into the queue,
  // it is reported for every GetInfo()
  for (int i = 0; i < 8; ++i) {
    AccessUnitQueue::Info info = au_queue.GetInfo();

    EXPECT_TRUE(info.has_eos);
    EXPECT_EQ(nullptr, info.configs);

    if (i == 7)
      EXPECT_TRUE(info.front_unit->is_end_of_stream);
    else
      VERIFY_FIRST_BYTE('0' + i, info);

    au_queue.Advance();
  }
}

TEST_F(AccessUnitQueueTest, HasConfigs) {
  AUDescriptor chunk[] = {
      {kNormal, "0"}, {kNormal, "1"}, {kNormal, "2"}, {kConfig, "3"}};

  AccessUnitQueue au_queue;
  au_queue.PushBack(CreateDemuxerData(chunk, ARRAY_SIZE(chunk)));

  for (int i = 0; i < 4; ++i) {
    AccessUnitQueue::Info info = au_queue.GetInfo();

    if (i != 3)
      EXPECT_EQ(nullptr, info.configs);
    else
      EXPECT_NE(nullptr, info.configs);

    au_queue.Advance();
  }
}

TEST_F(AccessUnitQueueTest, ConfigsAndKeyFrame) {
  // Two chunks
  AUDescriptor chunk[][4] = {
      {{kNormal, "0"}, {kKeyFrame, "1"}, {kNormal, "2"}, {kConfig, "3"}},
      {{kKeyFrame, "4"}, {kNormal, "5"}, {kNormal, "6"}, {kNormal, "7"}}};

  AccessUnitQueue::Info info;

  AccessUnitQueue au_queue;
  au_queue.PushBack(CreateDemuxerData(chunk[0], ARRAY_SIZE(chunk[0])));
  au_queue.PushBack(CreateDemuxerData(chunk[1], ARRAY_SIZE(chunk[1])));

  // There is no prior key frame
  EXPECT_FALSE(au_queue.RewindToLastKeyFrame());

  // Consume first access unit.
  au_queue.Advance();

  // Now the current one is the key frame. It would be safe to configure codec
  // at this moment, so RewindToLastKeyFrame() should return true.
  EXPECT_TRUE(au_queue.RewindToLastKeyFrame());

  info = au_queue.GetInfo();
  VERIFY_FIRST_BYTE('1', info);

  au_queue.Advance();  // now current unit is "2"

  info = au_queue.GetInfo();
  VERIFY_FIRST_BYTE('2', info);

  EXPECT_TRUE(au_queue.RewindToLastKeyFrame());  // should go back to "1"

  info = au_queue.GetInfo();
  VERIFY_FIRST_BYTE('1', info);

  au_queue.Advance();  // now current unit is "2"
  au_queue.Advance();  // now current unit is "3"

  // Verify that we are at "3".
  info = au_queue.GetInfo();
  EXPECT_NE(nullptr, info.configs);

  // Although it would be safe to configure codec (with old config) in this
  // position since it will be immediately reconfigured from the next unit "3",
  // current implementation returns unit "1".

  EXPECT_TRUE(au_queue.RewindToLastKeyFrame());  // should go back to "1"

  info = au_queue.GetInfo();
  VERIFY_FIRST_BYTE('1', info);

  au_queue.Advance();  // now current unit is "2"
  au_queue.Advance();  // now current unit is "3"
  au_queue.Advance();  // now current unit is "4"

  info = au_queue.GetInfo();
  VERIFY_FIRST_BYTE('4', info);

  EXPECT_TRUE(au_queue.RewindToLastKeyFrame());  // should stay at "4"

  info = au_queue.GetInfo();
  VERIFY_FIRST_BYTE('4', info);

  au_queue.Advance();  // now current unit is "5"
  au_queue.Advance();  // now current unit is "6"

  info = au_queue.GetInfo();
  VERIFY_FIRST_BYTE('6', info);

  EXPECT_TRUE(au_queue.RewindToLastKeyFrame());  // should go back to "4"

  info = au_queue.GetInfo();
  VERIFY_FIRST_BYTE('4', info);
}

TEST_F(AccessUnitQueueTest, KeyFrameWithLongHistory) {
  // Four chunks
  AUDescriptor chunk[][4] = {
      {{kNormal, "0"}, {kKeyFrame, "1"}, {kNormal, "2"}, {kNormal, "3"}},
      {{kNormal, "4"}, {kNormal, "5"}, {kNormal, "6"}, {kNormal, "7"}},
      {{kNormal, "8"}, {kNormal, "9"}, {kNormal, "a"}, {kNormal, "b"}},
      {{kNormal, "c"}, {kNormal, "d"}, {kKeyFrame, "e"}, {kNormal, "f"}}};

  AccessUnitQueue::Info info;

  AccessUnitQueue au_queue;
  for (int i = 0; i < 4; ++i)
    au_queue.PushBack(CreateDemuxerData(chunk[i], ARRAY_SIZE(chunk[i])));

  au_queue.SetHistorySizeForTesting(3);

  // Advance to '3'.
  for (int i = 0; i < 3; ++i)
    au_queue.Advance();

  info = au_queue.GetInfo();
  VERIFY_FIRST_BYTE('3', info);

  // Rewind to key frame, the current unit should be '1'.
  EXPECT_TRUE(au_queue.RewindToLastKeyFrame());
  info = au_queue.GetInfo();
  VERIFY_FIRST_BYTE('1', info);

  // Advance to 'c'.
  for (int i = 0; i < 11; ++i)
    au_queue.Advance();

  info = au_queue.GetInfo();
  VERIFY_FIRST_BYTE('c', info);

  // Rewind to key frame, the current unit should be '1' again.
  EXPECT_TRUE(au_queue.RewindToLastKeyFrame());
  info = au_queue.GetInfo();
  VERIFY_FIRST_BYTE('1', info);

  // Set history size to 0 (default)
  au_queue.SetHistorySizeForTesting(0);

  // Advance to 'd'. Should erase all chunks except the last.
  for (int i = 0; i < 12; ++i)
    au_queue.Advance();

  info = au_queue.GetInfo();
  VERIFY_FIRST_BYTE('d', info);

  // Rewind should not find any key frames.
  EXPECT_FALSE(au_queue.RewindToLastKeyFrame());

  au_queue.Advance();  // Advance to key frame 'e'.
  info = au_queue.GetInfo();
  VERIFY_FIRST_BYTE('e', info);

  // Rewind should find the same unit 'e.
  EXPECT_TRUE(au_queue.RewindToLastKeyFrame());
  info = au_queue.GetInfo();
  VERIFY_FIRST_BYTE('e', info);
}

}  // namespace media

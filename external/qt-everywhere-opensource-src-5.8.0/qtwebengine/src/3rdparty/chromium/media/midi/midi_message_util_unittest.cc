// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_message_util.h"

#include <stdint.h>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace midi {
namespace {

const uint8_t kGMOn[] = {0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7};
const uint8_t kNoteOn[] = {0x90, 0x3c, 0x7f};
const uint8_t kSystemCommonMessageReserved1[] = {0xf4};
const uint8_t kSystemCommonMessageReserved2[] = {0xf5};
const uint8_t kSystemCommonMessageTuneRequest[] = {0xf6};
const uint8_t kChannelPressure[] = {0xd0, 0x01};
const uint8_t kTimingClock[] = {0xf8};

TEST(MidiMessageUtilTest, GetMidiMessageLength) {
  // Check basic functionarity
  EXPECT_EQ(arraysize(kNoteOn), GetMidiMessageLength(kNoteOn[0]));
  EXPECT_EQ(arraysize(kChannelPressure),
            GetMidiMessageLength(kChannelPressure[0]));
  EXPECT_EQ(arraysize(kTimingClock), GetMidiMessageLength(kTimingClock[0]));
  EXPECT_EQ(arraysize(kSystemCommonMessageTuneRequest),
            GetMidiMessageLength(kSystemCommonMessageTuneRequest[0]));

  // SysEx message should be mapped to 0-length
  EXPECT_EQ(0u, GetMidiMessageLength(kGMOn[0]));

  // Any reserved message should be mapped to 0-length
  EXPECT_EQ(0u, GetMidiMessageLength(kSystemCommonMessageReserved1[0]));
  EXPECT_EQ(0u, GetMidiMessageLength(kSystemCommonMessageReserved2[0]));

  // Any data byte should be mapped to 0-length
  EXPECT_EQ(0u, GetMidiMessageLength(kGMOn[1]));
  EXPECT_EQ(0u, GetMidiMessageLength(kNoteOn[1]));
  EXPECT_EQ(0u, GetMidiMessageLength(kChannelPressure[1]));
}

}  // namespace
}  // namespace midi
}  // namespace media

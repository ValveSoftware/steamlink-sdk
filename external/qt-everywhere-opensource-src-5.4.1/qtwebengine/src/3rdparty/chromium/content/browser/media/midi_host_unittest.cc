// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/midi_host.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const uint8 kGMOn[] = { 0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7 };
const uint8 kGSOn[] = {
  0xf0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7f, 0x00, 0x41, 0xf7,
};
const uint8 kNoteOn[] = { 0x90, 0x3c, 0x7f };
const uint8 kNoteOnWithRunningStatus[] = {
  0x90, 0x3c, 0x7f, 0x3c, 0x7f, 0x3c, 0x7f,
};
const uint8 kChannelPressure[] = { 0xd0, 0x01 };
const uint8 kChannelPressureWithRunningStatus[] = {
  0xd0, 0x01, 0x01, 0x01,
};
const uint8 kTimingClock[] = { 0xf8 };
const uint8 kBrokenData1[] = { 0x90 };
const uint8 kBrokenData2[] = { 0xf7 };
const uint8 kBrokenData3[] = { 0xf2, 0x00 };
const uint8 kDataByte0[] = { 0x00 };

template <typename T, size_t N>
const std::vector<T> AsVector(const T(&data)[N]) {
  std::vector<T> buffer;
  buffer.insert(buffer.end(), data, data + N);
  return buffer;
}

template <typename T, size_t N>
void PushToVector(const T(&data)[N], std::vector<T>* buffer) {
  buffer->insert(buffer->end(), data, data + N);
}

}  // namespace

TEST(MidiHostTest, IsValidWebMIDIData) {
  // Test single event scenario
  EXPECT_TRUE(MidiHost::IsValidWebMIDIData(AsVector(kGMOn)));
  EXPECT_TRUE(MidiHost::IsValidWebMIDIData(AsVector(kGSOn)));
  EXPECT_TRUE(MidiHost::IsValidWebMIDIData(AsVector(kNoteOn)));
  EXPECT_TRUE(MidiHost::IsValidWebMIDIData(AsVector(kChannelPressure)));
  EXPECT_TRUE(MidiHost::IsValidWebMIDIData(AsVector(kTimingClock)));
  EXPECT_FALSE(MidiHost::IsValidWebMIDIData(AsVector(kBrokenData1)));
  EXPECT_FALSE(MidiHost::IsValidWebMIDIData(AsVector(kBrokenData2)));
  EXPECT_FALSE(MidiHost::IsValidWebMIDIData(AsVector(kBrokenData3)));
  EXPECT_FALSE(MidiHost::IsValidWebMIDIData(AsVector(kDataByte0)));

  // MIDI running status should be disallowed
  EXPECT_FALSE(MidiHost::IsValidWebMIDIData(
      AsVector(kNoteOnWithRunningStatus)));
  EXPECT_FALSE(MidiHost::IsValidWebMIDIData(
      AsVector(kChannelPressureWithRunningStatus)));

  // Multiple messages are allowed as long as each of them is complete.
  {
    std::vector<uint8> buffer;
    PushToVector(kGMOn, &buffer);
    PushToVector(kNoteOn, &buffer);
    PushToVector(kGSOn, &buffer);
    PushToVector(kTimingClock, &buffer);
    PushToVector(kNoteOn, &buffer);
    EXPECT_TRUE(MidiHost::IsValidWebMIDIData(buffer));
    PushToVector(kBrokenData1, &buffer);
    EXPECT_FALSE(MidiHost::IsValidWebMIDIData(buffer));
  }

  // MIDI realtime message can be placed at any position.
  {
    const uint8 kNoteOnWithRealTimeClock[] = {
      0x90, 0xf8, 0x3c, 0x7f, 0x90, 0xf8, 0x3c, 0xf8, 0x7f, 0xf8,
    };
    EXPECT_TRUE(MidiHost::IsValidWebMIDIData(
        AsVector(kNoteOnWithRealTimeClock)));

    const uint8 kGMOnWithRealTimeClock[] = {
      0xf0, 0xf8, 0x7e, 0x7f, 0x09, 0x01, 0xf8, 0xf7,
    };
    EXPECT_TRUE(MidiHost::IsValidWebMIDIData(
        AsVector(kGMOnWithRealTimeClock)));
  }
}

}  // namespace conent

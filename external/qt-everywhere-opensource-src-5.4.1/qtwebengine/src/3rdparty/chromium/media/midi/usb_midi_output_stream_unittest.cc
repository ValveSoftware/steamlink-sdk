// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/usb_midi_output_stream.h"

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "media/midi/usb_midi_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

template<typename T, size_t N>
std::vector<T> ToVector(const T((&array)[N])) {
  return std::vector<T>(array, array + N);
}

class MockUsbMidiDevice : public UsbMidiDevice {
 public:
  MockUsbMidiDevice() {}
  virtual ~MockUsbMidiDevice() {}

  virtual std::vector<uint8> GetDescriptor() OVERRIDE {
    return std::vector<uint8>();
  }

  virtual void Send(int endpoint_number, const std::vector<uint8>& data)
      OVERRIDE {
    for (size_t i = 0; i < data.size(); ++i) {
      log_ += base::StringPrintf("0x%02x ", data[i]);
    }
    log_ += base::StringPrintf("(endpoint = %d)\n", endpoint_number);
  }

  const std::string& log() const { return log_; }

  void ClearLog() { log_ = ""; }

 private:
  std::string log_;

  DISALLOW_COPY_AND_ASSIGN(MockUsbMidiDevice);
};

class UsbMidiOutputStreamTest : public ::testing::Test {
 protected:
  UsbMidiOutputStreamTest() {
    UsbMidiJack jack(&device_, 1, 2, 4);
    stream_.reset(new UsbMidiOutputStream(jack));
  }

  MockUsbMidiDevice device_;
  scoped_ptr<UsbMidiOutputStream> stream_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UsbMidiOutputStreamTest);
};

TEST_F(UsbMidiOutputStreamTest, SendEmpty) {
  stream_->Send(std::vector<uint8>());

  EXPECT_EQ("", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendNoteOn) {
  uint8 data[] = { 0x90, 0x45, 0x7f};

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x29 0x90 0x45 0x7f (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendNoteOnPending) {
  stream_->Send(std::vector<uint8>(1, 0x90));
  stream_->Send(std::vector<uint8>(1, 0x45));
  EXPECT_EQ("", device_.log());

  stream_->Send(std::vector<uint8>(1, 0x7f));
  EXPECT_EQ("0x29 0x90 0x45 0x7f (endpoint = 4)\n", device_.log());
  device_.ClearLog();

  stream_->Send(std::vector<uint8>(1, 0x90));
  stream_->Send(std::vector<uint8>(1, 0x45));
  EXPECT_EQ("", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendNoteOnBurst) {
  uint8 data1[] = { 0x90, };
  uint8 data2[] = { 0x45, 0x7f, 0x90, 0x45, 0x71, 0x90, 0x45, 0x72, 0x90, };

  stream_->Send(ToVector(data1));
  stream_->Send(ToVector(data2));
  EXPECT_EQ("0x29 0x90 0x45 0x7f "
            "0x29 0x90 0x45 0x71 "
            "0x29 0x90 0x45 0x72 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendNoteOff) {
  uint8 data[] = { 0x80, 0x33, 0x44, };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x28 0x80 0x33 0x44 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendPolyphonicKeyPress) {
  uint8 data[] = { 0xa0, 0x33, 0x44, };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x2a 0xa0 0x33 0x44 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendControlChange) {
  uint8 data[] = { 0xb7, 0x33, 0x44, };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x2b 0xb7 0x33 0x44 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendProgramChange) {
  uint8 data[] = { 0xc2, 0x33, };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x2c 0xc2 0x33 0x00 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendChannelPressure) {
  uint8 data[] = { 0xd1, 0x33, 0x44, };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x2d 0xd1 0x33 0x44 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendPitchWheelChange) {
  uint8 data[] = { 0xe4, 0x33, 0x44, };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x2e 0xe4 0x33 0x44 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendTwoByteSysEx) {
  uint8 data[] = { 0xf0, 0xf7, };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x26 0xf0 0xf7 0x00 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendThreeByteSysEx) {
  uint8 data[] = { 0xf0, 0x4f, 0xf7, };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x27 0xf0 0x4f 0xf7 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendFourByteSysEx) {
  uint8 data[] = { 0xf0, 0x00, 0x01, 0xf7, };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x24 0xf0 0x00 0x01 "
            "0x25 0xf7 0x00 0x00 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendFiveByteSysEx) {
  uint8 data[] = { 0xf0, 0x00, 0x01, 0x02, 0xf7, };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x24 0xf0 0x00 0x01 "
            "0x26 0x02 0xf7 0x00 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendSixByteSysEx) {
  uint8 data[] = { 0xf0, 0x00, 0x01, 0x02, 0x03, 0xf7, };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x24 0xf0 0x00 0x01 "
            "0x27 0x02 0x03 0xf7 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendPendingSysEx) {
  uint8 data1[] = { 0xf0, 0x33, };
  uint8 data2[] = { 0x44, 0x55, 0x66, };
  uint8 data3[] = { 0x77, 0x88, 0x99, 0xf7, };

  stream_->Send(ToVector(data1));
  EXPECT_EQ("", device_.log());

  stream_->Send(ToVector(data2));
  EXPECT_EQ("0x24 0xf0 0x33 0x44 (endpoint = 4)\n", device_.log());
  device_.ClearLog();

  stream_->Send(ToVector(data3));
  EXPECT_EQ("0x24 0x55 0x66 0x77 0x27 0x88 0x99 0xf7 (endpoint = 4)\n",
            device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendNoteOnAfterSysEx) {
  uint8 data[] = { 0xf0, 0x00, 0x01, 0x02, 0x03, 0xf7, 0x90, 0x44, 0x33, };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x24 0xf0 0x00 0x01 "
            "0x27 0x02 0x03 0xf7 "
            "0x29 0x90 0x44 0x33 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendTimeCodeQuarterFrame) {
  uint8 data[] = { 0xf1, 0x22, };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x22 0xf1 0x22 0x00 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendSongPositionPointer) {
  uint8 data[] = { 0xf2, 0x22, 0x33, };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x23 0xf2 0x22 0x33 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendSongSelect) {
  uint8 data[] = { 0xf3, 0x22, };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x22 0xf3 0x22 0x00 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, TuneRequest) {
  uint8 data[] = { 0xf6, };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x25 0xf6 0x00 0x00 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendSongPositionPointerPending) {
  uint8 data1[] = { 0xf2, 0x22, };
  uint8 data2[] = { 0x33, };

  stream_->Send(ToVector(data1));
  EXPECT_EQ("", device_.log());

  stream_->Send(ToVector(data2));
  EXPECT_EQ("0x23 0xf2 0x22 0x33 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendRealTimeMessages) {
  uint8 data[] = { 0xf8, 0xfa, 0xfb, 0xfc, 0xfe, 0xff, };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x25 0xf8 0x00 0x00 "
            "0x25 0xfa 0x00 0x00 "
            "0x25 0xfb 0x00 0x00 "
            "0x25 0xfc 0x00 0x00 "
            "0x25 0xfe 0x00 0x00 "
            "0x25 0xff 0x00 0x00 (endpoint = 4)\n", device_.log());
}

TEST_F(UsbMidiOutputStreamTest, SendRealTimeInSysExMessage) {
  uint8 data[] = {
    0xf0, 0x00, 0x01, 0x02,
    0xf8, 0xfa,
    0x03, 0xf7,
  };

  stream_->Send(ToVector(data));
  EXPECT_EQ("0x24 0xf0 0x00 0x01 "
            "0x25 0xf8 0x00 0x00 "
            "0x25 0xfa 0x00 0x00 "
            "0x27 0x02 0x03 0xf7 (endpoint = 4)\n", device_.log());
}

}  // namespace

}  // namespace media

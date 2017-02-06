// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/midi_host.h"

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "content/common/media/midi_messages.h"
#include "content/public/test/test_browser_thread.h"
#include "media/midi/midi_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const uint8_t kGMOn[] = {0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7};
const uint8_t kGSOn[] = {
    0xf0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7f, 0x00, 0x41, 0xf7,
};
const uint8_t kNoteOn[] = {0x90, 0x3c, 0x7f};
const uint8_t kNoteOnWithRunningStatus[] = {
    0x90, 0x3c, 0x7f, 0x3c, 0x7f, 0x3c, 0x7f,
};
const uint8_t kChannelPressure[] = {0xd0, 0x01};
const uint8_t kChannelPressureWithRunningStatus[] = {
    0xd0, 0x01, 0x01, 0x01,
};
const uint8_t kTimingClock[] = {0xf8};
const uint8_t kBrokenData1[] = {0x90};
const uint8_t kBrokenData2[] = {0xf7};
const uint8_t kBrokenData3[] = {0xf2, 0x00};
const uint8_t kDataByte0[] = {0x00};

const int kRenderProcessId = 0;

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

enum MidiEventType {
  DISPATCH_SEND_MIDI_DATA,
};

struct MidiEvent {
  MidiEvent(MidiEventType in_type,
            uint32_t in_port_index,
            const std::vector<uint8_t>& in_data,
            double in_timestamp)
      : type(in_type),
        port_index(in_port_index),
        data(in_data),
        timestamp(in_timestamp) {}

  MidiEventType type;
  uint32_t port_index;
  std::vector<uint8_t> data;
  double timestamp;
};

class FakeMidiManager : public media::midi::MidiManager {
 public:
  void DispatchSendMidiData(media::midi::MidiManagerClient* client,
                            uint32_t port_index,
                            const std::vector<uint8_t>& data,
                            double timestamp) override {
    events_.push_back(MidiEvent(DISPATCH_SEND_MIDI_DATA,
                                port_index,
                                data,
                                timestamp));
  }
  std::vector<MidiEvent> events_;
};

class MidiHostForTesting : public MidiHost {
 public:
  MidiHostForTesting(int renderer_process_id,
                     media::midi::MidiManager* midi_manager)
      : MidiHost(renderer_process_id, midi_manager) {}

 private:
  ~MidiHostForTesting() override {}

  // BrowserMessageFilter implementation.
  // Override ShutdownForBadMessage() to do nothing since the original
  // implementation to kill a malicious renderer process causes a check failure
  // in unit tests.
  void ShutdownForBadMessage() override {}
};

class MidiHostTest : public testing::Test {
 public:
  MidiHostTest()
      : io_browser_thread_(BrowserThread::IO, &message_loop_),
        host_(new MidiHostForTesting(kRenderProcessId, &manager_)),
        data_(kNoteOn, kNoteOn + arraysize(kNoteOn)),
        port_id_(0) {}
  ~MidiHostTest() override {
    manager_.Shutdown();
    RunLoopUntilIdle();
  }

 protected:
  void AddOutputPort() {
    const std::string id = base::StringPrintf("i-can-%d", port_id_++);
    const std::string manufacturer("yukatan");
    const std::string name("doki-doki-pi-pine");
    const std::string version("3.14159265359");
    media::midi::MidiPortState state = media::midi::MIDI_PORT_CONNECTED;
    media::midi::MidiPortInfo info(id, manufacturer, name, version, state);

    host_->AddOutputPort(info);
  }

  void OnSendData(uint32_t port) {
    std::unique_ptr<IPC::Message> message(
        new MidiHostMsg_SendData(port, data_, 0.0));
    host_->OnMessageReceived(*message.get());
  }

  size_t GetEventSize() const {
    return manager_.events_.size();
  }

  void CheckSendEventAt(size_t at, uint32_t port) {
    EXPECT_EQ(DISPATCH_SEND_MIDI_DATA, manager_.events_[at].type);
    EXPECT_EQ(port, manager_.events_[at].port_index);
    EXPECT_EQ(data_, manager_.events_[at].data);
    EXPECT_EQ(0.0, manager_.events_[at].timestamp);
  }

  void RunLoopUntilIdle() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

 private:
  base::MessageLoop message_loop_;
  TestBrowserThread io_browser_thread_;

  FakeMidiManager manager_;
  scoped_refptr<MidiHostForTesting> host_;
  std::vector<uint8_t> data_;
  int32_t port_id_;

  DISALLOW_COPY_AND_ASSIGN(MidiHostTest);
};

}  // namespace

TEST_F(MidiHostTest, IsValidWebMIDIData) {
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
    std::vector<uint8_t> buffer;
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
    const uint8_t kNoteOnWithRealTimeClock[] = {
        0x90, 0xf8, 0x3c, 0x7f, 0x90, 0xf8, 0x3c, 0xf8, 0x7f, 0xf8,
    };
    EXPECT_TRUE(MidiHost::IsValidWebMIDIData(
        AsVector(kNoteOnWithRealTimeClock)));

    const uint8_t kGMOnWithRealTimeClock[] = {
        0xf0, 0xf8, 0x7e, 0x7f, 0x09, 0x01, 0xf8, 0xf7,
    };
    EXPECT_TRUE(MidiHost::IsValidWebMIDIData(
        AsVector(kGMOnWithRealTimeClock)));
  }
}

// Test if sending data to out of range port is ignored.
TEST_F(MidiHostTest, OutputPortCheck) {
  // Only one output port is available.
  AddOutputPort();

  // Sending data to port 0 should be delivered.
  uint32_t port0 = 0;
  OnSendData(port0);
  RunLoopUntilIdle();
  EXPECT_EQ(1U, GetEventSize());
  CheckSendEventAt(0, port0);

  // Sending data to port 1 should not be delivered.
  uint32_t port1 = 1;
  OnSendData(port1);
  RunLoopUntilIdle();
  EXPECT_EQ(1U, GetEventSize());

  // Two output ports are available from now on.
  AddOutputPort();

  // Sending data to port 0 and 1 should be delivered now.
  OnSendData(port0);
  OnSendData(port1);
  RunLoopUntilIdle();
  EXPECT_EQ(3U, GetEventSize());
  CheckSendEventAt(1, port0);
  CheckSendEventAt(2, port1);
}

}  // namespace conent

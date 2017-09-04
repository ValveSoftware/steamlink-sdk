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

using midi::mojom::PortState;

const uint8_t kNoteOn[] = {0x90, 0x3c, 0x7f};
const int kRenderProcessId = 0;

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

class FakeMidiManager : public midi::MidiManager {
 public:
  void DispatchSendMidiData(midi::MidiManagerClient* client,
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
                     midi::MidiManager* midi_manager)
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
    PortState state = PortState::CONNECTED;
    midi::MidiPortInfo info(id, manufacturer, name, version, state);

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

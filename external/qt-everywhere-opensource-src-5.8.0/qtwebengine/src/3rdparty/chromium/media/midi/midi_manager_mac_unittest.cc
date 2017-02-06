// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_mac.h"

#include <CoreMIDI/MIDIServices.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace midi {

namespace {

void Noop(const MIDIPacketList*, void*, void*) {}

class FakeMidiManagerClient : public MidiManagerClient {
 public:
  FakeMidiManagerClient()
      : result_(Result::NOT_SUPPORTED),
        wait_for_result_(true),
        wait_for_port_(true),
        unexpected_callback_(false) {}

  // MidiManagerClient implementation.
  void AddInputPort(const MidiPortInfo& info) override {}
  void AddOutputPort(const MidiPortInfo& info) override {
    base::AutoLock lock(lock_);
    // AddOutputPort may be called before CompleteStartSession() is invoked
    // if one or more MIDI devices including virtual ports are connected.
    // Just ignore in such cases.
    if (wait_for_result_)
      return;

    // Check if this is the first call after CompleteStartSession(), and
    // the case should not happen twice.
    if (!wait_for_port_)
      unexpected_callback_ = true;

    info_ = info;
    wait_for_port_ = false;
  }
  void SetInputPortState(uint32_t port_index, MidiPortState state) override {}
  void SetOutputPortState(uint32_t port_index, MidiPortState state) override {}

  void CompleteStartSession(Result result) override {
    base::AutoLock lock(lock_);
    if (!wait_for_result_)
      unexpected_callback_ = true;

    result_ = result;
    wait_for_result_ = false;
  }

  void ReceiveMidiData(uint32_t port_index,
                       const uint8_t* data,
                       size_t size,
                       double timestamp) override {}
  void AccumulateMidiBytesSent(size_t size) override {}
  void Detach() override {}

  bool GetWaitForResult() {
    base::AutoLock lock(lock_);
    return wait_for_result_;
  }

  bool GetWaitForPort() {
    base::AutoLock lock(lock_);
    return wait_for_port_;
  }

  Result WaitForResult() {
    while (GetWaitForResult()) {
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
    EXPECT_FALSE(unexpected_callback_);
    return result_;
  }
  MidiPortInfo WaitForPort() {
    while (GetWaitForPort()) {
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
    EXPECT_FALSE(unexpected_callback_);
    return info_;
  }

 private:
  base::Lock lock_;
  Result result_;
  bool wait_for_result_;
  MidiPortInfo info_;
  bool wait_for_port_;
  bool unexpected_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeMidiManagerClient);
};

class MidiManagerMacTest : public ::testing::Test {
 public:
  MidiManagerMacTest()
      : manager_(new MidiManagerMac),
        message_loop_(new base::MessageLoop) {}
  ~MidiManagerMacTest() override {
    manager_->Shutdown();
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

 protected:
  void StartSession(MidiManagerClient* client) {
    manager_->StartSession(client);
  }
  void EndSession(MidiManagerClient* client) {
    manager_->EndSession(client);
  }

 private:
  std::unique_ptr<MidiManager> manager_;
  std::unique_ptr<base::MessageLoop> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MidiManagerMacTest);
};


TEST_F(MidiManagerMacTest, MidiNotification) {
  std::unique_ptr<FakeMidiManagerClient> client(new FakeMidiManagerClient);
  StartSession(client.get());

  Result result = client->WaitForResult();
  EXPECT_EQ(Result::OK, result);

  // Create MIDIClient, and MIDIEndpoint as a MIDIDestination. This should
  // notify MIDIManagerMac as a MIDIObjectAddRemoveNotification.
  MIDIClientRef midi_client = 0;
  OSStatus status = MIDIClientCreate(
      CFSTR("MidiManagerMacTest"), nullptr, nullptr, &midi_client);
  EXPECT_EQ(noErr, status);

  MIDIEndpointRef ep = 0;
  status = MIDIDestinationCreate(
      midi_client, CFSTR("DestinationTest"), Noop, nullptr, &ep);
  EXPECT_EQ(noErr, status);
  SInt32 id;
  status = MIDIObjectGetIntegerProperty(ep, kMIDIPropertyUniqueID, &id);
  EXPECT_EQ(noErr, status);
  EXPECT_NE(0, id);

  // Wait until the created device is notified to MidiManagerMac.
  MidiPortInfo info = client->WaitForPort();
  EXPECT_EQ("DestinationTest", info.name);

  EndSession(client.get());
  if (ep)
    MIDIEndpointDispose(ep);
  if (midi_client)
    MIDIClientDispose(midi_client);
}

}  // namespace

}  // namespace midi
}  // namespace media

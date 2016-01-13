// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager.h"

#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

class FakeMidiManager : public MidiManager {
 public:
  FakeMidiManager() : start_initialization_is_called_(false) {}
  virtual ~FakeMidiManager() {}

  // MidiManager implementation.
  virtual void StartInitialization() OVERRIDE {
    start_initialization_is_called_ = true;
  }

  virtual void DispatchSendMidiData(MidiManagerClient* client,
                                    uint32 port_index,
                                    const std::vector<uint8>& data,
                                    double timestamp) OVERRIDE {}

  // Utility functions for testing.
  void CallCompleteInitialization(MidiResult result) {
    CompleteInitialization(result);
  }

  size_t GetClientCount() const {
    return clients_size_for_testing();
  }

  size_t GetPendingClientCount() const {
    return pending_clients_size_for_testing();
  }

  bool start_initialization_is_called_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeMidiManager);
};

class FakeMidiManagerClient : public MidiManagerClient {
 public:
  explicit FakeMidiManagerClient(int client_id)
      : client_id_(client_id),
        result_(MIDI_NOT_SUPPORTED),
        wait_for_result_(true) {}
  virtual ~FakeMidiManagerClient() {}

  // MidiManagerClient implementation.
  virtual void CompleteStartSession(int client_id, MidiResult result) OVERRIDE {
    EXPECT_TRUE(wait_for_result_);
    CHECK_EQ(client_id_, client_id);
    result_ = result;
    wait_for_result_ = false;
  }

  virtual void ReceiveMidiData(uint32 port_index, const uint8* data,
                               size_t size, double timestamp) OVERRIDE {}
  virtual void AccumulateMidiBytesSent(size_t size) OVERRIDE {}

  int client_id() const { return client_id_; }
  MidiResult result() const { return result_; }

  MidiResult WaitForResult() {
    base::RunLoop run_loop;
    // CompleteStartSession() is called inside the message loop on the same
    // thread. Protection for |wait_for_result_| is not needed.
    while (wait_for_result_)
      run_loop.RunUntilIdle();
    return result();
  }

 private:
  int client_id_;
  MidiResult result_;
  bool wait_for_result_;

  DISALLOW_COPY_AND_ASSIGN(FakeMidiManagerClient);
};

class MidiManagerTest : public ::testing::Test {
 public:
  MidiManagerTest()
      : manager_(new FakeMidiManager),
        message_loop_(new base::MessageLoop) {}
  virtual ~MidiManagerTest() {}

 protected:
  void StartTheFirstSession(FakeMidiManagerClient* client) {
    EXPECT_FALSE(manager_->start_initialization_is_called_);
    EXPECT_EQ(0U, manager_->GetClientCount());
    EXPECT_EQ(0U, manager_->GetPendingClientCount());
    manager_->StartSession(client, client->client_id());
    EXPECT_EQ(0U, manager_->GetClientCount());
    EXPECT_EQ(1U, manager_->GetPendingClientCount());
    EXPECT_TRUE(manager_->start_initialization_is_called_);
    EXPECT_EQ(0U, manager_->GetClientCount());
    EXPECT_EQ(1U, manager_->GetPendingClientCount());
    EXPECT_TRUE(manager_->start_initialization_is_called_);
  }

  void StartTheNthSession(FakeMidiManagerClient* client, size_t nth) {
    EXPECT_EQ(nth != 1, manager_->start_initialization_is_called_);
    EXPECT_EQ(0U, manager_->GetClientCount());
    EXPECT_EQ(nth - 1, manager_->GetPendingClientCount());

    // StartInitialization() should not be called for the second and later
    // sessions.
    manager_->start_initialization_is_called_ = false;
    manager_->StartSession(client, client->client_id());
    EXPECT_EQ(nth == 1, manager_->start_initialization_is_called_);
    manager_->start_initialization_is_called_ = true;
  }

  void EndSession(FakeMidiManagerClient* client, size_t before, size_t after) {
    EXPECT_EQ(before, manager_->GetClientCount());
    manager_->EndSession(client);
    EXPECT_EQ(after, manager_->GetClientCount());
  }

  void CompleteInitialization(MidiResult result) {
    manager_->CallCompleteInitialization(result);
  }

  void RunLoopUntilIdle() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

 protected:
  scoped_ptr<FakeMidiManager> manager_;

 private:
  scoped_ptr<base::MessageLoop> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MidiManagerTest);
};

TEST_F(MidiManagerTest, StartAndEndSession) {
  scoped_ptr<FakeMidiManagerClient> client;
  client.reset(new FakeMidiManagerClient(0));

  StartTheFirstSession(client.get());
  CompleteInitialization(MIDI_OK);
  EXPECT_EQ(MIDI_OK, client->WaitForResult());
  EndSession(client.get(), 1U, 0U);
}

TEST_F(MidiManagerTest, StartAndEndSessionWithError) {
  scoped_ptr<FakeMidiManagerClient> client;
  client.reset(new FakeMidiManagerClient(1));

  StartTheFirstSession(client.get());
  CompleteInitialization(MIDI_INITIALIZATION_ERROR);
  EXPECT_EQ(MIDI_INITIALIZATION_ERROR, client->WaitForResult());
  EndSession(client.get(), 0U, 0U);
}

TEST_F(MidiManagerTest, StartMultipleSessions) {
  scoped_ptr<FakeMidiManagerClient> client1;
  scoped_ptr<FakeMidiManagerClient> client2;
  scoped_ptr<FakeMidiManagerClient> client3;
  client1.reset(new FakeMidiManagerClient(0));
  client2.reset(new FakeMidiManagerClient(1));
  client3.reset(new FakeMidiManagerClient(1));

  StartTheFirstSession(client1.get());
  StartTheNthSession(client2.get(), 2);
  StartTheNthSession(client3.get(), 3);
  CompleteInitialization(MIDI_OK);
  EXPECT_EQ(MIDI_OK, client1->WaitForResult());
  EXPECT_EQ(MIDI_OK, client2->WaitForResult());
  EXPECT_EQ(MIDI_OK, client3->WaitForResult());
  EndSession(client1.get(), 3U, 2U);
  EndSession(client2.get(), 2U, 1U);
  EndSession(client3.get(), 1U, 0U);
}

// TODO(toyoshim): Add a test for a MidiManagerClient that has multiple
// sessions with multiple client_id.

TEST_F(MidiManagerTest, TooManyPendingSessions) {
  // Push as many client requests for starting session as possible.
  ScopedVector<FakeMidiManagerClient> many_existing_clients;
  many_existing_clients.resize(MidiManager::kMaxPendingClientCount);
  for (size_t i = 0; i < MidiManager::kMaxPendingClientCount; ++i) {
    many_existing_clients[i] = new FakeMidiManagerClient(i);
    StartTheNthSession(many_existing_clients[i], i + 1);
  }

  // Push the last client that should be rejected for too many pending requests.
  scoped_ptr<FakeMidiManagerClient> additional_client(
      new FakeMidiManagerClient(MidiManager::kMaxPendingClientCount));
  manager_->start_initialization_is_called_ = false;
  manager_->StartSession(additional_client.get(),
                         additional_client->client_id());
  EXPECT_FALSE(manager_->start_initialization_is_called_);
  EXPECT_EQ(MIDI_INITIALIZATION_ERROR, additional_client->result());

  // Other clients still should not receive a result.
  RunLoopUntilIdle();
  for (size_t i = 0; i < many_existing_clients.size(); ++i)
    EXPECT_EQ(MIDI_NOT_SUPPORTED, many_existing_clients[i]->result());

  // The result MIDI_OK should be distributed to other clients.
  CompleteInitialization(MIDI_OK);
  for (size_t i = 0; i < many_existing_clients.size(); ++i)
    EXPECT_EQ(MIDI_OK, many_existing_clients[i]->WaitForResult());

  // Close all successful sessions in FIFO order.
  size_t sessions = many_existing_clients.size();
  for (size_t i = 0; i < many_existing_clients.size(); ++i, --sessions)
    EndSession(many_existing_clients[i], sessions, sessions - 1);
}

TEST_F(MidiManagerTest, AbortSession) {
  // A client starting a session can be destructed while an asynchronous
  // initialization is performed.
  scoped_ptr<FakeMidiManagerClient> client;
  client.reset(new FakeMidiManagerClient(0));

  StartTheFirstSession(client.get());
  EndSession(client.get(), 0, 0);
  client.reset();

  // Following function should not call the destructed |client| function.
  CompleteInitialization(MIDI_OK);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

TEST_F(MidiManagerTest, CreateMidiManager) {
  scoped_ptr<FakeMidiManagerClient> client;
  client.reset(new FakeMidiManagerClient(0));

  scoped_ptr<MidiManager> manager(MidiManager::Create());
  manager->StartSession(client.get(), client->client_id());

  MidiResult result = client->WaitForResult();
  // This #ifdef needs to be identical to the one in media/midi/midi_manager.cc.
  // Do not change the condition for disabling this test.
#if !defined(OS_MACOSX) && !defined(OS_WIN) && !defined(USE_ALSA) && \
    !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  EXPECT_EQ(MIDI_NOT_SUPPORTED, result);
#elif defined(USE_ALSA)
  // Temporary until http://crbug.com/371230 is resolved.
  EXPECT_TRUE((result == MIDI_OK) || (result == MIDI_INITIALIZATION_ERROR));
#else
  EXPECT_EQ(MIDI_OK, result);
#endif
}

}  // namespace

}  // namespace media

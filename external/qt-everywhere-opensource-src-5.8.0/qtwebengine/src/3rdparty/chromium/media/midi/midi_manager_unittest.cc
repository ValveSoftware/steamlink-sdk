// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/system_monitor/system_monitor.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace midi {

namespace {

class FakeMidiManager : public MidiManager {
 public:
  FakeMidiManager()
      : start_initialization_is_called_(false), finalize_is_called_(false) {}
  ~FakeMidiManager() override {}

  // MidiManager implementation.
  void StartInitialization() override {
    start_initialization_is_called_ = true;
  }

  void Finalize() override { finalize_is_called_ = true; }

  void DispatchSendMidiData(MidiManagerClient* client,
                            uint32_t port_index,
                            const std::vector<uint8_t>& data,
                            double timestamp) override {}

  // Utility functions for testing.
  void CallCompleteInitialization(Result result) {
    CompleteInitialization(result);
  }

  size_t GetClientCount() const {
    return clients_size_for_testing();
  }

  size_t GetPendingClientCount() const {
    return pending_clients_size_for_testing();
  }

  bool start_initialization_is_called_;
  bool finalize_is_called_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeMidiManager);
};

class FakeMidiManagerClient : public MidiManagerClient {
 public:
  FakeMidiManagerClient()
      : result_(Result::NOT_SUPPORTED), wait_for_result_(true) {}
  ~FakeMidiManagerClient() override {}

  // MidiManagerClient implementation.
  void AddInputPort(const MidiPortInfo& info) override {}
  void AddOutputPort(const MidiPortInfo& info) override {}
  void SetInputPortState(uint32_t port_index, MidiPortState state) override {}
  void SetOutputPortState(uint32_t port_index, MidiPortState state) override {}

  void CompleteStartSession(Result result) override {
    EXPECT_TRUE(wait_for_result_);
    result_ = result;
    wait_for_result_ = false;
  }

  void ReceiveMidiData(uint32_t port_index,
                       const uint8_t* data,
                       size_t size,
                       double timestamp) override {}
  void AccumulateMidiBytesSent(size_t size) override {}
  void Detach() override {}

  Result result() const { return result_; }

  Result WaitForResult() {
    while (wait_for_result_) {
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
    return result();
  }

 private:
  Result result_;
  bool wait_for_result_;

  DISALLOW_COPY_AND_ASSIGN(FakeMidiManagerClient);
};

class MidiManagerTest : public ::testing::Test {
 public:
  MidiManagerTest()
      : manager_(new FakeMidiManager),
        message_loop_(new base::MessageLoop) {}
  ~MidiManagerTest() override {
    manager_->Shutdown();
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
    EXPECT_EQ(manager_->start_initialization_is_called_,
              manager_->finalize_is_called_);
  }

 protected:
  void StartTheFirstSession(FakeMidiManagerClient* client) {
    EXPECT_FALSE(manager_->start_initialization_is_called_);
    EXPECT_EQ(0U, manager_->GetClientCount());
    EXPECT_EQ(0U, manager_->GetPendingClientCount());
    manager_->StartSession(client);
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
    manager_->StartSession(client);
    EXPECT_EQ(nth == 1, manager_->start_initialization_is_called_);
    manager_->start_initialization_is_called_ = true;
  }

  void EndSession(FakeMidiManagerClient* client, size_t before, size_t after) {
    EXPECT_EQ(before, manager_->GetClientCount());
    manager_->EndSession(client);
    EXPECT_EQ(after, manager_->GetClientCount());
  }

  void CompleteInitialization(Result result) {
    manager_->CallCompleteInitialization(result);
  }

  void RunLoopUntilIdle() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

 protected:
  std::unique_ptr<FakeMidiManager> manager_;

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MidiManagerTest);
};

TEST_F(MidiManagerTest, StartAndEndSession) {
  std::unique_ptr<FakeMidiManagerClient> client;
  client.reset(new FakeMidiManagerClient);

  StartTheFirstSession(client.get());
  CompleteInitialization(Result::OK);
  EXPECT_EQ(Result::OK, client->WaitForResult());
  EndSession(client.get(), 1U, 0U);
}

TEST_F(MidiManagerTest, StartAndEndSessionWithError) {
  std::unique_ptr<FakeMidiManagerClient> client;
  client.reset(new FakeMidiManagerClient);

  StartTheFirstSession(client.get());
  CompleteInitialization(Result::INITIALIZATION_ERROR);
  EXPECT_EQ(Result::INITIALIZATION_ERROR, client->WaitForResult());
  EndSession(client.get(), 0U, 0U);
}

TEST_F(MidiManagerTest, StartMultipleSessions) {
  std::unique_ptr<FakeMidiManagerClient> client1;
  std::unique_ptr<FakeMidiManagerClient> client2;
  std::unique_ptr<FakeMidiManagerClient> client3;
  client1.reset(new FakeMidiManagerClient);
  client2.reset(new FakeMidiManagerClient);
  client3.reset(new FakeMidiManagerClient);

  StartTheFirstSession(client1.get());
  StartTheNthSession(client2.get(), 2);
  StartTheNthSession(client3.get(), 3);
  CompleteInitialization(Result::OK);
  EXPECT_EQ(Result::OK, client1->WaitForResult());
  EXPECT_EQ(Result::OK, client2->WaitForResult());
  EXPECT_EQ(Result::OK, client3->WaitForResult());
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
    many_existing_clients[i] = new FakeMidiManagerClient;
    StartTheNthSession(many_existing_clients[i], i + 1);
  }
  EXPECT_TRUE(manager_->start_initialization_is_called_);

  // Push the last client that should be rejected for too many pending requests.
  std::unique_ptr<FakeMidiManagerClient> additional_client(
      new FakeMidiManagerClient);
  manager_->start_initialization_is_called_ = false;
  manager_->StartSession(additional_client.get());
  EXPECT_FALSE(manager_->start_initialization_is_called_);
  manager_->start_initialization_is_called_ = true;
  EXPECT_EQ(Result::INITIALIZATION_ERROR, additional_client->result());

  // Other clients still should not receive a result.
  RunLoopUntilIdle();
  for (size_t i = 0; i < many_existing_clients.size(); ++i)
    EXPECT_EQ(Result::NOT_SUPPORTED, many_existing_clients[i]->result());

  // The Result::OK should be distributed to other clients.
  CompleteInitialization(Result::OK);
  for (size_t i = 0; i < many_existing_clients.size(); ++i)
    EXPECT_EQ(Result::OK, many_existing_clients[i]->WaitForResult());

  // Close all successful sessions in FIFO order.
  size_t sessions = many_existing_clients.size();
  for (size_t i = 0; i < many_existing_clients.size(); ++i, --sessions)
    EndSession(many_existing_clients[i], sessions, sessions - 1);
}

TEST_F(MidiManagerTest, AbortSession) {
  // A client starting a session can be destructed while an asynchronous
  // initialization is performed.
  std::unique_ptr<FakeMidiManagerClient> client;
  client.reset(new FakeMidiManagerClient);

  StartTheFirstSession(client.get());
  EndSession(client.get(), 0, 0);
  client.reset();

  // Following function should not call the destructed |client| function.
  CompleteInitialization(Result::OK);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

TEST_F(MidiManagerTest, CreateMidiManager) {
  // SystemMonitor is needed on Windows.
  base::SystemMonitor system_monitor;

  std::unique_ptr<FakeMidiManagerClient> client;
  client.reset(new FakeMidiManagerClient);

  std::unique_ptr<MidiManager> manager(MidiManager::Create());
  manager->StartSession(client.get());

  Result result = client->WaitForResult();
  // This #ifdef needs to be identical to the one in media/midi/midi_manager.cc.
  // Do not change the condition for disabling this test.
#if !defined(OS_MACOSX) && !defined(OS_WIN) && \
    !(defined(USE_ALSA) && defined(USE_UDEV)) && !defined(OS_ANDROID)
  EXPECT_EQ(Result::NOT_SUPPORTED, result);
#elif defined(USE_ALSA)
  // Temporary until http://crbug.com/371230 is resolved.
  EXPECT_TRUE(result == Result::OK || result == Result::INITIALIZATION_ERROR);
#else
  EXPECT_EQ(Result::OK, result);
#endif

  manager->Shutdown();
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

// TODO(toyoshim): Add multi-threaded unit tests to check races around
// StartInitialization(), CompleteInitialization(), and Finalize().

}  // namespace

}  // namespace midi
}  // namespace media

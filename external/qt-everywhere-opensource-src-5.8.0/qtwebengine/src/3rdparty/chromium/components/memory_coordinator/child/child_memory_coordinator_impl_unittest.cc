// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_coordinator/child/child_memory_coordinator_impl.h"

#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_coordinator {

namespace {

class ChildMemoryCoordinatorImplTest : public testing::Test {
 public:
  ChildMemoryCoordinatorImplTest()
      : clients_(new ChildMemoryCoordinatorImpl::ClientList),
        message_loop_(new base::MessageLoop),
        coordinator_impl_(mojo::GetProxy(&coordinator_), clients_) {}

  void RegisterClient(MemoryCoordinatorClient* client) {
    clients_->AddObserver(client);
  }

  void UnregisterClient(MemoryCoordinatorClient* client) {
    clients_->RemoveObserver(client);
  }

  mojom::ChildMemoryCoordinatorPtr& coordinator() { return coordinator_; }
  ChildMemoryCoordinatorImpl& coordinator_impl() { return coordinator_impl_; }

  void ChangeState(mojom::MemoryState state) {
    base::RunLoop loop;
    coordinator()->OnStateChange(state);
    loop.RunUntilIdle();
  }

 protected:
  scoped_refptr<ChildMemoryCoordinatorImpl::ClientList> clients_;

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;
  mojom::ChildMemoryCoordinatorPtr coordinator_ = nullptr;
  ChildMemoryCoordinatorImpl coordinator_impl_;

  DISALLOW_COPY_AND_ASSIGN(ChildMemoryCoordinatorImplTest);
};

class MockMemoryCoordinatorClient final : public MemoryCoordinatorClient {
public:
  void OnMemoryStateChange(mojom::MemoryState state) override {
    last_state_ = state;
  }

  mojom::MemoryState last_state() const { return last_state_; }

 private:
  mojom::MemoryState last_state_ = mojom::MemoryState::UNKNOWN;
};

class MemoryCoordinatorTestThread : public base::Thread,
                                    public MemoryCoordinatorClient {
 public:
  MemoryCoordinatorTestThread(
      const std::string& name,
      scoped_refptr<ChildMemoryCoordinatorImpl::ClientList> clients)
      : Thread(name), clients_(clients) {}
  ~MemoryCoordinatorTestThread() override { Stop(); }

  void Init() override {
    clients_->AddObserver(this);
  }

  void OnMemoryStateChange(mojom::MemoryState state) override {
    EXPECT_EQ(message_loop(), base::MessageLoop::current());
    last_state_ = state;
  }

  void CheckLastState(mojom::MemoryState state) {
    task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&MemoryCoordinatorTestThread::CheckLastStateInternal,
                   base::Unretained(this), state));
  }

 private:
  void CheckLastStateInternal(mojom::MemoryState state) {
    base::RunLoop loop;
    loop.RunUntilIdle();
    EXPECT_EQ(state, last_state_);
  }

  scoped_refptr<ChildMemoryCoordinatorImpl::ClientList> clients_;
  mojom::MemoryState last_state_ = mojom::MemoryState::UNKNOWN;
};

TEST_F(ChildMemoryCoordinatorImplTest, SingleClient) {
  MockMemoryCoordinatorClient client;
  RegisterClient(&client);

  ChangeState(mojom::MemoryState::THROTTLED);
  EXPECT_EQ(mojom::MemoryState::THROTTLED, client.last_state());

  ChangeState(mojom::MemoryState::NORMAL);
  EXPECT_EQ(mojom::MemoryState::NORMAL, client.last_state());

  UnregisterClient(&client);
  ChangeState(mojom::MemoryState::THROTTLED);
  EXPECT_TRUE(mojom::MemoryState::THROTTLED != client.last_state());
}

TEST_F(ChildMemoryCoordinatorImplTest, MultipleClients) {
  MemoryCoordinatorTestThread t1("thread 1", clients_);
  MemoryCoordinatorTestThread t2("thread 2", clients_);

  t1.StartAndWaitForTesting();
  t2.StartAndWaitForTesting();

  ChangeState(mojom::MemoryState::THROTTLED);
  t1.CheckLastState(mojom::MemoryState::THROTTLED);
  t2.CheckLastState(mojom::MemoryState::THROTTLED);

  ChangeState(mojom::MemoryState::NORMAL);
  t1.CheckLastState(mojom::MemoryState::NORMAL);
  t2.CheckLastState(mojom::MemoryState::NORMAL);

  t1.Stop();
  t2.Stop();
}

}  // namespace

}  // namespace memory_coordinator

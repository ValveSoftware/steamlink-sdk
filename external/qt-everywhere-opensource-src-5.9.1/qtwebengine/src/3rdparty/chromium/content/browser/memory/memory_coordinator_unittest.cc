// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_coordinator.h"

#include "base/memory/memory_pressure_monitor.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

void RunUntilIdle() {
  base::RunLoop loop;
  loop.RunUntilIdle();
}

}  // namespace

// A mock ChildMemoryCoordinator, for testing interaction between MC and CMC.
class MockChildMemoryCoordinator : public mojom::ChildMemoryCoordinator {
 public:
  MockChildMemoryCoordinator()
      : state_(mojom::MemoryState::UNKNOWN),
        on_state_change_calls_(0) {}

  ~MockChildMemoryCoordinator() override {}

  void OnStateChange(mojom::MemoryState state) override {
    state_ = state;
    ++on_state_change_calls_;
  }

  mojom::MemoryState state() const { return state_; }
  int on_state_change_calls() const { return on_state_change_calls_; }

 private:
  mojom::MemoryState state_;
  int on_state_change_calls_;
};

// A MemoryCoordinator that can be directly constructed.
class TestMemoryCoordinator : public MemoryCoordinator {
 public:
  // Mojo machinery for wrapping a mock ChildMemoryCoordinator.
  struct Child {
    Child(mojom::ChildMemoryCoordinatorPtr* cmc_ptr) : cmc_binding(&cmc) {
      cmc_binding.Bind(mojo::GetProxy(cmc_ptr));
      RunUntilIdle();
    }

    MockChildMemoryCoordinator cmc;
    mojo::Binding<mojom::ChildMemoryCoordinator> cmc_binding;
  };

  TestMemoryCoordinator() {}
  ~TestMemoryCoordinator() override {}

  using MemoryCoordinator::OnConnectionError;

  MockChildMemoryCoordinator* CreateChildMemoryCoordinator(
      int process_id) {
    mojom::ChildMemoryCoordinatorPtr cmc_ptr;
    children_.push_back(std::unique_ptr<Child>(new Child(&cmc_ptr)));
    AddChildForTesting(process_id, std::move(cmc_ptr));
    return &children_.back()->cmc;
  }

  // Wrapper of MemoryCoordinator::SetMemoryState that also calls RunUntilIdle.
  bool SetChildMemoryState(
      int render_process_id, mojom::MemoryState memory_state) {
    bool result = MemoryCoordinator::SetChildMemoryState(
        render_process_id, memory_state);
    RunUntilIdle();
    return result;
  }

  std::vector<std::unique_ptr<Child>> children_;
};

// Test fixture.
class MemoryCoordinatorTest : public testing::Test {
 private:
  // Needed for mojo.
  base::MessageLoop message_loop_;
};

TEST_F(MemoryCoordinatorTest, ChildRemovedOnConnectionError) {
  TestMemoryCoordinator mc;
  mc.CreateChildMemoryCoordinator(1);
  ASSERT_EQ(1u, mc.NumChildrenForTesting());
  mc.OnConnectionError(1);
  EXPECT_EQ(0u, mc.NumChildrenForTesting());
}

TEST_F(MemoryCoordinatorTest, SetMemoryStateFailsInvalidState) {
  TestMemoryCoordinator mc;
  auto cmc1 = mc.CreateChildMemoryCoordinator(1);

  EXPECT_FALSE(mc.SetChildMemoryState(1, mojom::MemoryState::UNKNOWN));
  EXPECT_EQ(0, cmc1->on_state_change_calls());
}

TEST_F(MemoryCoordinatorTest, SetMemoryStateFailsInvalidRenderer) {
  TestMemoryCoordinator mc;
  auto cmc1 = mc.CreateChildMemoryCoordinator(1);

  EXPECT_FALSE(mc.SetChildMemoryState(2, mojom::MemoryState::THROTTLED));
  EXPECT_EQ(0, cmc1->on_state_change_calls());
}

TEST_F(MemoryCoordinatorTest, SetMemoryStateNotDeliveredNop) {
  TestMemoryCoordinator mc;
  auto cmc1 = mc.CreateChildMemoryCoordinator(1);

  EXPECT_FALSE(mc.SetChildMemoryState(2, mojom::MemoryState::NORMAL));
  EXPECT_EQ(0, cmc1->on_state_change_calls());
}

TEST_F(MemoryCoordinatorTest, SetMemoryStateDelivered) {
  TestMemoryCoordinator mc;
  auto cmc1 = mc.CreateChildMemoryCoordinator(1);
  auto cmc2 = mc.CreateChildMemoryCoordinator(2);

  EXPECT_TRUE(mc.SetChildMemoryState(1, mojom::MemoryState::THROTTLED));
  EXPECT_EQ(1, cmc1->on_state_change_calls());
  EXPECT_EQ(mojom::MemoryState::THROTTLED, cmc1->state());

  EXPECT_TRUE(mc.SetChildMemoryState(2, mojom::MemoryState::SUSPENDED));
  EXPECT_EQ(1, cmc2->on_state_change_calls());
  EXPECT_EQ(mojom::MemoryState::SUSPENDED, cmc2->state());
}

}  // namespace content

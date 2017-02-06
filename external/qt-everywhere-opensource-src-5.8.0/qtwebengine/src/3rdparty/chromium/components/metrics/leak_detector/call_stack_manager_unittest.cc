// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/call_stack_manager.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/macros.h"
#include "components/metrics/leak_detector/custom_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace leak_detector {

namespace {

// Some test call stacks. The addresses are 64-bit but they should automatically
// be truncated to 32 bits on a 32-bit machine.
const void* kRawStack0[] = {
    reinterpret_cast<const void*>(0x8899aabbccddeeff),
    reinterpret_cast<const void*>(0x0000112233445566),
    reinterpret_cast<const void*>(0x5566778899aabbcc),
    reinterpret_cast<const void*>(0x9988776655443322),
};
// This is similar to kRawStack0, differing only in one address by 1. It should
// still produce a distinct CallStack object and hash.
const void* kRawStack1[] = {
    kRawStack0[0], kRawStack0[1],
    reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(kRawStack0[2]) +
                                  1),
    kRawStack0[3],
};
const void* kRawStack2[] = {
    reinterpret_cast<const void*>(0x900df00dcab58888),
    reinterpret_cast<const void*>(0x00001337cafedeed),
    reinterpret_cast<const void*>(0x0000deafbabe1234),
};
const void* kRawStack3[] = {
    reinterpret_cast<const void*>(0x0000000012345678),
    reinterpret_cast<const void*>(0x00000000abcdef01),
    reinterpret_cast<const void*>(0x00000000fdecab98),
    reinterpret_cast<const void*>(0x0000deadbeef0001),
    reinterpret_cast<const void*>(0x0000900ddeed0002),
    reinterpret_cast<const void*>(0x0000f00dcafe0003),
    reinterpret_cast<const void*>(0x0000f00d900d0004),
    reinterpret_cast<const void*>(0xdeedcafebabe0005),
};

// Creates a copy of a call stack as a scoped_ptr to a raw stack. The depth is
// the same as the original stack, but it is not stored in the result.
std::unique_ptr<const void* []> CopyStack(const CallStack* stack) {
  std::unique_ptr<const void* []> stack_copy(new const void*[stack->depth]);
  std::copy(stack->stack, stack->stack + stack->depth, stack_copy.get());
  return stack_copy;
}

}  // namespace

class CallStackManagerTest : public ::testing::Test {
 public:
  CallStackManagerTest() {}

  void SetUp() override { CustomAllocator::Initialize(); }
  void TearDown() override { EXPECT_TRUE(CustomAllocator::Shutdown()); }

 private:
  DISALLOW_COPY_AND_ASSIGN(CallStackManagerTest);
};

TEST_F(CallStackManagerTest, NewStacks) {
  CallStackManager manager;
  EXPECT_EQ(0U, manager.size());

  // Request some new stacks and make sure their creation is reflected in the
  // size of |manager|.
  const CallStack* stack0 =
      manager.GetCallStack(arraysize(kRawStack0), kRawStack0);
  EXPECT_EQ(arraysize(kRawStack0), stack0->depth);
  EXPECT_EQ(1U, manager.size());

  const CallStack* stack1 =
      manager.GetCallStack(arraysize(kRawStack1), kRawStack1);
  EXPECT_EQ(arraysize(kRawStack1), stack1->depth);
  EXPECT_EQ(2U, manager.size());

  const CallStack* stack2 =
      manager.GetCallStack(arraysize(kRawStack2), kRawStack2);
  EXPECT_EQ(arraysize(kRawStack2), stack2->depth);
  EXPECT_EQ(3U, manager.size());

  const CallStack* stack3 =
      manager.GetCallStack(arraysize(kRawStack3), kRawStack3);
  EXPECT_EQ(arraysize(kRawStack3), stack3->depth);
  EXPECT_EQ(4U, manager.size());

  // Call stack objects should be unique.
  EXPECT_NE(stack0, stack1);
  EXPECT_NE(stack0, stack2);
  EXPECT_NE(stack0, stack3);
  EXPECT_NE(stack1, stack2);
  EXPECT_NE(stack1, stack3);
  EXPECT_NE(stack2, stack3);
}

TEST_F(CallStackManagerTest, Hashes) {
  CallStackManager manager;

  const CallStack* stack0 =
      manager.GetCallStack(arraysize(kRawStack0), kRawStack0);
  const CallStack* stack1 =
      manager.GetCallStack(arraysize(kRawStack1), kRawStack1);
  const CallStack* stack2 =
      manager.GetCallStack(arraysize(kRawStack2), kRawStack2);
  const CallStack* stack3 =
      manager.GetCallStack(arraysize(kRawStack3), kRawStack3);

  // Hash values should be unique. This test is not designed to make sure the
  // hash function is generating unique hashes, but that CallStackManager is
  // properly storing the hashes in CallStack structs.
  EXPECT_NE(stack0->hash, stack1->hash);
  EXPECT_NE(stack0->hash, stack2->hash);
  EXPECT_NE(stack0->hash, stack3->hash);
  EXPECT_NE(stack1->hash, stack2->hash);
  EXPECT_NE(stack1->hash, stack3->hash);
  EXPECT_NE(stack2->hash, stack3->hash);
}

TEST_F(CallStackManagerTest, MultipleManagersHashes) {
  CallStackManager manager1;
  const CallStack* stack10 =
      manager1.GetCallStack(arraysize(kRawStack0), kRawStack0);
  const CallStack* stack11 =
      manager1.GetCallStack(arraysize(kRawStack1), kRawStack1);
  const CallStack* stack12 =
      manager1.GetCallStack(arraysize(kRawStack2), kRawStack2);
  const CallStack* stack13 =
      manager1.GetCallStack(arraysize(kRawStack3), kRawStack3);

  CallStackManager manager2;
  const CallStack* stack20 =
      manager2.GetCallStack(arraysize(kRawStack0), kRawStack0);
  const CallStack* stack21 =
      manager2.GetCallStack(arraysize(kRawStack1), kRawStack1);
  const CallStack* stack22 =
      manager2.GetCallStack(arraysize(kRawStack2), kRawStack2);
  const CallStack* stack23 =
      manager2.GetCallStack(arraysize(kRawStack3), kRawStack3);

  // Different CallStackManagers should still generate the same hashes.
  EXPECT_EQ(stack10->hash, stack20->hash);
  EXPECT_EQ(stack11->hash, stack21->hash);
  EXPECT_EQ(stack12->hash, stack22->hash);
  EXPECT_EQ(stack13->hash, stack23->hash);
}

TEST_F(CallStackManagerTest, HashWithReducedDepth) {
  CallStackManager manager;
  const CallStack* stack =
      manager.GetCallStack(arraysize(kRawStack3), kRawStack3);

  // Hash function should only operate on the first |CallStack::depth| elements
  // of CallStack::stack. To test this, reduce the depth value of one of the
  // stacks and make sure the hash changes.
  EXPECT_NE(stack->hash,
            manager.GetCallStack(stack->depth - 1, stack->stack)->hash);
  EXPECT_NE(stack->hash,
            manager.GetCallStack(stack->depth - 2, stack->stack)->hash);
  EXPECT_NE(stack->hash,
            manager.GetCallStack(stack->depth - 3, stack->stack)->hash);
  EXPECT_NE(stack->hash,
            manager.GetCallStack(stack->depth - 4, stack->stack)->hash);

  // Also try subsets of the stack that don't start from the beginning.
  EXPECT_NE(stack->hash,
            manager.GetCallStack(stack->depth - 1, stack->stack + 1)->hash);
  EXPECT_NE(stack->hash,
            manager.GetCallStack(stack->depth - 2, stack->stack + 2)->hash);
  EXPECT_NE(stack->hash,
            manager.GetCallStack(stack->depth - 3, stack->stack + 3)->hash);
  EXPECT_NE(stack->hash,
            manager.GetCallStack(stack->depth - 4, stack->stack + 4)->hash);
}

TEST_F(CallStackManagerTest, DuplicateStacks) {
  CallStackManager manager;
  EXPECT_EQ(0U, manager.size());

  // Calling manager.GetCallStack() multiple times with the same raw stack
  // arguments will not result in creation of new call stack objects after the
  // first call. Instead, the previously created object will be returned, and
  // the size of |manager| will remain unchanged.
  //
  // Thus a call to GetCallStack() will always return the same result, given the
  // same inputs.

  // Add stack0.
  const CallStack* stack0 =
      manager.GetCallStack(arraysize(kRawStack0), kRawStack0);

  std::unique_ptr<const void* []> rawstack0_duplicate0 = CopyStack(stack0);
  const CallStack* stack0_duplicate0 =
      manager.GetCallStack(arraysize(kRawStack0), rawstack0_duplicate0.get());
  EXPECT_EQ(1U, manager.size());
  EXPECT_EQ(stack0, stack0_duplicate0);

  // Add stack1.
  const CallStack* stack1 =
      manager.GetCallStack(arraysize(kRawStack1), kRawStack1);
  EXPECT_EQ(2U, manager.size());

  std::unique_ptr<const void* []> rawstack0_duplicate1 = CopyStack(stack0);
  const CallStack* stack0_duplicate1 =
      manager.GetCallStack(arraysize(kRawStack0), rawstack0_duplicate1.get());
  EXPECT_EQ(2U, manager.size());
  EXPECT_EQ(stack0, stack0_duplicate1);

  std::unique_ptr<const void* []> rawstack1_duplicate0 = CopyStack(stack1);
  const CallStack* stack1_duplicate0 =
      manager.GetCallStack(arraysize(kRawStack1), rawstack1_duplicate0.get());
  EXPECT_EQ(2U, manager.size());
  EXPECT_EQ(stack1, stack1_duplicate0);

  // Add stack2 and stack3.
  const CallStack* stack2 =
      manager.GetCallStack(arraysize(kRawStack2), kRawStack2);
  const CallStack* stack3 =
      manager.GetCallStack(arraysize(kRawStack3), kRawStack3);
  EXPECT_EQ(4U, manager.size());

  std::unique_ptr<const void* []> rawstack1_duplicate1 = CopyStack(stack1);
  const CallStack* stack1_duplicate1 =
      manager.GetCallStack(arraysize(kRawStack1), rawstack1_duplicate1.get());
  EXPECT_EQ(4U, manager.size());
  EXPECT_EQ(stack1, stack1_duplicate1);

  std::unique_ptr<const void* []> rawstack0_duplicate2 = CopyStack(stack0);
  const CallStack* stack0_duplicate2 =
      manager.GetCallStack(arraysize(kRawStack0), rawstack0_duplicate2.get());
  EXPECT_EQ(4U, manager.size());
  EXPECT_EQ(stack0, stack0_duplicate2);

  std::unique_ptr<const void* []> rawstack3_duplicate0 = CopyStack(stack3);
  const CallStack* stack3_duplicate0 =
      manager.GetCallStack(arraysize(kRawStack3), rawstack3_duplicate0.get());
  EXPECT_EQ(4U, manager.size());
  EXPECT_EQ(stack3, stack3_duplicate0);

  std::unique_ptr<const void* []> rawstack2_duplicate0 = CopyStack(stack2);
  const CallStack* stack2_duplicate0 =
      manager.GetCallStack(arraysize(kRawStack2), rawstack2_duplicate0.get());
  EXPECT_EQ(4U, manager.size());
  EXPECT_EQ(stack2, stack2_duplicate0);
}

}  // namespace leak_detector
}  // namespace metrics

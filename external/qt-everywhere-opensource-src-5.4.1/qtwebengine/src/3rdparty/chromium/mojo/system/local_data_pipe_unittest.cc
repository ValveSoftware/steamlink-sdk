// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/local_data_pipe.h"

#include <string.h>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "mojo/system/data_pipe.h"
#include "mojo/system/waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

const uint32_t kSizeOfOptions =
    static_cast<uint32_t>(sizeof(MojoCreateDataPipeOptions));

// Validate options.
TEST(LocalDataPipeTest, Creation) {
  // Create using default options.
  {
    // Get default options.
    MojoCreateDataPipeOptions default_options = { 0 };
    EXPECT_EQ(MOJO_RESULT_OK,
              DataPipe::ValidateCreateOptions(NULL, &default_options));
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(default_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }

  // Create using non-default options.
  {
    const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,  // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      1,  // |element_num_bytes|.
      1000  // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions validated_options = { 0 };
    EXPECT_EQ(MOJO_RESULT_OK,
              DataPipe::ValidateCreateOptions(&options, &validated_options));
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }
  {
    const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,  // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
      4,  // |element_num_bytes|.
      4000  // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions validated_options = { 0 };
    EXPECT_EQ(MOJO_RESULT_OK,
              DataPipe::ValidateCreateOptions(&options, &validated_options));
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }
  {
    const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,  // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD,  // |flags|.
      7,  // |element_num_bytes|.
      7000000  // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions validated_options = { 0 };
    EXPECT_EQ(MOJO_RESULT_OK,
              DataPipe::ValidateCreateOptions(&options, &validated_options));
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }
  // Default capacity.
  {
    const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,  // |struct_size|.
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD,  // |flags|.
      100,  // |element_num_bytes|.
      0  // |capacity_num_bytes|.
    };
    MojoCreateDataPipeOptions validated_options = { 0 };
    EXPECT_EQ(MOJO_RESULT_OK,
              DataPipe::ValidateCreateOptions(&options, &validated_options));
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));
    dp->ProducerClose();
    dp->ConsumerClose();
  }
}

TEST(LocalDataPipeTest, SimpleReadWrite) {
  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
    static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
    1000 * sizeof(int32_t)  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateCreateOptions(&options, &validated_options));

  scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

  int32_t elements[10] = { 0 };
  uint32_t num_bytes = 0;

  // Try reading; nothing there yet.
  num_bytes = static_cast<uint32_t>(arraysize(elements) * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            dp->ConsumerReadData(elements, &num_bytes, false));

  // Query; nothing there yet.
  num_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(0u, num_bytes);

  // Discard; nothing there yet.
  num_bytes = static_cast<uint32_t>(5u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            dp->ConsumerDiscardData(&num_bytes, false));

  // Read with invalid |num_bytes|.
  num_bytes = sizeof(elements[0]) + 1;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dp->ConsumerReadData(elements, &num_bytes, false));

  // Write two elements.
  elements[0] = 123;
  elements[1] = 456;
  num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(elements, &num_bytes, false));
  // It should have written everything (even without "all or none").
  EXPECT_EQ(2u * sizeof(elements[0]), num_bytes);

  // Query.
  num_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(2 * sizeof(elements[0]), num_bytes);

  // Read one element.
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(elements, &num_bytes, false));
  EXPECT_EQ(1u * sizeof(elements[0]), num_bytes);
  EXPECT_EQ(123, elements[0]);
  EXPECT_EQ(-1, elements[1]);

  // Query.
  num_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(1 * sizeof(elements[0]), num_bytes);

  // Try to read two elements, with "all or none".
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ConsumerReadData(elements, &num_bytes, true));
  EXPECT_EQ(-1, elements[0]);
  EXPECT_EQ(-1, elements[1]);

  // Try to read two elements, without "all or none".
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(elements, &num_bytes, false));
  EXPECT_EQ(456, elements[0]);
  EXPECT_EQ(-1, elements[1]);

  // Query.
  num_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(0u, num_bytes);

  dp->ProducerClose();
  dp->ConsumerClose();
}

// Note: The "basic" waiting tests test that the "wait states" are correct in
// various situations; they don't test that waiters are properly awoken on state
// changes. (For that, we need to use multiple threads.)
TEST(LocalDataPipeTest, BasicProducerWaiting) {
  // Note: We take advantage of the fact that for |LocalDataPipe|, capacities
  // are strict maximums. This is not guaranteed by the API.

  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
    static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
    2 * sizeof(int32_t)  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateCreateOptions(&options, &validated_options));

  scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));
  Waiter waiter;
  uint32_t context = 0;

  // Never readable.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            dp->ProducerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 12));

  // Already writable.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            dp->ProducerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 34));

  // Write two elements.
  int32_t elements[2] = { 123, 456 };
  uint32_t num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(elements, &num_bytes, true));
  EXPECT_EQ(static_cast<uint32_t>(2u * sizeof(elements[0])), num_bytes);

  // Adding a waiter should now succeed.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 56));
  // And it shouldn't be writable yet.
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, waiter.Wait(0, NULL));
  dp->ProducerRemoveWaiter(&waiter);

  // Do it again.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 78));

  // Read one element.
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(elements, &num_bytes, true));
  EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
  EXPECT_EQ(123, elements[0]);
  EXPECT_EQ(-1, elements[1]);

  // Waiting should now succeed.
  EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(1000, &context));
  EXPECT_EQ(78u, context);
  dp->ProducerRemoveWaiter(&waiter);

  // Try writing, using a two-phase write.
  void* buffer = NULL;
  num_bytes = static_cast<uint32_t>(3u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(&buffer, &num_bytes, false));
  EXPECT_TRUE(buffer != NULL);
  EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);

  static_cast<int32_t*>(buffer)[0] = 789;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerEndWriteData(
                static_cast<uint32_t>(1u * sizeof(elements[0]))));

  // Add a waiter.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 90));

  // Read one element, using a two-phase read.
  const void* read_buffer = NULL;
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(&read_buffer, &num_bytes, false));
  EXPECT_TRUE(read_buffer != NULL);
  // Since we only read one element (after having written three in all), the
  // two-phase read should only allow us to read one. This checks an
  // implementation detail!
  EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
  EXPECT_EQ(456, static_cast<const int32_t*>(read_buffer)[0]);
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerEndReadData(
                static_cast<uint32_t>(1u * sizeof(elements[0]))));

  // Waiting should succeed.
  EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(1000, &context));
  EXPECT_EQ(90u, context);
  dp->ProducerRemoveWaiter(&waiter);

  // Write one element.
  elements[0] = 123;
  num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(elements, &num_bytes, false));
  EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);

  // Add a waiter.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 12));

  // Close the consumer.
  dp->ConsumerClose();

  // It should now be never-writable.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, waiter.Wait(1000, &context));
  EXPECT_EQ(12u, context);
  dp->ProducerRemoveWaiter(&waiter);

  dp->ProducerClose();
}

TEST(LocalDataPipeTest, BasicConsumerWaiting) {
  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
    static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
    1000 * sizeof(int32_t)  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateCreateOptions(&options, &validated_options));

  {
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));
    Waiter waiter;
    uint32_t context = 0;

    // Never writable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 12));

    // Not yet readable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 34));
    EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, waiter.Wait(0, NULL));
    dp->ConsumerRemoveWaiter(&waiter);

    // Write two elements.
    int32_t elements[2] = { 123, 456 };
    uint32_t num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(elements, &num_bytes, true));

    // Should already be readable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
              dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 56));

    // Discard one element.
    num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerDiscardData(&num_bytes, true));
    EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);

    // Should still be readable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
              dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 78));

    // Read one element.
    elements[0] = -1;
    elements[1] = -1;
    num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(elements, &num_bytes, true));
    EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
    EXPECT_EQ(456, elements[0]);
    EXPECT_EQ(-1, elements[1]);

    // Adding a waiter should now succeed.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 90));

    // Write one element.
    elements[0] = 789;
    elements[1] = -1;
    num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(elements, &num_bytes, true));

    // Waiting should now succeed.
    EXPECT_EQ(MOJO_RESULT_OK, waiter.Wait(1000, &context));
    EXPECT_EQ(90u, context);
    dp->ConsumerRemoveWaiter(&waiter);

    // Close the producer.
    dp->ProducerClose();

    // Should still be readable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
              dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 12));

    // Read one element.
    elements[0] = -1;
    elements[1] = -1;
    num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(elements, &num_bytes, true));
    EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
    EXPECT_EQ(789, elements[0]);
    EXPECT_EQ(-1, elements[1]);

    // Should be never-readable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 34));

    dp->ConsumerClose();
  }

  // Test with two-phase APIs and closing the producer with an active consumer
  // waiter.
  {
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));
    Waiter waiter;
    uint32_t context = 0;

    // Write two elements.
    int32_t* elements = NULL;
    void* buffer = NULL;
    // Request room for three (but we'll only write two).
    uint32_t num_bytes = static_cast<uint32_t>(3u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerBeginWriteData(&buffer, &num_bytes, true));
    EXPECT_TRUE(buffer != NULL);
    EXPECT_GE(num_bytes, static_cast<uint32_t>(3u * sizeof(elements[0])));
    elements = static_cast<int32_t*>(buffer);
    elements[0] = 123;
    elements[1] = 456;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerEndWriteData(
                  static_cast<uint32_t>(2u * sizeof(elements[0]))));

    // Should already be readable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
              dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 12));

    // Read one element.
    // Request two in all-or-none mode, but only read one.
    const void* read_buffer = NULL;
    num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(&read_buffer, &num_bytes, true));
    EXPECT_TRUE(read_buffer != NULL);
    EXPECT_EQ(static_cast<uint32_t>(2u * sizeof(elements[0])), num_bytes);
    const int32_t* read_elements = static_cast<const int32_t*>(read_buffer);
    EXPECT_EQ(123, read_elements[0]);
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerEndReadData(
                  static_cast<uint32_t>(1u * sizeof(elements[0]))));

    // Should still be readable.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
              dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 34));

    // Read one element.
    // Request three, but not in all-or-none mode.
    read_buffer = NULL;
    num_bytes = static_cast<uint32_t>(3u * sizeof(elements[0]));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(&read_buffer, &num_bytes, false));
    EXPECT_TRUE(read_buffer != NULL);
    EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
    read_elements = static_cast<const int32_t*>(read_buffer);
    EXPECT_EQ(456, read_elements[0]);
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerEndReadData(
                  static_cast<uint32_t>(1u * sizeof(elements[0]))));

    // Adding a waiter should now succeed.
    waiter.Init();
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 56));

    // Close the producer.
    dp->ProducerClose();

    // Should be never-readable.
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, waiter.Wait(1000, &context));
    EXPECT_EQ(56u, context);
    dp->ConsumerRemoveWaiter(&waiter);

    dp->ConsumerClose();
  }
}

// Tests that data pipes aren't writable/readable during two-phase writes/reads.
TEST(LocalDataPipeTest, BasicTwoPhaseWaiting) {
  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
    static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
    1000 * sizeof(int32_t)  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateCreateOptions(&options, &validated_options));

  scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));
  Waiter waiter;

  // It should be writable.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            dp->ProducerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 0));

  uint32_t num_bytes = static_cast<uint32_t>(1u * sizeof(int32_t));
  void* write_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(&write_ptr, &num_bytes, false));
  EXPECT_TRUE(write_ptr != NULL);
  EXPECT_GE(num_bytes, static_cast<uint32_t>(1u * sizeof(int32_t)));

  // At this point, it shouldn't be writable.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 1));
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, waiter.Wait(0, NULL));
  dp->ProducerRemoveWaiter(&waiter);

  // It shouldn't be readable yet either.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 2));
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, waiter.Wait(0, NULL));
  dp->ConsumerRemoveWaiter(&waiter);

  static_cast<int32_t*>(write_ptr)[0] = 123;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerEndWriteData(
                static_cast<uint32_t>(1u * sizeof(int32_t))));

  // It should be writable again.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            dp->ProducerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 3));

  // And readable.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 4));

  // Start another two-phase write and check that it's readable even in the
  // middle of it.
  num_bytes = static_cast<uint32_t>(1u * sizeof(int32_t));
  write_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(&write_ptr, &num_bytes, false));
  EXPECT_TRUE(write_ptr != NULL);
  EXPECT_GE(num_bytes, static_cast<uint32_t>(1u * sizeof(int32_t)));

  // It should be readable.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 5));

  // End the two-phase write without writing anything.
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(0u));

  // Start a two-phase read.
  num_bytes = static_cast<uint32_t>(1u * sizeof(int32_t));
  const void* read_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(&read_ptr, &num_bytes, false));
  EXPECT_TRUE(read_ptr != NULL);
  EXPECT_EQ(static_cast<uint32_t>(1u * sizeof(int32_t)), num_bytes);

  // At this point, it should still be writable.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            dp->ProducerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 6));

  // But not readable.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 7));
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, waiter.Wait(0, NULL));
  dp->ConsumerRemoveWaiter(&waiter);

  // End the two-phase read without reading anything.
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(0u));

  // It should be readable again.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 8));

  dp->ProducerClose();
  dp->ConsumerClose();
}

// Test that a "may discard" data pipe is writable even when it's full.
TEST(LocalDataPipeTest, BasicMayDiscardWaiting) {
  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD,  // |flags|.
    static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
    1 * sizeof(int32_t)  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateCreateOptions(&options, &validated_options));

  scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));
  Waiter waiter;

  // Writable.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            dp->ProducerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 0));

  // Not readable.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 1));
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, waiter.Wait(0, NULL));
  dp->ConsumerRemoveWaiter(&waiter);

  uint32_t num_bytes = static_cast<uint32_t>(sizeof(int32_t));
  int32_t element = 123;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(&element, &num_bytes, false));
  EXPECT_EQ(static_cast<uint32_t>(sizeof(int32_t)), num_bytes);

  // Still writable (even though it's full).
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            dp->ProducerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 2));

  // Now readable.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 3));

  // Overwrite that element.
  num_bytes = static_cast<uint32_t>(sizeof(int32_t));
  element = 456;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(&element, &num_bytes, false));
  EXPECT_EQ(static_cast<uint32_t>(sizeof(int32_t)), num_bytes);

  // Still writable.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            dp->ProducerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 4));

  // And still readable.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 5));

  // Read that element.
  num_bytes = static_cast<uint32_t>(sizeof(int32_t));
  element = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerReadData(&element, &num_bytes, false));
  EXPECT_EQ(static_cast<uint32_t>(sizeof(int32_t)), num_bytes);
  EXPECT_EQ(456, element);

  // Still writable.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            dp->ProducerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_WRITABLE, 6));

  // No longer readable.
  waiter.Init();
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerAddWaiter(&waiter, MOJO_HANDLE_SIGNAL_READABLE, 7));
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, waiter.Wait(0, NULL));
  dp->ConsumerRemoveWaiter(&waiter);

  dp->ProducerClose();
  dp->ConsumerClose();
}

void Seq(int32_t start, size_t count, int32_t* out) {
  for (size_t i = 0; i < count; i++)
    out[i] = start + static_cast<int32_t>(i);
}

TEST(LocalDataPipeTest, MayDiscard) {
  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD,  // |flags|.
    static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
    10 * sizeof(int32_t)  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateCreateOptions(&options, &validated_options));

  scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

  int32_t buffer[100] = { 0 };
  uint32_t num_bytes = 0;

  num_bytes = 20u * sizeof(int32_t);
  Seq(0, arraysize(buffer), buffer);
  // Try writing more than capacity. (This test relies on the implementation
  // enforcing the capacity strictly.)
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(buffer, &num_bytes, false));
  EXPECT_EQ(10u * sizeof(int32_t), num_bytes);

  // Read half of what we wrote.
  num_bytes = 5u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(buffer, &num_bytes, false));
  EXPECT_EQ(5u * sizeof(int32_t), num_bytes);
  int32_t expected_buffer[100];
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  Seq(0, 5u, expected_buffer);
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));
  // Internally, a circular buffer would now look like:
  //   -, -, -, -, -, 5, 6, 7, 8, 9

  // Write a bit more than the space that's available.
  num_bytes = 8u * sizeof(int32_t);
  Seq(100, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(buffer, &num_bytes, false));
  EXPECT_EQ(8u * sizeof(int32_t), num_bytes);
  // Internally, a circular buffer would now look like:
  //   100, 101, 102, 103, 104, 105, 106, 107, 8, 9

  // Read half of what's available.
  num_bytes = 5u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(buffer, &num_bytes, false));
  EXPECT_EQ(5u * sizeof(int32_t), num_bytes);
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  expected_buffer[0] = 8;
  expected_buffer[1] = 9;
  expected_buffer[2] = 100;
  expected_buffer[3] = 101;
  expected_buffer[4] = 102;
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));
  // Internally, a circular buffer would now look like:
  //   -, -, -, 103, 104, 105, 106, 107, -, -

  // Write one integer.
  num_bytes = 1u * sizeof(int32_t);
  Seq(200, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(buffer, &num_bytes, false));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);
  // Internally, a circular buffer would now look like:
  //   -, -, -, 103, 104, 105, 106, 107, 200, -

  // Write five more.
  num_bytes = 5u * sizeof(int32_t);
  Seq(300, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(buffer, &num_bytes, false));
  EXPECT_EQ(5u * sizeof(int32_t), num_bytes);
  // Internally, a circular buffer would now look like:
  //   301, 302, 303, 304, 104, 105, 106, 107, 200, 300

  // Read it all.
  num_bytes = sizeof(buffer);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(buffer, &num_bytes, false));
  EXPECT_EQ(10u * sizeof(int32_t), num_bytes);
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  expected_buffer[0] = 104;
  expected_buffer[1] = 105;
  expected_buffer[2] = 106;
  expected_buffer[3] = 107;
  expected_buffer[4] = 200;
  expected_buffer[5] = 300;
  expected_buffer[6] = 301;
  expected_buffer[7] = 302;
  expected_buffer[8] = 303;
  expected_buffer[9] = 304;
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Test two-phase writes, including in all-or-none mode.
  // Note: Again, the following depends on an implementation detail -- namely
  // that the write pointer will point at the 5th element of the buffer (and the
  // buffer has exactly the capacity requested).

  num_bytes = 0u;
  void* write_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(&write_ptr, &num_bytes, false));
  EXPECT_TRUE(write_ptr != NULL);
  EXPECT_EQ(6u * sizeof(int32_t), num_bytes);
  Seq(400, 6, static_cast<int32_t*>(write_ptr));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(6u * sizeof(int32_t)));
  // Internally, a circular buffer would now look like:
  //   -, -, -, -, 400, 401, 402, 403, 404, 405

  // |ProducerBeginWriteData()| ignores |*num_bytes| except in "all-or-none"
  // mode.
  num_bytes = 6u * sizeof(int32_t);
  write_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(&write_ptr, &num_bytes, false));
  EXPECT_EQ(4u * sizeof(int32_t), num_bytes);
  static_cast<int32_t*>(write_ptr)[0] = 500;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(1u * sizeof(int32_t)));
  // Internally, a circular buffer would now look like:
  //   500, -, -, -, 400, 401, 402, 403, 404, 405

  // Requesting a 10-element buffer in all-or-none mode fails at this point.
  num_bytes = 10u * sizeof(int32_t);
  write_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ProducerBeginWriteData(&write_ptr, &num_bytes, true));

  // But requesting, say, a 5-element (up to 9, really) buffer should be okay.
  // It will discard two elements.
  num_bytes = 5u * sizeof(int32_t);
  write_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(&write_ptr, &num_bytes, true));
  EXPECT_EQ(5u * sizeof(int32_t), num_bytes);
  // Only write 4 elements though.
  Seq(600, 4, static_cast<int32_t*>(write_ptr));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(4u * sizeof(int32_t)));
  // Internally, a circular buffer would now look like:
  //   500, 600, 601, 602, 603, -, 402, 403, 404, 405

  // Do this again. Make sure we can get a buffer all the way out to the end of
  // the internal buffer.
  num_bytes = 5u * sizeof(int32_t);
  write_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(&write_ptr, &num_bytes, true));
  EXPECT_EQ(5u * sizeof(int32_t), num_bytes);
  // Only write 3 elements though.
  Seq(700, 3, static_cast<int32_t*>(write_ptr));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(3u * sizeof(int32_t)));
  // Internally, a circular buffer would now look like:
  //   500, 600, 601, 602, 603, 700, 701, 702, -, -

  // Read everything.
  num_bytes = sizeof(buffer);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(buffer, &num_bytes, false));
  EXPECT_EQ(8u * sizeof(int32_t), num_bytes);
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  expected_buffer[0] = 500;
  expected_buffer[1] = 600;
  expected_buffer[2] = 601;
  expected_buffer[3] = 602;
  expected_buffer[4] = 603;
  expected_buffer[5] = 700;
  expected_buffer[6] = 701;
  expected_buffer[7] = 702;
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  dp->ProducerClose();
  dp->ConsumerClose();
}

TEST(LocalDataPipeTest, AllOrNone) {
  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
    static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
    10 * sizeof(int32_t)  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateCreateOptions(&options, &validated_options));

  scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

  // Try writing way too much.
  uint32_t num_bytes = 20u * sizeof(int32_t);
  int32_t buffer[100];
  Seq(0, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ProducerWriteData(buffer, &num_bytes, true));

  // Should still be empty.
  num_bytes = ~0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(0u, num_bytes);

  // Write some data.
  num_bytes = 5u * sizeof(int32_t);
  Seq(100, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(buffer, &num_bytes, true));
  EXPECT_EQ(5u * sizeof(int32_t), num_bytes);

  // Half full.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(5u * sizeof(int32_t), num_bytes);

  // Too much.
  num_bytes = 6u * sizeof(int32_t);
  Seq(200, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ProducerWriteData(buffer, &num_bytes, true));

  // Try reading too much.
  num_bytes = 11u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ConsumerReadData(buffer, &num_bytes, true));
  int32_t expected_buffer[100];
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Try discarding too much.
  num_bytes = 11u * sizeof(int32_t);
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ConsumerDiscardData(&num_bytes, true));

  // Just a little.
  num_bytes = 2u * sizeof(int32_t);
  Seq(300, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(buffer, &num_bytes, true));
  EXPECT_EQ(2u * sizeof(int32_t), num_bytes);

  // Just right.
  num_bytes = 3u * sizeof(int32_t);
  Seq(400, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(buffer, &num_bytes, true));
  EXPECT_EQ(3u * sizeof(int32_t), num_bytes);

  // Exactly full.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(10u * sizeof(int32_t), num_bytes);

  // Read half.
  num_bytes = 5u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(buffer, &num_bytes, true));
  EXPECT_EQ(5u * sizeof(int32_t), num_bytes);
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  Seq(100, 5, expected_buffer);
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Try reading too much again.
  num_bytes = 6u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ConsumerReadData(buffer, &num_bytes, true));
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Try discarding too much again.
  num_bytes = 6u * sizeof(int32_t);
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ConsumerDiscardData(&num_bytes, true));

  // Discard a little.
  num_bytes = 2u * sizeof(int32_t);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerDiscardData(&num_bytes, true));
  EXPECT_EQ(2u * sizeof(int32_t), num_bytes);

  // Three left.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(3u * sizeof(int32_t), num_bytes);

  // Close the producer, then test producer-closed cases.
  dp->ProducerClose();

  // Try reading too much; "failed precondition" since the producer is closed.
  num_bytes = 4u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            dp->ConsumerReadData(buffer, &num_bytes, true));
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Try discarding too much; "failed precondition" again.
  num_bytes = 4u * sizeof(int32_t);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            dp->ConsumerDiscardData(&num_bytes, true));

  // Read a little.
  num_bytes = 2u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(buffer, &num_bytes, true));
  EXPECT_EQ(2u * sizeof(int32_t), num_bytes);
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  Seq(400, 2, expected_buffer);
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Discard the remaining element.
  num_bytes = 1u * sizeof(int32_t);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerDiscardData(&num_bytes, true));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);

  // Empty again.
  num_bytes = ~0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(0u, num_bytes);

  dp->ConsumerClose();
}

TEST(LocalDataPipeTest, AllOrNoneMayDiscard) {
  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD,  // |flags|.
    static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
    10 * sizeof(int32_t)  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateCreateOptions(&options, &validated_options));

  scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

  // Try writing way too much.
  uint32_t num_bytes = 20u * sizeof(int32_t);
  int32_t buffer[100];
  Seq(0, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ProducerWriteData(buffer, &num_bytes, true));

  // Write some stuff.
  num_bytes = 5u * sizeof(int32_t);
  Seq(100, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(buffer, &num_bytes, true));
  EXPECT_EQ(5u * sizeof(int32_t), num_bytes);

  // Write lots of stuff (discarding all but "104").
  num_bytes = 9u * sizeof(int32_t);
  Seq(200, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(buffer, &num_bytes, true));
  EXPECT_EQ(9u * sizeof(int32_t), num_bytes);

  // Read one.
  num_bytes = 1u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(buffer, &num_bytes, true));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);
  int32_t expected_buffer[100];
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  expected_buffer[0] = 104;
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Try reading too many.
  num_bytes = 10u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ConsumerReadData(buffer, &num_bytes, true));
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Try discarding too many.
  num_bytes = 10u * sizeof(int32_t);
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ConsumerDiscardData(&num_bytes, true));

  // Discard a bunch.
  num_bytes = 4u * sizeof(int32_t);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerDiscardData(&num_bytes, true));

  // Half full.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(5u * sizeof(int32_t), num_bytes);

  // Write as much as possible.
  num_bytes = 10u * sizeof(int32_t);
  Seq(300, arraysize(buffer), buffer);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(buffer, &num_bytes, true));
  EXPECT_EQ(10u * sizeof(int32_t), num_bytes);

  // Read everything.
  num_bytes = 10u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerReadData(buffer, &num_bytes, true));
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  EXPECT_EQ(10u * sizeof(int32_t), num_bytes);
  Seq(300, 10, expected_buffer);
  EXPECT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Note: All-or-none two-phase writes on a "may discard" data pipe are tested
  // in LocalDataPipeTest.MayDiscard.

  dp->ProducerClose();
  dp->ConsumerClose();
}

TEST(LocalDataPipeTest, TwoPhaseAllOrNone) {
  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
    static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
    10 * sizeof(int32_t)  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateCreateOptions(&options, &validated_options));

  scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

  // Try writing way too much (two-phase).
  uint32_t num_bytes = 20u * sizeof(int32_t);
  void* write_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ProducerBeginWriteData(&write_ptr, &num_bytes, true));

  // Try writing an amount which isn't a multiple of the element size
  // (two-phase).
  COMPILE_ASSERT(sizeof(int32_t) > 1u, wow_int32_ts_have_size_1);
  num_bytes = 1u;
  write_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dp->ProducerBeginWriteData(&write_ptr, &num_bytes, true));

  // Try reading way too much (two-phase).
  num_bytes = 20u * sizeof(int32_t);
  const void* read_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ConsumerBeginReadData(&read_ptr, &num_bytes, true));

  // Write half (two-phase).
  num_bytes = 5u * sizeof(int32_t);
  write_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(&write_ptr, &num_bytes, true));
  // May provide more space than requested.
  EXPECT_GE(num_bytes, 5u * sizeof(int32_t));
  EXPECT_TRUE(write_ptr != NULL);
  Seq(0, 5, static_cast<int32_t*>(write_ptr));
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(5u * sizeof(int32_t)));

  // Try reading an amount which isn't a multiple of the element size
  // (two-phase).
  num_bytes = 1u;
  read_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dp->ConsumerBeginReadData(&read_ptr, &num_bytes, true));

  // Read one (two-phase).
  num_bytes = 1u * sizeof(int32_t);
  read_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(&read_ptr, &num_bytes, true));
  EXPECT_GE(num_bytes, 1u * sizeof(int32_t));
  EXPECT_EQ(0, static_cast<const int32_t*>(read_ptr)[0]);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(1u * sizeof(int32_t)));

  // We should have four left, leaving room for six.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(4u * sizeof(int32_t), num_bytes);

  // Assuming a tight circular buffer of the specified capacity, we can't do a
  // two-phase write of six now.
  num_bytes = 6u * sizeof(int32_t);
  write_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ProducerBeginWriteData(&write_ptr, &num_bytes, true));

  // Write six elements (simple), filling the buffer.
  num_bytes = 6u * sizeof(int32_t);
  int32_t buffer[100];
  Seq(100, 6, buffer);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(buffer, &num_bytes, true));
  EXPECT_EQ(6u * sizeof(int32_t), num_bytes);

  // We have ten.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(10u * sizeof(int32_t), num_bytes);

  // But a two-phase read of ten should fail.
  num_bytes = 10u * sizeof(int32_t);
  read_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            dp->ConsumerBeginReadData(&read_ptr, &num_bytes, true));

  // Close the producer.
  dp->ProducerClose();

  // A two-phase read of nine should work.
  num_bytes = 9u * sizeof(int32_t);
  read_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(&read_ptr, &num_bytes, true));
  EXPECT_GE(num_bytes, 9u * sizeof(int32_t));
  EXPECT_EQ(1, static_cast<const int32_t*>(read_ptr)[0]);
  EXPECT_EQ(2, static_cast<const int32_t*>(read_ptr)[1]);
  EXPECT_EQ(3, static_cast<const int32_t*>(read_ptr)[2]);
  EXPECT_EQ(4, static_cast<const int32_t*>(read_ptr)[3]);
  EXPECT_EQ(100, static_cast<const int32_t*>(read_ptr)[4]);
  EXPECT_EQ(101, static_cast<const int32_t*>(read_ptr)[5]);
  EXPECT_EQ(102, static_cast<const int32_t*>(read_ptr)[6]);
  EXPECT_EQ(103, static_cast<const int32_t*>(read_ptr)[7]);
  EXPECT_EQ(104, static_cast<const int32_t*>(read_ptr)[8]);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(9u * sizeof(int32_t)));

  // A two-phase read of two should fail, with "failed precondition".
  num_bytes = 2u * sizeof(int32_t);
  read_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            dp->ConsumerBeginReadData(&read_ptr, &num_bytes, true));

  dp->ConsumerClose();
}

// Tests that |ProducerWriteData()| and |ConsumerReadData()| writes and reads,
// respectively, as much as possible, even if it has to "wrap around" the
// internal circular buffer. (Note that the two-phase write and read do not do
// this.)
TEST(LocalDataPipeTest, WrapAround) {
  unsigned char test_data[1000];
  for (size_t i = 0; i < arraysize(test_data); i++)
    test_data[i] = static_cast<unsigned char>(i);

  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
    1u,  // |element_num_bytes|.
    100u  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateCreateOptions(&options, &validated_options));
  // This test won't be valid if |ValidateCreateOptions()| decides to give the
  // pipe more space.
  ASSERT_EQ(100u, validated_options.capacity_num_bytes);

  scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

  // Write 20 bytes.
  uint32_t num_bytes = 20u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(&test_data[0], &num_bytes, false));
  EXPECT_EQ(20u, num_bytes);

  // Read 10 bytes.
  unsigned char read_buffer[1000] = { 0 };
  num_bytes = 10u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerReadData(read_buffer, &num_bytes, false));
  EXPECT_EQ(10u, num_bytes);
  EXPECT_EQ(0, memcmp(read_buffer, &test_data[0], 10u));

  // Check that a two-phase write can now only write (at most) 80 bytes. (This
  // checks an implementation detail; this behavior is not guaranteed, but we
  // need it for this test.)
  void* write_buffer_ptr = NULL;
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(&write_buffer_ptr, &num_bytes, false));
  EXPECT_TRUE(write_buffer_ptr != NULL);
  EXPECT_EQ(80u, num_bytes);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(0u));

  // Write as much data as we can (using |ProducerWriteData()|). We should write
  // 90 bytes.
  num_bytes = 200u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerWriteData(&test_data[20], &num_bytes, false));
  EXPECT_EQ(90u, num_bytes);

  // Check that a two-phase read can now only read (at most) 90 bytes. (This
  // checks an implementation detail; this behavior is not guaranteed, but we
  // need it for this test.)
  const void* read_buffer_ptr = NULL;
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(&read_buffer_ptr, &num_bytes, false));
  EXPECT_TRUE(read_buffer_ptr != NULL);
  EXPECT_EQ(90u, num_bytes);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(0u));

  // Read as much as possible (using |ConsumerReadData()|). We should read 100
  // bytes.
  num_bytes =
      static_cast<uint32_t>(arraysize(read_buffer) * sizeof(read_buffer[0]));
  memset(read_buffer, 0, num_bytes);
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerReadData(read_buffer, &num_bytes, false));
  EXPECT_EQ(100u, num_bytes);
  EXPECT_EQ(0, memcmp(read_buffer, &test_data[10], 100u));

  dp->ProducerClose();
  dp->ConsumerClose();
}

// Tests the behavior of closing the producer or consumer with respect to
// writes and reads (simple and two-phase).
TEST(LocalDataPipeTest, CloseWriteRead) {
  const char kTestData[] = "hello world";
  const uint32_t kTestDataSize = static_cast<uint32_t>(sizeof(kTestData));

  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
    1u,  // |element_num_bytes|.
    1000u  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateCreateOptions(&options, &validated_options));

  // Close producer first, then consumer.
  {
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

    // Write some data, so we'll have something to read.
    uint32_t num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(kTestData, &num_bytes, false));
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Write it again, so we'll have something left over.
    num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(kTestData, &num_bytes, false));
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Start two-phase write.
    void* write_buffer_ptr = NULL;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerBeginWriteData(&write_buffer_ptr, &num_bytes, false));
    EXPECT_TRUE(write_buffer_ptr != NULL);
    EXPECT_GT(num_bytes, 0u);

    // Start two-phase read.
    const void* read_buffer_ptr = NULL;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(&read_buffer_ptr, &num_bytes, false));
    EXPECT_TRUE(read_buffer_ptr != NULL);
    EXPECT_EQ(2u * kTestDataSize, num_bytes);

    // Close the producer.
    dp->ProducerClose();

    // The consumer can finish its two-phase read.
    EXPECT_EQ(0, memcmp(read_buffer_ptr, kTestData, kTestDataSize));
    EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(kTestDataSize));

    // And start another.
    read_buffer_ptr = NULL;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(&read_buffer_ptr, &num_bytes, false));
    EXPECT_TRUE(read_buffer_ptr != NULL);
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Close the consumer, which cancels the two-phase read.
    dp->ConsumerClose();
  }

  // Close consumer first, then producer.
  {
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

    // Write some data, so we'll have something to read.
    uint32_t num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(kTestData, &num_bytes, false));
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Start two-phase write.
    void* write_buffer_ptr = NULL;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerBeginWriteData(&write_buffer_ptr, &num_bytes, false));
    EXPECT_TRUE(write_buffer_ptr != NULL);
    ASSERT_GT(num_bytes, kTestDataSize);

    // Start two-phase read.
    const void* read_buffer_ptr = NULL;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerBeginReadData(&read_buffer_ptr, &num_bytes, false));
    EXPECT_TRUE(read_buffer_ptr != NULL);
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Close the consumer.
    dp->ConsumerClose();

    // Actually write some data. (Note: Premature freeing of the buffer would
    // probably only be detected under ASAN or similar.)
    memcpy(write_buffer_ptr, kTestData, kTestDataSize);
    // Note: Even though the consumer has been closed, ending the two-phase
    // write will report success.
    EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerEndWriteData(kTestDataSize));

    // But trying to write should result in failure.
    num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ProducerWriteData(kTestData, &num_bytes, false));

    // As will trying to start another two-phase write.
    write_buffer_ptr = NULL;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ProducerBeginWriteData(&write_buffer_ptr, &num_bytes, false));

    dp->ProducerClose();
  }

  // Test closing the consumer first, then the producer, with an active
  // two-phase write.
  {
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

    // Start two-phase write.
    void* write_buffer_ptr = NULL;
    uint32_t num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerBeginWriteData(&write_buffer_ptr, &num_bytes, false));
    EXPECT_TRUE(write_buffer_ptr != NULL);
    ASSERT_GT(num_bytes, kTestDataSize);

    dp->ConsumerClose();
    dp->ProducerClose();
  }

  // Test closing the producer and then trying to read (with no data).
  {
    scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

    // Write some data, so we'll have something to read.
    uint32_t num_bytes = kTestDataSize;
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ProducerWriteData(kTestData, &num_bytes, false));
    EXPECT_EQ(kTestDataSize, num_bytes);

    // Close the producer.
    dp->ProducerClose();

    // Read that data.
    char buffer[1000];
    num_bytes = static_cast<uint32_t>(sizeof(buffer));
    EXPECT_EQ(MOJO_RESULT_OK,
              dp->ConsumerReadData(buffer, &num_bytes, false));
    EXPECT_EQ(kTestDataSize, num_bytes);
    EXPECT_EQ(0, memcmp(buffer, kTestData, kTestDataSize));

    // A second read should fail.
    num_bytes = static_cast<uint32_t>(sizeof(buffer));
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerReadData(buffer, &num_bytes, false));

    // A two-phase read should also fail.
    const void* read_buffer_ptr = NULL;
    num_bytes = 0u;
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerBeginReadData(&read_buffer_ptr, &num_bytes, false));

    // Ditto for discard.
    num_bytes = 10u;
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              dp->ConsumerDiscardData(&num_bytes, false));

    dp->ConsumerClose();
  }
}

TEST(LocalDataPipeTest, TwoPhaseMoreInvalidArguments) {
  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
    static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
    10 * sizeof(int32_t)  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateCreateOptions(&options, &validated_options));

  scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

  // No data.
  uint32_t num_bytes = 1000u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(0u, num_bytes);

  // Try "ending" a two-phase write when one isn't active.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            dp->ProducerEndWriteData(1u * sizeof(int32_t)));

  // Still no data.
  num_bytes = 1000u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(0u, num_bytes);

  // Try ending a two-phase write with an invalid amount (too much).
  num_bytes = 0u;
  void* write_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(&write_ptr, &num_bytes, false));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dp->ProducerEndWriteData(
                num_bytes + static_cast<uint32_t>(sizeof(int32_t))));

  // But the two-phase write still ended.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, dp->ProducerEndWriteData(0u));

  // Still no data.
  num_bytes = 1000u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(0u, num_bytes);

  // Try ending a two-phase write with an invalid amount (not a multiple of the
  // element size).
  num_bytes = 0u;
  write_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ProducerBeginWriteData(&write_ptr, &num_bytes, false));
  EXPECT_GE(num_bytes, 1u);
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, dp->ProducerEndWriteData(1u));

  // But the two-phase write still ended.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, dp->ProducerEndWriteData(0u));

  // Still no data.
  num_bytes = 1000u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(0u, num_bytes);

  // Now write some data, so we'll be able to try reading.
  int32_t element = 123;
  num_bytes = 1u * sizeof(int32_t);
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(&element, &num_bytes, false));

  // One element available.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);

  // Try "ending" a two-phase read when one isn't active.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            dp->ConsumerEndReadData(1u * sizeof(int32_t)));

  // Still one element available.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);

  // Try ending a two-phase read with an invalid amount (too much).
  num_bytes = 0u;
  const void* read_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(&read_ptr, &num_bytes, false));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dp->ConsumerEndReadData(
                num_bytes + static_cast<uint32_t>(sizeof(int32_t))));

  // Still one element available.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);

  // Try ending a two-phase read with an invalid amount (not a multiple of the
  // element size).
  num_bytes = 0u;
  read_ptr = NULL;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(&read_ptr, &num_bytes, false));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);
  EXPECT_EQ(123, static_cast<const int32_t*>(read_ptr)[0]);
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, dp->ConsumerEndReadData(1u));

  // Still one element available.
  num_bytes = 0u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerQueryData(&num_bytes));
  EXPECT_EQ(1u * sizeof(int32_t), num_bytes);

  dp->ProducerClose();
  dp->ConsumerClose();
}

// Tests that even with "may discard", the data won't change under a two-phase
// read.
// TODO(vtl): crbug.com/348644: We currently don't pass this. (There are two
// related issues: First, we don't recognize that the data given to
// |ConsumerBeginReadData()| isn't discardable until |ConsumerEndReadData()|,
// and thus we erroneously allow |ProducerWriteData()| to succeed. Second, the
// |ProducerWriteData()| then changes the data underneath the two-phase read.)
TEST(LocalDataPipeTest, DISABLED_MayDiscardTwoPhaseConsistent) {
  const MojoCreateDataPipeOptions options = {
    kSizeOfOptions,  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD,  // |flags|.
    1,  // |element_num_bytes|.
    2  // |capacity_num_bytes|.
  };
  MojoCreateDataPipeOptions validated_options = { 0 };
  EXPECT_EQ(MOJO_RESULT_OK,
            DataPipe::ValidateCreateOptions(&options, &validated_options));

  scoped_refptr<LocalDataPipe> dp(new LocalDataPipe(validated_options));

  // Write some elements.
  char elements[2] = { 'a', 'b' };
  uint32_t num_bytes = 2u;
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(elements, &num_bytes, false));
  EXPECT_EQ(2u, num_bytes);

  // Begin reading.
  const void* read_ptr = NULL;
  num_bytes = 2u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(&read_ptr, &num_bytes, false));
  EXPECT_EQ(2u, num_bytes);
  EXPECT_EQ('a', static_cast<const char*>(read_ptr)[0]);
  EXPECT_EQ('b', static_cast<const char*>(read_ptr)[1]);

  // Try to write some more. But nothing should be discardable right now.
  elements[0] = 'x';
  elements[1] = 'y';
  num_bytes = 2u;
  // TODO(vtl): This should be:
  //  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
  //            dp->ProducerWriteData(elements, &num_bytes, false));
  // but we incorrectly think that the bytes being read are discardable. Letting
  // this through reveals the significant consequence.
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(elements, &num_bytes, false));

  // Check that our read buffer hasn't changed underneath us.
  EXPECT_EQ('a', static_cast<const char*>(read_ptr)[0]);
  EXPECT_EQ('b', static_cast<const char*>(read_ptr)[1]);

  // End reading.
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(2u));

  // Now writing should succeed.
  EXPECT_EQ(MOJO_RESULT_OK, dp->ProducerWriteData(elements, &num_bytes, false));

  // And if we read, we should get the new values.
  read_ptr = NULL;
  num_bytes = 2u;
  EXPECT_EQ(MOJO_RESULT_OK,
            dp->ConsumerBeginReadData(&read_ptr, &num_bytes, false));
  EXPECT_EQ(2u, num_bytes);
  EXPECT_EQ('x', static_cast<const char*>(read_ptr)[0]);
  EXPECT_EQ('y', static_cast<const char*>(read_ptr)[1]);

  // End reading.
  EXPECT_EQ(MOJO_RESULT_OK, dp->ConsumerEndReadData(2u));

  dp->ProducerClose();
  dp->ConsumerClose();
}

}  // namespace
}  // namespace system
}  // namespace mojo

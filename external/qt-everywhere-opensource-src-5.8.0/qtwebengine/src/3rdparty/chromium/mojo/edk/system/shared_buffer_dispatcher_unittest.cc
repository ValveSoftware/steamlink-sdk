// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/shared_buffer_dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/system/dispatcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace edk {
namespace {

// NOTE(vtl): There's currently not much to test for in
// |SharedBufferDispatcher::ValidateCreateOptions()|, but the tests should be
// expanded if/when options are added, so I've kept the general form of the
// tests from data_pipe_unittest.cc.

const uint32_t kSizeOfCreateOptions = sizeof(MojoCreateSharedBufferOptions);

// Does a cursory sanity check of |validated_options|. Calls
// |ValidateCreateOptions()| on already-validated options. The validated options
// should be valid, and the revalidated copy should be the same.
void RevalidateCreateOptions(
    const MojoCreateSharedBufferOptions& validated_options) {
  EXPECT_EQ(kSizeOfCreateOptions, validated_options.struct_size);
  // Nothing to check for flags.

  MojoCreateSharedBufferOptions revalidated_options = {};
  EXPECT_EQ(MOJO_RESULT_OK,
            SharedBufferDispatcher::ValidateCreateOptions(
                &validated_options, &revalidated_options));
  EXPECT_EQ(validated_options.struct_size, revalidated_options.struct_size);
  EXPECT_EQ(validated_options.flags, revalidated_options.flags);
}

class SharedBufferDispatcherTest : public testing::Test {
 public:
  SharedBufferDispatcherTest() {}
  ~SharedBufferDispatcherTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedBufferDispatcherTest);
};

// Tests valid inputs to |ValidateCreateOptions()|.
TEST_F(SharedBufferDispatcherTest, ValidateCreateOptionsValid) {
  // Default options.
  {
    MojoCreateSharedBufferOptions validated_options = {};
    EXPECT_EQ(MOJO_RESULT_OK, SharedBufferDispatcher::ValidateCreateOptions(
                                  nullptr, &validated_options));
    RevalidateCreateOptions(validated_options);
  }

  // Different flags.
  MojoCreateSharedBufferOptionsFlags flags_values[] = {
      MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE};
  for (size_t i = 0; i < arraysize(flags_values); i++) {
    const MojoCreateSharedBufferOptionsFlags flags = flags_values[i];

    // Different capacities (size 1).
    for (uint32_t capacity = 1; capacity <= 100 * 1000 * 1000; capacity *= 10) {
      MojoCreateSharedBufferOptions options = {
          kSizeOfCreateOptions,  // |struct_size|.
          flags                  // |flags|.
      };
      MojoCreateSharedBufferOptions validated_options = {};
      EXPECT_EQ(MOJO_RESULT_OK,
                SharedBufferDispatcher::ValidateCreateOptions(
                    &options, &validated_options))
          << capacity;
      RevalidateCreateOptions(validated_options);
      EXPECT_EQ(options.flags, validated_options.flags);
    }
  }
}

TEST_F(SharedBufferDispatcherTest, ValidateCreateOptionsInvalid) {
  // Invalid |struct_size|.
  {
    MojoCreateSharedBufferOptions options = {
        1,                                           // |struct_size|.
        MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE  // |flags|.
    };
    MojoCreateSharedBufferOptions unused;
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              SharedBufferDispatcher::ValidateCreateOptions(
                  &options, &unused));
  }

  // Unknown |flags|.
  {
    MojoCreateSharedBufferOptions options = {
        kSizeOfCreateOptions,  // |struct_size|.
        ~0u                    // |flags|.
    };
    MojoCreateSharedBufferOptions unused;
    EXPECT_EQ(MOJO_RESULT_UNIMPLEMENTED,
              SharedBufferDispatcher::ValidateCreateOptions(
                  &options, &unused));
  }
}

TEST_F(SharedBufferDispatcherTest, CreateAndMapBuffer) {
  scoped_refptr<SharedBufferDispatcher> dispatcher;
  EXPECT_EQ(MOJO_RESULT_OK, SharedBufferDispatcher::Create(
                                SharedBufferDispatcher::kDefaultCreateOptions,
                                nullptr, 100, &dispatcher));
  ASSERT_TRUE(dispatcher);
  EXPECT_EQ(Dispatcher::Type::SHARED_BUFFER, dispatcher->GetType());

  // Make a couple of mappings.
  std::unique_ptr<PlatformSharedBufferMapping> mapping1;
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->MapBuffer(
                                0, 100, MOJO_MAP_BUFFER_FLAG_NONE, &mapping1));
  ASSERT_TRUE(mapping1);
  ASSERT_TRUE(mapping1->GetBase());
  EXPECT_EQ(100u, mapping1->GetLength());
  // Write something.
  static_cast<char*>(mapping1->GetBase())[50] = 'x';

  std::unique_ptr<PlatformSharedBufferMapping> mapping2;
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->MapBuffer(
                                50, 50, MOJO_MAP_BUFFER_FLAG_NONE, &mapping2));
  ASSERT_TRUE(mapping2);
  ASSERT_TRUE(mapping2->GetBase());
  EXPECT_EQ(50u, mapping2->GetLength());
  EXPECT_EQ('x', static_cast<char*>(mapping2->GetBase())[0]);

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->Close());

  // Check that we can still read/write to mappings after the dispatcher has
  // gone away.
  static_cast<char*>(mapping2->GetBase())[1] = 'y';
  EXPECT_EQ('y', static_cast<char*>(mapping1->GetBase())[51]);
}

TEST_F(SharedBufferDispatcherTest, CreateAndMapBufferFromPlatformBuffer) {
  scoped_refptr<PlatformSharedBuffer> platform_shared_buffer =
      PlatformSharedBuffer::Create(100);
  ASSERT_TRUE(platform_shared_buffer);
  scoped_refptr<SharedBufferDispatcher> dispatcher;
  EXPECT_EQ(MOJO_RESULT_OK,
            SharedBufferDispatcher::CreateFromPlatformSharedBuffer(
                platform_shared_buffer, &dispatcher));
  ASSERT_TRUE(dispatcher);
  EXPECT_EQ(Dispatcher::Type::SHARED_BUFFER, dispatcher->GetType());

  // Make a couple of mappings.
  std::unique_ptr<PlatformSharedBufferMapping> mapping1;
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->MapBuffer(
                                0, 100, MOJO_MAP_BUFFER_FLAG_NONE, &mapping1));
  ASSERT_TRUE(mapping1);
  ASSERT_TRUE(mapping1->GetBase());
  EXPECT_EQ(100u, mapping1->GetLength());
  // Write something.
  static_cast<char*>(mapping1->GetBase())[50] = 'x';

  std::unique_ptr<PlatformSharedBufferMapping> mapping2;
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->MapBuffer(
                                50, 50, MOJO_MAP_BUFFER_FLAG_NONE, &mapping2));
  ASSERT_TRUE(mapping2);
  ASSERT_TRUE(mapping2->GetBase());
  EXPECT_EQ(50u, mapping2->GetLength());
  EXPECT_EQ('x', static_cast<char*>(mapping2->GetBase())[0]);

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->Close());

  // Check that we can still read/write to mappings after the dispatcher has
  // gone away.
  static_cast<char*>(mapping2->GetBase())[1] = 'y';
  EXPECT_EQ('y', static_cast<char*>(mapping1->GetBase())[51]);
}

TEST_F(SharedBufferDispatcherTest, DuplicateBufferHandle) {
  scoped_refptr<SharedBufferDispatcher> dispatcher1;
  EXPECT_EQ(MOJO_RESULT_OK, SharedBufferDispatcher::Create(
                                SharedBufferDispatcher::kDefaultCreateOptions,
                                nullptr, 100, &dispatcher1));

  // Map and write something.
  std::unique_ptr<PlatformSharedBufferMapping> mapping;
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher1->MapBuffer(
                                0, 100, MOJO_MAP_BUFFER_FLAG_NONE, &mapping));
  static_cast<char*>(mapping->GetBase())[0] = 'x';
  mapping.reset();

  // Duplicate |dispatcher1| and then close it.
  scoped_refptr<Dispatcher> dispatcher2;
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher1->DuplicateBufferHandle(
                                nullptr, &dispatcher2));
  ASSERT_TRUE(dispatcher2);
  EXPECT_EQ(Dispatcher::Type::SHARED_BUFFER, dispatcher2->GetType());

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher1->Close());

  // Map |dispatcher2| and read something.
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher2->MapBuffer(
                                0, 100, MOJO_MAP_BUFFER_FLAG_NONE, &mapping));
  EXPECT_EQ('x', static_cast<char*>(mapping->GetBase())[0]);

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher2->Close());
}

TEST_F(SharedBufferDispatcherTest, DuplicateBufferHandleOptionsValid) {
  scoped_refptr<SharedBufferDispatcher> dispatcher1;
  EXPECT_EQ(MOJO_RESULT_OK, SharedBufferDispatcher::Create(
                                SharedBufferDispatcher::kDefaultCreateOptions,
                                nullptr, 100, &dispatcher1));

  MojoDuplicateBufferHandleOptions options[] = {
      {sizeof(MojoDuplicateBufferHandleOptions),
       MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE},
      {sizeof(MojoDuplicateBufferHandleOptions),
       MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_READ_ONLY},
      {sizeof(MojoDuplicateBufferHandleOptionsFlags), ~0u}};
  for (size_t i = 0; i < arraysize(options); i++) {
    scoped_refptr<Dispatcher> dispatcher2;
    EXPECT_EQ(MOJO_RESULT_OK, dispatcher1->DuplicateBufferHandle(
                                  &options[i], &dispatcher2));
    ASSERT_TRUE(dispatcher2);
    EXPECT_EQ(Dispatcher::Type::SHARED_BUFFER, dispatcher2->GetType());
    {
      std::unique_ptr<PlatformSharedBufferMapping> mapping;
      EXPECT_EQ(MOJO_RESULT_OK, dispatcher2->MapBuffer(0, 100, 0, &mapping));
    }
    EXPECT_EQ(MOJO_RESULT_OK, dispatcher2->Close());
  }

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher1->Close());
}

TEST_F(SharedBufferDispatcherTest, DuplicateBufferHandleOptionsInvalid) {
  scoped_refptr<SharedBufferDispatcher> dispatcher1;
  EXPECT_EQ(MOJO_RESULT_OK, SharedBufferDispatcher::Create(
                                SharedBufferDispatcher::kDefaultCreateOptions,
                                nullptr, 100, &dispatcher1));

  // Invalid |struct_size|.
  {
    MojoDuplicateBufferHandleOptions options = {
        1u, MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE};
    scoped_refptr<Dispatcher> dispatcher2;
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              dispatcher1->DuplicateBufferHandle(&options, &dispatcher2));
    EXPECT_FALSE(dispatcher2);
  }

  // Unknown |flags|.
  {
    MojoDuplicateBufferHandleOptions options = {
        sizeof(MojoDuplicateBufferHandleOptions), ~0u};
    scoped_refptr<Dispatcher> dispatcher2;
    EXPECT_EQ(MOJO_RESULT_UNIMPLEMENTED,
              dispatcher1->DuplicateBufferHandle(&options, &dispatcher2));
    EXPECT_FALSE(dispatcher2);
  }

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher1->Close());
}

TEST_F(SharedBufferDispatcherTest, CreateInvalidNumBytes) {
  // Size too big.
  scoped_refptr<SharedBufferDispatcher> dispatcher;
  EXPECT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            SharedBufferDispatcher::Create(
                SharedBufferDispatcher::kDefaultCreateOptions, nullptr,
                std::numeric_limits<uint64_t>::max(), &dispatcher));
  EXPECT_FALSE(dispatcher);

  // Zero size.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            SharedBufferDispatcher::Create(
                SharedBufferDispatcher::kDefaultCreateOptions, nullptr, 0,
                &dispatcher));
  EXPECT_FALSE(dispatcher);
}

TEST_F(SharedBufferDispatcherTest, MapBufferInvalidArguments) {
  scoped_refptr<SharedBufferDispatcher> dispatcher;
  EXPECT_EQ(MOJO_RESULT_OK, SharedBufferDispatcher::Create(
                                SharedBufferDispatcher::kDefaultCreateOptions,
                                nullptr, 100, &dispatcher));

  std::unique_ptr<PlatformSharedBufferMapping> mapping;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dispatcher->MapBuffer(0, 101, MOJO_MAP_BUFFER_FLAG_NONE, &mapping));
  EXPECT_FALSE(mapping);

  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dispatcher->MapBuffer(1, 100, MOJO_MAP_BUFFER_FLAG_NONE, &mapping));
  EXPECT_FALSE(mapping);

  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dispatcher->MapBuffer(0, 0, MOJO_MAP_BUFFER_FLAG_NONE, &mapping));
  EXPECT_FALSE(mapping);

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->Close());
}

}  // namespace
}  // namespace edk
}  // namespace mojo

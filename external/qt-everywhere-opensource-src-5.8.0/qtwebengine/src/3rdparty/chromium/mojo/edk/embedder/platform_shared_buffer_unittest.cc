// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/platform_shared_buffer.h"

#include <stddef.h>

#include <limits>
#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/sys_info.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace mojo {
namespace edk {
namespace {

TEST(PlatformSharedBufferTest, Basic) {
  const size_t kNumInts = 100;
  const size_t kNumBytes = kNumInts * sizeof(int);
  // A fudge so that we're not just writing zero bytes 75% of the time.
  const int kFudge = 1234567890;

  // Make some memory.
  scoped_refptr<PlatformSharedBuffer> buffer(
      PlatformSharedBuffer::Create(kNumBytes));
  ASSERT_TRUE(buffer);

  // Map it all, scribble some stuff, and then unmap it.
  {
    EXPECT_TRUE(buffer->IsValidMap(0, kNumBytes));
    std::unique_ptr<PlatformSharedBufferMapping> mapping(
        buffer->Map(0, kNumBytes));
    ASSERT_TRUE(mapping);
    ASSERT_TRUE(mapping->GetBase());
    int* stuff = static_cast<int*>(mapping->GetBase());
    for (size_t i = 0; i < kNumInts; i++)
      stuff[i] = static_cast<int>(i) + kFudge;
  }

  // Map it all again, check that our scribbling is still there, then do a
  // partial mapping and scribble on that, check that everything is coherent,
  // unmap the first mapping, scribble on some of the second mapping, and then
  // unmap it.
  {
    ASSERT_TRUE(buffer->IsValidMap(0, kNumBytes));
    // Use |MapNoCheck()| this time.
    std::unique_ptr<PlatformSharedBufferMapping> mapping1(
        buffer->MapNoCheck(0, kNumBytes));
    ASSERT_TRUE(mapping1);
    ASSERT_TRUE(mapping1->GetBase());
    int* stuff1 = static_cast<int*>(mapping1->GetBase());
    for (size_t i = 0; i < kNumInts; i++)
      EXPECT_EQ(static_cast<int>(i) + kFudge, stuff1[i]) << i;

    std::unique_ptr<PlatformSharedBufferMapping> mapping2(
        buffer->Map((kNumInts / 2) * sizeof(int), 2 * sizeof(int)));
    ASSERT_TRUE(mapping2);
    ASSERT_TRUE(mapping2->GetBase());
    int* stuff2 = static_cast<int*>(mapping2->GetBase());
    EXPECT_EQ(static_cast<int>(kNumInts / 2) + kFudge, stuff2[0]);
    EXPECT_EQ(static_cast<int>(kNumInts / 2) + 1 + kFudge, stuff2[1]);

    stuff2[0] = 123;
    stuff2[1] = 456;
    EXPECT_EQ(123, stuff1[kNumInts / 2]);
    EXPECT_EQ(456, stuff1[kNumInts / 2 + 1]);

    mapping1.reset();

    EXPECT_EQ(123, stuff2[0]);
    EXPECT_EQ(456, stuff2[1]);
    stuff2[1] = 789;
  }

  // Do another partial mapping and check that everything is the way we expect
  // it to be.
  {
    EXPECT_TRUE(buffer->IsValidMap(sizeof(int), kNumBytes - sizeof(int)));
    std::unique_ptr<PlatformSharedBufferMapping> mapping(
        buffer->Map(sizeof(int), kNumBytes - sizeof(int)));
    ASSERT_TRUE(mapping);
    ASSERT_TRUE(mapping->GetBase());
    int* stuff = static_cast<int*>(mapping->GetBase());

    for (size_t j = 0; j < kNumInts - 1; j++) {
      int i = static_cast<int>(j) + 1;
      if (i == kNumInts / 2) {
        EXPECT_EQ(123, stuff[j]);
      } else if (i == kNumInts / 2 + 1) {
        EXPECT_EQ(789, stuff[j]);
      } else {
        EXPECT_EQ(i + kFudge, stuff[j]) << i;
      }
    }
  }
}

// TODO(vtl): Bigger buffers.

TEST(PlatformSharedBufferTest, InvalidMappings) {
  scoped_refptr<PlatformSharedBuffer> buffer(PlatformSharedBuffer::Create(100));
  ASSERT_TRUE(buffer);

  // Zero length not allowed.
  EXPECT_FALSE(buffer->Map(0, 0));
  EXPECT_FALSE(buffer->IsValidMap(0, 0));

  // Okay:
  EXPECT_TRUE(buffer->Map(0, 100));
  EXPECT_TRUE(buffer->IsValidMap(0, 100));
  // Offset + length too big.
  EXPECT_FALSE(buffer->Map(0, 101));
  EXPECT_FALSE(buffer->IsValidMap(0, 101));
  EXPECT_FALSE(buffer->Map(1, 100));
  EXPECT_FALSE(buffer->IsValidMap(1, 100));

  // Okay:
  EXPECT_TRUE(buffer->Map(50, 50));
  EXPECT_TRUE(buffer->IsValidMap(50, 50));
  // Offset + length too big.
  EXPECT_FALSE(buffer->Map(50, 51));
  EXPECT_FALSE(buffer->IsValidMap(50, 51));
  EXPECT_FALSE(buffer->Map(51, 50));
  EXPECT_FALSE(buffer->IsValidMap(51, 50));
}

TEST(PlatformSharedBufferTest, TooBig) {
  // If |size_t| is 32-bit, it's quite possible/likely that |Create()| succeeds
  // (since it only involves creating a 4 GB file).
  size_t max_size = std::numeric_limits<size_t>::max();
  if (base::SysInfo::AmountOfVirtualMemory() &&
      max_size > static_cast<size_t>(base::SysInfo::AmountOfVirtualMemory()))
    max_size = static_cast<size_t>(base::SysInfo::AmountOfVirtualMemory());
  scoped_refptr<PlatformSharedBuffer> buffer(
      PlatformSharedBuffer::Create(max_size));
  // But, assuming |sizeof(size_t) == sizeof(void*)|, mapping all of it should
  // always fail.
  if (buffer)
    EXPECT_FALSE(buffer->Map(0, max_size));
}

// Tests that separate mappings get distinct addresses.
// Note: It's not inconceivable that the OS could ref-count identical mappings
// and reuse the same address, in which case we'd have to be more careful about
// using the address as the key for unmapping.
TEST(PlatformSharedBufferTest, MappingsDistinct) {
  scoped_refptr<PlatformSharedBuffer> buffer(PlatformSharedBuffer::Create(100));
  std::unique_ptr<PlatformSharedBufferMapping> mapping1(buffer->Map(0, 100));
  std::unique_ptr<PlatformSharedBufferMapping> mapping2(buffer->Map(0, 100));
  EXPECT_NE(mapping1->GetBase(), mapping2->GetBase());
}

TEST(PlatformSharedBufferTest, BufferZeroInitialized) {
  static const size_t kSizes[] = {10, 100, 1000, 10000, 100000};
  for (size_t i = 0; i < arraysize(kSizes); i++) {
    scoped_refptr<PlatformSharedBuffer> buffer(
        PlatformSharedBuffer::Create(kSizes[i]));
    std::unique_ptr<PlatformSharedBufferMapping> mapping(
        buffer->Map(0, kSizes[i]));
    for (size_t j = 0; j < kSizes[i]; j++) {
      // "Assert" instead of "expect" so we don't spam the output with thousands
      // of failures if we fail.
      ASSERT_EQ('\0', static_cast<char*>(mapping->GetBase())[j])
          << "size " << kSizes[i] << ", offset " << j;
    }
  }
}

TEST(PlatformSharedBufferTest, MappingsOutliveBuffer) {
  std::unique_ptr<PlatformSharedBufferMapping> mapping1;
  std::unique_ptr<PlatformSharedBufferMapping> mapping2;

  {
    scoped_refptr<PlatformSharedBuffer> buffer(
        PlatformSharedBuffer::Create(100));
    mapping1 = buffer->Map(0, 100);
    mapping2 = buffer->Map(50, 50);
    static_cast<char*>(mapping1->GetBase())[50] = 'x';
  }

  EXPECT_EQ('x', static_cast<char*>(mapping2->GetBase())[0]);

  static_cast<char*>(mapping2->GetBase())[1] = 'y';
  EXPECT_EQ('y', static_cast<char*>(mapping1->GetBase())[51]);
}

TEST(PlatformSharedBufferTest, FromSharedMemoryHandle) {
  const size_t kBufferSize = 1234;
  base::SharedMemoryCreateOptions options;
  options.size = kBufferSize;
  base::SharedMemory shared_memory;
  ASSERT_TRUE(shared_memory.Create(options));
  ASSERT_TRUE(shared_memory.Map(kBufferSize));

  base::SharedMemoryHandle shm_handle =
      base::SharedMemory::DuplicateHandle(shared_memory.handle());
  scoped_refptr<PlatformSharedBuffer> simple_buffer(
      PlatformSharedBuffer::CreateFromSharedMemoryHandle(
          kBufferSize, false /* read_only */, shm_handle));
  ASSERT_TRUE(simple_buffer);

  std::unique_ptr<PlatformSharedBufferMapping> mapping =
      simple_buffer->Map(0, kBufferSize);
  ASSERT_TRUE(mapping);

  const int kOffset = 123;
  char* base_memory = static_cast<char*>(shared_memory.memory());
  char* mojo_memory = static_cast<char*>(mapping->GetBase());
  base_memory[kOffset] = 0;
  EXPECT_EQ(0, mojo_memory[kOffset]);
  base_memory[kOffset] = 'a';
  EXPECT_EQ('a', mojo_memory[kOffset]);
  mojo_memory[kOffset] = 'z';
  EXPECT_EQ('z', base_memory[kOffset]);
}

}  // namespace
}  // namespace edk
}  // namespace mojo

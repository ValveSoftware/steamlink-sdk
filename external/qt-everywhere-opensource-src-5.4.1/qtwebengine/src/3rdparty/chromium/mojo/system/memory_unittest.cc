// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/memory.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "mojo/public/c/system/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

TEST(MemoryTest, Valid) {
  char my_char;
  int32_t my_int32;
  int64_t my_int64;
  char my_char_array[5];
  int32_t my_int32_array[5];
  int64_t my_int64_array[5];

  // |VerifyUserPointer|:

  EXPECT_TRUE(VerifyUserPointer<char>(&my_char));
  EXPECT_TRUE(VerifyUserPointer<int32_t>(&my_int32));
  EXPECT_TRUE(VerifyUserPointer<int64_t>(&my_int64));

  // |VerifyUserPointerWithCount|:

  EXPECT_TRUE(VerifyUserPointerWithCount<char>(my_char_array, 5));
  EXPECT_TRUE(VerifyUserPointerWithCount<int32_t>(my_int32_array, 5));
  EXPECT_TRUE(VerifyUserPointerWithCount<int64_t>(my_int64_array, 5));

  // It shouldn't care what the pointer is if the count is zero.
  EXPECT_TRUE(VerifyUserPointerWithCount<char>(NULL, 0));
  EXPECT_TRUE(VerifyUserPointerWithCount<int32_t>(NULL, 0));
  EXPECT_TRUE(VerifyUserPointerWithCount<int32_t>(
                  reinterpret_cast<const int32_t*>(1), 0));
  EXPECT_TRUE(VerifyUserPointerWithCount<int64_t>(NULL, 0));
  EXPECT_TRUE(VerifyUserPointerWithCount<int64_t>(
                  reinterpret_cast<const int64_t*>(1), 0));

  // |VerifyUserPointerWithSize|:

  EXPECT_TRUE(VerifyUserPointerWithSize<1>(&my_char, sizeof(my_char)));
  EXPECT_TRUE(VerifyUserPointerWithSize<1>(&my_int32, sizeof(my_int32)));
  EXPECT_TRUE(VerifyUserPointerWithSize<MOJO_ALIGNOF(int32_t)>(
                  &my_int32, sizeof(my_int32)));
  EXPECT_TRUE(VerifyUserPointerWithSize<1>(&my_int64, sizeof(my_int64)));
  EXPECT_TRUE(VerifyUserPointerWithSize<MOJO_ALIGNOF(int64_t)>(
                  &my_int64, sizeof(my_int64)));

  EXPECT_TRUE(VerifyUserPointerWithSize<1>(my_char_array,
                                           sizeof(my_char_array)));
  EXPECT_TRUE(VerifyUserPointerWithSize<1>(my_int32_array,
                                           sizeof(my_int32_array)));
  EXPECT_TRUE(VerifyUserPointerWithSize<MOJO_ALIGNOF(int32_t)>(
                  my_int32_array, sizeof(my_int32_array)));
  EXPECT_TRUE(VerifyUserPointerWithSize<1>(my_int64_array,
                                           sizeof(my_int64_array)));
  EXPECT_TRUE(VerifyUserPointerWithSize<MOJO_ALIGNOF(int64_t)>(
                  my_int64_array, sizeof(my_int64_array)));
}

TEST(MemoryTest, Invalid) {
  // Note: |VerifyUserPointer...()| are defined to be "best effort" checks (and
  // may always return true). Thus these tests of invalid cases only reflect the
  // current implementation.

  // These tests depend on |int32_t| and |int64_t| having nontrivial alignment.
  MOJO_COMPILE_ASSERT(MOJO_ALIGNOF(int32_t) != 1,
                      int32_t_does_not_have_to_be_aligned);
  MOJO_COMPILE_ASSERT(MOJO_ALIGNOF(int64_t) != 1,
                      int64_t_does_not_have_to_be_aligned);

  int32_t my_int32;
  int64_t my_int64;

  // |VerifyUserPointer|:

  EXPECT_FALSE(VerifyUserPointer<char>(NULL));
  EXPECT_FALSE(VerifyUserPointer<int32_t>(NULL));
  EXPECT_FALSE(VerifyUserPointer<int64_t>(NULL));

  // Unaligned:
  EXPECT_FALSE(VerifyUserPointer<int32_t>(reinterpret_cast<const int32_t*>(1)));
  EXPECT_FALSE(VerifyUserPointer<int64_t>(reinterpret_cast<const int64_t*>(1)));

  // |VerifyUserPointerWithCount|:

  EXPECT_FALSE(VerifyUserPointerWithCount<char>(NULL, 1));
  EXPECT_FALSE(VerifyUserPointerWithCount<int32_t>(NULL, 1));
  EXPECT_FALSE(VerifyUserPointerWithCount<int64_t>(NULL, 1));

  // Unaligned:
  EXPECT_FALSE(VerifyUserPointerWithCount<int32_t>(
                   reinterpret_cast<const int32_t*>(1), 1));
  EXPECT_FALSE(VerifyUserPointerWithCount<int64_t>(
                   reinterpret_cast<const int64_t*>(1), 1));

  // Count too big:
  EXPECT_FALSE(VerifyUserPointerWithCount<int32_t>(
                   &my_int32, std::numeric_limits<size_t>::max()));
  EXPECT_FALSE(VerifyUserPointerWithCount<int64_t>(
                   &my_int64, std::numeric_limits<size_t>::max()));

  // |VerifyUserPointerWithSize|:

  EXPECT_FALSE(VerifyUserPointerWithSize<1>(NULL, 1));
  EXPECT_FALSE(VerifyUserPointerWithSize<4>(NULL, 1));
  EXPECT_FALSE(VerifyUserPointerWithSize<4>(NULL, 4));
  EXPECT_FALSE(VerifyUserPointerWithSize<8>(NULL, 1));
  EXPECT_FALSE(VerifyUserPointerWithSize<8>(NULL, 4));
  EXPECT_FALSE(VerifyUserPointerWithSize<8>(NULL, 8));

  // Unaligned:
  EXPECT_FALSE(VerifyUserPointerWithSize<4>(reinterpret_cast<const int32_t*>(1),
                                            1));
  EXPECT_FALSE(VerifyUserPointerWithSize<4>(reinterpret_cast<const int32_t*>(1),
                                            4));
  EXPECT_FALSE(VerifyUserPointerWithSize<8>(reinterpret_cast<const int32_t*>(1),
                                            1));
  EXPECT_FALSE(VerifyUserPointerWithSize<8>(reinterpret_cast<const int32_t*>(1),
                                            4));
  EXPECT_FALSE(VerifyUserPointerWithSize<8>(reinterpret_cast<const int32_t*>(1),
                                            8));
}

}  // namespace
}  // namespace system
}  // namespace mojo

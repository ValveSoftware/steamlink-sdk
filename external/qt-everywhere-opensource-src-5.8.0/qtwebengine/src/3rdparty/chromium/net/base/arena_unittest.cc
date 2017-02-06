// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/arena.h"

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;

namespace net {
namespace {

size_t kDefaultBlockSize = 2048;
const char kTestString[] = "This is a decently long test string.";

TEST(UnsafeArenaTest, Memdup) {
  UnsafeArena arena(kDefaultBlockSize);
  const size_t length = strlen(kTestString);
  char* c = arena.Memdup(kTestString, length);
  EXPECT_NE(nullptr, c);
  EXPECT_NE(c, kTestString);
  EXPECT_EQ(StringPiece(c, length), kTestString);
}

TEST(UnsafeArenaTest, MemdupLargeString) {
  UnsafeArena arena(10 /* block size */);
  const size_t length = strlen(kTestString);
  char* c = arena.Memdup(kTestString, length);
  EXPECT_NE(nullptr, c);
  EXPECT_NE(c, kTestString);
  EXPECT_EQ(StringPiece(c, length), kTestString);
}

TEST(UnsafeArenaTest, MultipleBlocks) {
  UnsafeArena arena(40 /* block size */);
  std::vector<std::string> strings = {
      "One decently long string.", "Another string.",
      "A third string that will surely go in a different block."};
  std::vector<StringPiece> copies;
  for (const std::string& s : strings) {
    StringPiece sp(arena.Memdup(s.data(), s.size()), s.size());
    copies.push_back(sp);
  }
  EXPECT_EQ(strings.size(), copies.size());
  for (size_t i = 0; i < strings.size(); ++i) {
    EXPECT_EQ(copies[i], strings[i]);
  }
}

TEST(UnsafeArenaTest, UseAfterReset) {
  UnsafeArena arena(kDefaultBlockSize);
  const size_t length = strlen(kTestString);
  char* c = arena.Memdup(kTestString, length);
  arena.Reset();
  c = arena.Memdup(kTestString, length);
  EXPECT_NE(nullptr, c);
  EXPECT_NE(c, kTestString);
  EXPECT_EQ(StringPiece(c, length), kTestString);
}

TEST(UnsafeArenaTest, Free) {
  UnsafeArena arena(kDefaultBlockSize);
  const size_t length = strlen(kTestString);
  // Freeing memory not owned by the arena should be a no-op, and freeing
  // before any allocations from the arena should be a no-op.
  arena.Free(const_cast<char*>(kTestString), length);
  char* c1 = arena.Memdup("Foo", 3);
  char* c2 = arena.Memdup(kTestString, length);
  arena.Free(const_cast<char*>(kTestString), length);
  char* c3 = arena.Memdup("Bar", 3);
  char* c4 = arena.Memdup(kTestString, length);
  EXPECT_NE(c1, c2);
  EXPECT_NE(c1, c3);
  EXPECT_NE(c1, c4);
  EXPECT_NE(c2, c3);
  EXPECT_NE(c2, c4);
  EXPECT_NE(c3, c4);
  // Freeing c4 should succeed, since it was the most recent allocation.
  arena.Free(c4, length);
  // Freeing c2 should be a no-op.
  arena.Free(c2, length);
  // c5 should reuse memory that was previously used by c4.
  char* c5 = arena.Memdup("Baz", 3);
  EXPECT_EQ(c4, c5);
}

}  // namespace
}  // namespace net

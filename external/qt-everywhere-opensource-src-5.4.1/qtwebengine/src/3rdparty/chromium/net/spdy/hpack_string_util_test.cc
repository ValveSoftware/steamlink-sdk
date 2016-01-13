// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_string_util.h"

#include <cstddef>
#include <cstring>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using std::string;

// Make sure StringPiecesEqualConstantTime() behaves like the regular
// string equality operator.
TEST(HpackStringUtilTest, StringPiecesEqualConstantTime) {
  EXPECT_TRUE(StringPiecesEqualConstantTime("foo", "foo"));
  EXPECT_FALSE(StringPiecesEqualConstantTime("foo", "foox"));
  EXPECT_FALSE(StringPiecesEqualConstantTime("foo", "bar"));
}

// TODO(jgraettinger): Support this benchmark.
/*
enum BM_StringPieceEqualityType {
  STRCMP_EQUAL,
  STRCMP_FIRST_CHAR_DIFFERS,
  STRING_PIECES_EQUAL_CONSTANT_TIME_EQUAL,
  STRING_PIECES_EQUAL_CONSTANT_TIME_FIRST_CHAR_DIFFERS,
};

void BM_StringPieceEquality(int iters, int size, int type_int) {
  BM_StringPieceEqualityType type =
      static_cast<BM_StringPieceEqualityType>(type_int);
  string str_a(size, 'x');
  string str_b(size, 'x');
  int result = 0;
  switch (type) {
    case STRCMP_EQUAL:
      for (int i = 0; i < iters; ++i) {
        result |= std::strcmp(str_a.c_str(), str_b.c_str());
      }
      CHECK_EQ(result, 0);
      return;

    case STRCMP_FIRST_CHAR_DIFFERS:
      str_b[0] = 'y';
      for (int i = 0; i < iters; ++i) {
        result |= std::strcmp(str_a.c_str(), str_b.c_str());
      }
      CHECK_LT(result, 0);
      return;

    case STRING_PIECES_EQUAL_CONSTANT_TIME_EQUAL:
      for (int i = 0; i < iters; ++i) {
        result |= StringPiecesEqualConstantTime(str_a, str_b);
      }
      CHECK_EQ(result, 1);
      return;

    case STRING_PIECES_EQUAL_CONSTANT_TIME_FIRST_CHAR_DIFFERS:
      str_b[0] = 'y';
      for (int i = 0; i < iters; ++i) {
        result |= StringPiecesEqualConstantTime(str_a, str_b);
      }
      CHECK_EQ(result, 0);
      return;
  }

  DCHECK(false);
}

// Results should resemble the table below, where 0 and 1 are clearly
// different (STRCMP), but 2 and 3 are roughly the same
// (STRING_PIECES_EQUAL_CONSTANT_TIME).
//
// DEBUG: Benchmark                     Time(ns)    CPU(ns) Iterations
// -------------------------------------------------------------------
// DEBUG: BM_StringPieceEquality/1M/0      77796      77141       7778
// DEBUG: BM_StringPieceEquality/1M/1         10         10   70000000
// DEBUG: BM_StringPieceEquality/1M/2    7729735    7700000        100
// DEBUG: BM_StringPieceEquality/1M/3    7803051    7800000        100
BENCHMARK(BM_StringPieceEquality)
  ->ArgPair(1<<20, STRCMP_EQUAL)
  ->ArgPair(1<<20, STRCMP_FIRST_CHAR_DIFFERS)
  ->ArgPair(1<<20, STRING_PIECES_EQUAL_CONSTANT_TIME_EQUAL)
  ->ArgPair(1<<20, STRING_PIECES_EQUAL_CONSTANT_TIME_FIRST_CHAR_DIFFERS);
*/

}  // namespace

}  // namespace net

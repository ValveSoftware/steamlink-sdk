// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/delta_encoding.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace blimp {
namespace {

// Provides an example of field elision across structs.
// For each CompoundStruct element in a sequence:
//    |str| overrides the previous element's |str|, if set.
//    |str| inherits the previous element's |str|, if empty.
//    |number| is computed as a delta from the previous element's |number|.
struct CompoundStruct {
  static CompoundStruct Difference(const CompoundStruct& lhs,
                                   const CompoundStruct& rhs) {
    CompoundStruct output;
    output.number = lhs.number - rhs.number;
    if (lhs.str != rhs.str) {
      output.str = lhs.str;
    }
    return output;
  }

  static CompoundStruct Merge(const CompoundStruct& lhs,
                              const CompoundStruct& rhs) {
    CompoundStruct output;
    output.number = lhs.number + rhs.number;
    output.str = (rhs.str.empty() ? lhs.str : rhs.str);
    return output;
  }

  // Sortable by |number|.
  friend bool operator<(const CompoundStruct& lhs, const CompoundStruct& rhs) {
    return lhs.number < rhs.number;
  }

  int number;
  std::string str;
};

class TestDeltaEncoding : public testing::Test {
 public:
  TestDeltaEncoding() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDeltaEncoding);
};

TEST_F(TestDeltaEncoding, DeltaEncode) {
  std::vector<int> test{5, 1, 3};
  DeltaEncode(test.begin(), test.end());
  EXPECT_THAT(test, ElementsAre(5, -4, 2));
}

TEST_F(TestDeltaEncoding, DeltaEncodeEmpty) {
  std::vector<int> test;
  DeltaEncode(test.begin(), test.end());
  EXPECT_TRUE(test.empty());
}

TEST_F(TestDeltaEncoding, DeltaEncodeSingleValue) {
  std::vector<int> test;
  test.push_back(1);
  DeltaEncode(test.begin(), test.end());
  EXPECT_THAT(test, ElementsAre(1));
}

TEST_F(TestDeltaEncoding, DeltaDecode) {
  std::vector<int> test{1, 1, 2};
  DeltaDecode(test.begin(), test.end());
  EXPECT_THAT(test, ElementsAre(1, 2, 4));
}

TEST_F(TestDeltaEncoding, DeltaDecodeEmpty) {
  std::vector<int> test;
  DeltaDecode(test.begin(), test.end());
  EXPECT_TRUE(test.empty());
}

TEST_F(TestDeltaEncoding, DeltaDecodeSingleValue) {
  std::vector<int> test{1};
  DeltaEncode(test.begin(), test.end());
  EXPECT_THAT(test, ElementsAre(1));
}

TEST_F(TestDeltaEncoding, SortAndDeltaEncodeRoundTrip) {
  std::vector<int> test{8, 3, 5};
  SortAndDeltaEncode(test.begin(), test.end());
  EXPECT_THAT(test, ElementsAre(3, 2, 3));
  DeltaDecode(test.begin(), test.end());
  EXPECT_THAT(test, ElementsAre(3, 5, 8));
}

TEST_F(TestDeltaEncoding, DeltaEncodeStruct) {
  std::vector<CompoundStruct> test{{2, "foo"}, {3, "foo"}, {0, "bar"}};
  DeltaEncode(test.begin(), test.end(), &CompoundStruct::Difference);

  EXPECT_EQ(2, test[0].number);
  EXPECT_EQ(1, test[1].number);
  EXPECT_EQ(-3, test[2].number);

  EXPECT_EQ("foo", test[0].str);
  EXPECT_EQ("", test[1].str);
  EXPECT_EQ("bar", test[2].str);
}

TEST_F(TestDeltaEncoding, DeltaDecodeStruct) {
  std::vector<CompoundStruct> test{{2, "foo"}, {-1, ""}, {0, "bar"}};
  DeltaDecode(test.begin(), test.end(), &CompoundStruct::Merge);

  EXPECT_EQ(2, test[0].number);
  EXPECT_EQ(1, test[1].number);
  EXPECT_EQ(1, test[2].number);

  EXPECT_EQ("foo", test[0].str);
  EXPECT_EQ("foo", test[1].str);
  EXPECT_EQ("bar", test[2].str);
}

TEST_F(TestDeltaEncoding, SortAndDeltaEncodeStructRoundTrip) {
  std::vector<CompoundStruct> test{{2, "foo"}, {3, "foo"}, {0, "bar"}};
  SortAndDeltaEncode(test.begin(), test.end(), &CompoundStruct::Difference);

  EXPECT_EQ(0, test[0].number);
  EXPECT_EQ(2, test[1].number);
  EXPECT_EQ(1, test[2].number);

  EXPECT_EQ("bar", test[0].str);
  EXPECT_EQ("foo", test[1].str);
  EXPECT_EQ("", test[2].str);

  DeltaDecode(test.begin(), test.end(), &CompoundStruct::Merge);
  EXPECT_EQ(0, test[0].number);
  EXPECT_EQ(2, test[1].number);
  EXPECT_EQ(3, test[2].number);

  EXPECT_EQ("bar", test[0].str);
  EXPECT_EQ("foo", test[1].str);
  EXPECT_EQ("foo", test[2].str);
}

}  // namespace
}  // namespace blimp

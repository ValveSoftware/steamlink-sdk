// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/json/JSONValues.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {

class JSONValueDeletionVerifier : public JSONValue {
 public:
  JSONValueDeletionVerifier(int& counter) : m_counter(counter) {}

  ~JSONValueDeletionVerifier() override { ++m_counter; }

 private:
  int& m_counter;
};

}  // namespace

TEST(JSONValuesTest, ArrayCastDoesNotLeak) {
  int deletionCount = 0;
  std::unique_ptr<JSONValueDeletionVerifier> notAnArray(
      new JSONValueDeletionVerifier(deletionCount));
  EXPECT_EQ(nullptr, JSONArray::from(std::move(notAnArray)));
  EXPECT_EQ(1, deletionCount);
}

TEST(JSONValuesTest, ObjectCastDoesNotLeak) {
  int deletionCount = 0;
  std::unique_ptr<JSONValueDeletionVerifier> notAnObject(
      new JSONValueDeletionVerifier(deletionCount));
  EXPECT_EQ(nullptr, JSONArray::from(std::move(notAnObject)));
  EXPECT_EQ(1, deletionCount);
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DocumentUserGestureToken.h"

#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class DocumentUserGestureTokenTest : public ::testing::Test {
 public:
  void SetUp() override {
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    ASSERT_FALSE(document().hasReceivedUserGesture());
  }

  Document& document() const { return m_dummyPageHolder->document(); }

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

TEST_F(DocumentUserGestureTokenTest, NoGesture) {
  // A nullptr Document* will not set user gesture state.
  DocumentUserGestureToken::create(nullptr);
  EXPECT_FALSE(document().hasReceivedUserGesture());
}

TEST_F(DocumentUserGestureTokenTest, PossiblyExisting) {
  // A non-null Document* will set state, but a subsequent nullptr Document*
  // token will not override it.
  DocumentUserGestureToken::create(&document());
  EXPECT_TRUE(document().hasReceivedUserGesture());
  DocumentUserGestureToken::create(nullptr);
  EXPECT_TRUE(document().hasReceivedUserGesture());
}

TEST_F(DocumentUserGestureTokenTest, NewGesture) {
  // UserGestureToken::Status doesn't impact Document gesture state.
  DocumentUserGestureToken::create(&document(), UserGestureToken::NewGesture);
  EXPECT_TRUE(document().hasReceivedUserGesture());
}

}  // namespace blink

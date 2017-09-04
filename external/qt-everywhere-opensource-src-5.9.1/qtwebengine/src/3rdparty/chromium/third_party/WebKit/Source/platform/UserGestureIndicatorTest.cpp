// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/UserGestureIndicator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/CurrentTime.h"

namespace blink {

static double s_currentTime = 1000.0;

static void advanceClock(double seconds) {
  s_currentTime += seconds;
}

static double mockTimeFunction() {
  return s_currentTime;
}

class TestUserGestureToken final : public UserGestureToken {
  WTF_MAKE_NONCOPYABLE(TestUserGestureToken);

 public:
  static PassRefPtr<UserGestureToken> create(
      Status status = PossiblyExistingGesture) {
    return adoptRef(new TestUserGestureToken(status));
  }

 private:
  TestUserGestureToken(Status status) : UserGestureToken(status) {}
};

// Checks for the initial state of UserGestureIndicator.
TEST(UserGestureIndicatorTest, InitialState) {
  EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_EQ(nullptr, UserGestureIndicator::currentToken());
  EXPECT_FALSE(UserGestureIndicator::consumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithNewUserGesture) {
  UserGestureIndicator userGestureScope(
      TestUserGestureToken::create(UserGestureToken::NewGesture));

  EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::currentToken());

  EXPECT_TRUE(UserGestureIndicator::consumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithUserGesture) {
  UserGestureIndicator userGestureScope(TestUserGestureToken::create());

  EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::currentToken());

  EXPECT_TRUE(UserGestureIndicator::consumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithNoUserGesture) {
  UserGestureIndicator userGestureScope(nullptr);

  EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_EQ(nullptr, UserGestureIndicator::currentToken());

  EXPECT_FALSE(UserGestureIndicator::consumeUserGesture());
}

// Check that after UserGestureIndicator destruction state will be cleared.
TEST(UserGestureIndicatorTest, DestructUserGestureIndicator) {
  {
    UserGestureIndicator userGestureScope(TestUserGestureToken::create());

    EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_NE(nullptr, UserGestureIndicator::currentToken());
  }

  EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_EQ(nullptr, UserGestureIndicator::currentToken());
  EXPECT_FALSE(UserGestureIndicator::consumeUserGesture());
}

// Tests creation of scoped UserGestureIndicator objects.
TEST(UserGestureIndicatorTest, ScopedNewUserGestureIndicators) {
  // Root GestureIndicator and GestureToken.
  UserGestureIndicator userGestureScope(
      TestUserGestureToken::create(UserGestureToken::NewGesture));

  EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::currentToken());
  {
    // Construct inner UserGestureIndicator.
    // It should share GestureToken with the root indicator.
    UserGestureIndicator innerUserGesture(
        TestUserGestureToken::create(UserGestureToken::NewGesture));

    EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_NE(nullptr, UserGestureIndicator::currentToken());

    // Consume inner gesture.
    EXPECT_TRUE(UserGestureIndicator::consumeUserGesture());
  }

  EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::currentToken());

  // Consume root gesture.
  EXPECT_TRUE(UserGestureIndicator::consumeUserGesture());
  EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::currentToken());
}

TEST(UserGestureIndicatorTest, MultipleGesturesWithTheSameToken) {
  UserGestureIndicator indicator(
      TestUserGestureToken::create(UserGestureToken::NewGesture));
  EXPECT_TRUE(UserGestureIndicator::processingUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::currentToken());
  {
    // Construct an inner indicator that shares the same token.
    UserGestureIndicator innerIndicator(UserGestureIndicator::currentToken());
    EXPECT_TRUE(UserGestureIndicator::processingUserGesture());
    EXPECT_NE(nullptr, UserGestureIndicator::currentToken());
  }
  // Though the inner indicator was destroyed, the outer is still present (and
  // the gesture hasn't been consumed), so it should still be processing a user
  // gesture.
  EXPECT_TRUE(UserGestureIndicator::processingUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::currentToken());
}

class UsedCallback : public UserGestureUtilizedCallback {
 public:
  UsedCallback() : m_usedCount(0) {}

  void userGestureUtilized() override { m_usedCount++; }

  unsigned getAndResetUsedCount() {
    unsigned curCount = m_usedCount;
    m_usedCount = 0;
    return curCount;
  }

 private:
  unsigned m_usedCount;
};

// Tests callback invocation.
TEST(UserGestureIndicatorTest, Callback) {
  UsedCallback cb;

  {
    UserGestureIndicator userGestureScope(TestUserGestureToken::create());
    UserGestureIndicator::currentToken()->setUserGestureUtilizedCallback(&cb);
    EXPECT_EQ(0u, cb.getAndResetUsedCount());

    // Untracked doesn't invoke the callback
    EXPECT_TRUE(UserGestureIndicator::processingUserGesture());
    EXPECT_EQ(0u, cb.getAndResetUsedCount());

    // But processingUserGesture does
    EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_EQ(1u, cb.getAndResetUsedCount());

    // But only the first time
    EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_TRUE(UserGestureIndicator::consumeUserGesture());
    EXPECT_EQ(0u, cb.getAndResetUsedCount());
  }
  EXPECT_EQ(0u, cb.getAndResetUsedCount());

  {
    UserGestureIndicator userGestureScope(TestUserGestureToken::create());
    UserGestureIndicator::currentToken()->setUserGestureUtilizedCallback(&cb);

    // Consume also invokes the callback
    EXPECT_TRUE(UserGestureIndicator::consumeUserGesture());
    EXPECT_EQ(1u, cb.getAndResetUsedCount());

    // But only once
    EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_FALSE(UserGestureIndicator::consumeUserGesture());
    EXPECT_EQ(0u, cb.getAndResetUsedCount());
  }

  {
    std::unique_ptr<UserGestureIndicator> userGestureScope(
        new UserGestureIndicator(TestUserGestureToken::create()));
    RefPtr<UserGestureToken> token = UserGestureIndicator::currentToken();
    token->setUserGestureUtilizedCallback(&cb);
    userGestureScope.reset();

    // The callback should be cleared when the UseGestureIndicator is deleted.
    EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_EQ(0u, cb.getAndResetUsedCount());
  }

  // The callback isn't invoked outside the scope of the UGI
  EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_EQ(0u, cb.getAndResetUsedCount());
  EXPECT_FALSE(UserGestureIndicator::consumeUserGesture());
  EXPECT_EQ(0u, cb.getAndResetUsedCount());
}

TEST(UserGestureIndicatorTest, Timeouts) {
  TimeFunction previous = setTimeFunctionsForTesting(mockTimeFunction);

  {
    // Token times out after 1 second.
    RefPtr<UserGestureToken> token = TestUserGestureToken::create();
    EXPECT_TRUE(token->hasGestures());
    UserGestureIndicator userGestureScope(token.get());
    EXPECT_TRUE(token->hasGestures());
    advanceClock(0.75);
    EXPECT_TRUE(token->hasGestures());
    advanceClock(0.75);
    EXPECT_FALSE(token->hasGestures());
  }

  {
    // Timestamp is reset when a token is put in a UserGestureIndicator.
    RefPtr<UserGestureToken> token = TestUserGestureToken::create();
    EXPECT_TRUE(token->hasGestures());
    advanceClock(0.75);
    EXPECT_TRUE(token->hasGestures());
    UserGestureIndicator userGestureScope(token.get());
    advanceClock(0.75);
    EXPECT_TRUE(token->hasGestures());
    advanceClock(0.75);
    EXPECT_FALSE(token->hasGestures());
  }

  setTimeFunctionsForTesting(previous);
}

}  // namespace blink

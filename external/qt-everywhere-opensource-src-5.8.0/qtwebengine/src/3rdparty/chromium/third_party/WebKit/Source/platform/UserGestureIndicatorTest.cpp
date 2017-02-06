// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/UserGestureIndicator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// Checks for the initial state of UserGestureIndicator.
TEST(UserGestureIndicatorTest, InitialState)
{
    EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_FALSE(UserGestureIndicator::processedUserGestureSinceLoad());
    EXPECT_EQ(nullptr, UserGestureIndicator::currentToken());
    EXPECT_FALSE(UserGestureIndicator::consumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithNewUserGesture)
{
    UserGestureIndicator::clearProcessedUserGestureSinceLoad();
    UserGestureIndicator userGestureScope(DefinitelyProcessingNewUserGesture);

    EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_TRUE(UserGestureIndicator::processedUserGestureSinceLoad());
    EXPECT_NE(nullptr, UserGestureIndicator::currentToken());

    EXPECT_TRUE(UserGestureIndicator::consumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithUserGesture)
{
    UserGestureIndicator::clearProcessedUserGestureSinceLoad();
    UserGestureIndicator userGestureScope(DefinitelyProcessingUserGesture);

    EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_TRUE(UserGestureIndicator::processedUserGestureSinceLoad());
    EXPECT_NE(nullptr, UserGestureIndicator::currentToken());

    EXPECT_TRUE(UserGestureIndicator::consumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithNoUserGesture)
{
    UserGestureIndicator::clearProcessedUserGestureSinceLoad();
    UserGestureIndicator userGestureScope(DefinitelyNotProcessingUserGesture);

    EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_FALSE(UserGestureIndicator::processedUserGestureSinceLoad());
    EXPECT_NE(nullptr, UserGestureIndicator::currentToken());

    EXPECT_FALSE(UserGestureIndicator::consumeUserGesture());
}

// Check that after UserGestureIndicator destruction state will be cleared.
TEST(UserGestureIndicatorTest, DestructUserGestureIndicator)
{
    {
        UserGestureIndicator userGestureScope(DefinitelyProcessingUserGesture);

        EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
        EXPECT_TRUE(UserGestureIndicator::processedUserGestureSinceLoad());
        EXPECT_NE(nullptr, UserGestureIndicator::currentToken());
    }

    EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_EQ(nullptr, UserGestureIndicator::currentToken());
    EXPECT_FALSE(UserGestureIndicator::consumeUserGesture());
}

// Tests creation of scoped UserGestureIndicator objects.
TEST(UserGestureIndicatorTest, ScopedNewUserGestureIndicators)
{
    // Root GestureIndicator and GestureToken.
    UserGestureIndicator userGestureScope(DefinitelyProcessingNewUserGesture);

    EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_TRUE(UserGestureIndicator::processedUserGestureSinceLoad());
    EXPECT_NE(nullptr, UserGestureIndicator::currentToken());
    {
        // Construct inner UserGestureIndicator.
        // It should share GestureToken with the root indicator.
        UserGestureIndicator innerUserGesture(DefinitelyProcessingNewUserGesture);

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

class UsedCallback : public UserGestureUtilizedCallback {
public:
    UsedCallback() : m_usedCount(0)
    {
    }

    void userGestureUtilized() override
    {
        m_usedCount++;
    }

    unsigned getAndResetUsedCount()
    {
        unsigned curCount = m_usedCount;
        m_usedCount = 0;
        return curCount;
    }

private:
    unsigned m_usedCount;
};

// Tests callback invocation.
TEST(UserGestureIndicatorTest, Callback)
{
    UsedCallback cb;

    {
        UserGestureIndicator userGestureScope(DefinitelyProcessingUserGesture, &cb);
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
        UserGestureIndicator userGestureScope(DefinitelyProcessingUserGesture, &cb);

        // Consume also invokes the callback
        EXPECT_TRUE(UserGestureIndicator::consumeUserGesture());
        EXPECT_EQ(1u, cb.getAndResetUsedCount());

        // But only once
        EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
        EXPECT_FALSE(UserGestureIndicator::consumeUserGesture());
        EXPECT_EQ(0u, cb.getAndResetUsedCount());
    }

    {
        UserGestureIndicator userGestureScope(DefinitelyNotProcessingUserGesture, &cb);

        // Callback not invoked when there isn't actually a user gesture
        EXPECT_FALSE(UserGestureIndicator::processingUserGesture());
        EXPECT_EQ(0u, cb.getAndResetUsedCount());
    }

    // The callback isn't invoked outside the scope of the UGI
    EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_EQ(0u, cb.getAndResetUsedCount());
    EXPECT_FALSE(UserGestureIndicator::consumeUserGesture());
    EXPECT_EQ(0u, cb.getAndResetUsedCount());
}

} // namespace blink

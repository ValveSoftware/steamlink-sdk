// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationData.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/testing/NullExecutionContext.h"
#include "modules/notifications/Notification.h"
#include "modules/notifications/NotificationOptions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/CurrentTime.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"

namespace blink {
namespace {

const char kNotificationTitle[] = "My Notification";

const char kNotificationDir[] = "rtl";
const char kNotificationLang[] = "nl";
const char kNotificationBody[] = "Hello, world";
const char kNotificationTag[] = "my_tag";
const char kNotificationEmptyTag[] = "";
const char kNotificationIcon[] = "https://example.com/icon.png";
const char kNotificationIconInvalid[] = "https://invalid:icon:url";
const char kNotificationBadge[] = "https://example.com/badge.png";
const unsigned kNotificationVibration[] = { 42, 10, 20, 30, 40 };
const unsigned long long kNotificationTimestamp = 621046800ull;
const bool kNotificationRenotify = true;
const bool kNotificationSilent = false;
const bool kNotificationRequireInteraction = true;

const WebNotificationAction::Type kWebNotificationActionType = WebNotificationAction::Text;
const char kNotificationActionType[] = "text";
const char kNotificationActionAction[] = "my_action";
const char kNotificationActionTitle[] = "My Action";
const char kNotificationActionIcon[] = "https://example.com/action_icon.png";
const char kNotificationActionPlaceholder[] = "Placeholder...";

const unsigned kNotificationVibrationUnnormalized[] = { 10, 1000000, 50, 42 };
const int kNotificationVibrationNormalized[] = { 10, 10000, 50 };

class NotificationDataTest : public ::testing::Test {
public:
    void SetUp() override
    {
        m_executionContext = new NullExecutionContext();
    }

    ExecutionContext* getExecutionContext() { return m_executionContext.get(); }

private:
    Persistent<ExecutionContext> m_executionContext;
};

TEST_F(NotificationDataTest, ReflectProperties)
{
    Vector<unsigned> vibrationPattern;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(kNotificationVibration); ++i)
        vibrationPattern.append(kNotificationVibration[i]);

    UnsignedLongOrUnsignedLongSequence vibrationSequence;
    vibrationSequence.setUnsignedLongSequence(vibrationPattern);

    HeapVector<NotificationAction> actions;
    for (size_t i = 0; i < Notification::maxActions(); ++i) {
        NotificationAction action;
        action.setType(kNotificationActionType);
        action.setAction(kNotificationActionAction);
        action.setTitle(kNotificationActionTitle);
        action.setIcon(kNotificationActionIcon);
        action.setPlaceholder(kNotificationActionPlaceholder);

        actions.append(action);
    }

    NotificationOptions options;
    options.setDir(kNotificationDir);
    options.setLang(kNotificationLang);
    options.setBody(kNotificationBody);
    options.setTag(kNotificationTag);
    options.setIcon(kNotificationIcon);
    options.setBadge(kNotificationBadge);
    options.setVibrate(vibrationSequence);
    options.setTimestamp(kNotificationTimestamp);
    options.setRenotify(kNotificationRenotify);
    options.setSilent(kNotificationSilent);
    options.setRequireInteraction(kNotificationRequireInteraction);
    options.setActions(actions);

    // TODO(peter): Test |options.data| and |notificationData.data|.

    TrackExceptionState exceptionState;
    WebNotificationData notificationData = createWebNotificationData(getExecutionContext(), kNotificationTitle, options, exceptionState);
    ASSERT_FALSE(exceptionState.hadException());

    EXPECT_EQ(kNotificationTitle, notificationData.title);

    EXPECT_EQ(WebNotificationData::DirectionRightToLeft, notificationData.direction);
    EXPECT_EQ(kNotificationLang, notificationData.lang);
    EXPECT_EQ(kNotificationBody, notificationData.body);
    EXPECT_EQ(kNotificationTag, notificationData.tag);

    // TODO(peter): Test the various icon properties when ExecutionContext::completeURL() works in this test.

    ASSERT_EQ(vibrationPattern.size(), notificationData.vibrate.size());
    for (size_t i = 0; i < vibrationPattern.size(); ++i)
        EXPECT_EQ(vibrationPattern[i], static_cast<unsigned>(notificationData.vibrate[i]));

    EXPECT_EQ(kNotificationTimestamp, notificationData.timestamp);
    EXPECT_EQ(kNotificationRenotify, notificationData.renotify);
    EXPECT_EQ(kNotificationSilent, notificationData.silent);
    EXPECT_EQ(kNotificationRequireInteraction, notificationData.requireInteraction);
    EXPECT_EQ(actions.size(), notificationData.actions.size());
    for (const auto& action : notificationData.actions) {
        EXPECT_EQ(kWebNotificationActionType, action.type);
        EXPECT_EQ(kNotificationActionAction, action.action);
        EXPECT_EQ(kNotificationActionTitle, action.title);
        EXPECT_EQ(kNotificationActionPlaceholder, action.placeholder);
    }
}

TEST_F(NotificationDataTest, SilentNotificationWithVibration)
{
    Vector<unsigned> vibrationPattern;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(kNotificationVibration); ++i)
        vibrationPattern.append(kNotificationVibration[i]);

    UnsignedLongOrUnsignedLongSequence vibrationSequence;
    vibrationSequence.setUnsignedLongSequence(vibrationPattern);

    NotificationOptions options;
    options.setVibrate(vibrationSequence);
    options.setSilent(true);

    TrackExceptionState exceptionState;
    WebNotificationData notificationData = createWebNotificationData(getExecutionContext(), kNotificationTitle, options, exceptionState);
    ASSERT_TRUE(exceptionState.hadException());

    EXPECT_EQ("Silent notifications must not specify vibration patterns.", exceptionState.message());
}

TEST_F(NotificationDataTest, ActionTypeButtonWithPlaceholder)
{
    HeapVector<NotificationAction> actions;
    NotificationAction action;
    action.setType("button");
    action.setPlaceholder("I'm afraid I can't do that...");
    actions.append(action);

    NotificationOptions options;
    options.setActions(actions);

    TrackExceptionState exceptionState;
    WebNotificationData notificationData = createWebNotificationData(getExecutionContext(), kNotificationTitle, options, exceptionState);
    ASSERT_TRUE(exceptionState.hadException());

    EXPECT_EQ("Notifications of type \"button\" cannot specify a placeholder.", exceptionState.message());
}

TEST_F(NotificationDataTest, RenotifyWithEmptyTag)
{
    NotificationOptions options;
    options.setTag(kNotificationEmptyTag);
    options.setRenotify(true);

    TrackExceptionState exceptionState;
    WebNotificationData notificationData = createWebNotificationData(getExecutionContext(), kNotificationTitle, options, exceptionState);
    ASSERT_TRUE(exceptionState.hadException());

    EXPECT_EQ("Notifications which set the renotify flag must specify a non-empty tag.", exceptionState.message());
}

TEST_F(NotificationDataTest, InvalidIconUrls)
{
    HeapVector<NotificationAction> actions;
    for (size_t i = 0; i < Notification::maxActions(); ++i) {
        NotificationAction action;
        action.setAction(kNotificationActionAction);
        action.setTitle(kNotificationActionTitle);
        action.setIcon(kNotificationIconInvalid);
        actions.append(action);
    }

    NotificationOptions options;
    options.setIcon(kNotificationIconInvalid);
    options.setBadge(kNotificationIconInvalid);
    options.setActions(actions);

    TrackExceptionState exceptionState;
    WebNotificationData notificationData = createWebNotificationData(getExecutionContext(), kNotificationTitle, options, exceptionState);
    ASSERT_FALSE(exceptionState.hadException());

    EXPECT_TRUE(notificationData.icon.isEmpty());
    EXPECT_TRUE(notificationData.badge.isEmpty());
    for (const auto& action : notificationData.actions)
        EXPECT_TRUE(action.icon.isEmpty());
}

TEST_F(NotificationDataTest, VibrationNormalization)
{
    Vector<unsigned> unnormalizedPattern;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(kNotificationVibrationUnnormalized); ++i)
        unnormalizedPattern.append(kNotificationVibrationUnnormalized[i]);

    UnsignedLongOrUnsignedLongSequence vibrationSequence;
    vibrationSequence.setUnsignedLongSequence(unnormalizedPattern);

    NotificationOptions options;
    options.setVibrate(vibrationSequence);

    TrackExceptionState exceptionState;
    WebNotificationData notificationData = createWebNotificationData(getExecutionContext(), kNotificationTitle, options, exceptionState);
    EXPECT_FALSE(exceptionState.hadException());

    Vector<int> normalizedPattern;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(kNotificationVibrationNormalized); ++i)
        normalizedPattern.append(kNotificationVibrationNormalized[i]);

    ASSERT_EQ(normalizedPattern.size(), notificationData.vibrate.size());
    for (size_t i = 0; i < normalizedPattern.size(); ++i)
        EXPECT_EQ(normalizedPattern[i], notificationData.vibrate[i]);
}

TEST_F(NotificationDataTest, DefaultTimestampValue)
{
    NotificationOptions options;

    TrackExceptionState exceptionState;
    WebNotificationData notificationData = createWebNotificationData(getExecutionContext(), kNotificationTitle, options, exceptionState);
    EXPECT_FALSE(exceptionState.hadException());

    // The timestamp should be set to the current time since the epoch if it wasn't supplied by the developer.
    // "32" has no significance, but an equal comparison of the value could lead to flaky failures.
    EXPECT_NEAR(notificationData.timestamp, WTF::currentTimeMS(), 32);
}

TEST_F(NotificationDataTest, DirectionValues)
{
    WTF::HashMap<String, WebNotificationData::Direction> mappings;
    mappings.add("ltr", WebNotificationData::DirectionLeftToRight);
    mappings.add("rtl", WebNotificationData::DirectionRightToLeft);
    mappings.add("auto", WebNotificationData::DirectionAuto);

    // Invalid values should default to "auto".
    mappings.add("peter", WebNotificationData::DirectionAuto);

    for (const String& direction : mappings.keys()) {
        NotificationOptions options;
        options.setDir(direction);

        TrackExceptionState exceptionState;
        WebNotificationData notificationData = createWebNotificationData(getExecutionContext(), kNotificationTitle, options, exceptionState);
        ASSERT_FALSE(exceptionState.hadException());

        EXPECT_EQ(mappings.get(direction), notificationData.direction);
    }
}

TEST_F(NotificationDataTest, MaximumActionCount)
{
    HeapVector<NotificationAction> actions;
    for (size_t i = 0; i < Notification::maxActions() + 2; ++i) {
        NotificationAction action;
        action.setAction(String::number(i));
        action.setTitle(kNotificationActionTitle);

        actions.append(action);
    }

    NotificationOptions options;
    options.setActions(actions);

    TrackExceptionState exceptionState;
    WebNotificationData notificationData = createWebNotificationData(getExecutionContext(), kNotificationTitle, options, exceptionState);
    ASSERT_FALSE(exceptionState.hadException());

    // The stored actions will be capped to |maxActions| entries.
    ASSERT_EQ(Notification::maxActions(), notificationData.actions.size());

    for (size_t i = 0; i < Notification::maxActions(); ++i) {
        WebString expectedAction = String::number(i);
        EXPECT_EQ(expectedAction, notificationData.actions[i].action);
    }
}

} // namespace
} // namespace blink

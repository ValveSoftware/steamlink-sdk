// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/PushMessageData.h"

#include "public/platform/WebString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

const char kPushMessageData[] = "Push Message valid data string.";

TEST(PushMessageDataTest, ValidPayload)
{
    // Create a WebString with the test message, then create a
    // PushMessageData from that.
    WebString s(blink::WebString::fromUTF8(kPushMessageData));
    PushMessageData* data = PushMessageData::create(s);

    ASSERT_NE(data, nullptr);
    EXPECT_STREQ(kPushMessageData, data->text().utf8().data());
}

TEST(PushMessageDataTest, ValidEmptyPayload)
{
    // Create a WebString with a valid but empty test message, then create
    // a PushMessageData from that.
    WebString s("");
    PushMessageData* data = PushMessageData::create(s);

    ASSERT_NE(data, nullptr);
    EXPECT_STREQ("", data->text().utf8().data());
}

TEST(PushMessageDataTest, NullPayload)
{
    // Create a PushMessageData with a null payload.
    WebString s;
    PushMessageData* data = PushMessageData::create(s);

    EXPECT_EQ(data, nullptr);
}

} // anonymous namespace
} // namespace blink

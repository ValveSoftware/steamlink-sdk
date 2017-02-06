// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaSession.h"

#include "core/dom/Document.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/mediasession/MediaMetadata.h"
#include "modules/mediasession/MediaMetadataInit.h"
#include "public/platform/modules/mediasession/WebMediaSession.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

using ::testing::_;
using ::testing::Invoke;

namespace blink {

class MediaSessionTest : public ::testing::Test {
protected:
    MediaSessionTest()
        : m_page(DummyPageHolder::create(IntSize(1, 1)))
    {}

    MediaSession* createMediaSession(WebMediaSession* webMediaSession)
    {
        // The MediaSession takes ownership of the WebMediaSession, and the
        // caller must take care to not end up with a stale pointer.
        return new MediaSession(wrapUnique(webMediaSession));
    }

    Document& document() { return m_page->document(); }
    ScriptState* mainScriptState() { return ScriptState::forMainWorld(document().frame()); }
private:
    std::unique_ptr<DummyPageHolder> m_page;
};

namespace {

class MockWebMediaSession : public WebMediaSession {
public:
    MOCK_METHOD1(activate, void(WebMediaSessionActivateCallback*));
    MOCK_METHOD1(deactivate, void(WebMediaSessionDeactivateCallback*));
    MOCK_METHOD1(setMetadata, void(const WebMediaMetadata*));
};

class Helper {
public:
    void activate(WebMediaSessionActivateCallback* cb) { delete cb; }
    void deactivate(WebMediaSessionDeactivateCallback* cb) { delete cb; }
};

TEST_F(MediaSessionTest, Activate)
{
    ScriptState::Scope scope(mainScriptState());
    MockWebMediaSession* mockWebMediaSession = new MockWebMediaSession;
    MediaSession* mediaSession = createMediaSession(mockWebMediaSession);

    Helper helper;
    EXPECT_CALL(*mockWebMediaSession, activate(_)).WillOnce(Invoke(&helper, &Helper::activate));
    mediaSession->activate(mainScriptState());
}

TEST_F(MediaSessionTest, Deactivate)
{
    ScriptState::Scope scope(mainScriptState());
    MockWebMediaSession* mockWebMediaSession = new MockWebMediaSession;
    MediaSession* mediaSession = createMediaSession(mockWebMediaSession);

    Helper helper;
    EXPECT_CALL(*mockWebMediaSession, deactivate(_)).WillOnce(Invoke(&helper, &Helper::deactivate));
    mediaSession->deactivate(mainScriptState());
}

TEST_F(MediaSessionTest, SetMetadata_Null)
{
    MockWebMediaSession* mockWebMediaSession = new MockWebMediaSession;
    MediaSession* mediaSession = createMediaSession(mockWebMediaSession);

    EXPECT_CALL(*mockWebMediaSession, setMetadata(testing::IsNull()));
    mediaSession->setMetadata(nullptr);

    EXPECT_EQ(nullptr, mediaSession->metadata());
}

TEST_F(MediaSessionTest, SetMetadata_NotNull)
{
    MockWebMediaSession* mockWebMediaSession = new MockWebMediaSession;
    MediaSession* mediaSession = createMediaSession(mockWebMediaSession);

    EXPECT_CALL(*mockWebMediaSession, setMetadata(testing::NotNull()));
    mediaSession->setMetadata(MediaMetadata::create(&document(), MediaMetadataInit()));

    EXPECT_NE(nullptr, mediaSession->metadata());
}

} // namespace
} // namespace blink

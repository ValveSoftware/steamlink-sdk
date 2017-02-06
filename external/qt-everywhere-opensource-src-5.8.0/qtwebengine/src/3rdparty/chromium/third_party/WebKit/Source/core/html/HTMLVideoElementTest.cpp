// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLVideoElement.h"

#include "core/dom/Document.h"
#include "core/loader/EmptyClients.h"
#include "core/page/NetworkStateNotifier.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/UserGestureIndicator.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/WebMediaPlayer.h"
#include "public/platform/WebSize.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

class EmptyWebMediaPlayer : public WebMediaPlayer {
public:
    void load(LoadType, const WebMediaPlayerSource&, CORSMode) override { };
    void play() override { };
    void pause() override { };
    bool supportsSave() const override { return false; };
    void seek(double seconds) override { };
    void setRate(double) override { };
    void setVolume(double) override { };
    WebTimeRanges buffered() const override { return WebTimeRanges(); };
    WebTimeRanges seekable() const override { return WebTimeRanges(); };
    void setSinkId(const WebString& sinkId, const WebSecurityOrigin&, WebSetSinkIdCallbacks*) override { };
    bool hasVideo() const override { return false; };
    bool hasAudio() const override { return false; };
    WebSize naturalSize() const override { return WebSize(0, 0); };
    bool paused() const override { return false; };
    bool seeking() const override { return false; };
    double duration() const override { return 0.0; };
    double currentTime() const override { return 0.0; };
    NetworkState getNetworkState() const override { return NetworkStateEmpty; };
    ReadyState getReadyState() const override { return ReadyStateHaveNothing; };
    WebString getErrorMessage() override { return WebString(); };
    bool didLoadingProgress() override { return false; };
    bool hasSingleSecurityOrigin() const override { return true; };
    bool didPassCORSAccessCheck() const override { return true; };
    double mediaTimeForTimeValue(double timeValue) const override { return timeValue; };
    unsigned decodedFrameCount() const override { return 0; };
    unsigned droppedFrameCount() const override { return 0; };
    size_t audioDecodedByteCount() const override { return 0; };
    size_t videoDecodedByteCount() const override { return 0; };
    void paint(WebCanvas*, const WebRect&, unsigned char alpha, SkXfermode::Mode) override { };
};

class MockWebMediaPlayer : public EmptyWebMediaPlayer {
public:
    MOCK_METHOD1(setBufferingStrategy, void(BufferingStrategy));
};

class StubFrameLoaderClient : public EmptyFrameLoaderClient {
public:
    static StubFrameLoaderClient* create()
    {
        return new StubFrameLoaderClient;
    }

    std::unique_ptr<WebMediaPlayer> createWebMediaPlayer(HTMLMediaElement&, const WebMediaPlayerSource&, WebMediaPlayerClient*) override
    {
        return wrapUnique(new MockWebMediaPlayer);
    }
};

} // namespace

class HTMLVideoElementTest : public ::testing::Test {
protected:
    HTMLVideoElementTest()
        : m_dummyPageHolder(DummyPageHolder::create(IntSize(640, 360), nullptr, StubFrameLoaderClient::create()))
    {
        // TODO(sandersd): This should be done by a settings initializer.
        networkStateNotifier().setWebConnection(WebConnectionTypeWifi, 54.0);
        m_video = HTMLVideoElement::create(m_dummyPageHolder->document());
    }

    void setSrc(const AtomicString& url)
    {
        m_video->setSrc(url);
        testing::runPendingTasks();
    }

    MockWebMediaPlayer* webMediaPlayer()
    {
        return static_cast<MockWebMediaPlayer*>(m_video->webMediaPlayer());
    }

    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
    Persistent<HTMLVideoElement> m_video;
};

TEST_F(HTMLVideoElementTest, setBufferingStrategy_NonUserPause)
{
    setSrc("http://foo.bar/");
    MockWebMediaPlayer* player = webMediaPlayer();
    ASSERT_TRUE(player);

    // On play, the strategy is set to normal.
    EXPECT_CALL(*player, setBufferingStrategy(WebMediaPlayer::BufferingStrategy::Normal));
    m_video->play();
    ::testing::Mock::VerifyAndClearExpectations(player);

    // On a non-user pause, the strategy is not changed.
    m_video->pause();
    ::testing::Mock::VerifyAndClearExpectations(player);

    // On play, the strategy is set to normal.
    EXPECT_CALL(*player, setBufferingStrategy(WebMediaPlayer::BufferingStrategy::Normal));
    m_video->play();
    ::testing::Mock::VerifyAndClearExpectations(player);
}

TEST_F(HTMLVideoElementTest, setBufferingStrategy_UserPause)
{
    setSrc("http://foo.bar/");
    MockWebMediaPlayer* player = webMediaPlayer();
    ASSERT_TRUE(player);

    // On play, the strategy is set to normal.
    EXPECT_CALL(*player, setBufferingStrategy(WebMediaPlayer::BufferingStrategy::Normal));
    m_video->play();
    ::testing::Mock::VerifyAndClearExpectations(player);

    // On a user pause, the strategy is set to aggressive.
    EXPECT_CALL(*player, setBufferingStrategy(WebMediaPlayer::BufferingStrategy::Aggressive));
    {
        UserGestureIndicator gesture(DefinitelyProcessingUserGesture);
        m_video->pause();
    }
    ::testing::Mock::VerifyAndClearExpectations(player);

    // On play, the strategy is set to normal.
    EXPECT_CALL(*player, setBufferingStrategy(WebMediaPlayer::BufferingStrategy::Normal));
    m_video->play();
    ::testing::Mock::VerifyAndClearExpectations(player);
}

} // namespace blink

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/html/AutoplayExperimentHelper.h"
#include "platform/UserGestureIndicator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// msvc refuses to compile if we use all of ::testing, due to a conflict with
// WTF::NotNull.  So, we just use what we need.
using ::testing::Return;
using ::testing::NiceMock;
using ::testing::_;

namespace blink {

class MockAutoplayClient : public AutoplayExperimentHelper::Client {
public:
    enum ElementType { Video, Audio };

    MockAutoplayClient(const char* mode, ElementType type)
        : m_mode(mode)
        , m_duration(100)
    {
        // Set up default behaviors that the helper is allowed to use or cache
        // during construction.

        ON_CALL(*this, autoplayExperimentMode())
            .WillByDefault(Return(m_mode));

        // Use m_isVideo to answer these.
        ON_CALL(*this, isHTMLVideoElement())
            .WillByDefault(Return(type == Video));
        ON_CALL(*this, isHTMLAudioElement())
            .WillByDefault(Return(type == Audio));

        // Now set up some useful defaults.
        // Remember that this are only evaluated once.
        ON_CALL(*this, duration())
            .WillByDefault(Return(m_duration));
        ON_CALL(*this, currentTime())
            .WillByDefault(Return(0));

        // Default to "not optimized for mobile" page.
        ON_CALL(*this, isLegacyViewportType())
            .WillByDefault(Return(false));

        // Other handy defaults.
        ON_CALL(*this, paused())
            .WillByDefault(Return(true));
        ON_CALL(*this, ended())
            .WillByDefault(Return(false));
        ON_CALL(*this, pageVisibilityState())
            .WillByDefault(Return(PageVisibilityStateVisible));
        ON_CALL(*this, isCrossOrigin())
            .WillByDefault(Return(false));
        ON_CALL(*this, isAutoplayAllowedPerSettings())
            .WillByDefault(Return(true));
        ON_CALL(*this, absoluteBoundingBoxRect())
            .WillByDefault(Return(
                IntRect(10, 10, 100, 100)));

        // Normally, the autoplay experiment should not modify lots of other
        // state unless we explicitly expect it.
        EXPECT_CALL(*this, setMuted(_))
            .Times(0);
        EXPECT_CALL(*this, unlockUserGesture())
            .Times(0);
        EXPECT_CALL(*this, setRequestPositionUpdates(true))
            .Times(0);
        EXPECT_CALL(*this, recordAutoplayMetric(_))
            .Times(0);
    }

    virtual ~MockAutoplayClient() {}

    MOCK_CONST_METHOD0(currentTime, double());
    MOCK_CONST_METHOD0(duration, double());
    MOCK_CONST_METHOD0(paused, bool());
    MOCK_CONST_METHOD0(ended, bool());
    MOCK_CONST_METHOD0(muted, bool());
    MOCK_METHOD1(setMuted, void(bool));
    MOCK_METHOD0(playInternal, void());
    MOCK_METHOD0(pauseInternal, void());
    MOCK_CONST_METHOD0(isLockedPendingUserGesture, bool());
    MOCK_METHOD0(unlockUserGesture, void());
    MOCK_METHOD1(recordAutoplayMetric, void(AutoplayMetrics));
    MOCK_METHOD0(shouldAutoplay, bool());
    MOCK_CONST_METHOD0(isHTMLVideoElement, bool());
    MOCK_CONST_METHOD0(isHTMLAudioElement, bool());
    MOCK_METHOD0(isLegacyViewportType, bool());
    MOCK_CONST_METHOD0(pageVisibilityState, PageVisibilityState());
    MOCK_CONST_METHOD0(autoplayExperimentMode, String());
    MOCK_CONST_METHOD0(isCrossOrigin, bool());
    MOCK_CONST_METHOD0(isAutoplayAllowedPerSettings, bool());
    MOCK_METHOD1(setRequestPositionUpdates, void(bool));
    MOCK_CONST_METHOD0(absoluteBoundingBoxRect, IntRect());

    const char* m_mode;
    // const since changes to it won't affect the mocked value.
    const double m_duration;
};

class AutoplayExperimentTest : public ::testing::Test {
public:
    AutoplayExperimentTest() {}

    ~AutoplayExperimentTest() {}

    bool isEligible()
    {
        return m_helper->isEligible();
    }

    bool meetsVisibilityRequirements()
    {
        return m_helper->meetsVisibilityRequirements();
    }

    void setInterface(MockAutoplayClient* client)
    {
        m_client = client;

        // Set some defaults.
        setUserGestureRequiredForPlay(true);
        setShouldAutoplay(true);
        setIsMuted(false);

        m_helper = AutoplayExperimentHelper::create(m_client.get());
    }

    void TearDown()
    {
        // Be sure that the mock is destructed before the test, so that any
        // missing expectations are noticed.  Otherwise, the test will pass
        // and then missing expectations will show up in the log, without
        // causing a test failure.
        m_helper.clear();
        m_client.clear();
        ThreadHeap::collectAllGarbage();
    }

    Persistent<MockAutoplayClient> m_client;
    Persistent<AutoplayExperimentHelper> m_helper;

    // Mirror updatePlayState to transition to play.
    void startPlayback()
    {
        EXPECT_CALL(*m_client, recordAutoplayMetric(AnyPlaybackStarted))
            .Times(1);
        m_helper->playbackStarted();
    }

    void startPlaybackWithoutUserGesture()
    {
        EXPECT_FALSE(UserGestureIndicator::processingUserGesture());
        startPlayback();
    }

    void startPlaybackWithUserGesture()
    {
        UserGestureIndicator indicator(DefinitelyProcessingUserGesture);
        EXPECT_TRUE(UserGestureIndicator::processingUserGesture());
        startPlayback();
    }

    void setUserGestureRequiredForPlay(bool required)
    {
        ON_CALL(*m_client, isLockedPendingUserGesture())
            .WillByDefault(Return(required));
    }

    void setShouldAutoplay(bool should)
    {
        ON_CALL(*m_client, shouldAutoplay())
            .WillByDefault(Return(should));
    }

    void setIsMuted(bool isMuted)
    {
        ON_CALL(*m_client, muted())
            .WillByDefault(Return(isMuted));
    }

    void pausePlaybackExpectingBailout(bool expectingAutoplay)
    {
        EXPECT_CALL(*m_client, recordAutoplayMetric(AnyPlaybackPaused))
            .Times(1);
        EXPECT_CALL(*m_client, recordAutoplayMetric(AnyPlaybackBailout))
            .Times(1);
        if (expectingAutoplay) {
            EXPECT_CALL(*m_client, recordAutoplayMetric(AutoplayPaused))
                .Times(1);
            EXPECT_CALL(*m_client, recordAutoplayMetric(AutoplayBailout))
                .Times(1);
        }
        m_helper->pauseMethodCalled();
        m_helper->playbackStopped();
    }

    void pausePlaybackNotExpectingBailout(bool expectingAutoplay)
    {
        EXPECT_CALL(*m_client, recordAutoplayMetric(AnyPlaybackPaused))
            .Times(1);
        if (expectingAutoplay)
            EXPECT_CALL(*m_client, recordAutoplayMetric(AutoplayPaused)).Times(1);
        EXPECT_CALL(*m_client, recordAutoplayMetric(AnyPlaybackBailout)).Times(0);
        EXPECT_CALL(*m_client, recordAutoplayMetric(AutoplayBailout)).Times(0);

        // Move past the bailout point, but not to the end.  That way, we don't
        // have to set ended().
        ON_CALL(*m_client, currentTime())
            .WillByDefault(Return(m_client->m_duration-1));

        m_helper->pauseMethodCalled();
        m_helper->playbackStopped();
    }

    void endPlayback(bool expectingAutoplay)
    {
        EXPECT_CALL(*m_client, recordAutoplayMetric(AnyPlaybackComplete))
            .Times(1);
        if (expectingAutoplay)
            EXPECT_CALL(*m_client, recordAutoplayMetric(AutoplayComplete)).Times(1);
        ON_CALL(*m_client, currentTime())
            .WillByDefault(Return(m_client->m_duration));
        ON_CALL(*m_client, ended())
            .WillByDefault(Return(true));
        m_helper->playbackStopped();
    }

    void moveIntoViewport()
    {
        m_helper->positionChanged(IntRect(0, 0, 200, 200));
        m_helper->triggerAutoplayViewportCheckForTesting();
    }
};

TEST_F(AutoplayExperimentTest, IsNotEligibleWithEmptyMode)
{
    setInterface(new NiceMock<MockAutoplayClient>("", MockAutoplayClient::Video));
    EXPECT_FALSE(isEligible());
}

TEST_F(AutoplayExperimentTest, IsVideoEligibleForVideoMode)
{
    // Video should be eligible in "forvideo" mode.
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo", MockAutoplayClient::Video));
    EXPECT_TRUE(isEligible());
}

TEST_F(AutoplayExperimentTest, IsAudioNotEligibleForVideoMode)
{
    // Audio should not be eligible for video mode.
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo", MockAutoplayClient::Audio));
    EXPECT_FALSE(isEligible());
}

TEST_F(AutoplayExperimentTest, IsEligibleRequiresUserGesture)
{
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo", MockAutoplayClient::Video));
    // If a user gesture is not required, then we're not eligible.
    ON_CALL(*m_client, isLockedPendingUserGesture())
        .WillByDefault(Return(false));
    EXPECT_FALSE(isEligible());
}

TEST_F(AutoplayExperimentTest, IsEligibleRequiresShouldAutoplay)
{
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo", MockAutoplayClient::Video));
    // If we shouldn't autoplay, then we're not eligible.
    ON_CALL(*m_client, shouldAutoplay())
        .WillByDefault(Return(false));
    EXPECT_FALSE(isEligible());
}

TEST_F(AutoplayExperimentTest, IsAudioEligibleForAudioMode)
{
    setInterface(new NiceMock<MockAutoplayClient>("enabled-foraudio", MockAutoplayClient::Audio));
    EXPECT_TRUE(isEligible());
}

TEST_F(AutoplayExperimentTest, EligibleIfOptimizedForMobile)
{
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo-ifmobile", MockAutoplayClient::Video));
    // Should not be eligible with our default of "not mobile".
    EXPECT_FALSE(isEligible());

    ON_CALL(*m_client, isLegacyViewportType())
        .WillByDefault(Return(true));
    EXPECT_TRUE(isEligible());
}

TEST_F(AutoplayExperimentTest, EligibleIfMuted)
{
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo-ifmuted", MockAutoplayClient::Video));
    // Should not be eligible with our default of "not muted".
    EXPECT_FALSE(isEligible());

    ON_CALL(*m_client, muted())
        .WillByDefault(Return(true));
    EXPECT_TRUE(isEligible());
}

TEST_F(AutoplayExperimentTest, BecameReadyAutoplayThenBailout)
{
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo", MockAutoplayClient::Video));

    EXPECT_CALL(*m_client, recordAutoplayMetric(AutoplayMediaFound))
        .Times(1);
    m_helper->becameReadyToPlay();
    EXPECT_TRUE(m_helper->isGestureRequirementOverridden());

    EXPECT_CALL(*m_client, recordAutoplayMetric(GesturelessPlaybackStartedByAutoplayFlagImmediately))
        .Times(1);
    startPlaybackWithoutUserGesture();

    pausePlaybackExpectingBailout(true);
}

TEST_F(AutoplayExperimentTest, BecameReadyAutoplayThenPause)
{
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo", MockAutoplayClient::Video));

    EXPECT_CALL(*m_client, recordAutoplayMetric(AutoplayMediaFound))
        .Times(1);
    m_helper->becameReadyToPlay();
    EXPECT_TRUE(m_helper->isGestureRequirementOverridden());

    EXPECT_CALL(*m_client, recordAutoplayMetric(GesturelessPlaybackStartedByAutoplayFlagImmediately))
        .Times(1);
    startPlaybackWithoutUserGesture();

    pausePlaybackNotExpectingBailout(true);
}

TEST_F(AutoplayExperimentTest, BecameReadyAutoplayThenComplete)
{
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo", MockAutoplayClient::Video));

    EXPECT_CALL(*m_client, recordAutoplayMetric(AutoplayMediaFound))
        .Times(1);
    m_helper->becameReadyToPlay();
    EXPECT_TRUE(m_helper->isGestureRequirementOverridden());

    EXPECT_CALL(*m_client, recordAutoplayMetric(GesturelessPlaybackStartedByAutoplayFlagImmediately))
        .Times(1);
    startPlaybackWithoutUserGesture();

    // Now stop at the end.
    endPlayback(true);
}

TEST_F(AutoplayExperimentTest, NoUserGestureNeededShouldNotOverride)
{
    // Make sure that we don't override the user gesture if it isn't needed.
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo", MockAutoplayClient::Video));
    setUserGestureRequiredForPlay(false);

    // It is still autoplay media, though.
    EXPECT_CALL(*m_client, recordAutoplayMetric(AutoplayMediaFound))
        .Times(1);
    m_helper->becameReadyToPlay();

    EXPECT_CALL(*m_client, recordAutoplayMetric(GesturelessPlaybackNotOverridden))
        .Times(1);
    startPlaybackWithoutUserGesture();
}

TEST_F(AutoplayExperimentTest, NoAutoplayMetricsIfNoAutoplay)
{
    // If playback is started while processing a user gesture, then nothing
    // should be overridden or logged about autoplay.
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo", MockAutoplayClient::Video));
    setUserGestureRequiredForPlay(false);
    setShouldAutoplay(false);
    startPlaybackWithUserGesture();

    // Expect bailout, but not from autoplay.
    pausePlaybackExpectingBailout(false);
}

TEST_F(AutoplayExperimentTest, PlayMethodThenBailout)
{
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo", MockAutoplayClient::Video));
    setShouldAutoplay(false); // No autoplay attribute.

    EXPECT_CALL(*m_client, recordAutoplayMetric(AutoplayMediaFound))
        .Times(1);
    m_helper->playMethodCalled();
    EXPECT_TRUE(m_helper->isGestureRequirementOverridden());

    EXPECT_CALL(*m_client, recordAutoplayMetric(GesturelessPlaybackStartedByPlayMethodImmediately))
        .Times(1);
    startPlaybackWithoutUserGesture();

    pausePlaybackExpectingBailout(true);
}

TEST_F(AutoplayExperimentTest, DeferAutoplayUntilMuted)
{
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo-ifmuted", MockAutoplayClient::Video));

    // Should not override the gesture requirement yet.
    EXPECT_CALL(*m_client, recordAutoplayMetric(AutoplayMediaFound))
        .Times(1);
    m_helper->becameReadyToPlay();

    // When we toggle the muted attribute, it should become eligible to start.
    EXPECT_FALSE(m_helper->isGestureRequirementOverridden());
    setIsMuted(true);
    EXPECT_TRUE(m_helper->isGestureRequirementOverridden());
}

TEST_F(AutoplayExperimentTest, DeferPlaybackUntilInViewport)
{
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo-ifviewport", MockAutoplayClient::Video));

    // Should not override the gesture requirement yet.
    EXPECT_CALL(*m_client, recordAutoplayMetric(AutoplayMediaFound))
        .Times(1);
    EXPECT_CALL(*m_client, setRequestPositionUpdates(true))
        .Times(1);
    m_helper->becameReadyToPlay();

    EXPECT_CALL(*m_client, playInternal())
        .Times(1);
    EXPECT_CALL(*m_client, setRequestPositionUpdates(false))
        .Times(1);
    moveIntoViewport();
    EXPECT_TRUE(m_helper->isGestureRequirementOverridden());
}

TEST_F(AutoplayExperimentTest, WithSameOriginTests)
{
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo-ifsameorigin", MockAutoplayClient::Video));
    ON_CALL(*m_client, isCrossOrigin()).WillByDefault(Return(false));
    EXPECT_TRUE(isEligible());
    ON_CALL(*m_client, isCrossOrigin()).WillByDefault(Return(true));
    EXPECT_FALSE(isEligible());
}

TEST_F(AutoplayExperimentTest, WithoutSameOriginTests)
{
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo", MockAutoplayClient::Video));
    ON_CALL(*m_client, isCrossOrigin()).WillByDefault(Return(false));
    EXPECT_TRUE(isEligible());
    ON_CALL(*m_client, isCrossOrigin()).WillByDefault(Return(true));
    EXPECT_TRUE(isEligible());
}

TEST_F(AutoplayExperimentTest, AudioPageVisibility)
{
    setInterface(new NiceMock<MockAutoplayClient>("enabled-foraudio-ifpagevisible", MockAutoplayClient::Audio));
    ON_CALL(*m_client, pageVisibilityState()).WillByDefault(Return(PageVisibilityStateVisible));
    EXPECT_TRUE(isEligible());
    EXPECT_TRUE(meetsVisibilityRequirements());

    ON_CALL(*m_client, pageVisibilityState()).WillByDefault(Return(PageVisibilityStateHidden));
    EXPECT_TRUE(isEligible());
    EXPECT_FALSE(meetsVisibilityRequirements());
}

TEST_F(AutoplayExperimentTest, PlayTwiceIsIgnored)
{
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo", MockAutoplayClient::Video));
    setShouldAutoplay(false); // No autoplay attribute.

    EXPECT_CALL(*m_client, recordAutoplayMetric(AutoplayMediaFound))
        .Times(1);
    m_helper->playMethodCalled();
    ON_CALL(*m_client, paused()).WillByDefault(Return(false));
    m_helper->playMethodCalled();
}

TEST_F(AutoplayExperimentTest, CrossOriginMutedTests)
{
    setInterface(new NiceMock<MockAutoplayClient>("enabled-forvideo-ifsameorigin-ormuted", MockAutoplayClient::Video));
    ON_CALL(*m_client, isCrossOrigin()).WillByDefault(Return(true));

    // Cross-orgin unmuted content should be eligible.
    setIsMuted(true);
    EXPECT_TRUE(isEligible());

    // Cross-origin muted content should not be eligible.
    setIsMuted(false);
    EXPECT_FALSE(isEligible());

    // Start playback.
    EXPECT_CALL(*m_client, recordAutoplayMetric(AutoplayMediaFound))
        .Times(1);
    m_helper->becameReadyToPlay();
    ON_CALL(*m_client, paused()).WillByDefault(Return(false));
    setIsMuted(true);
    m_helper->mutedChanged();

    // Verify that unmuting pauses playback.
    setIsMuted(false);
    EXPECT_CALL(*m_client, pauseInternal())
        .Times(1);
    m_helper->mutedChanged();
}

}

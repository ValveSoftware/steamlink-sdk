// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/BaseAudioContext.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/frame/FrameOwner.h"
#include "core/frame/FrameTypes.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/UserGestureIndicator.h"
#include "platform/testing/HistogramTester.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/WebAudioDevice.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

const char* const kCrossOriginMetric = "WebAudio.Autoplay.CrossOrigin";

class MockCrossOriginFrameLoaderClient final : public EmptyFrameLoaderClient {
 public:
  static MockCrossOriginFrameLoaderClient* create(Frame* parent) {
    return new MockCrossOriginFrameLoaderClient(parent);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_parent);
    EmptyFrameLoaderClient::trace(visitor);
  }

  Frame* parent() const override { return m_parent.get(); }
  Frame* top() const override { return m_parent.get(); }

 private:
  explicit MockCrossOriginFrameLoaderClient(Frame* parent) : m_parent(parent) {}

  Member<Frame> m_parent;
};

class MockWebAudioDevice : public WebAudioDevice {
 public:
  explicit MockWebAudioDevice(double sampleRate) : m_sampleRate(sampleRate) {}
  ~MockWebAudioDevice() override = default;

  void start() override {}
  void stop() override {}
  double sampleRate() override { return m_sampleRate; }

 private:
  double m_sampleRate;
};

class BaseAudioContextTestPlatform : public TestingPlatformSupport {
 public:
  WebAudioDevice* createAudioDevice(size_t bufferSize,
                                    unsigned numberOfInputChannels,
                                    unsigned numberOfChannels,
                                    double sampleRate,
                                    WebAudioDevice::RenderCallback*,
                                    const WebString& deviceId,
                                    const WebSecurityOrigin&) override {
    return new MockWebAudioDevice(sampleRate);
  }

  double audioHardwareSampleRate() override { return 44100; }
};

}  // anonymous namespace

class BaseAudioContextTest : public ::testing::Test {
 protected:
  using AutoplayStatus = BaseAudioContext::AutoplayStatus;

  void SetUp() override {
    m_dummyPageHolder = DummyPageHolder::create();
    m_dummyFrameOwner = DummyFrameOwner::create();
    document().updateSecurityOrigin(
        SecurityOrigin::create("https", "example.com", 80));
  }

  void TearDown() override {
    if (m_childFrame) {
      m_childDocumentLoader->detachFromFrame();
      m_childDocumentLoader.clear();
      m_childFrame->detach(FrameDetachType::Remove);
    }
  }

  void createChildFrame() {
    m_childFrame = LocalFrame::create(
        MockCrossOriginFrameLoaderClient::create(document().frame()),
        document().frame()->host(), m_dummyFrameOwner.get());
    m_childFrame->setView(FrameView::create(*m_childFrame, IntSize(500, 500)));
    m_childFrame->init();
    m_childDocumentLoader = DocumentLoader::create(
        m_childFrame.get(), ResourceRequest("https://www.example.com"),
        SubstituteData(), ClientRedirectPolicy::NotClientRedirect);

    childDocument().updateSecurityOrigin(
        SecurityOrigin::create("https", "cross-origin.com", 80));
  }

  Document& document() { return m_dummyPageHolder->document(); }

  Document& childDocument() { return *m_childFrame->document(); }

  ScriptState* getScriptStateFrom(const Document& document) {
    return ScriptState::forMainWorld(document.frame());
  }

  void rejectPendingResolvers(BaseAudioContext* audioContext) {
    audioContext->rejectPendingResolvers();
  }

  void recordAutoplayStatus(BaseAudioContext* audioContext) {
    audioContext->recordAutoplayStatus();
  }

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  Persistent<DummyFrameOwner> m_dummyFrameOwner;

  Persistent<LocalFrame> m_childFrame;
  Persistent<DocumentLoader> m_childDocumentLoader;

  BaseAudioContextTestPlatform m_testPlatform;
};

TEST_F(BaseAudioContextTest, AutoplayMetrics_NoRestriction) {
  HistogramTester histogramTester;

  BaseAudioContext* audioContext =
      BaseAudioContext::create(document(), ASSERT_NO_EXCEPTION);
  recordAutoplayStatus(audioContext);

  histogramTester.expectTotalCount(kCrossOriginMetric, 0);
}

TEST_F(BaseAudioContextTest, AutoplayMetrics_CreateNoGesture) {
  HistogramTester histogramTester;
  createChildFrame();
  childDocument().settings()->setMediaPlaybackRequiresUserGesture(true);

  BaseAudioContext* audioContext =
      BaseAudioContext::create(childDocument(), ASSERT_NO_EXCEPTION);
  recordAutoplayStatus(audioContext);

  histogramTester.expectBucketCount(kCrossOriginMetric,
                                    AutoplayStatus::AutoplayStatusFailed, 1);
  histogramTester.expectTotalCount(kCrossOriginMetric, 1);
}

TEST_F(BaseAudioContextTest, AutoplayMetrics_CallResumeNoGesture) {
  HistogramTester histogramTester;
  createChildFrame();
  childDocument().settings()->setMediaPlaybackRequiresUserGesture(true);

  ScriptState::Scope scope(getScriptStateFrom(childDocument()));

  BaseAudioContext* audioContext =
      BaseAudioContext::create(childDocument(), ASSERT_NO_EXCEPTION);
  audioContext->resumeContext(getScriptStateFrom(childDocument()));
  rejectPendingResolvers(audioContext);
  recordAutoplayStatus(audioContext);

  histogramTester.expectBucketCount(kCrossOriginMetric,
                                    AutoplayStatus::AutoplayStatusFailed, 1);
  histogramTester.expectTotalCount(kCrossOriginMetric, 1);
}

TEST_F(BaseAudioContextTest, AutoplayMetrics_CreateGesture) {
  HistogramTester histogramTester;
  createChildFrame();
  childDocument().settings()->setMediaPlaybackRequiresUserGesture(true);

  UserGestureIndicator userGestureScope(DocumentUserGestureToken::create(
      &childDocument(), UserGestureToken::NewGesture));

  BaseAudioContext* audioContext =
      BaseAudioContext::create(childDocument(), ASSERT_NO_EXCEPTION);
  recordAutoplayStatus(audioContext);

  histogramTester.expectBucketCount(kCrossOriginMetric,
                                    AutoplayStatus::AutoplayStatusSucceeded, 1);
  histogramTester.expectTotalCount(kCrossOriginMetric, 1);
}

TEST_F(BaseAudioContextTest, AutoplayMetrics_CallResumeGesture) {
  HistogramTester histogramTester;
  createChildFrame();
  childDocument().settings()->setMediaPlaybackRequiresUserGesture(true);

  ScriptState::Scope scope(getScriptStateFrom(childDocument()));

  BaseAudioContext* audioContext =
      BaseAudioContext::create(childDocument(), ASSERT_NO_EXCEPTION);

  UserGestureIndicator userGestureScope(DocumentUserGestureToken::create(
      &childDocument(), UserGestureToken::NewGesture));

  audioContext->resumeContext(getScriptStateFrom(childDocument()));
  rejectPendingResolvers(audioContext);
  recordAutoplayStatus(audioContext);

  histogramTester.expectBucketCount(kCrossOriginMetric,
                                    AutoplayStatus::AutoplayStatusSucceeded, 1);
  histogramTester.expectTotalCount(kCrossOriginMetric, 1);
}

TEST_F(BaseAudioContextTest, AutoplayMetrics_NodeStartNoGesture) {
  HistogramTester histogramTester;
  createChildFrame();
  childDocument().settings()->setMediaPlaybackRequiresUserGesture(true);

  BaseAudioContext* audioContext =
      BaseAudioContext::create(childDocument(), ASSERT_NO_EXCEPTION);
  audioContext->maybeRecordStartAttempt();
  recordAutoplayStatus(audioContext);

  histogramTester.expectBucketCount(kCrossOriginMetric,
                                    AutoplayStatus::AutoplayStatusFailed, 1);
  histogramTester.expectTotalCount(kCrossOriginMetric, 1);
}

TEST_F(BaseAudioContextTest, AutoplayMetrics_NodeStartGesture) {
  HistogramTester histogramTester;
  createChildFrame();
  childDocument().settings()->setMediaPlaybackRequiresUserGesture(true);

  BaseAudioContext* audioContext =
      BaseAudioContext::create(childDocument(), ASSERT_NO_EXCEPTION);

  UserGestureIndicator userGestureScope(DocumentUserGestureToken::create(
      &childDocument(), UserGestureToken::NewGesture));
  audioContext->maybeRecordStartAttempt();
  recordAutoplayStatus(audioContext);

  histogramTester.expectBucketCount(
      kCrossOriginMetric, AutoplayStatus::AutoplayStatusFailedWithStart, 1);
  histogramTester.expectTotalCount(kCrossOriginMetric, 1);
}

TEST_F(BaseAudioContextTest, AutoplayMetrics_NodeStartNoGestureThenSuccess) {
  HistogramTester histogramTester;
  createChildFrame();
  childDocument().settings()->setMediaPlaybackRequiresUserGesture(true);

  ScriptState::Scope scope(getScriptStateFrom(childDocument()));

  BaseAudioContext* audioContext =
      BaseAudioContext::create(childDocument(), ASSERT_NO_EXCEPTION);
  audioContext->maybeRecordStartAttempt();

  UserGestureIndicator userGestureScope(DocumentUserGestureToken::create(
      &childDocument(), UserGestureToken::NewGesture));
  audioContext->resumeContext(getScriptStateFrom(childDocument()));
  rejectPendingResolvers(audioContext);
  recordAutoplayStatus(audioContext);

  histogramTester.expectBucketCount(kCrossOriginMetric,
                                    AutoplayStatus::AutoplayStatusSucceeded, 1);
  histogramTester.expectTotalCount(kCrossOriginMetric, 1);
}

TEST_F(BaseAudioContextTest, AutoplayMetrics_NodeStartGestureThenSucces) {
  HistogramTester histogramTester;
  createChildFrame();
  childDocument().settings()->setMediaPlaybackRequiresUserGesture(true);

  ScriptState::Scope scope(getScriptStateFrom(childDocument()));

  BaseAudioContext* audioContext =
      BaseAudioContext::create(childDocument(), ASSERT_NO_EXCEPTION);

  UserGestureIndicator userGestureScope(DocumentUserGestureToken::create(
      &childDocument(), UserGestureToken::NewGesture));
  audioContext->maybeRecordStartAttempt();
  audioContext->resumeContext(getScriptStateFrom(childDocument()));
  rejectPendingResolvers(audioContext);
  recordAutoplayStatus(audioContext);

  histogramTester.expectBucketCount(kCrossOriginMetric,
                                    AutoplayStatus::AutoplayStatusSucceeded, 1);
  histogramTester.expectTotalCount(kCrossOriginMetric, 1);
}

}  // namespace blink

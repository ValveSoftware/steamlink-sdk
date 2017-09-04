// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/shadow/MediaControls.h"

#include "core/HTMLNames.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLVideoElement.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Handle.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/WebMediaPlayer.h"
#include "public/platform/WebSize.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackAvailability.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackClient.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

namespace {

class MockVideoWebMediaPlayer : public WebMediaPlayer {
 public:
  void load(LoadType, const WebMediaPlayerSource&, CORSMode) override{};
  void play() override{};
  void pause() override{};
  bool supportsSave() const override { return false; };
  void seek(double seconds) override{};
  void setRate(double) override{};
  void setVolume(double) override{};
  WebTimeRanges buffered() const override { return WebTimeRanges(); };
  WebTimeRanges seekable() const override { return WebTimeRanges(); };
  void setSinkId(const WebString& sinkId,
                 const WebSecurityOrigin&,
                 WebSetSinkIdCallbacks*) override{};
  bool hasVideo() const override { return true; };
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
  double mediaTimeForTimeValue(double timeValue) const override {
    return timeValue;
  };
  unsigned decodedFrameCount() const override { return 0; };
  unsigned droppedFrameCount() const override { return 0; };
  size_t audioDecodedByteCount() const override { return 0; };
  size_t videoDecodedByteCount() const override { return 0; };
  void paint(WebCanvas*, const WebRect&, SkPaint&) override{};
};

class MockWebRemotePlaybackClient : public WebRemotePlaybackClient {
 public:
  void stateChanged(WebRemotePlaybackState) override {}
  void availabilityChanged(
      WebRemotePlaybackAvailability availability) override {
    m_availability = availability;
  }
  void promptCancelled() override {}
  bool remotePlaybackAvailable() const override {
    return m_availability == WebRemotePlaybackAvailability::DeviceAvailable;
  }

 private:
  WebRemotePlaybackAvailability m_availability =
      WebRemotePlaybackAvailability::Unknown;
};

class StubFrameLoaderClient : public EmptyFrameLoaderClient {
 public:
  static StubFrameLoaderClient* create() { return new StubFrameLoaderClient; }

  std::unique_ptr<WebMediaPlayer> createWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*) override {
    return wrapUnique(new MockVideoWebMediaPlayer);
  }

  WebRemotePlaybackClient* createWebRemotePlaybackClient(
      HTMLMediaElement&) override {
    if (!m_remotePlaybackClient) {
      m_remotePlaybackClient = wrapUnique(new MockWebRemotePlaybackClient);
    }
    return m_remotePlaybackClient.get();
  }

 private:
  std::unique_ptr<MockWebRemotePlaybackClient> m_remotePlaybackClient;
};

Element* getElementByShadowPseudoId(Node& rootNode,
                                    const char* shadowPseudoId) {
  for (Element& element : ElementTraversal::descendantsOf(rootNode)) {
    if (element.shadowPseudoId() == shadowPseudoId)
      return &element;
  }
  return nullptr;
}

bool isElementVisible(Element& element) {
  const StylePropertySet* inlineStyle = element.inlineStyle();

  if (!inlineStyle)
    return true;

  if (inlineStyle->getPropertyValue(CSSPropertyDisplay) == "none")
    return false;

  if (inlineStyle->hasProperty(CSSPropertyOpacity) &&
      inlineStyle->getPropertyValue(CSSPropertyOpacity).toDouble() == 0.0) {
    return false;
  }

  if (inlineStyle->getPropertyValue(CSSPropertyVisibility) == "hidden")
    return false;

  if (Element* parent = element.parentElement())
    return isElementVisible(*parent);

  return true;
}

}  // namespace

class MediaControlsTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    m_pageHolder = DummyPageHolder::create(IntSize(800, 600), nullptr,
                                           StubFrameLoaderClient::create());
    Document& document = this->document();

    document.write("<video>");
    HTMLVideoElement& video =
        toHTMLVideoElement(*document.querySelector("video"));
    m_mediaControls = video.mediaControls();

    // If scripts are not enabled, controls will always be shown.
    m_pageHolder->frame().settings()->setScriptEnabled(true);
  }

  void simulateRouteAvailabe() {
    m_mediaControls->mediaElement().remoteRouteAvailabilityChanged(
        WebRemotePlaybackAvailability::DeviceAvailable);
  }

  void ensureLayout() {
    // Force a relayout, so that the controls know the width.  Otherwise,
    // they don't know if, for example, the cast button will fit.
    m_mediaControls->mediaElement().clientWidth();
  }

  void simulateHideMediaControlsTimerFired() {
    m_mediaControls->hideMediaControlsTimerFired(nullptr);
  }

  MediaControls& mediaControls() { return *m_mediaControls; }
  Document& document() { return m_pageHolder->document(); }

 private:
  std::unique_ptr<DummyPageHolder> m_pageHolder;
  Persistent<MediaControls> m_mediaControls;
};

TEST_F(MediaControlsTest, HideAndShow) {
  mediaControls().mediaElement().setBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);

  Element* panel = getElementByShadowPseudoId(mediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  ASSERT_TRUE(isElementVisible(*panel));
  mediaControls().hide();
  ASSERT_FALSE(isElementVisible(*panel));
  mediaControls().show();
  ASSERT_TRUE(isElementVisible(*panel));
}

TEST_F(MediaControlsTest, Reset) {
  mediaControls().mediaElement().setBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);

  Element* panel = getElementByShadowPseudoId(mediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  ASSERT_TRUE(isElementVisible(*panel));
  mediaControls().reset();
  ASSERT_TRUE(isElementVisible(*panel));
}

TEST_F(MediaControlsTest, HideAndReset) {
  mediaControls().mediaElement().setBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);

  Element* panel = getElementByShadowPseudoId(mediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  ASSERT_TRUE(isElementVisible(*panel));
  mediaControls().hide();
  ASSERT_FALSE(isElementVisible(*panel));
  mediaControls().reset();
  ASSERT_FALSE(isElementVisible(*panel));
}

TEST_F(MediaControlsTest, ResetDoesNotTriggerInitialLayout) {
  Document& document = this->document();
  int oldElementCount = document.styleEngine().styleForElementCount();
  // Also assert that there are no layouts yet.
  ASSERT_EQ(0, oldElementCount);
  mediaControls().reset();
  int newElementCount = document.styleEngine().styleForElementCount();
  ASSERT_EQ(oldElementCount, newElementCount);
}

TEST_F(MediaControlsTest, CastButtonRequiresRoute) {
  ensureLayout();
  mediaControls().mediaElement().setBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);

  Element* castButton = getElementByShadowPseudoId(
      mediaControls(), "-internal-media-controls-cast-button");
  ASSERT_NE(nullptr, castButton);

  ASSERT_FALSE(isElementVisible(*castButton));

  simulateRouteAvailabe();
  ASSERT_TRUE(isElementVisible(*castButton));
}

TEST_F(MediaControlsTest, CastButtonDisableRemotePlaybackAttr) {
  ensureLayout();
  mediaControls().mediaElement().setBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);

  Element* castButton = getElementByShadowPseudoId(
      mediaControls(), "-internal-media-controls-cast-button");
  ASSERT_NE(nullptr, castButton);

  ASSERT_FALSE(isElementVisible(*castButton));
  simulateRouteAvailabe();
  ASSERT_TRUE(isElementVisible(*castButton));

  mediaControls().mediaElement().setBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, true);
  ASSERT_FALSE(isElementVisible(*castButton));

  mediaControls().mediaElement().setBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, false);
  ASSERT_TRUE(isElementVisible(*castButton));
}

TEST_F(MediaControlsTest, CastOverlayDefault) {
  Element* castOverlayButton = getElementByShadowPseudoId(
      mediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, castOverlayButton);

  simulateRouteAvailabe();
  ASSERT_TRUE(isElementVisible(*castOverlayButton));
}

TEST_F(MediaControlsTest, CastOverlayDisableRemotePlaybackAttr) {
  Element* castOverlayButton = getElementByShadowPseudoId(
      mediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, castOverlayButton);

  ASSERT_FALSE(isElementVisible(*castOverlayButton));
  simulateRouteAvailabe();
  ASSERT_TRUE(isElementVisible(*castOverlayButton));

  mediaControls().mediaElement().setBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, true);
  ASSERT_FALSE(isElementVisible(*castOverlayButton));

  mediaControls().mediaElement().setBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, false);
  ASSERT_TRUE(isElementVisible(*castOverlayButton));
}

TEST_F(MediaControlsTest, KeepControlsVisibleIfOverflowListVisible) {
  Element* overflowList = getElementByShadowPseudoId(
      mediaControls(), "-internal-media-controls-overflow-menu-list");
  ASSERT_NE(nullptr, overflowList);

  Element* panel = getElementByShadowPseudoId(mediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  mediaControls().mediaElement().setSrc("http://example.com");
  mediaControls().mediaElement().play();
  testing::runPendingTasks();

  mediaControls().show();
  mediaControls().toggleOverflowMenu();
  EXPECT_TRUE(isElementVisible(*overflowList));

  simulateHideMediaControlsTimerFired();
  EXPECT_TRUE(isElementVisible(*overflowList));
  EXPECT_TRUE(isElementVisible(*panel));
}

}  // namespace blink

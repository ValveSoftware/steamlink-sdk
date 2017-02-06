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
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

namespace {

Element* getElementByShadowPseudoId(Node& rootNode, const char* shadowPseudoId)
{
    for (Element& element : ElementTraversal::descendantsOf(rootNode)) {
        if (element.shadowPseudoId() == shadowPseudoId)
            return &element;
    }
    return nullptr;
}

bool isElementVisible(Element& element)
{
    const StylePropertySet* inlineStyle = element.inlineStyle();

    if (!inlineStyle)
        return true;

    if (inlineStyle->getPropertyValue(CSSPropertyDisplay) == "none")
        return false;

    if (inlineStyle->hasProperty(CSSPropertyOpacity)
        && inlineStyle->getPropertyValue(CSSPropertyOpacity).toDouble() == 0.0)
        return false;

    if (inlineStyle->getPropertyValue(CSSPropertyVisibility) == "hidden")
        return false;

    if (Element* parent = element.parentElement())
        return isElementVisible(*parent);

    return true;
}

} // namespace

class MediaControlsTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        m_pageHolder = DummyPageHolder::create(IntSize(800, 600));
        Document& document = this->document();

        document.write("<video>");
        HTMLVideoElement& video = toHTMLVideoElement(*document.querySelector("video", ASSERT_NO_EXCEPTION));
        m_mediaControls = video.mediaControls();

        // If scripts are not enabled, controls will always be shown.
        m_pageHolder->frame().settings()->setScriptEnabled(true);
    }

    void simulateRouteAvailabe()
    {
        m_mediaControls->mediaElement().remoteRouteAvailabilityChanged(true);
    }

    void ensureLayout()
    {
        // Force a relayout, so that the controls know the width.  Otherwise,
        // they don't know if, for example, the cast button will fit.
        m_mediaControls->mediaElement().clientWidth();
    }

    MediaControls& mediaControls() { return *m_mediaControls; }
    Document& document() { return m_pageHolder->document(); }

private:
    std::unique_ptr<DummyPageHolder> m_pageHolder;
    Persistent<MediaControls> m_mediaControls;
};

TEST_F(MediaControlsTest, HideAndShow)
{
    mediaControls().mediaElement().setBooleanAttribute(HTMLNames::controlsAttr, true);

    Element* panel = getElementByShadowPseudoId(mediaControls(), "-webkit-media-controls-panel");
    ASSERT_NE(nullptr, panel);

    ASSERT_TRUE(isElementVisible(*panel));
    mediaControls().hide();
    ASSERT_FALSE(isElementVisible(*panel));
    mediaControls().show();
    ASSERT_TRUE(isElementVisible(*panel));
}

TEST_F(MediaControlsTest, Reset)
{
    mediaControls().mediaElement().setBooleanAttribute(HTMLNames::controlsAttr, true);

    Element* panel = getElementByShadowPseudoId(mediaControls(), "-webkit-media-controls-panel");
    ASSERT_NE(nullptr, panel);

    ASSERT_TRUE(isElementVisible(*panel));
    mediaControls().reset();
    ASSERT_TRUE(isElementVisible(*panel));
}

TEST_F(MediaControlsTest, HideAndReset)
{
    mediaControls().mediaElement().setBooleanAttribute(HTMLNames::controlsAttr, true);

    Element* panel = getElementByShadowPseudoId(mediaControls(), "-webkit-media-controls-panel");
    ASSERT_NE(nullptr, panel);

    ASSERT_TRUE(isElementVisible(*panel));
    mediaControls().hide();
    ASSERT_FALSE(isElementVisible(*panel));
    mediaControls().reset();
    ASSERT_FALSE(isElementVisible(*panel));
}

TEST_F(MediaControlsTest, ResetDoesNotTriggerInitialLayout)
{
    Document& document = this->document();
    int oldElementCount = document.styleEngine().styleForElementCount();
    // Also assert that there are no layouts yet.
    ASSERT_EQ(0, oldElementCount);
    mediaControls().reset();
    int newElementCount = document.styleEngine().styleForElementCount();
    ASSERT_EQ(oldElementCount, newElementCount);
}

TEST_F(MediaControlsTest, CastButtonRequiresRoute)
{
    ensureLayout();
    mediaControls().mediaElement().setBooleanAttribute(HTMLNames::controlsAttr, true);

    Element* castButton = getElementByShadowPseudoId(mediaControls(), "-internal-media-controls-cast-button");
    ASSERT_NE(nullptr, castButton);

    ASSERT_FALSE(isElementVisible(*castButton));

    simulateRouteAvailabe();
    ASSERT_TRUE(isElementVisible(*castButton));
}

TEST_F(MediaControlsTest, CastButtonDisableRemotePlaybackAttr)
{
    ensureLayout();
    mediaControls().mediaElement().setBooleanAttribute(HTMLNames::controlsAttr, true);

    Element* castButton = getElementByShadowPseudoId(mediaControls(), "-internal-media-controls-cast-button");
    ASSERT_NE(nullptr, castButton);

    mediaControls().mediaElement().setBooleanAttribute(HTMLNames::disableremoteplaybackAttr, true);
    simulateRouteAvailabe();
    ASSERT_FALSE(isElementVisible(*castButton));

    mediaControls().mediaElement().setBooleanAttribute(HTMLNames::disableremoteplaybackAttr, false);
    mediaControls().reset();
    ASSERT_TRUE(isElementVisible(*castButton));
}

TEST_F(MediaControlsTest, CastOverlayDefault)
{
    Element* castOverlayButton = getElementByShadowPseudoId(mediaControls(), "-internal-media-controls-overlay-cast-button");
    ASSERT_NE(nullptr, castOverlayButton);

    simulateRouteAvailabe();
    ASSERT_TRUE(isElementVisible(*castOverlayButton));
}

TEST_F(MediaControlsTest, CastOverlayDisableRemotePlaybackAttr)
{
    Element* castOverlayButton = getElementByShadowPseudoId(mediaControls(), "-internal-media-controls-overlay-cast-button");
    ASSERT_NE(nullptr, castOverlayButton);

    mediaControls().mediaElement().setBooleanAttribute(HTMLNames::disableremoteplaybackAttr, true);
    simulateRouteAvailabe();
    ASSERT_FALSE(isElementVisible(*castOverlayButton));

    mediaControls().mediaElement().setBooleanAttribute(HTMLNames::disableremoteplaybackAttr, false);
    mediaControls().reset();
    ASSERT_TRUE(isElementVisible(*castOverlayButton));
}

} // namespace blink

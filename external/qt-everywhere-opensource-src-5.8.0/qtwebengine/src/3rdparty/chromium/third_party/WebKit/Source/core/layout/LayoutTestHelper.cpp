// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTestHelper.h"

#include "core/frame/FrameHost.h"
#include "core/html/HTMLIFrameElement.h"
#include "platform/scroll/ScrollbarTheme.h"

namespace blink {

RenderingTest::RenderingTest(FrameLoaderClient* frameLoaderClient)
    : m_frameLoaderClient(frameLoaderClient) { }

void RenderingTest::SetUp()
{
    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    DEFINE_STATIC_LOCAL(EmptyChromeClient, chromeClient, (EmptyChromeClient::create()));
    pageClients.chromeClient = &chromeClient;
    m_pageHolder = DummyPageHolder::create(IntSize(800, 600), &pageClients, m_frameLoaderClient.release(), settingOverrider());

    Settings::setMockScrollbarsEnabled(true);
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(true);
    EXPECT_TRUE(ScrollbarTheme::theme().usesOverlayScrollbars());

    // This ensures that the minimal DOM tree gets attached
    // correctly for tests that don't call setBodyInnerHTML.
    document().view()->updateAllLifecyclePhases();
}

void RenderingTest::TearDown()
{
    if (m_subframe) {
        m_subframe->detach(FrameDetachType::Remove);
        static_cast<SingleChildFrameLoaderClient*>(document().frame()->client())->setChild(nullptr);
        document().frame()->host()->decrementSubframeCount();
    }

    // We need to destroy most of the Blink structure here because derived tests may restore
    // RuntimeEnabledFeatures setting during teardown, which happens before our destructor
    // getting invoked, breaking the assumption that REF can't change during Blink lifetime.
    m_pageHolder = nullptr;
}

Document& RenderingTest::setupChildIframe(const AtomicString& iframeElementId, const String& htmlContentOfIframe)
{
    // TODO(pdr): This should be refactored to share code with the actual setup
    // instead of partially duplicating it here (e.g., LocalFrame::createView).
    HTMLIFrameElement& iframe = *toHTMLIFrameElement(document().getElementById(iframeElementId));
    m_childFrameLoaderClient = FrameLoaderClientWithParent::create(document().frame());
    m_subframe = LocalFrame::create(m_childFrameLoaderClient.get(), document().frame()->host(), &iframe);
    m_subframe->setView(FrameView::create(m_subframe.get(), IntSize(500, 500)));
    m_subframe->init();
    m_subframe->view()->setParentVisible(true);
    m_subframe->view()->setSelfVisible(true);
    iframe.setWidget(m_subframe->view());
    static_cast<SingleChildFrameLoaderClient*>(document().frame()->client())->setChild(m_subframe.get());
    document().frame()->host()->incrementSubframeCount();
    Document& frameDocument = *iframe.contentDocument();

    frameDocument.setBaseURLOverride(KURL(ParsedURLString, "http://test.com"));
    frameDocument.body()->setInnerHTML(htmlContentOfIframe, ASSERT_NO_EXCEPTION);
    return frameDocument;
}

} // namespace blink

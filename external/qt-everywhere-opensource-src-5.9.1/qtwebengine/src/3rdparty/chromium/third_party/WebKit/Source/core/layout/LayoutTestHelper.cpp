// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTestHelper.h"

#include "core/fetch/MemoryCache.h"
#include "core/frame/FrameHost.h"
#include "core/html/HTMLIFrameElement.h"
#include "platform/scroll/ScrollbarTheme.h"

namespace blink {

LocalFrame* SingleChildFrameLoaderClient::createFrame(
    const FrameLoadRequest&,
    const AtomicString& name,
    HTMLFrameOwnerElement* ownerElement) {
  DCHECK(!m_child) << "This test helper only supports one child frame.";

  LocalFrame* parentFrame = ownerElement->document().frame();
  auto* childClient = FrameLoaderClientWithParent::create(parentFrame);
  m_child = LocalFrame::create(childClient, parentFrame->host(), ownerElement);
  m_child->createView(IntSize(500, 500), Color(), true /* transparent */);
  m_child->init();

  return m_child.get();
}

void FrameLoaderClientWithParent::detached(FrameDetachType) {
  static_cast<SingleChildFrameLoaderClient*>(parent()->client())
      ->didDetachChild();
}

RenderingTest::RenderingTest(FrameLoaderClient* frameLoaderClient)
    : m_frameLoaderClient(frameLoaderClient) {}

void RenderingTest::SetUp() {
  Page::PageClients pageClients;
  fillWithEmptyClients(pageClients);
  DEFINE_STATIC_LOCAL(EmptyChromeClient, chromeClient,
                      (EmptyChromeClient::create()));
  pageClients.chromeClient = &chromeClient;
  m_pageHolder = DummyPageHolder::create(
      IntSize(800, 600), &pageClients, m_frameLoaderClient, settingOverrider());

  Settings::setMockScrollbarsEnabled(true);
  RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(true);
  EXPECT_TRUE(ScrollbarTheme::theme().usesOverlayScrollbars());

  // This ensures that the minimal DOM tree gets attached
  // correctly for tests that don't call setBodyInnerHTML.
  document().view()->updateAllLifecyclePhases();
}

void RenderingTest::TearDown() {
  // We need to destroy most of the Blink structure here because derived tests
  // may restore RuntimeEnabledFeatures setting during teardown, which happens
  // before our destructor getting invoked, breaking the assumption that REF
  // can't change during Blink lifetime.
  m_pageHolder = nullptr;

  // Clear memory cache, otherwise we can leak pruned resources.
  memoryCache()->evictResources();
}

void RenderingTest::setChildFrameHTML(const String& html) {
  childDocument().setBaseURLOverride(KURL(ParsedURLString, "http://test.com"));
  childDocument().body()->setInnerHTML(html, ASSERT_NO_EXCEPTION);
}

}  // namespace blink

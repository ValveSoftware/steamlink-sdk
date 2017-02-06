// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/ExternalPopupMenu.h"

#include "core/HTMLNames.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/frame/FrameHost.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLSelectElement.h"
#include "core/layout/LayoutMenuList.h"
#include "core/page/Page.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/PopupMenu.h"
#include "platform/testing/URLTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebCache.h"
#include "public/web/WebExternalPopupMenu.h"
#include "public/web/WebPopupMenuInfo.h"
#include "public/web/WebSettings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include <memory>

namespace blink {

class ExternalPopupMenuDisplayNoneItemsTest : public testing::Test {
public:
    ExternalPopupMenuDisplayNoneItemsTest() { }

protected:
    void SetUp() override
    {
        m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
        HTMLSelectElement* element = HTMLSelectElement::create(m_dummyPageHolder->document());
        // Set the 4th an 5th items to have "display: none" property
        element->setInnerHTML("<option><option><option><option style='display:none;'><option style='display:none;'><option><option>", ASSERT_NO_EXCEPTION);
        m_dummyPageHolder->document().body()->appendChild(element, ASSERT_NO_EXCEPTION);
        m_ownerElement = element;
        m_dummyPageHolder->document().updateStyleAndLayoutIgnorePendingStylesheets();
    }

    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
    Persistent<HTMLSelectElement> m_ownerElement;
};

TEST_F(ExternalPopupMenuDisplayNoneItemsTest, PopupMenuInfoSizeTest)
{
    WebPopupMenuInfo info;
    ExternalPopupMenu::getPopupMenuInfo(info, *m_ownerElement);
    EXPECT_EQ(5U, info.items.size());
}

TEST_F(ExternalPopupMenuDisplayNoneItemsTest, IndexMappingTest)
{
    // 6th indexed item in popupmenu would be the 4th item in ExternalPopupMenu,
    // and vice-versa.
    EXPECT_EQ(4, ExternalPopupMenu::toExternalPopupMenuItemIndex(6, *m_ownerElement));
    EXPECT_EQ(6, ExternalPopupMenu::toPopupMenuItemIndex(4, *m_ownerElement));

    // Invalid index, methods should return -1.
    EXPECT_EQ(-1, ExternalPopupMenu::toExternalPopupMenuItemIndex(8, *m_ownerElement));
    EXPECT_EQ(-1, ExternalPopupMenu::toPopupMenuItemIndex(8, *m_ownerElement));
}

class ExternalPopupMenuWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
public:
    WebExternalPopupMenu* createExternalPopupMenu(const WebPopupMenuInfo&, WebExternalPopupMenuClient*) override
    {
        return &m_mockWebExternalPopupMenu;
    }
    WebRect shownBounds() const
    {
        return m_mockWebExternalPopupMenu.shownBounds();
    }
private:
    class MockWebExternalPopupMenu : public WebExternalPopupMenu {
        void show(const WebRect& bounds) override
        {
            m_shownBounds = bounds;
        }
        void close() override { }

    public:
        WebRect shownBounds() const
        {
            return m_shownBounds;
        }

    private:
        WebRect m_shownBounds;
    };
    WebRect m_shownBounds;
    MockWebExternalPopupMenu m_mockWebExternalPopupMenu;
};

class ExternalPopupMenuTest : public testing::Test {
public:
    ExternalPopupMenuTest() : m_baseURL("http://www.test.com") { }

protected:
    void SetUp() override
    {
        m_helper.initialize(false, &m_webFrameClient, &m_webViewClient);
        webView()->setUseExternalPopupMenus(true);
    }
    void TearDown() override
    {
        Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
        WebCache::clear();
    }

    void registerMockedURLLoad(const std::string& fileName)
    {
        URLTestHelpers::registerMockedURLLoad(URLTestHelpers::toKURL(m_baseURL + fileName), WebString::fromUTF8(fileName.c_str()), WebString::fromUTF8("popup/"), WebString::fromUTF8("text/html"));
    }

    void loadFrame(const std::string& fileName)
    {
        FrameTestHelpers::loadFrame(mainFrame(), m_baseURL + fileName);
        webView()->resize(WebSize(800, 600));
        webView()->updateAllLifecyclePhases();
    }

    WebViewImpl* webView() const { return m_helper.webViewImpl(); }
    const ExternalPopupMenuWebFrameClient& client() const { return m_webFrameClient; }
    WebLocalFrameImpl* mainFrame() const { return m_helper.webViewImpl()->mainFrameImpl(); }

private:
    std::string m_baseURL;
    FrameTestHelpers::TestWebViewClient m_webViewClient;
    ExternalPopupMenuWebFrameClient m_webFrameClient;
    FrameTestHelpers::WebViewHelper m_helper;
};

TEST_F(ExternalPopupMenuTest, PopupAccountsForVisualViewportOffset)
{
    registerMockedURLLoad("select_mid_screen.html");
    loadFrame("select_mid_screen.html");

    webView()->resize(WebSize(100, 100));
    webView()->updateAllLifecyclePhases();

    HTMLSelectElement* select = toHTMLSelectElement(mainFrame()->frame()->document()->getElementById("select"));
    LayoutMenuList* menuList = toLayoutMenuList(select->layoutObject());
    ASSERT_TRUE(menuList);

    VisualViewport& visualViewport = webView()->page()->frameHost().visualViewport();

    IntRect rectInDocument = menuList->absoluteBoundingBoxRect();

    webView()->setPageScaleFactor(2);
    IntPoint scrollDelta(20, 30);
    visualViewport.move(scrollDelta);

    select->showPopup();

    EXPECT_EQ(rectInDocument.x() - scrollDelta.x(), client().shownBounds().x);
    EXPECT_EQ(rectInDocument.y() - scrollDelta.y(), client().shownBounds().y);
}

TEST_F(ExternalPopupMenuTest, DidAcceptIndex)
{
    registerMockedURLLoad("select.html");
    loadFrame("select.html");

    HTMLSelectElement* select = toHTMLSelectElement(mainFrame()->frame()->document()->getElementById("select"));
    LayoutMenuList* menuList = toLayoutMenuList(select->layoutObject());
    ASSERT_TRUE(menuList);

    select->showPopup();
    ASSERT_TRUE(select->popupIsVisible());

    WebExternalPopupMenuClient* client = static_cast<ExternalPopupMenu*>(select->popup());
    client->didAcceptIndex(2);
    EXPECT_FALSE(select->popupIsVisible());
    ASSERT_STREQ("2", menuList->text().utf8().data());
    EXPECT_EQ(2, select->selectedIndex());
}

TEST_F(ExternalPopupMenuTest, DidAcceptIndices)
{
    registerMockedURLLoad("select.html");
    loadFrame("select.html");

    HTMLSelectElement* select = toHTMLSelectElement(mainFrame()->frame()->document()->getElementById("select"));
    LayoutMenuList* menuList = toLayoutMenuList(select->layoutObject());
    ASSERT_TRUE(menuList);

    select->showPopup();
    ASSERT_TRUE(select->popupIsVisible());

    WebExternalPopupMenuClient* client = static_cast<ExternalPopupMenu*>(select->popup());
    int indices[] = { 2 };
    WebVector<int> indicesVector(indices, 1);
    client->didAcceptIndices(indicesVector);
    EXPECT_FALSE(select->popupIsVisible());
    EXPECT_STREQ("2", menuList->text().utf8().data());
    EXPECT_EQ(2, select->selectedIndex());
}

TEST_F(ExternalPopupMenuTest, DidAcceptIndicesClearSelect)
{
    registerMockedURLLoad("select.html");
    loadFrame("select.html");

    HTMLSelectElement* select = toHTMLSelectElement(mainFrame()->frame()->document()->getElementById("select"));
    LayoutMenuList* menuList = toLayoutMenuList(select->layoutObject());
    ASSERT_TRUE(menuList);

    select->showPopup();
    ASSERT_TRUE(select->popupIsVisible());

    WebExternalPopupMenuClient* client = static_cast<ExternalPopupMenu*>(select->popup());
    WebVector<int> indices;
    client->didAcceptIndices(indices);
    EXPECT_FALSE(select->popupIsVisible());
    EXPECT_EQ(-1, select->selectedIndex());
}

} // namespace blink

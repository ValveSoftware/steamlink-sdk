// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/ContextMenuController.h"

#include "core/clipboard/DataTransfer.h"
#include "core/events/MouseEvent.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/ContextMenu.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class ContextMenuControllerTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        m_pageHolder = DummyPageHolder::create(IntSize(800, 600));
    }

    Document& document() const { return m_pageHolder->document(); }

    void setBodyInnerHTML(const String& htmlContent)
    {
        document().body()->setInnerHTML(htmlContent, ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
    }

private:
    std::unique_ptr<DummyPageHolder> m_pageHolder;
};

TEST_F(ContextMenuControllerTest, TestCustomMenu)
{
    document().settings()->setScriptEnabled(true);
    // Load the the test page.
    setBodyInnerHTML(
        "<button id=\"button_id\" contextmenu=\"menu_id\" style=\"height: 100px; width: 100px;\">"
        "<menu type=\"context\" id=\"menu_id\">"
        "<menuitem label=\"Item1\" onclick='document.title = \"Title 1\";'>"
        "<menuitem label=\"Item2\" onclick='document.title = \"Title 2\";'>"
        "<menuitem label=\"Item3\" onclick='document.title = \"Title 3\";'>"
        "<menu label='Submenu'>"
        "<menuitem label=\"Item4\" onclick='document.title = \"Title 4\";'>"
        "<menuitem label=\"Item5\" onclick='document.title = \"Title 5\";'>"
        "<menuitem label=\"Item6\" onclick='document.title = \"Title 6\";'>"
        "</menu>"
        "<menuitem id=\"item7\" type=\"checkbox\" checked label=\"Item7\""
        "onclick='if (document.getElementById(\"item7\").hasAttribute(\"checked\"))"
        "document.title = \"Title 7 checked\"; else document.title = \"Title 7 not checked\";'>"
        "<menuitem id=\"item8\" type=\"radio\" radiogroup=\"group\" label=\"Item8\""
        "onclick='if (document.getElementById(\"item8\").hasAttribute(\"checked\"))"
        "document.title = \"Title 8 checked\"; else if (document.getElementById(\"item9\").hasAttribute(\"checked\"))"
        "document.title = \"Title 8 not checked and Title 9 checked\";'>"
        "<menuitem id=\"item9\" type=\"radio\" radiogroup=\"group\" checked label=\"Item9\""
        "onclick='if (document.getElementById(\"item9\").hasAttribute(\"checked\"))"
        "document.title = \"Title 9 checked\"; else document.title = \"Title 9 not checked\";'>"
        "<menuitem id=\"item10\" type=\"radio\" radiogroup=\"group\" label=\"Item10\""
        "onclick='if (document.getElementById(\"item10\").hasAttribute(\"checked\"))"
        "document.title = \"Title 10 checked\"; else if (document.getElementById(\"item8\").hasAttribute(\"checked\"))"
        "document.title = \"Title 10 not checked and Title 8 checked\";'>"
        "</menu>"
        "</button>");

    // Create right button click event and pass it to context menu controller.
    Event* event = MouseEvent::create(EventTypeNames::click, false, false,
        document().domWindow(), 50, 50, 0, 0, 0, 0, 0, PlatformEvent::NoModifiers, 1, 0, nullptr, 0,
        PlatformMouseEvent::RealOrIndistinguishable, String());
    document().getElementById("button_id")->focus();
    event->setTarget(document().getElementById("button_id"));
    document().page()->contextMenuController().handleContextMenuEvent(event);

    // Item 1
    // Item 2
    // Item 3
    // Submenu > Item 4
    //           Item 5
    //           Item 6
    // *Item 7
    // Item 8
    // *Item 9
    // Item 10
    const Vector<ContextMenuItem>& items = document().page()->contextMenuController().contextMenu()->items();
    EXPECT_EQ(8u, items.size());
    EXPECT_EQ(ActionType, items[0].type());
    EXPECT_STREQ("Item1", items[0].title().utf8().data());
    document().page()->contextMenuController().contextMenuItemSelected(&items[0]);
    EXPECT_STREQ("Title 1", document().title().utf8().data());
    EXPECT_EQ(SubmenuType, items[3].type());
    EXPECT_STREQ("Submenu", items[3].title().utf8().data());
    const Vector<ContextMenuItem>& subMenuItems = items[3].subMenuItems();
    EXPECT_EQ(3u, subMenuItems.size());
    EXPECT_STREQ("Item6", subMenuItems[2].title().utf8().data());
    document().page()->contextMenuController().contextMenuItemSelected(&subMenuItems[2]);
    EXPECT_STREQ("Title 6", document().title().utf8().data());
    document().page()->contextMenuController().contextMenuItemSelected(&items[4]);
    EXPECT_STREQ("Title 7 checked", document().title().utf8().data());
    document().page()->contextMenuController().contextMenuItemSelected(&items[4]);
    EXPECT_STREQ("Title 7 not checked", document().title().utf8().data());
    document().page()->contextMenuController().contextMenuItemSelected(&items[5]);
    EXPECT_STREQ("Title 8 not checked and Title 9 checked", document().title().utf8().data());
    document().page()->contextMenuController().contextMenuItemSelected(&items[7]);
    EXPECT_STREQ("Title 10 not checked and Title 8 checked", document().title().utf8().data());
}

} // namespace blink

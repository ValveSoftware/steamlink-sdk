// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/ChromeClient.h"

#include "core/dom/Document.h"
#include "core/layout/HitTestResult.h"
#include "core/loader/EmptyClients.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class ChromeClientToolTipLogger : public EmptyChromeClient {
public:
    void setToolTip(const String& text, TextDirection) override
    {
        m_toolTipForLastSetToolTip = text;
    }

    String toolTipForLastSetToolTip() const { return m_toolTipForLastSetToolTip; }
    void clearToolTipForLastSetToolTip() { m_toolTipForLastSetToolTip = String(); }

private:
    String m_toolTipForLastSetToolTip;
};

} // anonymous namespace

class ChromeClientTest : public testing::Test {
};

TEST_F(ChromeClientTest, SetToolTipFlood)
{
    ChromeClientToolTipLogger logger;
    ChromeClient* client = &logger;
    HitTestResult result(HitTestRequest(HitTestRequest::Move), LayoutPoint(10, 20));
    Document* doc = Document::create();
    Element* element = HTMLElement::create(HTMLNames::divTag, *doc);
    element->setAttribute(HTMLNames::titleAttr, "tooltip");
    result.setInnerNode(element);

    client->setToolTip(result);
    EXPECT_EQ("tooltip", logger.toolTipForLastSetToolTip());

    // seToolTip(HitTestResult) again in the same condition.
    logger.clearToolTipForLastSetToolTip();
    client->setToolTip(result);
    // setToolTip(String,TextDirection) should not be called.
    EXPECT_EQ(String(), logger.toolTipForLastSetToolTip());

    // Cancel the tooltip, and setToolTip(HitTestResult) again.
    client->clearToolTip();
    logger.clearToolTipForLastSetToolTip();
    client->setToolTip(result);
    // setToolTip(String,TextDirection) should not be called.
    EXPECT_EQ(String(), logger.toolTipForLastSetToolTip());

    logger.clearToolTipForLastSetToolTip();
    element->setAttribute(HTMLNames::titleAttr, "updated");
    client->setToolTip(result);
    // setToolTip(String,TextDirection) should be called because tooltip string
    // is different from the last one.
    EXPECT_EQ("updated", logger.toolTipForLastSetToolTip());
}

} // namespace blink

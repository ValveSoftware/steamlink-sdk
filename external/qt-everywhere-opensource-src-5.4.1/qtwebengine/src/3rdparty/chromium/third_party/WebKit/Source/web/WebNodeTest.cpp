// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/web/WebNode.h"

#include "core/testing/DummyPageHolder.h"
#include "public/web/WebElement.h"
#include "public/web/WebElementCollection.h"
#include <gtest/gtest.h>

namespace blink {

using WebCore::Document;
using WebCore::DummyPageHolder;
using WebCore::IntSize;

class WebNodeTest : public testing::Test {
protected:
    Document& document() { return m_pageHolder->document(); }

private:
    virtual void SetUp() OVERRIDE;

    OwnPtr<DummyPageHolder> m_pageHolder;
};

void WebNodeTest::SetUp()
{
    m_pageHolder = WebCore::DummyPageHolder::create(IntSize(800, 600));
}

TEST_F(WebNodeTest, GetElementsByTagName)
{
    document().documentElement()->setInnerHTML("<body><LABEL></LABEL><svg xmlns='http://www.w3.org/2000/svg'><label></label></svg></body>", ASSERT_NO_EXCEPTION);
    WebNode node(document().documentElement());
    // WebNode::getElementsByTagName returns only HTML elements.
    WebElementCollection collection = node.getElementsByTagName("label");
    EXPECT_EQ(1u, collection.length());
    EXPECT_TRUE(collection.firstItem().hasHTMLTagName("label"));
    // The argument should be lower-case.
    collection = node.getElementsByTagName("LABEL");
    EXPECT_EQ(0u, collection.length());
}

}

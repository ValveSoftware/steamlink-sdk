// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebNode.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/testing/DummyPageHolder.h"
#include "public/web/WebElement.h"
#include "public/web/WebElementCollection.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class WebNodeTest : public testing::Test {
 protected:
  Document& document() { return m_pageHolder->document(); }

  void setInnerHTML(const String& html) {
    document().documentElement()->setInnerHTML(html);
  }

  WebNode root() { return WebNode(document().documentElement()); }

 private:
  void SetUp() override;

  std::unique_ptr<DummyPageHolder> m_pageHolder;
};

void WebNodeTest::SetUp() {
  m_pageHolder = DummyPageHolder::create(IntSize(800, 600));
}

TEST_F(WebNodeTest, QuerySelectorMatches) {
  setInnerHTML("<div id=x><span class=a></span></div>");
  WebElement element = root().querySelector(".a");
  EXPECT_FALSE(element.isNull());
  EXPECT_TRUE(element.hasHTMLTagName("span"));
}

TEST_F(WebNodeTest, QuerySelectorDoesNotMatch) {
  setInnerHTML("<div id=x><span class=a></span></div>");
  WebElement element = root().querySelector("section");
  EXPECT_TRUE(element.isNull());
}

TEST_F(WebNodeTest, QuerySelectorError) {
  setInnerHTML("<div></div>");
  WebElement element = root().querySelector("@invalid-selector");
  EXPECT_TRUE(element.isNull());
}

TEST_F(WebNodeTest, GetElementsByHTMLTagName) {
  setInnerHTML(
      "<body><LABEL></LABEL><svg "
      "xmlns='http://www.w3.org/2000/svg'><label></label></svg></body>");
  // WebNode::getElementsByHTMLTagName returns only HTML elements.
  WebElementCollection collection = root().getElementsByHTMLTagName("label");
  EXPECT_EQ(1u, collection.length());
  EXPECT_TRUE(collection.firstItem().hasHTMLTagName("label"));
  // The argument should be lower-case.
  collection = root().getElementsByHTMLTagName("LABEL");
  EXPECT_EQ(0u, collection.length());
}

}  // namespace blink

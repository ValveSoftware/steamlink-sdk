// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLEmbedElement.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLObjectElement.h"
#include "core/style/ComputedStyle.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class HTMLEmbedElementTest : public testing::Test {
 protected:
  HTMLEmbedElementTest() {}

  void SetUp() override;

  Document& document() const { return *m_document; }

  void setHtmlInnerHTML(const char* htmlContent);

  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  Persistent<Document> m_document;
};

void HTMLEmbedElementTest::SetUp() {
  m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
  m_document = &m_dummyPageHolder->document();
  DCHECK(m_document);
}

void HTMLEmbedElementTest::setHtmlInnerHTML(const char* htmlContent) {
  document().documentElement()->setInnerHTML(String::fromUTF8(htmlContent));
  document().view()->updateAllLifecyclePhases();
}

TEST_F(HTMLEmbedElementTest, FallbackState) {
  // Load <object> element with a <embed> child.
  // This can be seen on sites with Flash cookies,
  // for example on www.yandex.ru
  setHtmlInnerHTML(
      "<div>"
      "<object classid='clsid:D27CDB6E-AE6D-11cf-96B8-444553540000' width='1' "
      "height='1' id='fco'>"
      "<param name='movie' value='//site.com/flash-cookie.swf'>"
      "<param name='allowScriptAccess' value='Always'>"
      "<embed src='//site.com/flash-cookie.swf' allowscriptaccess='Always' "
      "width='1' height='1' id='fce'>"
      "</object></div>");

  auto* objectElement = document().getElementById("fco");
  ASSERT_TRUE(objectElement);
  ASSERT_TRUE(isHTMLObjectElement(objectElement));
  HTMLObjectElement* object = toHTMLObjectElement(objectElement);

  // At this moment updateWidget() function is not called, so
  // useFallbackContent() will return false.
  // But the element will likely to use fallback content after updateWidget().
  EXPECT_TRUE(object->hasFallbackContent());
  EXPECT_FALSE(object->useFallbackContent());
  EXPECT_TRUE(object->willUseFallbackContentAtLayout());

  auto* embedElement = document().getElementById("fce");
  ASSERT_TRUE(embedElement);
  ASSERT_TRUE(isHTMLEmbedElement(embedElement));
  HTMLEmbedElement* embed = toHTMLEmbedElement(embedElement);

  document().view()->updateAllLifecyclePhases();

  // We should get |true| as a result and don't trigger a DCHECK.
  EXPECT_TRUE(static_cast<Element*>(embed)->layoutObjectIsNeeded(
      ComputedStyle::initialStyle()));

  // This call will update fallback state of the object.
  object->updateWidget();

  EXPECT_TRUE(object->hasFallbackContent());
  EXPECT_TRUE(object->useFallbackContent());
  EXPECT_TRUE(object->willUseFallbackContentAtLayout());

  document().view()->updateAllLifecyclePhases();
  EXPECT_TRUE(static_cast<Element*>(embed)->layoutObjectIsNeeded(
      ComputedStyle::initialStyle()));
}

}  // namespace blink

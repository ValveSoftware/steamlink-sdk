// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/SelectorQuery.h"

#include "core/css/parser/CSSParser.h"
#include "core/dom/Document.h"
#include "core/html/HTMLHtmlElement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

TEST(SelectorQueryTest, NotMatchingPseudoElement)
{
    Document* document = Document::create();
    HTMLHtmlElement* html = HTMLHtmlElement::create(*document);
    document->appendChild(html);
    document->documentElement()->setInnerHTML("<body><style>span::before { content: 'X' }</style><span></span></body>", ASSERT_NO_EXCEPTION);

    CSSSelectorList selectorList = CSSParser::parseSelector(CSSParserContext(*document, nullptr), nullptr, "span::before");
    std::unique_ptr<SelectorQuery> query = SelectorQuery::adopt(std::move(selectorList));
    Element* elm = query->queryFirst(*document);
    EXPECT_EQ(nullptr, elm);

    selectorList = CSSParser::parseSelector(CSSParserContext(*document, nullptr), nullptr, "span");
    query = SelectorQuery::adopt(std::move(selectorList));
    elm = query->queryFirst(*document);
    EXPECT_NE(nullptr, elm);
}

} // namespace blink

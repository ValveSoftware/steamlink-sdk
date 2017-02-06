// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/StyleEngine.h"

#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class StyleEngineTest : public ::testing::Test {
protected:
    void SetUp() override;

    Document& document() { return m_dummyPageHolder->document(); }
    StyleEngine& styleEngine() { return document().styleEngine(); }

    bool isDocumentStyleSheetCollectionClean() { return !styleEngine().shouldUpdateDocumentStyleSheetCollection(AnalyzedStyleUpdate); }

private:
    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

void StyleEngineTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
}

TEST_F(StyleEngineTest, DocumentDirtyAfterInject)
{
    StyleSheetContents* parsedSheet = StyleSheetContents::create(CSSParserContext(document(), nullptr));
    parsedSheet->parseString("div {}");
    styleEngine().injectAuthorSheet(parsedSheet);
    document().view()->updateAllLifecyclePhases();

    EXPECT_TRUE(isDocumentStyleSheetCollectionClean());
}

TEST_F(StyleEngineTest, AnalyzedInject)
{
    document().body()->setInnerHTML("<style>div { color: red }</style><div id='t1'>Green</div><div></div>", ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();

    Element* t1 = document().getElementById("t1");
    ASSERT_TRUE(t1);
    ASSERT_TRUE(t1->computedStyle());
    EXPECT_EQ(makeRGB(255, 0, 0), t1->computedStyle()->visitedDependentColor(CSSPropertyColor));

    unsigned beforeCount = styleEngine().styleForElementCount();

    StyleSheetContents* parsedSheet = StyleSheetContents::create(CSSParserContext(document(), nullptr));
    parsedSheet->parseString("#t1 { color: green }");
    styleEngine().injectAuthorSheet(parsedSheet);
    document().view()->updateAllLifecyclePhases();

    unsigned afterCount = styleEngine().styleForElementCount();
    EXPECT_EQ(1u, afterCount - beforeCount);

    ASSERT_TRUE(t1->computedStyle());
    EXPECT_EQ(makeRGB(0, 128, 0), t1->computedStyle()->visitedDependentColor(CSSPropertyColor));
}

} // namespace blink

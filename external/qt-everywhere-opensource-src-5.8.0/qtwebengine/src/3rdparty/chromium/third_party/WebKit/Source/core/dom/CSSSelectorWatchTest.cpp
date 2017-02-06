// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/CSSSelectorWatch.h"

#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class CSSSelectorWatchTest : public ::testing::Test {
protected:
    void SetUp() override;

    Document& document() { return m_dummyPageHolder->document(); }
    StyleEngine& styleEngine() { return document().styleEngine(); }

    static const HashSet<String> addedSelectors(const CSSSelectorWatch& watch) { return watch.m_addedSelectors; }
    static const HashSet<String> removedSelectors(const CSSSelectorWatch& watch) { return watch.m_removedSelectors; }
    static void clearAddedRemoved(CSSSelectorWatch&);

private:
    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

void CSSSelectorWatchTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
}

void CSSSelectorWatchTest::clearAddedRemoved(CSSSelectorWatch& watch)
{
    watch.m_addedSelectors.clear();
    watch.m_removedSelectors.clear();
}

TEST_F(CSSSelectorWatchTest, RecalcOnDocumentChange)
{
    document().body()->setInnerHTML(
        "<div>"
        "  <span id='x' class='a'></span>"
        "  <span id='y' class='b'><span></span></span>"
        "  <span id='z'><span></span></span>"
        "</div>", ASSERT_NO_EXCEPTION);

    CSSSelectorWatch& watch = CSSSelectorWatch::from(document());

    Vector<String> selectors;
    selectors.append(".a");
    watch.watchCSSSelectors(selectors);

    document().view()->updateAllLifecyclePhases();

    selectors.clear();
    selectors.append(".b");
    selectors.append(".c");
    selectors.append("#nomatch");
    watch.watchCSSSelectors(selectors);

    document().view()->updateAllLifecyclePhases();

    Element* x = document().getElementById("x");
    Element* y = document().getElementById("y");
    Element* z = document().getElementById("z");
    ASSERT_TRUE(x);
    ASSERT_TRUE(y);
    ASSERT_TRUE(z);

    x->removeAttribute(HTMLNames::classAttr);
    y->removeAttribute(HTMLNames::classAttr);
    z->setAttribute(HTMLNames::classAttr, "c");

    clearAddedRemoved(watch);

    unsigned beforeCount = styleEngine().styleForElementCount();
    document().view()->updateAllLifecyclePhases();
    unsigned afterCount = styleEngine().styleForElementCount();

    EXPECT_EQ(2u, afterCount - beforeCount);

    EXPECT_EQ(1u, addedSelectors(watch).size());
    EXPECT_TRUE(addedSelectors(watch).contains(".c"));

    EXPECT_EQ(1u, removedSelectors(watch).size());
    EXPECT_TRUE(removedSelectors(watch).contains(".b"));
}

} // namespace blink

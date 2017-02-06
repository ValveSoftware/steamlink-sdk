// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/EventPath.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/PseudoElement.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class EventPathTest : public ::testing::Test {
protected:
    Document& document() const { return m_dummyPageHolder->document(); }

private:
    void SetUp() override;

    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

void EventPathTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
}

TEST_F(EventPathTest, ShouldBeEmptyForPseudoElementWithoutParentElement)
{
    Element* div = document().createElement(HTMLNames::divTag, CreatedByCreateElement);
    PseudoElement* pseudo = PseudoElement::create(div, PseudoIdFirstLetter);
    pseudo->dispose();
    EventPath eventPath(*pseudo);
    EXPECT_TRUE(eventPath.isEmpty());
}

} // namespace blink

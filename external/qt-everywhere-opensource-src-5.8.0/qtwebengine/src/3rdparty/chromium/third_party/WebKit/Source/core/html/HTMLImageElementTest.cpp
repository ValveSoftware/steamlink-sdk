// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLImageElement.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

const int viewportWidth = 500;
const int viewportHeight = 600;
class HTMLImageElementTest : public testing::Test {
protected:
    HTMLImageElementTest()
        : m_dummyPageHolder(DummyPageHolder::create(IntSize(viewportWidth, viewportHeight)))
    {
    }

    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

TEST_F(HTMLImageElementTest, width)
{
    HTMLImageElement* image = HTMLImageElement::create(m_dummyPageHolder->document(), nullptr, /* createdByParser */ false);
    image->setAttribute(HTMLNames::widthAttr, "400");
    // TODO(yoav): `width` does not impact resourceWidth until we resolve https://github.com/ResponsiveImagesCG/picture-element/issues/268
    EXPECT_EQ(500, image->getResourceWidth().width);
    image->setAttribute(HTMLNames::sizesAttr, "100vw");
    EXPECT_EQ(500, image->getResourceWidth().width);
}

TEST_F(HTMLImageElementTest, sourceSize)
{
    HTMLImageElement* image = HTMLImageElement::create(m_dummyPageHolder->document(), nullptr, /* createdByParser */ false);
    image->setAttribute(HTMLNames::widthAttr, "400");
    EXPECT_EQ(viewportWidth, image->sourceSize(*image));
    image->setAttribute(HTMLNames::sizesAttr, "50vw");
    EXPECT_EQ(250, image->sourceSize(*image));
}

} // namespace blink

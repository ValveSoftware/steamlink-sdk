// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/LocalFrame.h"

#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/DragImage.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LocalFrameTest : public ::testing::Test {
protected:
    LocalFrameTest() = default;
    ~LocalFrameTest() override = default;

    Document& document() const { return m_dummyPageHolder->document(); }
    LocalFrame& frame() const { return *document().frame(); }

    void setBodyContent(const std::string& bodyContent)
    {
        document().body()->setInnerHTML(String::fromUTF8(bodyContent.c_str()), ASSERT_NO_EXCEPTION);
    }

    void updateAllLifecyclePhases()
    {
        document().view()->updateAllLifecyclePhases();
    }

private:
    void SetUp() override
    {
        m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    }

    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

TEST_F(LocalFrameTest, nodeImage)
{
    setBodyContent(
        "<style>"
        "#sample { width: 100px; height: 100px; }"
        "</style>"
        "<div id=sample></div>");
    Element* sample = document().getElementById("sample");
    const std::unique_ptr<DragImage> image = frame().nodeImage(*sample);
    EXPECT_EQ(IntSize(100, 100), image->size());
}

TEST_F(LocalFrameTest, nodeImageWithPsuedoClassWebKitDrag)
{
    setBodyContent(
        "<style>"
        "#sample { width: 100px; height: 100px; }"
        "#sample:-webkit-drag { width: 200px; height: 200px; }"
        "</style>"
        "<div id=sample></div>");
    Element* sample = document().getElementById("sample");
    const std::unique_ptr<DragImage> image = frame().nodeImage(*sample);
    EXPECT_EQ(IntSize(200, 200), image->size())
        << ":-webkit-drag should affect dragged image.";
}

TEST_F(LocalFrameTest, nodeImageWithoutDraggedLayoutObject)
{
    setBodyContent(
        "<style>"
        "#sample { width: 100px; height: 100px; }"
        "#sample:-webkit-drag { display:none }"
        "</style>"
        "<div id=sample></div>");
    Element* sample = document().getElementById("sample");
    const std::unique_ptr<DragImage> image = frame().nodeImage(*sample);
    EXPECT_EQ(nullptr, image.get())
        << ":-webkit-drag blows away layout object";
}

} // namespace blink

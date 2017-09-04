// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/LocalFrame.h"

#include "core/editing/FrameSelection.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/PerformanceMonitor.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLElement.h"
#include "core/layout/LayoutObject.h"
#include "core/testing/DummyPageHolder.h"
#include "core/timing/Performance.h"
#include "platform/DragImage.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LocalFrameTest : public ::testing::Test {
 protected:
  LocalFrameTest() = default;
  ~LocalFrameTest() override = default;

  Document& document() const { return m_dummyPageHolder->document(); }
  LocalFrame& frame() const { return *document().frame(); }
  Performance* performance() const { return m_performance; }

  void setBodyContent(const std::string& bodyContent) {
    document().body()->setInnerHTML(String::fromUTF8(bodyContent.c_str()));
    updateAllLifecyclePhases();
  }

  void updateAllLifecyclePhases() {
    document().view()->updateAllLifecyclePhases();
  }

 private:
  void SetUp() override {
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_performance = Performance::create(&frame());
  }

  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  Persistent<Performance> m_performance;
};

TEST_F(LocalFrameTest, nodeImage) {
  setBodyContent(
      "<style>"
      "#sample { width: 100px; height: 100px; }"
      "</style>"
      "<div id=sample></div>");
  Element* sample = document().getElementById("sample");
  const std::unique_ptr<DragImage> image = frame().nodeImage(*sample);
  EXPECT_EQ(IntSize(100, 100), image->size());
}

TEST_F(LocalFrameTest, nodeImageWithNestedElement) {
  setBodyContent(
      "<style>"
      "div { -webkit-user-drag: element }"
      "span:-webkit-drag { color: #0F0 }"
      "</style>"
      "<div id=sample><span>Green when dragged</span></div>");
  Element* sample = document().getElementById("sample");
  const std::unique_ptr<DragImage> image = frame().nodeImage(*sample);
  EXPECT_EQ(
      Color(0, 255, 0),
      sample->firstChild()->layoutObject()->resolveColor(CSSPropertyColor))
      << "Descendants node should have :-webkit-drag.";
}

TEST_F(LocalFrameTest, nodeImageWithPsuedoClassWebKitDrag) {
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

TEST_F(LocalFrameTest, nodeImageWithoutDraggedLayoutObject) {
  setBodyContent(
      "<style>"
      "#sample { width: 100px; height: 100px; }"
      "#sample:-webkit-drag { display:none }"
      "</style>"
      "<div id=sample></div>");
  Element* sample = document().getElementById("sample");
  const std::unique_ptr<DragImage> image = frame().nodeImage(*sample);
  EXPECT_EQ(nullptr, image.get()) << ":-webkit-drag blows away layout object";
}

TEST_F(LocalFrameTest, nodeImageWithChangingLayoutObject) {
  setBodyContent(
      "<style>"
      "#sample { color: blue; }"
      "#sample:-webkit-drag { display: inline-block; color: red; }"
      "</style>"
      "<span id=sample>foo</span>");
  Element* sample = document().getElementById("sample");
  updateAllLifecyclePhases();
  LayoutObject* beforeLayoutObject = sample->layoutObject();
  const std::unique_ptr<DragImage> image = frame().nodeImage(*sample);

  EXPECT_TRUE(sample->layoutObject() != beforeLayoutObject)
      << ":-webkit-drag causes sample to have different layout object.";
  EXPECT_EQ(Color(255, 0, 0),
            sample->layoutObject()->resolveColor(CSSPropertyColor))
      << "#sample has :-webkit-drag.";

  // Layout w/o :-webkit-drag
  updateAllLifecyclePhases();

  EXPECT_EQ(Color(0, 0, 255),
            sample->layoutObject()->resolveColor(CSSPropertyColor))
      << "#sample doesn't have :-webkit-drag.";
}

TEST_F(LocalFrameTest, dragImageForSelectionUsesPageScaleFactor) {
  setBodyContent(
      "<div>Hello world! This tests that the bitmap for drag image is scaled "
      "by page scale factor</div>");
  frame().host()->visualViewport().setScale(1);
  frame().selection().selectAll();
  updateAllLifecyclePhases();
  const std::unique_ptr<DragImage> image1(frame().dragImageForSelection(0.75f));
  frame().host()->visualViewport().setScale(2);
  frame().selection().selectAll();
  updateAllLifecyclePhases();
  const std::unique_ptr<DragImage> image2(frame().dragImageForSelection(0.75f));

  EXPECT_GT(image1->size().width(), 0);
  EXPECT_GT(image1->size().height(), 0);
  EXPECT_EQ(image1->size().width() * 2, image2->size().width());
  EXPECT_EQ(image1->size().height() * 2, image2->size().height());
}

}  // namespace blink

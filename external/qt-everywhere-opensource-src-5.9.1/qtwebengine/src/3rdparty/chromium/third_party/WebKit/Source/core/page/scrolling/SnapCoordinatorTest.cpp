// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SnapCoordinator.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/style/ComputedStyle.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include <gtest/gtest.h>
#include <memory>

namespace blink {

using HTMLNames::styleAttr;

typedef bool TestParamRootLayerScrolling;
class SnapCoordinatorTest
    : public testing::TestWithParam<TestParamRootLayerScrolling>,
      private ScopedRootLayerScrollingForTest {
 protected:
  SnapCoordinatorTest() : ScopedRootLayerScrollingForTest(GetParam()) {}

  void SetUp() override {
    m_pageHolder = DummyPageHolder::create();

    setHTML(
        "<style>"
        "    #snap-container {"
        "        height: 1000px;"
        "        width: 1000px;"
        "        overflow: scroll;"
        "        scroll-snap-type: mandatory;"
        "    }"
        "    #snap-element-fixed-position {"
        "         position: fixed;"
        "    }"
        "</style>"
        "<body>"
        "  <div id='snap-container'>"
        "    <div id='snap-element'></div>"
        "    <div id='intermediate'>"
        "       <div id='nested-snap-element'></div>"
        "    </div>"
        "    <div id='snap-element-fixed-position'></div>"
        "    <div style='width:2000px; height:2000px;'></div>"
        "  </div>"
        "</body>");
    document().updateStyleAndLayout();
  }

  void TearDown() override { m_pageHolder = nullptr; }

  Document& document() { return m_pageHolder->document(); }

  void setHTML(const char* htmlContent) {
    document().documentElement()->setInnerHTML(htmlContent);
  }

  Element& snapContainer() {
    return *document().getElementById("snap-container");
  }

  SnapCoordinator& coordinator() { return *document().snapCoordinator(); }

  Vector<double> snapOffsets(const ContainerNode& node,
                             ScrollbarOrientation orientation) {
    return coordinator().snapOffsets(node, orientation);
  }

  std::unique_ptr<DummyPageHolder> m_pageHolder;
};

INSTANTIATE_TEST_CASE_P(All, SnapCoordinatorTest, ::testing::Bool());

TEST_P(SnapCoordinatorTest, ValidRepeat) {
  snapContainer().setAttribute(styleAttr,
                               "scroll-snap-points-x: repeat(20%); "
                               "scroll-snap-points-y: repeat(400px);");
  document().updateStyleAndLayout();
  {
    const int expectedStepSize = snapContainer().clientWidth() * 0.2;
    Vector<double> actual = snapOffsets(snapContainer(), HorizontalScrollbar);
    EXPECT_EQ(5U, actual.size());
    for (size_t i = 0; i < actual.size(); ++i)
      EXPECT_EQ((i + 1) * expectedStepSize, actual[i]);
  }
  {
    Vector<double> actual = snapOffsets(snapContainer(), VerticalScrollbar);
    EXPECT_EQ(2U, actual.size());
    EXPECT_EQ(400, actual[0]);
    EXPECT_EQ(800, actual[1]);
  }
}

TEST_P(SnapCoordinatorTest, EmptyRepeat) {
  snapContainer().setAttribute(
      styleAttr, "scroll-snap-points-x: none; scroll-snap-points-y: none;");
  document().updateStyleAndLayout();

  EXPECT_EQ(0U, snapOffsets(snapContainer(), HorizontalScrollbar).size());
  EXPECT_EQ(0U, snapOffsets(snapContainer(), VerticalScrollbar).size());
}

TEST_P(SnapCoordinatorTest, ZeroAndNegativeRepeat) {
  // These be rejected as an invalid repeat values thus no snap offset is
  // created.
  snapContainer().setAttribute(
      styleAttr,
      "scroll-snap-points-x: repeat(-1px); scroll-snap-points-y: repeat(0px);");
  document().updateStyleAndLayout();

  EXPECT_EQ(0U, snapOffsets(snapContainer(), HorizontalScrollbar).size());
  EXPECT_EQ(0U, snapOffsets(snapContainer(), VerticalScrollbar).size());

  // Calc values are not be rejected outright but instead clamped to 1px min
  // repeat value.
  snapContainer().setAttribute(styleAttr,
                               "scroll-snap-points-x: repeat(calc(10px - "
                               "100%)); scroll-snap-points-y: "
                               "repeat(calc(0px));");
  document().updateStyleAndLayout();

  // A repeat value of 1px should give us |(scroll size - client size) / 1| snap
  // offsets.
  unsigned expectedHorizontalSnapOffsets =
      snapContainer().scrollWidth() - snapContainer().clientWidth();
  EXPECT_EQ(expectedHorizontalSnapOffsets,
            snapOffsets(snapContainer(), HorizontalScrollbar).size());
  unsigned expectedVerticalSnapOffsets =
      snapContainer().scrollHeight() - snapContainer().clientHeight();
  EXPECT_EQ(expectedVerticalSnapOffsets,
            snapOffsets(snapContainer(), VerticalScrollbar).size());
}

TEST_P(SnapCoordinatorTest, SimpleSnapElement) {
  Element& snapElement = *document().getElementById("snap-element");
  snapElement.setAttribute(styleAttr, "scroll-snap-coordinate: 10px 11px;");
  document().updateStyleAndLayout();

  EXPECT_EQ(10, snapOffsets(snapContainer(), HorizontalScrollbar)[0]);
  EXPECT_EQ(11, snapOffsets(snapContainer(), VerticalScrollbar)[0]);

  // Multiple coordinate and translates
  snapElement.setAttribute(styleAttr,
                           "scroll-snap-coordinate: 20px 21px, 40px 41px; "
                           "transform: translate(10px, 10px);");
  document().updateStyleAndLayout();

  Vector<double> result = snapOffsets(snapContainer(), HorizontalScrollbar);
  EXPECT_EQ(30, result[0]);
  EXPECT_EQ(50, result[1]);
  result = snapOffsets(snapContainer(), VerticalScrollbar);
  EXPECT_EQ(31, result[0]);
  EXPECT_EQ(51, result[1]);
}

TEST_P(SnapCoordinatorTest, NestedSnapElement) {
  Element& snapElement = *document().getElementById("nested-snap-element");
  snapElement.setAttribute(styleAttr, "scroll-snap-coordinate: 20px 25px;");
  document().updateStyleAndLayout();

  EXPECT_EQ(20, snapOffsets(snapContainer(), HorizontalScrollbar)[0]);
  EXPECT_EQ(25, snapOffsets(snapContainer(), VerticalScrollbar)[0]);
}

TEST_P(SnapCoordinatorTest, NestedSnapElementCaptured) {
  Element& snapElement = *document().getElementById("nested-snap-element");
  snapElement.setAttribute(styleAttr, "scroll-snap-coordinate: 20px 25px;");

  Element* intermediate = document().getElementById("intermediate");
  intermediate->setAttribute(styleAttr, "overflow: scroll;");

  document().updateStyleAndLayout();

  // Intermediate scroller captures nested snap elements first so ancestor
  // does not get them.
  EXPECT_EQ(0U, snapOffsets(snapContainer(), HorizontalScrollbar).size());
  EXPECT_EQ(0U, snapOffsets(snapContainer(), VerticalScrollbar).size());
}

TEST_P(SnapCoordinatorTest, PositionFixedSnapElement) {
  Element& snapElement =
      *document().getElementById("snap-element-fixed-position");
  snapElement.setAttribute(styleAttr, "scroll-snap-coordinate: 1px 1px;");
  document().updateStyleAndLayout();

  // Position fixed elements are contained in document and not its immediate
  // ancestor scroller. They cannot be a valid snap destination so they should
  // not contribute snap points to their immediate snap container or document
  // See: https://lists.w3.org/Archives/Public/www-style/2015Jun/0376.html
  EXPECT_EQ(0U, snapOffsets(snapContainer(), HorizontalScrollbar).size());
  EXPECT_EQ(0U, snapOffsets(snapContainer(), VerticalScrollbar).size());

  Element* body = document().viewportDefiningElement();
  EXPECT_EQ(0U, snapOffsets(*body, HorizontalScrollbar).size());
  EXPECT_EQ(0U, snapOffsets(*body, VerticalScrollbar).size());
}

TEST_P(SnapCoordinatorTest, RepeatAndSnapElementTogether) {
  document()
      .getElementById("snap-element")
      ->setAttribute(styleAttr, "scroll-snap-coordinate: 5px 10px;");
  document()
      .getElementById("nested-snap-element")
      ->setAttribute(styleAttr, "scroll-snap-coordinate: 250px 450px;");

  snapContainer().setAttribute(styleAttr,
                               "scroll-snap-points-x: repeat(200px); "
                               "scroll-snap-points-y: repeat(400px);");
  document().updateStyleAndLayout();

  {
    Vector<double> result = snapOffsets(snapContainer(), HorizontalScrollbar);
    EXPECT_EQ(7U, result.size());
    EXPECT_EQ(5, result[0]);
    EXPECT_EQ(200, result[1]);
    EXPECT_EQ(250, result[2]);
  }
  {
    Vector<double> result = snapOffsets(snapContainer(), VerticalScrollbar);
    EXPECT_EQ(4U, result.size());
    EXPECT_EQ(10, result[0]);
    EXPECT_EQ(400, result[1]);
    EXPECT_EQ(450, result[2]);
  }
}

TEST_P(SnapCoordinatorTest, SnapPointsAreScrollOffsetIndependent) {
  Element& snapElement = *document().getElementById("snap-element");
  snapElement.setAttribute(styleAttr, "scroll-snap-coordinate: 10px 11px;");
  snapContainer().scrollBy(100, 100);
  document().updateStyleAndLayout();

  EXPECT_EQ(snapContainer().scrollLeft(), 100);
  EXPECT_EQ(snapContainer().scrollTop(), 100);
  EXPECT_EQ(10, snapOffsets(snapContainer(), HorizontalScrollbar)[0]);
  EXPECT_EQ(11, snapOffsets(snapContainer(), VerticalScrollbar)[0]);
}

TEST_P(SnapCoordinatorTest, UpdateStyleForSnapElement) {
  Element& snapElement = *document().getElementById("snap-element");
  snapElement.setAttribute(styleAttr, "scroll-snap-coordinate: 10px 11px;");
  document().updateStyleAndLayout();

  EXPECT_EQ(10, snapOffsets(snapContainer(), HorizontalScrollbar)[0]);
  EXPECT_EQ(11, snapOffsets(snapContainer(), VerticalScrollbar)[0]);

  snapElement.remove();
  document().updateStyleAndLayout();

  EXPECT_EQ(0U, snapOffsets(snapContainer(), HorizontalScrollbar).size());
  EXPECT_EQ(0U, snapOffsets(snapContainer(), VerticalScrollbar).size());

  // Add a new snap element
  Element& container = *document().getElementById("snap-container");
  container.setInnerHTML(
      "<div style='scroll-snap-coordinate: 20px 22px;'><div "
      "style='width:2000px; height:2000px;'></div></div>");
  document().updateStyleAndLayout();

  EXPECT_EQ(20, snapOffsets(snapContainer(), HorizontalScrollbar)[0]);
  EXPECT_EQ(22, snapOffsets(snapContainer(), VerticalScrollbar)[0]);
}

TEST_P(SnapCoordinatorTest, LayoutViewCapturesWhenBodyElementViewportDefining) {
  setHTML(
      "<style>"
      "body {"
      "    overflow: scroll;"
      "    scroll-snap-type: mandatory;"
      "    height: 1000px;"
      "    width: 1000px;"
      "    margin: 5px;"
      "}"
      "</style>"
      "<body>"
      "    <div id='snap-element' style='scroll-snap-coordinate: 5px "
      "6px;'></div>"
      "    <div>"
      "       <div id='nested-snap-element' style='scroll-snap-coordinate: "
      "10px 11px;'></div>"
      "    </div>"
      "    <div style='width:2000px; height:2000px;'></div>"
      "</body>");

  document().updateStyleAndLayout();

  // Sanity check that body is the viewport defining element
  EXPECT_EQ(document().body(), document().viewportDefiningElement());

  // When body is viewport defining and overflows then any snap points on the
  // body element will be captured by layout view as the snap container.
  Vector<double> result = snapOffsets(document(), HorizontalScrollbar);
  EXPECT_EQ(10, result[0]);
  EXPECT_EQ(15, result[1]);
  result = snapOffsets(document(), VerticalScrollbar);
  EXPECT_EQ(11, result[0]);
  EXPECT_EQ(16, result[1]);
}

TEST_P(SnapCoordinatorTest,
       LayoutViewCapturesWhenDocumentElementViewportDefining) {
  setHTML(
      "<style>"
      ":root {"
      "    overflow: scroll;"
      "    scroll-snap-type: mandatory;"
      "    height: 500px;"
      "    width: 500px;"
      "}"
      "body {"
      "    margin: 5px;"
      "}"
      "</style>"
      "<html>"
      "   <body>"
      "       <div id='snap-element' style='scroll-snap-coordinate: 5px "
      "6px;'></div>"
      "       <div>"
      "         <div id='nested-snap-element' style='scroll-snap-coordinate: "
      "10px 11px;'></div>"
      "      </div>"
      "      <div style='width:2000px; height:2000px;'></div>"
      "   </body>"
      "</html>");

  document().updateStyleAndLayout();

  // Sanity check that document element is the viewport defining element
  EXPECT_EQ(document().documentElement(), document().viewportDefiningElement());

  // When body is viewport defining and overflows then any snap points on the
  // the document element will be captured by layout view as the snap
  // container.
  Vector<double> result = snapOffsets(document(), HorizontalScrollbar);
  EXPECT_EQ(10, result[0]);
  EXPECT_EQ(15, result[1]);
  result = snapOffsets(document(), VerticalScrollbar);
  EXPECT_EQ(11, result[0]);
  EXPECT_EQ(16, result[1]);
}

TEST_P(SnapCoordinatorTest,
       BodyCapturesWhenBodyOverflowAndDocumentElementViewportDefining) {
  setHTML(
      "<style>"
      ":root {"
      "    overflow: scroll;"
      "    scroll-snap-type: mandatory;"
      "    height: 500px;"
      "    width: 500px;"
      "}"
      "body {"
      "    overflow: scroll;"
      "    scroll-snap-type: mandatory;"
      "    height: 1000px;"
      "    width: 1000px;"
      "    margin: 5px;"
      "}"
      "</style>"
      "<html>"
      "   <body style='overflow: scroll; scroll-snap-type: mandatory; "
      "height:1000px; width:1000px;'>"
      "       <div id='snap-element' style='scroll-snap-coordinate: 5px "
      "6px;'></div>"
      "       <div>"
      "         <div id='nested-snap-element' style='scroll-snap-coordinate: "
      "10px 11px;'></div>"
      "      </div>"
      "      <div style='width:2000px; height:2000px;'></div>"
      "   </body>"
      "</html>");

  document().updateStyleAndLayout();

  // Sanity check that document element is the viewport defining element
  EXPECT_EQ(document().documentElement(), document().viewportDefiningElement());

  // When body and document elements are both scrollable then body element
  // should capture snap points defined on it as opposed to layout view.
  Element& body = *document().body();
  Vector<double> result = snapOffsets(body, HorizontalScrollbar);
  EXPECT_EQ(5, result[0]);
  EXPECT_EQ(10, result[1]);
  result = snapOffsets(body, VerticalScrollbar);
  EXPECT_EQ(6, result[0]);
  EXPECT_EQ(11, result[1]);
}

}  // namespace

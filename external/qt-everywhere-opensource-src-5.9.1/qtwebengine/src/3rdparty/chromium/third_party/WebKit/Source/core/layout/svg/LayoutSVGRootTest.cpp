// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/svg/LayoutSVGRoot.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/svg/LayoutSVGShape.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using LayoutSVGRootTest = RenderingTest;

TEST_F(LayoutSVGRootTest, VisualRectMappingWithoutViewportClipWithBorder) {
  setBodyInnerHTML(
      "<svg id='root' style='border: 10px solid red; width: 200px; height: "
      "100px; overflow: visible' viewBox='0 0 200 100'>"
      "   <rect id='rect' x='80' y='80' width='100' height='100'/>"
      "</svg>");

  const LayoutSVGRoot& root =
      *toLayoutSVGRoot(getLayoutObjectByElementId("root"));
  const LayoutSVGShape& svgRect =
      *toLayoutSVGShape(getLayoutObjectByElementId("rect"));

  LayoutRect rect = SVGLayoutSupport::visualRectInAncestorSpace(svgRect, root);
  // (80, 80, 100, 100) added by root's content rect offset from border rect,
  // not clipped.
  EXPECT_EQ(LayoutRect(90, 90, 100, 100), rect);

  LayoutRect rootVisualRect =
      static_cast<const LayoutObject&>(root).localVisualRect();
  // SVG root's overflow includes overflow from descendants.
  EXPECT_EQ(LayoutRect(0, 0, 220, 190), rootVisualRect);

  rect = rootVisualRect;
  EXPECT_TRUE(root.mapToVisualRectInAncestorSpace(&root, rect));
  EXPECT_EQ(LayoutRect(0, 0, 220, 190), rect);
}

TEST_F(LayoutSVGRootTest, VisualRectMappingWithViewportClipAndBorder) {
  setBodyInnerHTML(
      "<svg id='root' style='border: 10px solid red; width: 200px; height: "
      "100px; overflow: hidden' viewBox='0 0 200 100'>"
      "   <rect id='rect' x='80' y='80' width='100' height='100'/>"
      "</svg>");

  const LayoutSVGRoot& root =
      *toLayoutSVGRoot(getLayoutObjectByElementId("root"));
  const LayoutSVGShape& svgRect =
      *toLayoutSVGShape(getLayoutObjectByElementId("rect"));

  LayoutRect rect = SVGLayoutSupport::visualRectInAncestorSpace(svgRect, root);
  // (80, 80, 100, 100) added by root's content rect offset from border rect,
  // clipped by (10, 10, 200, 100).
  EXPECT_EQ(LayoutRect(90, 90, 100, 20), rect);

  LayoutRect rootVisualRect =
      static_cast<const LayoutObject&>(root).localVisualRect();
  // SVG root with overflow:hidden doesn't include overflow from children, just
  // border box rect.
  EXPECT_EQ(LayoutRect(0, 0, 220, 120), rootVisualRect);

  rect = rootVisualRect;
  EXPECT_TRUE(root.mapToVisualRectInAncestorSpace(&root, rect));
  // LayoutSVGRoot should not apply overflow clip on its own rect.
  EXPECT_EQ(LayoutRect(0, 0, 220, 120), rect);
}

TEST_F(LayoutSVGRootTest, VisualRectMappingWithViewportClipWithoutBorder) {
  setBodyInnerHTML(
      "<svg id='root' style='width: 200px; height: 100px; overflow: hidden' "
      "viewBox='0 0 200 100'>"
      "   <rect id='rect' x='80' y='80' width='100' height='100'/>"
      "</svg>");

  const LayoutSVGRoot& root =
      *toLayoutSVGRoot(getLayoutObjectByElementId("root"));
  const LayoutSVGShape& svgRect =
      *toLayoutSVGShape(getLayoutObjectByElementId("rect"));

  LayoutRect rect = SVGLayoutSupport::visualRectInAncestorSpace(svgRect, root);
  // (80, 80, 100, 100) clipped by (0, 0, 200, 100).
  EXPECT_EQ(LayoutRect(80, 80, 100, 20), rect);

  LayoutRect rootVisualRect =
      static_cast<const LayoutObject&>(root).localVisualRect();
  // SVG root doesn't have box decoration background, so just use clipped
  // overflow of children.
  EXPECT_EQ(LayoutRect(80, 80, 100, 20), rootVisualRect);

  rect = rootVisualRect;
  EXPECT_TRUE(root.mapToVisualRectInAncestorSpace(&root, rect));
  EXPECT_EQ(LayoutRect(80, 80, 100, 20), rect);
}

}  // namespace blink

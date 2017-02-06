// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "platform/geometry/TransformState.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MapCoordinatesTest : public RenderingTest {
public:
    MapCoordinatesTest()
        : RenderingTest(SingleChildFrameLoaderClient::create()) {}
    FloatPoint mapLocalToAncestor(const LayoutObject*, const LayoutBoxModelObject* ancestor, FloatPoint, MapCoordinatesFlags = 0) const;
    FloatQuad mapLocalToAncestor(const LayoutObject*, const LayoutBoxModelObject* ancestor, FloatQuad, MapCoordinatesFlags = 0) const;
    FloatPoint mapAncestorToLocal(const LayoutObject*, const LayoutBoxModelObject* ancestor, FloatPoint, MapCoordinatesFlags = 0) const;
    FloatQuad mapAncestorToLocal(const LayoutObject*, const LayoutBoxModelObject* ancestor, FloatQuad, MapCoordinatesFlags = 0) const;
};

// One note about tests here that operate on LayoutInline and LayoutText objects:
// mapLocalToAncestor() expects such objects to pass their static location and size (relatively to
// the border edge of their container) to mapLocalToAncestor() via the TransformState
// argument. mapLocalToAncestor() is then only expected to make adjustments for
// relative-positioning, container-specific characteristics (such as writing mode roots, multicol),
// and so on. This in contrast to LayoutBox objects, where the TransformState passed is relative to
// the box itself, not the container.

FloatPoint MapCoordinatesTest::mapLocalToAncestor(const LayoutObject* object, const LayoutBoxModelObject* ancestor, FloatPoint point, MapCoordinatesFlags mode) const
{
    TransformState transformState(TransformState::ApplyTransformDirection, point);
    object->mapLocalToAncestor(ancestor, transformState, mode);
    transformState.flatten();
    return transformState.lastPlanarPoint();
}

FloatQuad MapCoordinatesTest::mapLocalToAncestor(const LayoutObject* object, const LayoutBoxModelObject* ancestor, FloatQuad quad, MapCoordinatesFlags mode) const
{
    TransformState transformState(TransformState::ApplyTransformDirection, quad.boundingBox().center(), quad);
    object->mapLocalToAncestor(ancestor, transformState, mode);
    transformState.flatten();
    return transformState.lastPlanarQuad();
}

FloatPoint MapCoordinatesTest::mapAncestorToLocal(const LayoutObject* object, const LayoutBoxModelObject* ancestor, FloatPoint point, MapCoordinatesFlags mode) const
{
    TransformState transformState(TransformState::UnapplyInverseTransformDirection, point);
    object->mapAncestorToLocal(ancestor, transformState, mode);
    transformState.flatten();
    return transformState.lastPlanarPoint();
}

FloatQuad MapCoordinatesTest::mapAncestorToLocal(const LayoutObject* object, const LayoutBoxModelObject* ancestor, FloatQuad quad, MapCoordinatesFlags mode) const
{
    TransformState transformState(TransformState::UnapplyInverseTransformDirection, quad.boundingBox().center(), quad);
    object->mapAncestorToLocal(ancestor, transformState, mode);
    transformState.flatten();
    return transformState.lastPlanarQuad();
}

TEST_F(MapCoordinatesTest, SimpleText)
{
    setBodyInnerHTML("<div id='container'><br>text</div>");

    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));
    LayoutObject* text = toLayoutBlockFlow(container)->lastChild();
    ASSERT_TRUE(text->isText());
    FloatPoint mappedPoint = mapLocalToAncestor(text, container, FloatPoint(10, 30));
    EXPECT_EQ(FloatPoint(10, 30), mappedPoint);
    mappedPoint = mapAncestorToLocal(text, container, mappedPoint);
    EXPECT_EQ(FloatPoint(10, 30), mappedPoint);
}

TEST_F(MapCoordinatesTest, SimpleInline)
{
    setBodyInnerHTML("<div><span id='target'>text</span></div>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    FloatPoint mappedPoint = mapLocalToAncestor(target, toLayoutBoxModelObject(target->parent()), FloatPoint(10, 10));
    EXPECT_EQ(FloatPoint(10, 10), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, toLayoutBoxModelObject(target->parent()), mappedPoint);
    EXPECT_EQ(FloatPoint(10, 10), mappedPoint);
}

TEST_F(MapCoordinatesTest, SimpleBlock)
{
    setBodyInnerHTML(
        "<div style='margin:666px; border:8px solid; padding:7px;'>"
        "    <div id='target' style='margin:10px; border:666px; padding:666px;'></div>"
        "</div>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    FloatPoint mappedPoint = mapLocalToAncestor(target, toLayoutBoxModelObject(target->parent()), FloatPoint(100, 100));
    EXPECT_EQ(FloatPoint(125, 125), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, toLayoutBoxModelObject(target->parent()), mappedPoint);
    EXPECT_EQ(FloatPoint(100, 100), mappedPoint);
}

TEST_F(MapCoordinatesTest, OverflowClip)
{
    setBodyInnerHTML(
        "<div id='overflow' style='height: 100px; width: 100px; border:8px solid; padding:7px; overflow:scroll'>"
        "    <div style='height:200px; width:200px'></div>"
        "    <div id='target' style='margin:10px; border:666px; padding:666px;'></div>"
        "</div>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    LayoutObject* overflow = getLayoutObjectByElementId("overflow");
    toLayoutBox(overflow)->scrollToOffset(DoubleSize(32, 54));

    FloatPoint mappedPoint = mapLocalToAncestor(target, toLayoutBoxModelObject(target->parent()), FloatPoint(100, 100));
    EXPECT_EQ(FloatPoint(93, 271), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, toLayoutBoxModelObject(target->parent()), mappedPoint);
    EXPECT_EQ(FloatPoint(100, 100), mappedPoint);
}

TEST_F(MapCoordinatesTest, TextInRelPosInline)
{
    setBodyInnerHTML("<div><span style='position:relative; left:7px; top:4px;'><br id='sibling'>text</span></div>");

    LayoutObject* br = getLayoutObjectByElementId("sibling");
    LayoutObject* text = br->nextSibling();
    ASSERT_TRUE(text->isText());
    FloatPoint mappedPoint = mapLocalToAncestor(text, text->containingBlock(), FloatPoint(10, 30));
    EXPECT_EQ(FloatPoint(17, 34), mappedPoint);
    mappedPoint = mapAncestorToLocal(text, text->containingBlock(), mappedPoint);
    EXPECT_EQ(FloatPoint(10, 30), mappedPoint);
}

TEST_F(MapCoordinatesTest, RelposInline)
{
    setBodyInnerHTML("<span id='target' style='position:relative; left:50px; top:100px;'>text</span>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    FloatPoint mappedPoint = mapLocalToAncestor(target, toLayoutBoxModelObject(target->parent()), FloatPoint(10, 10));
    EXPECT_EQ(FloatPoint(60, 110), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, toLayoutBoxModelObject(target->parent()), mappedPoint);
    EXPECT_EQ(FloatPoint(10, 10), mappedPoint);
}

TEST_F(MapCoordinatesTest, RelposInlineInRelposInline)
{
    setBodyInnerHTML(
        "<div style='padding-left:10px;'>"
        "    <span style='position:relative; left:5px; top:6px;'>"
        "        <span id='target' style='position:relative; left:50px; top:100px;'>text</span>"
        "    </span>"
        "</div>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    LayoutInline* parent = toLayoutInline(target->parent());
    LayoutBlockFlow* containingBlock = toLayoutBlockFlow(parent->parent());

    FloatPoint mappedPoint = mapLocalToAncestor(target, containingBlock, FloatPoint(20, 10));
    EXPECT_EQ(FloatPoint(75, 116), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, containingBlock, mappedPoint);
    EXPECT_EQ(FloatPoint(20, 10), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    mappedPoint = mapLocalToAncestor(target, parent, FloatPoint(20, 10));
    EXPECT_EQ(FloatPoint(70, 110), mappedPoint);

    mappedPoint = mapLocalToAncestor(parent, containingBlock, mappedPoint);
    EXPECT_EQ(FloatPoint(75, 116), mappedPoint);

    mappedPoint = mapAncestorToLocal(parent, containingBlock, mappedPoint);
    EXPECT_EQ(FloatPoint(70, 110), mappedPoint);

    mappedPoint = mapAncestorToLocal(target, parent, mappedPoint);
    EXPECT_EQ(FloatPoint(20, 10), mappedPoint);
}

TEST_F(MapCoordinatesTest, RelPosBlock)
{
    setBodyInnerHTML(
        "<div id='container' style='margin:666px; border:8px solid; padding:7px;'>"
        "    <div id='middle' style='margin:30px; border:1px solid;'>"
        "        <div id='target' style='position:relative; left:50px; top:50px; margin:10px; border:666px; padding:666px;'></div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(106, 106), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, container, FloatPoint(110, 110));
    EXPECT_EQ(FloatPoint(4, 4), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* middle = toLayoutBox(getLayoutObjectByElementId("middle"));

    mappedPoint = mapLocalToAncestor(target, middle, FloatPoint());
    EXPECT_EQ(FloatPoint(61, 61), mappedPoint);

    mappedPoint = mapLocalToAncestor(middle, container, mappedPoint);
    EXPECT_EQ(FloatPoint(106, 106), mappedPoint);

    mappedPoint = mapAncestorToLocal(middle, container, mappedPoint);
    EXPECT_EQ(FloatPoint(61, 61), mappedPoint);

    mappedPoint = mapAncestorToLocal(target, middle, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);
}

TEST_F(MapCoordinatesTest, AbsPos)
{
    setBodyInnerHTML(
        "<div id='container' style='position:relative; margin:666px; border:8px solid; padding:7px;'>"
        "    <div id='staticChild' style='margin:30px; padding-top:666px;'>"
        "        <div style='padding-top:666px;'></div>"
        "        <div id='target' style='position:absolute; left:-1px; top:-1px; margin:10px; border:666px; padding:666px;'></div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(17, 17), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, container, FloatPoint(18, 18));
    EXPECT_EQ(FloatPoint(1, 1), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* staticChild = toLayoutBox(getLayoutObjectByElementId("staticChild"));

    mappedPoint = mapLocalToAncestor(target, staticChild, FloatPoint());
    EXPECT_EQ(FloatPoint(-28, -28), mappedPoint);

    mappedPoint = mapLocalToAncestor(staticChild, container, mappedPoint);
    EXPECT_EQ(FloatPoint(17, 17), mappedPoint);

    mappedPoint = mapAncestorToLocal(staticChild, container, mappedPoint);
    EXPECT_EQ(FloatPoint(-28, -28), mappedPoint);

    mappedPoint = mapAncestorToLocal(target, staticChild, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);
}

TEST_F(MapCoordinatesTest, AbsPosAuto)
{
    setBodyInnerHTML(
        "<div id='container' style='position:absolute; margin:666px; border:8px solid; padding:7px;'>"
        "    <div id='staticChild' style='margin:30px; padding-top:5px;'>"
        "        <div style='padding-top:20px;'></div>"
        "        <div id='target' style='position:absolute; margin:10px; border:666px; padding:666px;'></div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(55, 80), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, container, FloatPoint(56, 82));
    EXPECT_EQ(FloatPoint(1, 2), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* staticChild = toLayoutBox(getLayoutObjectByElementId("staticChild"));

    mappedPoint = mapLocalToAncestor(target, staticChild, FloatPoint());
    EXPECT_EQ(FloatPoint(10, 35), mappedPoint);

    mappedPoint = mapLocalToAncestor(staticChild, container, mappedPoint);
    EXPECT_EQ(FloatPoint(55, 80), mappedPoint);

    mappedPoint = mapAncestorToLocal(staticChild, container, mappedPoint);
    EXPECT_EQ(FloatPoint(10, 35), mappedPoint);

    mappedPoint = mapAncestorToLocal(target, staticChild, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);
}

TEST_F(MapCoordinatesTest, FixedPos)
{
    // Assuming BODY margin of 8px.
    setBodyInnerHTML(
        "<div id='container' style='position:absolute; margin:4px; border:5px solid; padding:7px;'>"
        "    <div id='staticChild' style='padding-top:666px;'>"
        "        <div style='padding-top:666px;'></div>"
        "        <div id='target' style='position:fixed; left:-1px; top:-1px; margin:10px; border:666px; padding:666px;'></div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* staticChild = toLayoutBox(getLayoutObjectByElementId("staticChild"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));
    LayoutBox* body = container->parentBox();
    LayoutBox* html = body->parentBox();
    LayoutBox* view = html->parentBox();
    ASSERT_TRUE(view->isLayoutView());

    FloatPoint mappedPoint = mapLocalToAncestor(target, view, FloatPoint());
    EXPECT_EQ(FloatPoint(9, 9), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, view, FloatPoint(10, 11));
    EXPECT_EQ(FloatPoint(1, 2), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    mappedPoint = mapLocalToAncestor(target, staticChild, FloatPoint());
    EXPECT_EQ(FloatPoint(-15, -15), mappedPoint);

    mappedPoint = mapLocalToAncestor(staticChild, container, mappedPoint);
    EXPECT_EQ(FloatPoint(-3, -3), mappedPoint);

    mappedPoint = mapLocalToAncestor(container, body, mappedPoint);
    EXPECT_EQ(FloatPoint(1, 1), mappedPoint);

    mappedPoint = mapLocalToAncestor(body, html, mappedPoint);
    EXPECT_EQ(FloatPoint(9, 9), mappedPoint);

    mappedPoint = mapLocalToAncestor(html, view, mappedPoint);
    EXPECT_EQ(FloatPoint(9, 9), mappedPoint);

    mappedPoint = mapAncestorToLocal(html, view, mappedPoint);
    EXPECT_EQ(FloatPoint(9, 9), mappedPoint);

    mappedPoint = mapAncestorToLocal(body, html, mappedPoint);
    EXPECT_EQ(FloatPoint(1, 1), mappedPoint);

    mappedPoint = mapAncestorToLocal(container, body, mappedPoint);
    EXPECT_EQ(FloatPoint(-3, -3), mappedPoint);

    mappedPoint = mapAncestorToLocal(staticChild, container, mappedPoint);
    EXPECT_EQ(FloatPoint(-15, -15), mappedPoint);

    mappedPoint = mapAncestorToLocal(target, staticChild, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);
}

TEST_F(MapCoordinatesTest, FixedPosAuto)
{
    // Assuming BODY margin of 8px.
    setBodyInnerHTML(
        "<div id='container' style='position:absolute; margin:3px; border:8px solid; padding:7px;'>"
        "    <div id='staticChild' style='padding-top:5px;'>"
        "        <div style='padding-top:20px;'></div>"
        "        <div id='target' style='position:fixed; margin:10px; border:666px; padding:666px;'></div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* staticChild = toLayoutBox(getLayoutObjectByElementId("staticChild"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));
    LayoutBox* body = container->parentBox();
    LayoutBox* html = body->parentBox();
    LayoutBox* view = html->parentBox();
    ASSERT_TRUE(view->isLayoutView());

    FloatPoint mappedPoint = mapLocalToAncestor(target, target->containingBlock(), FloatPoint());
    EXPECT_EQ(FloatPoint(36, 61), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, target->containingBlock(), FloatPoint(36, 61));
    EXPECT_EQ(FloatPoint(), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    mappedPoint = mapLocalToAncestor(target, staticChild, FloatPoint());
    EXPECT_EQ(FloatPoint(10, 35), mappedPoint);

    mappedPoint = mapLocalToAncestor(staticChild, container, mappedPoint);
    EXPECT_EQ(FloatPoint(25, 50), mappedPoint);

    mappedPoint = mapLocalToAncestor(container, body, mappedPoint);
    EXPECT_EQ(FloatPoint(28, 53), mappedPoint);

    mappedPoint = mapLocalToAncestor(body, html, mappedPoint);
    EXPECT_EQ(FloatPoint(36, 61), mappedPoint);

    mappedPoint = mapLocalToAncestor(html, view, mappedPoint);
    EXPECT_EQ(FloatPoint(36, 61), mappedPoint);

    mappedPoint = mapAncestorToLocal(html, view, mappedPoint);
    EXPECT_EQ(FloatPoint(36, 61), mappedPoint);

    mappedPoint = mapAncestorToLocal(body, html, mappedPoint);
    EXPECT_EQ(FloatPoint(28, 53), mappedPoint);

    mappedPoint = mapAncestorToLocal(container, body, mappedPoint);
    EXPECT_EQ(FloatPoint(25, 50), mappedPoint);

    mappedPoint = mapAncestorToLocal(staticChild, container, mappedPoint);
    EXPECT_EQ(FloatPoint(10, 35), mappedPoint);

    mappedPoint = mapAncestorToLocal(target, staticChild, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);
}

TEST_F(MapCoordinatesTest, FixedPosInFixedPos)
{
    // Assuming BODY margin of 8px.
    setBodyInnerHTML(
        "<div id='container' style='position:absolute; margin:4px; border:5px solid; padding:7px;'>"
        "    <div id='staticChild' style='padding-top:666px;'>"
        "        <div style='padding-top:666px;'></div>"
        "        <div id='outerFixed' style='position:fixed; left:100px; top:100px; margin:10px; border:666px; padding:666px;'>"
        "            <div id='target' style='position:fixed; left:-1px; top:-1px; margin:10px; border:666px; padding:666px;'></div>"
        "        </div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* outerFixed = toLayoutBox(getLayoutObjectByElementId("outerFixed"));
    LayoutBox* staticChild = toLayoutBox(getLayoutObjectByElementId("staticChild"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));
    LayoutBox* body = container->parentBox();
    LayoutBox* html = body->parentBox();
    LayoutBox* view = html->parentBox();
    ASSERT_TRUE(view->isLayoutView());

    FloatPoint mappedPoint = mapLocalToAncestor(target, view, FloatPoint());
    EXPECT_EQ(FloatPoint(9, 9), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, view, FloatPoint(9, 9));
    EXPECT_EQ(FloatPoint(), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    mappedPoint = mapLocalToAncestor(target, outerFixed, FloatPoint());
    EXPECT_EQ(FloatPoint(-101, -101), mappedPoint);

    mappedPoint = mapLocalToAncestor(outerFixed, staticChild, mappedPoint);
    EXPECT_EQ(FloatPoint(-15, -15), mappedPoint);

    mappedPoint = mapLocalToAncestor(staticChild, container, mappedPoint);
    EXPECT_EQ(FloatPoint(-3, -3), mappedPoint);

    mappedPoint = mapLocalToAncestor(container, body, mappedPoint);
    EXPECT_EQ(FloatPoint(1, 1), mappedPoint);

    mappedPoint = mapLocalToAncestor(body, html, mappedPoint);
    EXPECT_EQ(FloatPoint(9, 9), mappedPoint);

    mappedPoint = mapLocalToAncestor(html, view, mappedPoint);
    EXPECT_EQ(FloatPoint(9, 9), mappedPoint);

    mappedPoint = mapAncestorToLocal(html, view, mappedPoint);
    EXPECT_EQ(FloatPoint(9, 9), mappedPoint);

    mappedPoint = mapAncestorToLocal(body, html, mappedPoint);
    EXPECT_EQ(FloatPoint(1, 1), mappedPoint);

    mappedPoint = mapAncestorToLocal(container, body, mappedPoint);
    EXPECT_EQ(FloatPoint(-3, -3), mappedPoint);

    mappedPoint = mapAncestorToLocal(staticChild, container, mappedPoint);
    EXPECT_EQ(FloatPoint(-15, -15), mappedPoint);

    mappedPoint = mapAncestorToLocal(outerFixed, staticChild, mappedPoint);
    EXPECT_EQ(FloatPoint(-101, -101), mappedPoint);

    mappedPoint = mapAncestorToLocal(target, outerFixed, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);
}

TEST_F(MapCoordinatesTest, FixedPosInTransform)
{
    setBodyInnerHTML("<style>#container { transform: translateY(100px); position: absolute; left: 0; top: 100px; }"
        ".fixed { position: fixed; top: 0; }"
        ".spacer { height: 2000px; } </style>"
        "<div id='container'><div class='fixed' id='target'></div></div>"
        "<div class='spacer'></div>");

    document().view()->setScrollPosition(DoublePoint(0.0, 50), ProgrammaticScroll);
    document().view()->updateAllLifecyclePhases();
    EXPECT_EQ(50, document().view()->scrollPosition().y());

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));
    LayoutBox* body = container->parentBox();
    LayoutBox* html = body->parentBox();
    LayoutBox* view = html->parentBox();
    ASSERT_TRUE(view->isLayoutView());

    FloatPoint mappedPoint = mapLocalToAncestor(target, view, FloatPoint());
    EXPECT_EQ(FloatPoint(0, 100), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, view, FloatPoint(0, 100));
    EXPECT_EQ(FloatPoint(), mappedPoint);

    mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(0, 0), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, container, FloatPoint(0, 0));
    EXPECT_EQ(FloatPoint(), mappedPoint);

    mappedPoint = mapLocalToAncestor(container, view, FloatPoint());
    EXPECT_EQ(FloatPoint(0, 100), mappedPoint);
    mappedPoint = mapAncestorToLocal(container, view, FloatPoint(0, 100));
    EXPECT_EQ(FloatPoint(), mappedPoint);
}

TEST_F(MapCoordinatesTest, FixedPosInContainPaint)
{
    setBodyInnerHTML("<style>#container { contain: paint; position: absolute; left: 0; top: 100px; }"
        ".fixed { position: fixed; top: 0; }"
        ".spacer { height: 2000px; } </style>"
        "<div id='container'><div class='fixed' id='target'></div></div>"
        "<div class='spacer'></div>");

    document().view()->setScrollPosition(DoublePoint(0.0, 50), ProgrammaticScroll);
    document().view()->updateAllLifecyclePhases();
    EXPECT_EQ(50, document().view()->scrollPosition().y());

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));
    LayoutBox* body = container->parentBox();
    LayoutBox* html = body->parentBox();
    LayoutBox* view = html->parentBox();
    ASSERT_TRUE(view->isLayoutView());

    FloatPoint mappedPoint = mapLocalToAncestor(target, view, FloatPoint());
    EXPECT_EQ(FloatPoint(0, 100), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, view, FloatPoint(0, 100));
    EXPECT_EQ(FloatPoint(), mappedPoint);

    mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(0, 0), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, container, FloatPoint(0, 0));
    EXPECT_EQ(FloatPoint(), mappedPoint);

    mappedPoint = mapLocalToAncestor(container, view, FloatPoint());
    EXPECT_EQ(FloatPoint(0, 100), mappedPoint);
    mappedPoint = mapAncestorToLocal(container, view, FloatPoint(0, 100));
    EXPECT_EQ(FloatPoint(), mappedPoint);
}

// TODO(chrishtr): add more multi-frame tests.
TEST_F(MapCoordinatesTest, FixedPosInIFrameWhenMainFrameScrolled)
{
    document().setBaseURLOverride(KURL(ParsedURLString, "http://test.com"));
    setBodyInnerHTML(
        "<style>body { margin: 0; }</style>"
        "<div style='width: 200; height: 8000px'></div>"
        "<iframe id=frame src='http://test.com' width='500' height='500' frameBorder='0'>"
        "</iframe>");

    Document& frameDocument = setupChildIframe("frame", "<style>body { margin: 0; } #target { width: 200px; height: 200px; position:fixed}</style><div id=target></div>");

    document().view()->setScrollPosition(DoublePoint(0.0, 1000), ProgrammaticScroll);
    document().view()->updateAllLifecyclePhases();

    Element* target = frameDocument.getElementById("target");
    ASSERT_TRUE(target);
    FloatPoint mappedPoint = mapAncestorToLocal(target->layoutObject(), nullptr, FloatPoint(10, 70), TraverseDocumentBoundaries);

    // y = 70 - 8000, since the iframe is offset by 8000px from the main frame.
    // The scroll is not taken into account because the element is not fixed to the root LayoutView,
    // and the space of the root LayoutView does not include scroll.
    EXPECT_EQ(FloatPoint(10, -7930), mappedPoint);
}

TEST_F(MapCoordinatesTest, FixedPosInScrolledIFrameWithTransform)
{
    document().setBaseURLOverride(KURL(ParsedURLString, "http://test.com"));
    setBodyInnerHTML(
        "<style>* { margin: 0; }</style>"
        "<div style='position: absolute; left: 0px; top: 0px; width: 1024px; height: 768px; transform-origin: 0 0; transform: scale(0.5, 0.5);'>"
        "    <iframe id='frame' frameborder=0 src='http://test.com' class='frame' sandbox='allow-same-origin' width='1024' height='768'></iframe>"
        "</div>");

    Document& frameDocument = setupChildIframe("frame",
        "<style>* { margin: 0; } #target { width: 200px; height: 200px; position:fixed}</style><div id=target></div>"
        "<div style='width: 200; height: 8000px'></div>");

    document().view()->updateAllLifecyclePhases();
    frameDocument.view()->setScrollPosition(DoublePoint(0.0, 1000), ProgrammaticScroll);

    Element* target = frameDocument.getElementById("target");
    ASSERT_TRUE(target);
    FloatPoint mappedPoint = mapAncestorToLocal(target->layoutObject(), nullptr, FloatPoint(0, 0), UseTransforms | TraverseDocumentBoundaries);

    EXPECT_EQ(FloatPoint(0, 0), mappedPoint);
}

TEST_F(MapCoordinatesTest, MulticolWithText)
{
    setBodyInnerHTML(
        "<div id='multicol' style='columns:2; column-gap:20px; width:400px; line-height:50px; padding:5px; orphans:1; widows:1;'>"
        "    <br id='sibling'>"
        "    text"
        "</div>");

    LayoutObject* target = getLayoutObjectByElementId("sibling")->nextSibling();
    ASSERT_TRUE(target->isText());
    LayoutBox* flowThread = toLayoutBox(target->parent());
    ASSERT_TRUE(flowThread->isLayoutFlowThread());
    LayoutBox* multicol = toLayoutBox(getLayoutObjectByElementId("multicol"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, flowThread, FloatPoint(10, 70));
    EXPECT_EQ(FloatPoint(10, 70), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, flowThread, mappedPoint);
    EXPECT_EQ(FloatPoint(10, 70), mappedPoint);

    mappedPoint = mapLocalToAncestor(flowThread, multicol, FloatPoint(10, 70));
    EXPECT_EQ(FloatPoint(225, 25), mappedPoint);
    mappedPoint = mapAncestorToLocal(flowThread, multicol, mappedPoint);
    EXPECT_EQ(FloatPoint(10, 70), mappedPoint);
}

TEST_F(MapCoordinatesTest, MulticolWithInline)
{
    setBodyInnerHTML(
        "<div id='multicol' style='columns:2; column-gap:20px; width:400px; line-height:50px; padding:5px; orphans:1; widows:1;'>"
        "    <span id='target'><br>text</span>"
        "</div>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    LayoutBox* flowThread = toLayoutBox(target->parent());
    ASSERT_TRUE(flowThread->isLayoutFlowThread());
    LayoutBox* multicol = toLayoutBox(getLayoutObjectByElementId("multicol"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, flowThread, FloatPoint(10, 70));
    EXPECT_EQ(FloatPoint(10, 70), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, flowThread, mappedPoint);
    EXPECT_EQ(FloatPoint(10, 70), mappedPoint);

    mappedPoint = mapLocalToAncestor(flowThread, multicol, FloatPoint(10, 70));
    EXPECT_EQ(FloatPoint(225, 25), mappedPoint);
    mappedPoint = mapAncestorToLocal(flowThread, multicol, mappedPoint);
    EXPECT_EQ(FloatPoint(10, 70), mappedPoint);
}

TEST_F(MapCoordinatesTest, MulticolWithBlock)
{
    setBodyInnerHTML(
        "<div id='container' style='-webkit-columns:3; -webkit-column-gap:0; column-fill:auto; width:300px; height:100px; border:8px solid; padding:7px;'>"
        "    <div style='height:110px;'></div>"
        "    <div id='target' style='margin:10px; border:13px; padding:13px;'></div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(125, 35), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, container, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* flowThread = target->parentBox();
    ASSERT_TRUE(flowThread->isLayoutFlowThread());

    mappedPoint = mapLocalToAncestor(target, flowThread, FloatPoint());
    EXPECT_EQ(FloatPoint(10, 120), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, flowThread, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);

    mappedPoint = mapLocalToAncestor(flowThread, container, FloatPoint(10, 120));
    EXPECT_EQ(FloatPoint(125, 35), mappedPoint);
    mappedPoint = mapAncestorToLocal(flowThread, container, mappedPoint);
    EXPECT_EQ(FloatPoint(10, 120), mappedPoint);
}

TEST_F(MapCoordinatesTest, NestedMulticolWithBlock)
{
    setBodyInnerHTML(
        "<div id='outerMulticol' style='columns:2; column-gap:0; column-fill:auto; width:560px; height:215px; border:8px solid; padding:7px;'>"
        "    <div style='height:10px;'></div>"
        "    <div id='innerMulticol' style='columns:2; column-gap:0; border:8px solid; padding:7px;'>"
        "        <div style='height:630px;'></div>"
        "        <div id='target' style='width:50px; height:50px;'></div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* outerMulticol = toLayoutBox(getLayoutObjectByElementId("outerMulticol"));
    LayoutBox* innerMulticol = toLayoutBox(getLayoutObjectByElementId("innerMulticol"));
    LayoutBox* innerFlowThread = target->parentBox();
    ASSERT_TRUE(innerFlowThread->isLayoutFlowThread());
    LayoutBox* outerFlowThread = innerMulticol->parentBox();
    ASSERT_TRUE(outerFlowThread->isLayoutFlowThread());

    FloatPoint mappedPoint = mapLocalToAncestor(target, outerMulticol, FloatPoint());
    EXPECT_EQ(FloatPoint(435, 115), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, outerMulticol, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    mappedPoint = mapLocalToAncestor(target, innerFlowThread, FloatPoint());
    EXPECT_EQ(FloatPoint(0, 630), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, innerFlowThread, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);

    mappedPoint = mapLocalToAncestor(innerFlowThread, innerMulticol, FloatPoint(0, 630));
    EXPECT_EQ(FloatPoint(140, 305), mappedPoint);
    mappedPoint = mapAncestorToLocal(innerFlowThread, innerMulticol, mappedPoint);
    EXPECT_EQ(FloatPoint(0, 630), mappedPoint);

    mappedPoint = mapLocalToAncestor(innerMulticol, outerFlowThread, FloatPoint(140, 305));
    EXPECT_EQ(FloatPoint(140, 315), mappedPoint);
    mappedPoint = mapAncestorToLocal(innerMulticol, outerFlowThread, mappedPoint);
    EXPECT_EQ(FloatPoint(140, 305), mappedPoint);

    mappedPoint = mapLocalToAncestor(outerFlowThread, outerMulticol, FloatPoint(140, 315));
    EXPECT_EQ(FloatPoint(435, 115), mappedPoint);
    mappedPoint = mapAncestorToLocal(outerFlowThread, outerMulticol, mappedPoint);
    EXPECT_EQ(FloatPoint(140, 315), mappedPoint);
}

TEST_F(MapCoordinatesTest, MulticolWithAbsPosInRelPos)
{
    setBodyInnerHTML(
        "<div id='multicol' style='-webkit-columns:3; -webkit-column-gap:0; column-fill:auto; width:300px; height:100px; border:8px solid; padding:7px;'>"
        "    <div style='height:110px;'></div>"
        "    <div id='relpos' style='position:relative; left:4px; top:4px;'>"
        "        <div id='target' style='position:absolute; left:15px; top:15px; margin:10px; border:13px; padding:13px;'></div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* multicol = toLayoutBox(getLayoutObjectByElementId("multicol"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, multicol, FloatPoint());
    EXPECT_EQ(FloatPoint(144, 54), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, multicol, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* relpos = toLayoutBox(getLayoutObjectByElementId("relpos"));
    LayoutBox* flowThread = relpos->parentBox();
    ASSERT_TRUE(flowThread->isLayoutFlowThread());

    mappedPoint = mapLocalToAncestor(target, relpos, FloatPoint());
    EXPECT_EQ(FloatPoint(25, 25), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, relpos, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);

    mappedPoint = mapLocalToAncestor(relpos, flowThread, FloatPoint(25, 25));
    EXPECT_EQ(FloatPoint(29, 139), mappedPoint);
    mappedPoint = mapAncestorToLocal(relpos, flowThread, mappedPoint);
    EXPECT_EQ(FloatPoint(25, 25), mappedPoint);

    mappedPoint = mapLocalToAncestor(flowThread, multicol, FloatPoint(29, 139));
    EXPECT_EQ(FloatPoint(144, 54), mappedPoint);
    mappedPoint = mapAncestorToLocal(flowThread, multicol, mappedPoint);
    EXPECT_EQ(FloatPoint(29, 139), mappedPoint);
}

TEST_F(MapCoordinatesTest, MulticolWithAbsPosNotContained)
{
    setBodyInnerHTML(
        "<div id='container' style='position:relative; margin:666px; border:7px solid; padding:3px;'>"
        "    <div id='multicol' style='-webkit-columns:3; -webkit-column-gap:0; column-fill:auto; width:300px; height:100px; border:8px solid; padding:7px;'>"
        "        <div style='height:110px;'></div>"
        "        <div id='target' style='position:absolute; left:-1px; top:-1px; margin:10px; border:13px; padding:13px;'></div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    // The multicol container isn't in the containing block chain of the abspos #target.
    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(16, 16), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, container, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* multicol = toLayoutBox(getLayoutObjectByElementId("multicol"));
    LayoutBox* flowThread = target->parentBox();
    ASSERT_TRUE(flowThread->isLayoutFlowThread());

    mappedPoint = mapLocalToAncestor(target, flowThread, FloatPoint());
    EXPECT_EQ(FloatPoint(-9, -9), mappedPoint);

    mappedPoint = mapLocalToAncestor(flowThread, multicol, mappedPoint);
    EXPECT_EQ(FloatPoint(6, 6), mappedPoint);

    mappedPoint = mapLocalToAncestor(multicol, container, mappedPoint);
    EXPECT_EQ(FloatPoint(16, 16), mappedPoint);

    mappedPoint = mapAncestorToLocal(multicol, container, mappedPoint);
    EXPECT_EQ(FloatPoint(6, 6), mappedPoint);

    mappedPoint = mapAncestorToLocal(flowThread, multicol, mappedPoint);
    EXPECT_EQ(FloatPoint(-9, -9), mappedPoint);

    mappedPoint = mapAncestorToLocal(target, flowThread, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);
}

TEST_F(MapCoordinatesTest, FlippedBlocksWritingModeWithText)
{
    setBodyInnerHTML(
        "<div style='-webkit-writing-mode:vertical-rl;'>"
        "    <div style='width:13px;'></div>"
        "    <div style='width:200px; height:400px; line-height:50px;'>"
        "        <br id='sibling'>text"
        "    </div>"
        "    <div style='width:5px;'></div>"
        "</div>");

    LayoutObject* br = getLayoutObjectByElementId("sibling");
    LayoutObject* text = br->nextSibling();
    ASSERT_TRUE(text->isText());

    // Map to the nearest container. Flipping should occur.
    FloatPoint mappedPoint = mapLocalToAncestor(text, text->containingBlock(), FloatPoint(75, 10), ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(125, 10), mappedPoint);
    mappedPoint = mapAncestorToLocal(text, text->containingBlock(), mappedPoint, ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(75, 10), mappedPoint);

    // Map to a container further up in the tree. Flipping should still occur on the nearest
    // container. LayoutObject::mapLocalToAncestor() is called recursively until the ancestor is
    // reached, and the ApplyContainerFlip flag is cleared after having processed the innermost
    // object.
    mappedPoint = mapLocalToAncestor(text, text->containingBlock()->containingBlock(), FloatPoint(75, 10), ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(130, 10), mappedPoint);
    mappedPoint = mapAncestorToLocal(text, text->containingBlock()->containingBlock(), mappedPoint, ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(75, 10), mappedPoint);

    // If the ApplyContainerFlip flag isn't specified, no flipping should take place.
    mappedPoint = mapLocalToAncestor(text, text->containingBlock()->containingBlock(), FloatPoint(75, 10));
    EXPECT_EQ(FloatPoint(80, 10), mappedPoint);
    mappedPoint = mapAncestorToLocal(text, text->containingBlock()->containingBlock(), mappedPoint);
    EXPECT_EQ(FloatPoint(75, 10), mappedPoint);
}

TEST_F(MapCoordinatesTest, FlippedBlocksWritingModeWithInline)
{
    setBodyInnerHTML(
        "<div style='-webkit-writing-mode:vertical-rl;'>"
        "    <div style='width:13px;'></div>"
        "    <div style='width:200px; height:400px; line-height:50px;'>"
        "        <span>"
        "            <span id='target'><br>text</span>"
        "        </span>"
        "    </div>"
        "    <div style='width:7px;'></div>"
        "</div>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    ASSERT_TRUE(target);

    // First map to the parent SPAN. Nothing special should happen, since flipping occurs at the nearest container.
    FloatPoint mappedPoint = mapLocalToAncestor(target, toLayoutBoxModelObject(target->parent()), FloatPoint(75, 10), ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(75, 10), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, toLayoutBoxModelObject(target->parent()), mappedPoint, ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(75, 10), mappedPoint);

    // Continue to the nearest container. Flipping should occur.
    mappedPoint = mapLocalToAncestor(toLayoutBoxModelObject(target->parent()), target->containingBlock(), FloatPoint(75, 10), ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(125, 10), mappedPoint);
    mappedPoint = mapAncestorToLocal(toLayoutBoxModelObject(target->parent()), target->containingBlock(), mappedPoint, ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(75, 10), mappedPoint);

    // Now map from the innermost inline to the nearest container in one go.
    mappedPoint = mapLocalToAncestor(target, target->containingBlock(), FloatPoint(75, 10), ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(125, 10), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, target->containingBlock(), mappedPoint, ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(75, 10), mappedPoint);

    // Map to a container further up in the tree. Flipping should still only occur on the nearest container.
    mappedPoint = mapLocalToAncestor(target, target->containingBlock()->containingBlock(), FloatPoint(75, 10), ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(132, 10), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, target->containingBlock()->containingBlock(), mappedPoint, ApplyContainerFlip);
    EXPECT_EQ(FloatPoint(75, 10), mappedPoint);

    // If the ApplyContainerFlip flag isn't specified, no flipping should take place.
    mappedPoint = mapLocalToAncestor(target, target->containingBlock()->containingBlock(), FloatPoint(75, 10));
    EXPECT_EQ(FloatPoint(82, 10), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, target->containingBlock()->containingBlock(), mappedPoint);
    EXPECT_EQ(FloatPoint(75, 10), mappedPoint);
}

TEST_F(MapCoordinatesTest, FlippedBlocksWritingModeWithBlock)
{
    setBodyInnerHTML(
        "<div id='container' style='-webkit-writing-mode:vertical-rl; border:8px solid; padding:7px; width:200px; height:200px;'>"
        "    <div id='middle' style='border:1px solid;'>"
        "        <div style='width:30px;'></div>"
        "        <div id='target' style='margin:6px; width:25px;'></div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(153, 22), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, container, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* middle = toLayoutBox(getLayoutObjectByElementId("middle"));

    mappedPoint = mapLocalToAncestor(target, middle, FloatPoint());
    EXPECT_EQ(FloatPoint(7, 7), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, middle, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);

    mappedPoint = mapLocalToAncestor(middle, container, FloatPoint(7, 7));
    EXPECT_EQ(FloatPoint(153, 22), mappedPoint);
    mappedPoint = mapAncestorToLocal(middle, container, mappedPoint);
    EXPECT_EQ(FloatPoint(7, 7), mappedPoint);
}

TEST_F(MapCoordinatesTest, Table)
{
    setBodyInnerHTML(
        "<style>td { padding: 2px; }</style>"
        "<div id='container' style='border:3px solid;'>"
        "    <table style='margin:9px; border:5px solid; border-spacing:10px;'>"
        "        <thead>"
        "            <tr>"
        "                <td>"
        "                    <div style='width:100px; height:100px;'></div>"
        "                </td>"
        "            </tr>"
        "        </thead>"
        "        <tbody>"
        "            <tr>"
        "                <td>"
        "                    <div style='width:100px; height:100px;'></div>"
        "                 </td>"
        "            </tr>"
        "            <tr>"
        "                <td>"
        "                     <div style='width:100px; height:100px;'></div>"
        "                </td>"
        "                <td>"
        "                    <div id='target' style='width:100px; height:10px;'></div>"
        "                </td>"
        "            </tr>"
        "        </tbody>"
        "    </table>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(143, 302), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, container, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* td = target->parentBox();
    ASSERT_TRUE(td->isTableCell());
    mappedPoint = mapLocalToAncestor(target, td, FloatPoint());
    // Cells are middle-aligned by default.
    EXPECT_EQ(FloatPoint(2, 47), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, td, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);

    LayoutBox* tr = td->parentBox();
    ASSERT_TRUE(tr->isTableRow());
    mappedPoint = mapLocalToAncestor(td, tr, FloatPoint(2, 47));
    EXPECT_EQ(FloatPoint(126, 47), mappedPoint);
    mappedPoint = mapAncestorToLocal(td, tr, mappedPoint);
    EXPECT_EQ(FloatPoint(2, 47), mappedPoint);

    LayoutBox* tbody = tr->parentBox();
    ASSERT_TRUE(tbody->isTableSection());
    mappedPoint = mapLocalToAncestor(tr, tbody, FloatPoint(126, 47));
    EXPECT_EQ(FloatPoint(126, 161), mappedPoint);
    mappedPoint = mapAncestorToLocal(tr, tbody, mappedPoint);
    EXPECT_EQ(FloatPoint(126, 47), mappedPoint);

    LayoutBox* table = tbody->parentBox();
    ASSERT_TRUE(table->isTable());
    mappedPoint = mapLocalToAncestor(tbody, table, FloatPoint(126, 161));
    EXPECT_EQ(FloatPoint(131, 290), mappedPoint);
    mappedPoint = mapAncestorToLocal(tbody, table, mappedPoint);
    EXPECT_EQ(FloatPoint(126, 161), mappedPoint);

    mappedPoint = mapLocalToAncestor(table, container, FloatPoint(131, 290));
    EXPECT_EQ(FloatPoint(143, 302), mappedPoint);
    mappedPoint = mapAncestorToLocal(table, container, mappedPoint);
    EXPECT_EQ(FloatPoint(131, 290), mappedPoint);
}

static bool floatValuesAlmostEqual(float expected, float actual)
{
    return fabs(expected - actual) < 0.01;
}

static bool floatQuadsAlmostEqual(const FloatQuad& expected, const FloatQuad& actual)
{
    return floatValuesAlmostEqual(expected.p1().x(), actual.p1().x())
        && floatValuesAlmostEqual(expected.p1().y(), actual.p1().y())
        && floatValuesAlmostEqual(expected.p2().x(), actual.p2().x())
        && floatValuesAlmostEqual(expected.p2().y(), actual.p2().y())
        && floatValuesAlmostEqual(expected.p3().x(), actual.p3().x())
        && floatValuesAlmostEqual(expected.p3().y(), actual.p3().y())
        && floatValuesAlmostEqual(expected.p4().x(), actual.p4().x())
        && floatValuesAlmostEqual(expected.p4().y(), actual.p4().y());
}

// If comparison fails, pretty-print the error using EXPECT_EQ()
#define EXPECT_FLOAT_QUAD_EQ(expected, actual) \
    do { \
        if (!floatQuadsAlmostEqual(expected, actual)) { \
            EXPECT_EQ(expected, actual); \
        } \
    } while (false)


TEST_F(MapCoordinatesTest, Transforms)
{
    setBodyInnerHTML(
        "<div id='container'>"
        "    <div id='outerTransform' style='transform:rotate(45deg); width:200px; height:200px;'>"
        "        <div id='innerTransform' style='transform:rotate(45deg); width:200px; height:200px;'>"
        "            <div id='target' style='width:200px; height:200px;'></div>"
        "        </div>"
        "    </div>"
        "</div>");

    LayoutBox* target = toLayoutBox(getLayoutObjectByElementId("target"));
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatQuad initialQuad(FloatPoint(0, 0), FloatPoint(200, 0), FloatPoint(200, 200), FloatPoint(0, 200));
    FloatQuad mappedQuad = mapLocalToAncestor(target, container, initialQuad, UseTransforms);
    EXPECT_FLOAT_QUAD_EQ(FloatQuad(FloatPoint(200, 0), FloatPoint(200, 200), FloatPoint(0, 200), FloatPoint(0, 0)), mappedQuad);
    mappedQuad = mapAncestorToLocal(target, container, mappedQuad, UseTransforms);
    EXPECT_FLOAT_QUAD_EQ(initialQuad, mappedQuad);

    // Walk each ancestor in the chain separately, to verify each step on the way.
    LayoutBox* innerTransform = toLayoutBox(getLayoutObjectByElementId("innerTransform"));
    LayoutBox* outerTransform = toLayoutBox(getLayoutObjectByElementId("outerTransform"));

    mappedQuad = mapLocalToAncestor(target, innerTransform, initialQuad, UseTransforms);
    EXPECT_FLOAT_QUAD_EQ(FloatQuad(FloatPoint(0, 0), FloatPoint(200, 0), FloatPoint(200, 200), FloatPoint(0, 200)), mappedQuad);
    mappedQuad = mapAncestorToLocal(target, innerTransform, mappedQuad, UseTransforms);
    EXPECT_FLOAT_QUAD_EQ(initialQuad, mappedQuad);

    initialQuad = FloatQuad(FloatPoint(0, 0), FloatPoint(200, 0), FloatPoint(200, 200), FloatPoint(0, 200));
    mappedQuad = mapLocalToAncestor(innerTransform, outerTransform, initialQuad, UseTransforms);
    // Clockwise rotation by 45 degrees.
    EXPECT_FLOAT_QUAD_EQ(FloatQuad(FloatPoint(100, -41.42), FloatPoint(241.42, 100), FloatPoint(100, 241.42), FloatPoint(-41.42, 100)), mappedQuad);
    mappedQuad = mapAncestorToLocal(innerTransform, outerTransform, mappedQuad, UseTransforms);
    EXPECT_FLOAT_QUAD_EQ(initialQuad, mappedQuad);

    initialQuad = FloatQuad(FloatPoint(100, -41.42), FloatPoint(241.42, 100), FloatPoint(100, 241.42), FloatPoint(-41.42, 100));
    mappedQuad = mapLocalToAncestor(outerTransform, container, initialQuad, UseTransforms);
    // Another clockwise rotation by 45 degrees. So now 90 degrees in total.
    EXPECT_FLOAT_QUAD_EQ(FloatQuad(FloatPoint(200, 0), FloatPoint(200, 200), FloatPoint(0, 200), FloatPoint(0, 0)), mappedQuad);
    mappedQuad = mapAncestorToLocal(outerTransform, container, mappedQuad, UseTransforms);
    EXPECT_FLOAT_QUAD_EQ(initialQuad, mappedQuad);
}

TEST_F(MapCoordinatesTest, SVGShape)
{
    setBodyInnerHTML(
        "<svg id='container'>"
        "    <g transform='translate(100 200)'>"
        "        <rect id='target' width='100' height='100'/>"
        "    </g>"
        "</svg>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(100, 200), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, container, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);
}

TEST_F(MapCoordinatesTest, SVGShapeScale)
{
    setBodyInnerHTML(
        "<svg id='container'>"
        "    <g transform='scale(2) translate(50 40)'>"
        "        <rect id='target' transform='translate(50 80)' x='66' y='77' width='100' height='100'/>"
        "    </g>"
        "</svg>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(200, 240), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, container, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);
}

TEST_F(MapCoordinatesTest, SVGShapeWithViewBoxWithoutScale)
{
    setBodyInnerHTML(
        "<svg id='container' viewBox='0 0 200 200' width='400' height='200'>"
        "    <g transform='translate(100 50)'>"
        "        <rect id='target' width='100' height='100'/>"
        "    </g>"
        "</svg>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(200, 50), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, container, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);
}

TEST_F(MapCoordinatesTest, SVGShapeWithViewBoxWithScale)
{
    setBodyInnerHTML(
        "<svg id='container' viewBox='0 0 100 100' width='400' height='200'>"
        "    <g transform='translate(50 50)'>"
        "        <rect id='target' width='100' height='100'/>"
        "    </g>"
        "</svg>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(200, 100), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, container, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);
}

TEST_F(MapCoordinatesTest, SVGShapeWithViewBoxWithNonZeroOffset)
{
    setBodyInnerHTML(
        "<svg id='container' viewBox='100 100 200 200' width='400' height='200'>"
        "    <g transform='translate(100 50)'>"
        "        <rect id='target' transform='translate(100 100)' width='100' height='100'/>"
        "    </g>"
        "</svg>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(200, 50), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, container, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);
}

TEST_F(MapCoordinatesTest, SVGShapeWithViewBoxWithNonZeroOffsetAndScale)
{
    setBodyInnerHTML(
        "<svg id='container' viewBox='100 100 100 100' width='400' height='200'>"
        "    <g transform='translate(50 50)'>"
        "        <rect id='target' transform='translate(100 100)' width='100' height='100'/>"
        "    </g>"
        "</svg>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(200, 100), mappedPoint);
    mappedPoint = mapAncestorToLocal(target, container, mappedPoint);
    EXPECT_EQ(FloatPoint(), mappedPoint);
}

TEST_F(MapCoordinatesTest, SVGForeignObject)
{
    setBodyInnerHTML(
        "<svg id='container' viewBox='0 0 100 100' width='400' height='200'>"
        "    <g transform='translate(50 50)'>"
        "        <foreignObject transform='translate(-25 -25)'>"
        "            <div xmlns='http://www.w3.org/1999/xhtml' id='target' style='margin-left: 50px; border: 42px; padding: 84px; width: 50px; height: 50px'>"
        "            </div>"
        "        </foreignObject>"
        "    </g>"
        "</svg>");

    LayoutObject* target = getLayoutObjectByElementId("target");
    LayoutBox* container = toLayoutBox(getLayoutObjectByElementId("container"));

    FloatPoint mappedPoint = mapLocalToAncestor(target, container, FloatPoint());
    EXPECT_EQ(FloatPoint(250, 50), mappedPoint);
    // <svg>
    mappedPoint = mapAncestorToLocal(target->parent()->parent()->parent(), container, FloatPoint(250, 50));
    EXPECT_EQ(FloatPoint(250, 50), mappedPoint);
    // <g>
    mappedPoint = mapAncestorToLocal(target->parent()->parent(), container, FloatPoint(250, 50));
    EXPECT_EQ(FloatPoint(25, -25), mappedPoint);
    // <foreignObject>
    mappedPoint = mapAncestorToLocal(target->parent(), container, FloatPoint(250, 50));
    EXPECT_EQ(FloatPoint(50, 0), mappedPoint);
    // <div>
    mappedPoint = mapAncestorToLocal(target, container, FloatPoint(250, 50));
    EXPECT_EQ(FloatPoint(), mappedPoint);
}

TEST_F(MapCoordinatesTest, LocalToAbsoluteTransform)
{
    setBodyInnerHTML(
        "<div id='container' style='position: absolute; left: 0; top: 0;'>"
        "  <div id='scale' style='transform: scale(2.0); transform-origin: left top;'>"
        "    <div id='child'></div>"
        "  </div>"
        "</div>");
    LayoutBoxModelObject* container = toLayoutBoxModelObject(getLayoutObjectByElementId("container"));
    TransformationMatrix containerMatrix = container->localToAbsoluteTransform();
    EXPECT_TRUE(containerMatrix.isIdentity());

    LayoutObject* child = getLayoutObjectByElementId("child");
    TransformationMatrix childMatrix = child->localToAbsoluteTransform();
    EXPECT_FALSE(childMatrix.isIdentityOrTranslation());
    EXPECT_TRUE(childMatrix.isAffine());
    EXPECT_EQ(0.0, childMatrix.projectPoint(FloatPoint(0.0, 0.0)).x());
    EXPECT_EQ(0.0, childMatrix.projectPoint(FloatPoint(0.0, 0.0)).y());
    EXPECT_EQ(20.0, childMatrix.projectPoint(FloatPoint(10.0, 20.0)).x());
    EXPECT_EQ(40.0, childMatrix.projectPoint(FloatPoint(10.0, 20.0)).y());
}

TEST_F(MapCoordinatesTest, LocalToAncestorTransform)
{
    setBodyInnerHTML(
        "<div id='container'>"
        "  <div id='rotate1' style='transform: rotate(45deg); transform-origin: left top;'>"
        "    <div id='rotate2' style='transform: rotate(90deg); transform-origin: left top;'>"
        "      <div id='child'></div>"
        "    </div>"
        "  </div>"
        "</div>");
    LayoutBoxModelObject* container = toLayoutBoxModelObject(getLayoutObjectByElementId("container"));
    LayoutBoxModelObject* rotate1 = toLayoutBoxModelObject(getLayoutObjectByElementId("rotate1"));
    LayoutBoxModelObject* rotate2 = toLayoutBoxModelObject(getLayoutObjectByElementId("rotate2"));
    LayoutObject* child = getLayoutObjectByElementId("child");
    TransformationMatrix matrix;

    matrix = child->localToAncestorTransform(rotate2);
    EXPECT_TRUE(matrix.isIdentity());

    // Rotate (100, 0) 90 degrees to (0, 100)
    matrix = child->localToAncestorTransform(rotate1);
    EXPECT_FALSE(matrix.isIdentity());
    EXPECT_TRUE(matrix.isAffine());
    EXPECT_NEAR(0.0, matrix.projectPoint(FloatPoint(100.0, 0.0)).x(), LayoutUnit::epsilon());
    EXPECT_NEAR(100.0, matrix.projectPoint(FloatPoint(100.0, 0.0)).y(), LayoutUnit::epsilon());

    // Rotate (100, 0) 135 degrees to (-70.7, 70.7)
    matrix = child->localToAncestorTransform(container);
    EXPECT_FALSE(matrix.isIdentity());
    EXPECT_TRUE(matrix.isAffine());
    EXPECT_NEAR(-100.0 * sqrt(2.0) / 2.0, matrix.projectPoint(FloatPoint(100.0, 0.0)).x(), LayoutUnit::epsilon());
    EXPECT_NEAR(100.0 * sqrt(2.0) / 2.0, matrix.projectPoint(FloatPoint(100.0, 0.0)).y(), LayoutUnit::epsilon());
}

TEST_F(MapCoordinatesTest, LocalToAbsoluteTransformFlattens)
{
    document().frame()->settings()->setAcceleratedCompositingEnabled(true);
    setBodyInnerHTML(
        "<div style='position: absolute; left: 0; top: 0;'>"
        "  <div style='transform: rotateY(45deg); -webkit-transform-style:preserve-3d;'>"
        "    <div style='transform: rotateY(-45deg); -webkit-transform-style:preserve-3d;'>"
        "      <div id='child1'></div>"
        "    </div>"
        "  </div>"
        "  <div style='transform: rotateY(45deg);'>"
        "    <div style='transform: rotateY(-45deg);'>"
        "      <div id='child2'></div>"
        "    </div>"
        "  </div>"
        "</div>");
    LayoutObject* child1 = getLayoutObjectByElementId("child1");
    LayoutObject* child2 = getLayoutObjectByElementId("child2");
    TransformationMatrix matrix;

    matrix = child1->localToAbsoluteTransform();

    // With child1, the rotations cancel and points should map basically back to themselves.
    EXPECT_NEAR(100.0, matrix.projectPoint(FloatPoint(100.0, 50.0)).x(), LayoutUnit::epsilon());
    EXPECT_NEAR(50.0, matrix.projectPoint(FloatPoint(100.0, 50.0)).y(), LayoutUnit::epsilon());
    EXPECT_NEAR(50.0, matrix.projectPoint(FloatPoint(50.0, 100.0)).x(), LayoutUnit::epsilon());
    EXPECT_NEAR(100.0, matrix.projectPoint(FloatPoint(50.0, 100.0)).y(), LayoutUnit::epsilon());

    // With child2, each rotation gets flattened and the end result is approximately a 90-degree rotation.
    matrix = child2->localToAbsoluteTransform();
    EXPECT_NEAR(50.0, matrix.projectPoint(FloatPoint(100.0, 50.0)).x(), LayoutUnit::epsilon());
    EXPECT_NEAR(50.0, matrix.projectPoint(FloatPoint(100.0, 50.0)).y(), LayoutUnit::epsilon());
    EXPECT_NEAR(25.0, matrix.projectPoint(FloatPoint(50.0, 100.0)).x(), LayoutUnit::epsilon());
    EXPECT_NEAR(100.0, matrix.projectPoint(FloatPoint(50.0, 100.0)).y(), LayoutUnit::epsilon());
}

} // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintControllerPaintTest.h"

#include "core/layout/LayoutText.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/page/FocusController.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintLayerPainter.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"

namespace blink {

INSTANTIATE_TEST_CASE_P(All, PaintControllerPaintTestForSlimmingPaintV1AndV2, ::testing::Bool());

TEST_P(PaintControllerPaintTestForSlimmingPaintV1AndV2, FullDocumentPaintingWithCaret)
{
    setBodyInnerHTML("<div id='div' contentEditable='true' style='outline:none'>XYZ</div>");
    document().page()->focusController().setActive(true);
    document().page()->focusController().setFocused(true);
    Element& div = *toElement(document().body()->firstChild());
    LayoutObject& divLayoutObject = *document().body()->firstChild()->layoutObject();
    InlineTextBox& textInlineBox = *toLayoutText(div.firstChild()->layoutObject())->firstTextBox();

    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 2,
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(textInlineBox, foregroundType));

    div.focus();
    document().view()->updateAllLifecyclePhases();

    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 3,
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(textInlineBox, foregroundType),
        TestDisplayItem(divLayoutObject, DisplayItem::Caret)); // New!
}

TEST_P(PaintControllerPaintTestForSlimmingPaintV1AndV2, InlineRelayout)
{
    setBodyInnerHTML("<div id='div' style='width:100px; height: 200px'>AAAAAAAAAA BBBBBBBBBB</div>");
    Element& div = *toElement(document().body()->firstChild());
    LayoutBlock& divBlock = *toLayoutBlock(document().body()->firstChild()->layoutObject());
    LayoutText& text = *toLayoutText(divBlock.firstChild());
    InlineTextBox& firstTextBox = *text.firstTextBox();

    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 2,
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(firstTextBox, foregroundType));

    div.setAttribute(HTMLNames::styleAttr, "width: 10px; height: 200px");
    document().view()->updateAllLifecyclePhases();

    LayoutText& newText = *toLayoutText(divBlock.firstChild());
    InlineTextBox& newFirstTextBox = *newText.firstTextBox();
    InlineTextBox& secondTextBox = *newText.firstTextBox()->nextTextBox();

    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 3,
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(newFirstTextBox, foregroundType),
        TestDisplayItem(secondTextBox, foregroundType));
}

} // namespace blink

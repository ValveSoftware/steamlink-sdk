/**
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "core/rendering/RenderTextControlMultiLine.h"

#include "core/html/HTMLTextAreaElement.h"
#include "core/rendering/HitTestResult.h"

namespace WebCore {

RenderTextControlMultiLine::RenderTextControlMultiLine(HTMLTextAreaElement* element)
    : RenderTextControl(element)
{
    ASSERT(element);
}

RenderTextControlMultiLine::~RenderTextControlMultiLine()
{
}

bool RenderTextControlMultiLine::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (!RenderTextControl::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction))
        return false;

    if (result.innerNode() == node() || result.innerNode() == innerEditorElement())
        hitInnerEditorElement(result, locationInContainer.point(), accumulatedOffset);

    return true;
}

float RenderTextControlMultiLine::getAvgCharWidth(AtomicString family)
{
    // Since Lucida Grande is the default font, we want this to match the width
    // of Courier New, the default font for textareas in IE, Firefox and Safari Win.
    // 1229 is the avgCharWidth value in the OS/2 table for Courier New.
    if (family == "Lucida Grande")
        return scaleEmToUnits(1229);

    return RenderTextControl::getAvgCharWidth(family);
}

LayoutUnit RenderTextControlMultiLine::preferredContentLogicalWidth(float charWidth) const
{
    int factor = toHTMLTextAreaElement(node())->cols();
    return static_cast<LayoutUnit>(ceilf(charWidth * factor)) + scrollbarThickness();
}

LayoutUnit RenderTextControlMultiLine::computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const
{
    return lineHeight * toHTMLTextAreaElement(node())->rows() + nonContentHeight;
}

int RenderTextControlMultiLine::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    return RenderBox::baselinePosition(baselineType, firstLine, direction, linePositionMode);
}

PassRefPtr<RenderStyle> RenderTextControlMultiLine::createInnerEditorStyle(const RenderStyle* startStyle) const
{
    RefPtr<RenderStyle> textBlockStyle = RenderStyle::create();
    textBlockStyle->inheritFrom(startStyle);
    adjustInnerEditorStyle(textBlockStyle.get());
    textBlockStyle->setDisplay(BLOCK);
    textBlockStyle->setUnique();

    return textBlockStyle.release();
}

RenderObject* RenderTextControlMultiLine::layoutSpecialExcludedChild(bool relayoutChildren, SubtreeLayoutScope& layoutScope)
{
    RenderObject* placeholderRenderer = RenderTextControl::layoutSpecialExcludedChild(relayoutChildren, layoutScope);
    if (!placeholderRenderer)
        return 0;
    if (!placeholderRenderer->isBox())
        return placeholderRenderer;
    RenderBox* placeholderBox = toRenderBox(placeholderRenderer);
    placeholderBox->style()->setLogicalWidth(Length(contentLogicalWidth() - placeholderBox->borderAndPaddingLogicalWidth(), Fixed));
    placeholderBox->layoutIfNeeded();
    placeholderBox->setX(borderLeft() + paddingLeft());
    placeholderBox->setY(borderTop() + paddingTop());
    return placeholderRenderer;
}

}

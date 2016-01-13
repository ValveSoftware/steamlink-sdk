/*
 * Copyright (C) 2006, 2007, 2012 Apple Inc. All rights reserved.
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
#include "core/rendering/RenderFileUploadControl.h"

#include "core/HTMLNames.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/fileapi/FileList.h"
#include "core/html/HTMLInputElement.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderButton.h"
#include "core/rendering/RenderTheme.h"
#include "platform/fonts/Font.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/text/PlatformLocale.h"
#include "platform/text/TextRun.h"
#include <math.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

const int afterButtonSpacing = 4;
const int defaultWidthNumChars = 34;
const int buttonShadowHeight = 2;

RenderFileUploadControl::RenderFileUploadControl(HTMLInputElement* input)
    : RenderBlockFlow(input)
    , m_canReceiveDroppedFiles(input->canReceiveDroppedFiles())
{
}

RenderFileUploadControl::~RenderFileUploadControl()
{
}

void RenderFileUploadControl::updateFromElement()
{
    HTMLInputElement* input = toHTMLInputElement(node());
    ASSERT(input->isFileUpload());

    if (HTMLInputElement* button = uploadButton()) {
        bool newCanReceiveDroppedFilesState = input->canReceiveDroppedFiles();
        if (m_canReceiveDroppedFiles != newCanReceiveDroppedFilesState) {
            m_canReceiveDroppedFiles = newCanReceiveDroppedFilesState;
            button->setActive(newCanReceiveDroppedFilesState);
        }
    }

    // This only supports clearing out the files, but that's OK because for
    // security reasons that's the only change the DOM is allowed to make.
    FileList* files = input->files();
    ASSERT(files);
    if (files && files->isEmpty())
        paintInvalidationForWholeRenderer();
}

static int nodeWidth(Node* node)
{
    return (node && node->renderBox()) ? node->renderBox()->pixelSnappedWidth() : 0;
}

int RenderFileUploadControl::maxFilenameWidth() const
{
    return max(0, contentBoxRect().pixelSnappedWidth() - nodeWidth(uploadButton()) - afterButtonSpacing);
}

void RenderFileUploadControl::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (style()->visibility() != VISIBLE)
        return;

    // Push a clip.
    GraphicsContextStateSaver stateSaver(*paintInfo.context, false);
    if (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseChildBlockBackgrounds) {
        IntRect clipRect = enclosingIntRect(LayoutRect(paintOffset.x() + borderLeft(), paintOffset.y() + borderTop(),
                         width() - borderLeft() - borderRight(), height() - borderBottom() - borderTop() + buttonShadowHeight));
        if (clipRect.isEmpty())
            return;
        stateSaver.save();
        paintInfo.context->clip(clipRect);
    }

    if (paintInfo.phase == PaintPhaseForeground) {
        const String& displayedFilename = fileTextValue();
        const Font& font = style()->font();
        TextRun textRun = constructTextRun(this, font, displayedFilename, style(), TextRun::AllowTrailingExpansion, RespectDirection | RespectDirectionOverride);
        textRun.disableRoundingHacks();

        // Determine where the filename should be placed
        LayoutUnit contentLeft = paintOffset.x() + borderLeft() + paddingLeft();
        HTMLInputElement* button = uploadButton();
        if (!button)
            return;

        LayoutUnit buttonWidth = nodeWidth(button);
        LayoutUnit buttonAndSpacingWidth = buttonWidth + afterButtonSpacing;
        float textWidth = font.width(textRun);
        LayoutUnit textX;
        if (style()->isLeftToRightDirection())
            textX = contentLeft + buttonAndSpacingWidth;
        else
            textX = contentLeft + contentWidth() - buttonAndSpacingWidth - textWidth;

        LayoutUnit textY = 0;
        // We want to match the button's baseline
        // FIXME: Make this work with transforms.
        if (RenderButton* buttonRenderer = toRenderButton(button->renderer()))
            textY = paintOffset.y() + borderTop() + paddingTop() + buttonRenderer->baselinePosition(AlphabeticBaseline, true, HorizontalLine, PositionOnContainingLine);
        else
            textY = baselinePosition(AlphabeticBaseline, true, HorizontalLine, PositionOnContainingLine);
        TextRunPaintInfo textRunPaintInfo(textRun);
        // FIXME: Shouldn't these offsets be rounded? crbug.com/350474
        textRunPaintInfo.bounds = FloatRect(textX.toFloat(), textY.toFloat() - style()->fontMetrics().ascent(),
            textWidth, style()->fontMetrics().height());

        paintInfo.context->setFillColor(resolveColor(CSSPropertyColor));

        // Draw the filename
        paintInfo.context->drawBidiText(font, textRunPaintInfo, IntPoint(roundToInt(textX), roundToInt(textY)));
    }

    // Paint the children.
    RenderBlockFlow::paintObject(paintInfo, paintOffset);
}

void RenderFileUploadControl::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    // Figure out how big the filename space needs to be for a given number of characters
    // (using "0" as the nominal character).
    const UChar character = '0';
    const String characterAsString = String(&character, 1);
    const Font& font = style()->font();
    // FIXME: Remove the need for this const_cast by making constructTextRun take a const RenderObject*.
    RenderFileUploadControl* renderer = const_cast<RenderFileUploadControl*>(this);
    float minDefaultLabelWidth = defaultWidthNumChars * font.width(constructTextRun(renderer, font, characterAsString, style(), TextRun::AllowTrailingExpansion));

    const String label = toHTMLInputElement(node())->locale().queryString(blink::WebLocalizedString::FileButtonNoFileSelectedLabel);
    float defaultLabelWidth = font.width(constructTextRun(renderer, font, label, style(), TextRun::AllowTrailingExpansion));
    if (HTMLInputElement* button = uploadButton())
        if (RenderObject* buttonRenderer = button->renderer())
            defaultLabelWidth += buttonRenderer->maxPreferredLogicalWidth() + afterButtonSpacing;
    maxLogicalWidth = static_cast<int>(ceilf(max(minDefaultLabelWidth, defaultLabelWidth)));

    if (!style()->width().isPercent())
        minLogicalWidth = maxLogicalWidth;
}

void RenderFileUploadControl::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;
    RenderStyle* styleToUse = style();

    if (styleToUse->width().isFixed() && styleToUse->width().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(styleToUse->width().value());
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    if (styleToUse->minWidth().isFixed() && styleToUse->minWidth().value() > 0) {
        m_maxPreferredLogicalWidth = max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse->minWidth().value()));
        m_minPreferredLogicalWidth = max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse->minWidth().value()));
    }

    if (styleToUse->maxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse->maxWidth().value()));
        m_minPreferredLogicalWidth = min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse->maxWidth().value()));
    }

    int toAdd = borderAndPaddingWidth();
    m_minPreferredLogicalWidth += toAdd;
    m_maxPreferredLogicalWidth += toAdd;

    clearPreferredLogicalWidthsDirty();
}

PositionWithAffinity RenderFileUploadControl::positionForPoint(const LayoutPoint&)
{
    return PositionWithAffinity();
}

HTMLInputElement* RenderFileUploadControl::uploadButton() const
{
    // FIXME: This should be on HTMLInputElement as an API like innerButtonElement().
    HTMLInputElement* input = toHTMLInputElement(node());
    Node* buttonNode = input->userAgentShadowRoot()->firstChild();
    return isHTMLInputElement(buttonNode) ? toHTMLInputElement(buttonNode) : 0;
}

String RenderFileUploadControl::buttonValue()
{
    if (HTMLInputElement* button = uploadButton())
        return button->value();

    return String();
}

String RenderFileUploadControl::fileTextValue() const
{
    HTMLInputElement* input = toHTMLInputElement(node());
    ASSERT(input->files());
    return RenderTheme::theme().fileListNameForWidth(input->locale(), input->files(), style()->font(), maxFilenameWidth());
}

} // namespace WebCore

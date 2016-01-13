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
#include "core/rendering/RenderTextControl.h"

#include "core/html/HTMLTextFormControlElement.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderTheme.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "wtf/unicode/CharacterNames.h"

using namespace std;

namespace WebCore {

RenderTextControl::RenderTextControl(HTMLTextFormControlElement* element)
    : RenderBlockFlow(element)
{
    ASSERT(element);
}

RenderTextControl::~RenderTextControl()
{
}

HTMLTextFormControlElement* RenderTextControl::textFormControlElement() const
{
    return toHTMLTextFormControlElement(node());
}

HTMLElement* RenderTextControl::innerEditorElement() const
{
    return textFormControlElement()->innerEditorElement();
}

void RenderTextControl::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    // FIXME: This is a terrible hack to get the caret over the placeholder text since it'll
    // make us paint the placeholder first. (See https://trac.webkit.org/changeset/118733)
    Node* node = newChild->node();
    if (node && node->isElementNode() && toElement(node)->shadowPseudoId() == "-webkit-input-placeholder")
        RenderBlockFlow::addChild(newChild, firstChild());
    else
        RenderBlockFlow::addChild(newChild, beforeChild);
}

void RenderTextControl::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);
    Element* innerEditor = innerEditorElement();
    if (!innerEditor)
        return;
    RenderBlock* innerEditorRenderer = toRenderBlock(innerEditor->renderer());
    if (innerEditorRenderer) {
        // We may have set the width and the height in the old style in layout().
        // Reset them now to avoid getting a spurious layout hint.
        innerEditorRenderer->style()->setHeight(Length());
        innerEditorRenderer->style()->setWidth(Length());
        innerEditorRenderer->setStyle(createInnerEditorStyle(style()));
        innerEditor->setNeedsStyleRecalc(SubtreeStyleChange);
    }
    textFormControlElement()->updatePlaceholderVisibility(false);
}

static inline void updateUserModifyProperty(HTMLTextFormControlElement* node, RenderStyle* style)
{
    style->setUserModify(node->isDisabledOrReadOnly() ? READ_ONLY : READ_WRITE_PLAINTEXT_ONLY);
}

void RenderTextControl::adjustInnerEditorStyle(RenderStyle* textBlockStyle) const
{
    // The inner block, if present, always has its direction set to LTR,
    // so we need to inherit the direction and unicode-bidi style from the element.
    textBlockStyle->setDirection(style()->direction());
    textBlockStyle->setUnicodeBidi(style()->unicodeBidi());

    updateUserModifyProperty(textFormControlElement(), textBlockStyle);
}

int RenderTextControl::textBlockLogicalHeight() const
{
    return logicalHeight() - borderAndPaddingLogicalHeight();
}

int RenderTextControl::textBlockLogicalWidth() const
{
    Element* innerEditor = innerEditorElement();
    ASSERT(innerEditor);

    LayoutUnit unitWidth = logicalWidth() - borderAndPaddingLogicalWidth();
    if (innerEditor->renderer())
        unitWidth -= innerEditor->renderBox()->paddingStart() + innerEditor->renderBox()->paddingEnd();

    return unitWidth;
}

void RenderTextControl::updateFromElement()
{
    Element* innerEditor = innerEditorElement();
    if (innerEditor && innerEditor->renderer())
        updateUserModifyProperty(textFormControlElement(), innerEditor->renderer()->style());
}

int RenderTextControl::scrollbarThickness() const
{
    // FIXME: We should get the size of the scrollbar from the RenderTheme instead.
    return ScrollbarTheme::theme()->scrollbarThickness();
}

void RenderTextControl::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    HTMLElement* innerEditor = innerEditorElement();
    ASSERT(innerEditor);
    if (RenderBox* innerEditorBox = innerEditor->renderBox()) {
        LayoutUnit nonContentHeight = innerEditorBox->borderAndPaddingHeight() + innerEditorBox->marginHeight();
        logicalHeight = computeControlLogicalHeight(innerEditorBox->lineHeight(true, HorizontalLine, PositionOfInteriorLineBoxes), nonContentHeight);

        // We are able to have a horizontal scrollbar if the overflow style is scroll, or if its auto and there's no word wrap.
        if ((isHorizontalWritingMode() && (style()->overflowX() == OSCROLL ||  (style()->overflowX() == OAUTO && innerEditor->renderer()->style()->overflowWrap() == NormalOverflowWrap)))
            || (!isHorizontalWritingMode() && (style()->overflowY() == OSCROLL ||  (style()->overflowY() == OAUTO && innerEditor->renderer()->style()->overflowWrap() == NormalOverflowWrap))))
            logicalHeight += scrollbarThickness();

        // FIXME: The logical height of the inner text box should have been added before calling computeLogicalHeight to
        // avoid this hack.
        updateIntrinsicContentLogicalHeight(logicalHeight);

        logicalHeight += borderAndPaddingHeight();
    }

    RenderBox::computeLogicalHeight(logicalHeight, logicalTop, computedValues);
}

void RenderTextControl::hitInnerEditorElement(HitTestResult& result, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset)
{
    HTMLElement* innerEditor = innerEditorElement();
    if (!innerEditor->renderer())
        return;

    LayoutPoint adjustedLocation = accumulatedOffset + location();
    LayoutPoint localPoint = pointInContainer - toLayoutSize(adjustedLocation + innerEditor->renderBox()->location());
    if (hasOverflowClip())
        localPoint += scrolledContentOffset();
    result.setInnerNode(innerEditor);
    result.setInnerNonSharedNode(innerEditor);
    result.setLocalPoint(localPoint);
}

static const char* const fontFamiliesWithInvalidCharWidth[] = {
    "American Typewriter",
    "Arial Hebrew",
    "Chalkboard",
    "Cochin",
    "Corsiva Hebrew",
    "Courier",
    "Euphemia UCAS",
    "Geneva",
    "Gill Sans",
    "Hei",
    "Helvetica",
    "Hoefler Text",
    "InaiMathi",
    "Kai",
    "Lucida Grande",
    "Marker Felt",
    "Monaco",
    "Mshtakan",
    "New Peninim MT",
    "Osaka",
    "Raanana",
    "STHeiti",
    "Symbol",
    "Times",
    "Apple Braille",
    "Apple LiGothic",
    "Apple LiSung",
    "Apple Symbols",
    "AppleGothic",
    "AppleMyungjo",
    "#GungSeo",
    "#HeadLineA",
    "#PCMyungjo",
    "#PilGi",
};

// For font families where any of the fonts don't have a valid entry in the OS/2 table
// for avgCharWidth, fallback to the legacy webkit behavior of getting the avgCharWidth
// from the width of a '0'. This only seems to apply to a fixed number of Mac fonts,
// but, in order to get similar rendering across platforms, we do this check for
// all platforms.
bool RenderTextControl::hasValidAvgCharWidth(AtomicString family)
{
    static HashSet<AtomicString>* fontFamiliesWithInvalidCharWidthMap = 0;

    if (family.isEmpty())
        return false;

    if (!fontFamiliesWithInvalidCharWidthMap) {
        fontFamiliesWithInvalidCharWidthMap = new HashSet<AtomicString>;

        for (size_t i = 0; i < WTF_ARRAY_LENGTH(fontFamiliesWithInvalidCharWidth); ++i)
            fontFamiliesWithInvalidCharWidthMap->add(AtomicString(fontFamiliesWithInvalidCharWidth[i]));
    }

    return !fontFamiliesWithInvalidCharWidthMap->contains(family);
}

float RenderTextControl::getAvgCharWidth(AtomicString family)
{
    if (hasValidAvgCharWidth(family))
        return roundf(style()->font().primaryFont()->avgCharWidth());

    const UChar ch = '0';
    const String str = String(&ch, 1);
    const Font& font = style()->font();
    TextRun textRun = constructTextRun(this, font, str, style(), TextRun::AllowTrailingExpansion);
    textRun.disableRoundingHacks();
    return font.width(textRun);
}

float RenderTextControl::scaleEmToUnits(int x) const
{
    // This matches the unitsPerEm value for MS Shell Dlg and Courier New from the "head" font table.
    float unitsPerEm = 2048.0f;
    return roundf(style()->font().fontDescription().computedSize() * x / unitsPerEm);
}

void RenderTextControl::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    // Use average character width. Matches IE.
    AtomicString family = style()->font().fontDescription().family().family();
    maxLogicalWidth = preferredContentLogicalWidth(const_cast<RenderTextControl*>(this)->getAvgCharWidth(family));
    if (RenderBox* innerEditorRenderBox = innerEditorElement()->renderBox())
        maxLogicalWidth += innerEditorRenderBox->paddingStart() + innerEditorRenderBox->paddingEnd();
    if (!style()->logicalWidth().isPercent())
        minLogicalWidth = maxLogicalWidth;
}

void RenderTextControl::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;
    RenderStyle* styleToUse = style();

    if (styleToUse->logicalWidth().isFixed() && styleToUse->logicalWidth().value() >= 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(styleToUse->logicalWidth().value());
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    if (styleToUse->logicalMinWidth().isFixed() && styleToUse->logicalMinWidth().value() > 0) {
        m_maxPreferredLogicalWidth = max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse->logicalMinWidth().value()));
        m_minPreferredLogicalWidth = max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse->logicalMinWidth().value()));
    }

    if (styleToUse->logicalMaxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse->logicalMaxWidth().value()));
        m_minPreferredLogicalWidth = min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse->logicalMaxWidth().value()));
    }

    LayoutUnit toAdd = borderAndPaddingLogicalWidth();

    m_minPreferredLogicalWidth += toAdd;
    m_maxPreferredLogicalWidth += toAdd;

    clearPreferredLogicalWidthsDirty();
}

void RenderTextControl::addFocusRingRects(Vector<IntRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject*)
{
    if (!size().isEmpty())
        rects.append(pixelSnappedIntRect(additionalOffset, size()));
}

RenderObject* RenderTextControl::layoutSpecialExcludedChild(bool relayoutChildren, SubtreeLayoutScope& layoutScope)
{
    HTMLElement* placeholder = toHTMLTextFormControlElement(node())->placeholderElement();
    RenderObject* placeholderRenderer = placeholder ? placeholder->renderer() : 0;
    if (!placeholderRenderer)
        return 0;
    if (relayoutChildren)
        layoutScope.setChildNeedsLayout(placeholderRenderer);
    return placeholderRenderer;
}

} // namespace WebCore

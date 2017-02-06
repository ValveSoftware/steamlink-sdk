/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 */

#include "core/css/resolver/StyleAdjuster.h"

#include "core/HTMLNames.h"
#include "core/SVGNames.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/HTMLTableCellElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/layout/LayoutTheme.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/svg/SVGSVGElement.h"
#include "platform/Length.h"
#include "platform/transforms/TransformOperations.h"
#include "wtf/Assertions.h"

namespace blink {

using namespace HTMLNames;

static EDisplay equivalentBlockDisplay(EDisplay display)
{
    switch (display) {
    case BLOCK:
    case TABLE:
    case BOX:
    case FLEX:
    case GRID:
    case LIST_ITEM:
        return display;
    case INLINE_TABLE:
        return TABLE;
    case INLINE_BOX:
        return BOX;
    case INLINE_FLEX:
        return FLEX;
    case INLINE_GRID:
        return GRID;

    case INLINE:
    case INLINE_BLOCK:
    case TABLE_ROW_GROUP:
    case TABLE_HEADER_GROUP:
    case TABLE_FOOTER_GROUP:
    case TABLE_ROW:
    case TABLE_COLUMN_GROUP:
    case TABLE_COLUMN:
    case TABLE_CELL:
    case TABLE_CAPTION:
        return BLOCK;
    case NONE:
        ASSERT_NOT_REACHED();
        return NONE;
    }
    ASSERT_NOT_REACHED();
    return BLOCK;
}

static bool isOutermostSVGElement(const Element* element)
{
    return element && element->isSVGElement() && toSVGElement(*element).isOutermostSVGSVGElement();
}

// CSS requires text-decoration to be reset at each DOM element for
// inline blocks, inline tables, shadow DOM crossings, floating elements,
// and absolute or relatively positioned elements. Outermost <svg> roots are
// considered to be atomic inline-level.
static bool doesNotInheritTextDecoration(const ComputedStyle& style, const Element* element)
{
    return style.display() == INLINE_TABLE
        || style.display() == INLINE_BLOCK || style.display() == INLINE_BOX || isAtShadowBoundary(element)
        || style.isFloating() || style.hasOutOfFlowPosition() || isOutermostSVGElement(element);
}

// FIXME: This helper is only needed because pseudoStyleForElement passes a null
// element to adjustComputedStyle, so we can't just use element->isInTopLayer().
static bool isInTopLayer(const Element* element, const ComputedStyle& style)
{
    return (element && element->isInTopLayer()) || style.styleType() == PseudoIdBackdrop;
}

static bool parentStyleForcesZIndexToCreateStackingContext(const ComputedStyle& parentStyle)
{
    return parentStyle.isDisplayFlexibleOrGridBox();
}

static bool hasWillChangeThatCreatesStackingContext(const ComputedStyle& style)
{
    for (size_t i = 0; i < style.willChangeProperties().size(); ++i) {
        switch (style.willChangeProperties()[i]) {
        case CSSPropertyOpacity:
        case CSSPropertyTransform:
        case CSSPropertyAliasWebkitTransform:
        case CSSPropertyTransformStyle:
        case CSSPropertyAliasWebkitTransformStyle:
        case CSSPropertyPerspective:
        case CSSPropertyAliasWebkitPerspective:
        case CSSPropertyWebkitMask:
        case CSSPropertyWebkitMaskBoxImage:
        case CSSPropertyWebkitClipPath:
        case CSSPropertyWebkitBoxReflect:
        case CSSPropertyFilter:
        case CSSPropertyAliasWebkitFilter:
        case CSSPropertyBackdropFilter:
        case CSSPropertyZIndex:
        case CSSPropertyPosition:
        case CSSPropertyMixBlendMode:
        case CSSPropertyIsolation:
            return true;
        default:
            break;
        }
    }
    return false;
}

void StyleAdjuster::adjustComputedStyle(ComputedStyle& style, const ComputedStyle& parentStyle, Element* element)
{
    if (style.display() != NONE) {
        if (element && element->isHTMLElement())
            adjustStyleForHTMLElement(style, parentStyle, toHTMLElement(*element));

        // Per the spec, position 'static' and 'relative' in the top layer compute to 'absolute'.
        if (isInTopLayer(element, style) && (style.position() == StaticPosition || style.position() == RelativePosition))
            style.setPosition(AbsolutePosition);

        // Absolute/fixed positioned elements, floating elements and the document element need block-like outside display.
        if (style.hasOutOfFlowPosition() || style.isFloating() || (element && element->document().documentElement() == element))
            style.setDisplay(equivalentBlockDisplay(style.display()));

        // We don't adjust the first letter style earlier because we may change the display setting in
        // adjustStyeForTagName() above.
        adjustStyleForFirstLetter(style);

        adjustStyleForDisplay(style, parentStyle, element ? &element->document() : 0);

        // Paint containment forces a block formatting context, so we must coerce from inline.
        // https://drafts.csswg.org/css-containment/#containment-paint
        if (style.containsPaint() && style.display() == INLINE)
            style.setDisplay(BLOCK);
    } else {
        adjustStyleForFirstLetter(style);
    }

    if (element && element->hasCompositorProxy())
        style.setHasCompositorProxy(true);

    // Make sure our z-index value is only applied if the object is positioned.
    if (style.position() == StaticPosition && !parentStyleForcesZIndexToCreateStackingContext(parentStyle))
        style.setHasAutoZIndex();

    if (style.overflowX() != OverflowVisible || style.overflowY() != OverflowVisible)
        adjustOverflow(style);

    // Auto z-index becomes 0 for the root element and transparent objects. This prevents
    // cases where objects that should be blended as a single unit end up with a non-transparent
    // object wedged in between them. Auto z-index also becomes 0 for objects that specify transforms/masks/reflections.
    if (style.hasAutoZIndex() && ((element && element->document().documentElement() == element)
        || style.hasOpacity()
        || style.hasTransformRelatedProperty()
        || style.hasMask()
        || style.clipPath()
        || style.boxReflect()
        || style.hasFilterInducingProperty()
        || style.hasBlendMode()
        || style.hasIsolation()
        || style.hasViewportConstrainedPosition()
        || isInTopLayer(element, style)
        || hasWillChangeThatCreatesStackingContext(style)
        || style.containsPaint()))
        style.setZIndex(0);

    if (doesNotInheritTextDecoration(style, element))
        style.clearAppliedTextDecorations();

    style.applyTextDecorations();

    // Cull out any useless layers and also repeat patterns into additional layers.
    style.adjustBackgroundLayers();
    style.adjustMaskLayers();

    // Let the theme also have a crack at adjusting the style.
    if (style.hasAppearance())
        LayoutTheme::theme().adjustStyle(style, element);

    // If we have first-letter pseudo style, transitions, or animations, do not share this style.
    if (style.hasPseudoStyle(PseudoIdFirstLetter) || style.transitions() || style.animations())
        style.setUnique();

    adjustStyleForEditing(style);

    bool isSVGElement = element && element->isSVGElement();
    if (isSVGElement) {
        // Only the root <svg> element in an SVG document fragment tree honors css position
        if (!(isSVGSVGElement(*element) && element->parentNode() && !element->parentNode()->isSVGElement()))
            style.setPosition(ComputedStyle::initialPosition());

        // SVG text layout code expects us to be a block-level style element.
        if ((isSVGForeignObjectElement(*element) || isSVGTextElement(*element)) && style.isDisplayInlineType())
            style.setDisplay(BLOCK);

        // Columns don't apply to svg text elements.
        if (isSVGTextElement(*element))
            style.clearMultiCol();
    }
    adjustStyleForAlignment(style, parentStyle);
}

void StyleAdjuster::adjustStyleForEditing(ComputedStyle& style)
{
    if (style.userModify() != READ_WRITE_PLAINTEXT_ONLY)
        return;
    // Collapsing whitespace is harmful in plain-text editing.
    if (style.whiteSpace() == NORMAL)
        style.setWhiteSpace(PRE_WRAP);
    else if (style.whiteSpace() == NOWRAP)
        style.setWhiteSpace(PRE);
    else if (style.whiteSpace() == PRE_LINE)
        style.setWhiteSpace(PRE_WRAP);
}

void StyleAdjuster::adjustStyleForFirstLetter(ComputedStyle& style)
{
    if (style.styleType() != PseudoIdFirstLetter)
        return;

    // Force inline display (except for floating first-letters).
    style.setDisplay(style.isFloating() ? BLOCK : INLINE);

    // CSS2 says first-letter can't be positioned.
    style.setPosition(StaticPosition);
}

void StyleAdjuster::adjustStyleForAlignment(ComputedStyle& style, const ComputedStyle& parentStyle)
{
    bool isFlexOrGrid = style.isDisplayFlexibleOrGridBox();
    bool absolutePositioned = style.position() == AbsolutePosition;

    // If the inherited value of justify-items includes the legacy keyword, 'auto'
    // computes to the the inherited value.
    // Otherwise, auto computes to:
    //  - 'stretch' for flex containers and grid containers.
    //  - 'start' for everything else.
    if (style.justifyItemsPosition() == ItemPositionAuto) {
        if (parentStyle.justifyItemsPositionType() == LegacyPosition)
            style.setJustifyItems(parentStyle.justifyItems());
        else if (isFlexOrGrid)
            style.setJustifyItemsPosition(ItemPositionStretch);
    }

    // The 'auto' keyword computes to 'stretch' on absolutely-positioned elements,
    // and to the computed value of justify-items on the parent (minus
    // any legacy keywords) on all other boxes.
    if (style.justifySelfPosition() == ItemPositionAuto) {
        if (absolutePositioned)
            style.setJustifySelfPosition(ItemPositionStretch);
        else
            style.setJustifySelf(parentStyle.justifyItems());
    }

    // The 'auto' keyword computes to:
    //  - 'stretch' for flex containers and grid containers,
    //  - 'start' for everything else.
    if (style.alignItemsPosition() == ItemPositionAuto) {
        if (isFlexOrGrid)
            style.setAlignItemsPosition(ItemPositionStretch);
    }
}

void StyleAdjuster::adjustStyleForHTMLElement(ComputedStyle& style, const ComputedStyle& parentStyle, HTMLElement& element)
{
    // <div> and <span> are the most common elements on the web, we skip all the work for them.
    if (isHTMLDivElement(element) || isHTMLSpanElement(element))
        return;

    if (isHTMLTableCellElement(element)) {
        if (style.whiteSpace() == KHTML_NOWRAP) {
            // Figure out if we are really nowrapping or if we should just
            // use normal instead. If the width of the cell is fixed, then
            // we don't actually use NOWRAP.
            if (style.width().isFixed())
                style.setWhiteSpace(NORMAL);
            else
                style.setWhiteSpace(NOWRAP);
        }
        return;
    }

    if (isHTMLTableElement(element)) {
        // Tables never support the -webkit-* values for text-align and will reset back to the default.
        if (style.textAlign() == WEBKIT_LEFT || style.textAlign() == WEBKIT_CENTER || style.textAlign() == WEBKIT_RIGHT)
            style.setTextAlign(TASTART);
        return;
    }

    if (isHTMLFrameElement(element) || isHTMLFrameSetElement(element)) {
        // Frames and framesets never honor position:relative or position:absolute. This is necessary to
        // fix a crash where a site tries to position these objects. They also never honor display.
        style.setPosition(StaticPosition);
        style.setDisplay(BLOCK);
        return;
    }

    if (isHTMLFrameElementBase(element)) {
        // Frames cannot overflow (they are always the size we ask them to be).
        // Some compositing code paths may try to draw scrollbars anyhow.
        style.setOverflowX(OverflowVisible);
        style.setOverflowY(OverflowVisible);
        return;
    }

    if (isHTMLRTElement(element)) {
        // Ruby text does not support float or position. This might change with evolution of the specification.
        style.setPosition(StaticPosition);
        style.setFloating(NoFloat);
        return;
    }

    if (isHTMLMarqueeElement(element)) {
        // For now, <marquee> requires an overflow clip to work properly.
        style.setOverflowX(OverflowHidden);
        style.setOverflowY(OverflowHidden);
        return;
    }

    if (isHTMLTextAreaElement(element)) {
        // Textarea considers overflow visible as auto.
        style.setOverflowX(style.overflowX() == OverflowVisible ? OverflowAuto : style.overflowX());
        style.setOverflowY(style.overflowY() == OverflowVisible ? OverflowAuto : style.overflowY());
        return;
    }

    if (isHTMLPlugInElement(element)) {
        style.setRequiresAcceleratedCompositingForExternalReasons(toHTMLPlugInElement(element).shouldAccelerate());
        return;
    }
}

void StyleAdjuster::adjustOverflow(ComputedStyle& style)
{
    ASSERT(style.overflowX() != OverflowVisible || style.overflowY() != OverflowVisible);

    if (style.display() == TABLE || style.display() == INLINE_TABLE) {
        // Tables only support overflow:hidden and overflow:visible and ignore anything else,
        // see http://dev.w3.org/csswg/css2/visufx.html#overflow. As a table is not a block
        // container box the rules for resolving conflicting x and y values in CSS Overflow Module
        // Level 3 do not apply. Arguably overflow-x and overflow-y aren't allowed on tables but
        // all UAs allow it.
        if (style.overflowX() != OverflowHidden)
            style.setOverflowX(OverflowVisible);
        if (style.overflowY() != OverflowHidden)
            style.setOverflowY(OverflowVisible);
        // If we are left with conflicting overflow values for the x and y axes on a table then resolve
        // both to OverflowVisible. This is interoperable behaviour but is not specced anywhere.
        if (style.overflowX() == OverflowVisible)
            style.setOverflowY(OverflowVisible);
        else if (style.overflowY() == OverflowVisible)
            style.setOverflowX(OverflowVisible);
    } else if (style.overflowX() == OverflowVisible && style.overflowY() != OverflowVisible) {
        // If either overflow value is not visible, change to auto.
        // FIXME: Once we implement pagination controls, overflow-x should default to hidden
        // if overflow-y is set to -webkit-paged-x or -webkit-page-y. For now, we'll let it
        // default to auto so we can at least scroll through the pages.
        style.setOverflowX(OverflowAuto);
    } else if (style.overflowY() == OverflowVisible && style.overflowX() != OverflowVisible) {
        style.setOverflowY(OverflowAuto);
    }

    // Menulists should have visible overflow
    if (style.appearance() == MenulistPart) {
        style.setOverflowX(OverflowVisible);
        style.setOverflowY(OverflowVisible);
    }
}

void StyleAdjuster::adjustStyleForDisplay(ComputedStyle& style, const ComputedStyle& parentStyle, Document* document)
{
    if (style.display() == BLOCK && !style.isFloating())
        return;

    // FIXME: Don't support this mutation for pseudo styles like first-letter or first-line, since it's not completely
    // clear how that should work.
    if (style.display() == INLINE && style.styleType() == PseudoIdNone && style.getWritingMode() != parentStyle.getWritingMode())
        style.setDisplay(INLINE_BLOCK);

    // After performing the display mutation, check table rows. We do not honor position: relative table rows or cells.
    // This has been established for position: relative in CSS2.1 (and caused a crash in containingBlock()
    // on some sites).
    if ((style.display() == TABLE_HEADER_GROUP || style.display() == TABLE_ROW_GROUP
        || style.display() == TABLE_FOOTER_GROUP || style.display() == TABLE_ROW)
        && style.position() == RelativePosition)
        style.setPosition(StaticPosition);

    // Cannot support position: sticky for table columns and column groups because current code is only doing
    // background painting through columns / column groups
    if ((style.display() == TABLE_COLUMN_GROUP || style.display() == TABLE_COLUMN)
        && style.position() == StickyPosition)
        style.setPosition(StaticPosition);

    // writing-mode does not apply to table row groups, table column groups, table rows, and table columns.
    // FIXME: Table cells should be allowed to be perpendicular or flipped with respect to the table, though.
    if (style.display() == TABLE_COLUMN || style.display() == TABLE_COLUMN_GROUP || style.display() == TABLE_FOOTER_GROUP
        || style.display() == TABLE_HEADER_GROUP || style.display() == TABLE_ROW || style.display() == TABLE_ROW_GROUP
        || style.display() == TABLE_CELL)
        style.setWritingMode(parentStyle.getWritingMode());

    // FIXME: Since we don't support block-flow on flexible boxes yet, disallow setting
    // of block-flow to anything other than TopToBottomWritingMode.
    // https://bugs.webkit.org/show_bug.cgi?id=46418 - Flexible box support.
    if (style.getWritingMode() != TopToBottomWritingMode && (style.display() == BOX || style.display() == INLINE_BOX))
        style.setWritingMode(TopToBottomWritingMode);

    if (parentStyle.isDisplayFlexibleOrGridBox()) {
        style.setFloating(NoFloat);
        style.setDisplay(equivalentBlockDisplay(style.display()));

        // We want to count vertical percentage paddings/margins on flex items because our current
        // behavior is different from the spec and we want to gather compatibility data.
        if (style.paddingBefore().hasPercent() || style.paddingAfter().hasPercent())
            UseCounter::count(document, UseCounter::FlexboxPercentagePaddingVertical);
        if (style.marginBefore().hasPercent() || style.marginAfter().hasPercent())
            UseCounter::count(document, UseCounter::FlexboxPercentageMarginVertical);
    }
}

} // namespace blink

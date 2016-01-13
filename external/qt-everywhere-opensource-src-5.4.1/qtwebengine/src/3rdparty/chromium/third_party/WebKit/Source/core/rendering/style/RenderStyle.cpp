/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
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
#include "core/rendering/style/RenderStyle.h"

#include <algorithm>
#include "core/css/resolver/StyleResolver.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/TextAutosizer.h"
#include "core/rendering/style/AppliedTextDecoration.h"
#include "core/rendering/style/ContentData.h"
#include "core/rendering/style/QuotesData.h"
#include "core/rendering/style/ShadowList.h"
#include "core/rendering/style/StyleImage.h"
#include "core/rendering/style/StyleInheritedData.h"
#include "platform/LengthFunctions.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/FontSelector.h"
#include "platform/geometry/FloatRoundedRect.h"
#include "wtf/MathExtras.h"

using namespace std;

namespace WebCore {

struct SameSizeAsBorderValue {
    RGBA32 m_color;
    unsigned m_width;
};

COMPILE_ASSERT(sizeof(BorderValue) == sizeof(SameSizeAsBorderValue), BorderValue_should_not_grow);

struct SameSizeAsRenderStyle : public RefCounted<SameSizeAsRenderStyle> {
    void* dataRefs[7];
    void* ownPtrs[1];
    void* dataRefSvgStyle;

    struct InheritedFlags {
        unsigned m_bitfields[2];
    } inherited_flags;

    struct NonInheritedFlags {
        unsigned m_bitfields[2];
    } noninherited_flags;
};

COMPILE_ASSERT(sizeof(RenderStyle) == sizeof(SameSizeAsRenderStyle), RenderStyle_should_stay_small);

inline RenderStyle* defaultStyle()
{
    DEFINE_STATIC_REF(RenderStyle, s_defaultStyle, (RenderStyle::createDefaultStyle()));
    return s_defaultStyle;
}

PassRefPtr<RenderStyle> RenderStyle::create()
{
    return adoptRef(new RenderStyle());
}

PassRefPtr<RenderStyle> RenderStyle::createDefaultStyle()
{
    return adoptRef(new RenderStyle(DefaultStyle));
}

PassRefPtr<RenderStyle> RenderStyle::createAnonymousStyleWithDisplay(const RenderStyle* parentStyle, EDisplay display)
{
    RefPtr<RenderStyle> newStyle = RenderStyle::create();
    newStyle->inheritFrom(parentStyle);
    newStyle->inheritUnicodeBidiFrom(parentStyle);
    newStyle->setDisplay(display);
    return newStyle;
}

PassRefPtr<RenderStyle> RenderStyle::clone(const RenderStyle* other)
{
    return adoptRef(new RenderStyle(*other));
}

ALWAYS_INLINE RenderStyle::RenderStyle()
    : m_box(defaultStyle()->m_box)
    , visual(defaultStyle()->visual)
    , m_background(defaultStyle()->m_background)
    , surround(defaultStyle()->surround)
    , rareNonInheritedData(defaultStyle()->rareNonInheritedData)
    , rareInheritedData(defaultStyle()->rareInheritedData)
    , inherited(defaultStyle()->inherited)
    , m_svgStyle(defaultStyle()->m_svgStyle)
{
    setBitDefaults(); // Would it be faster to copy this from the default style?
    COMPILE_ASSERT((sizeof(InheritedFlags) <= 8), InheritedFlags_does_not_grow);
    COMPILE_ASSERT((sizeof(NonInheritedFlags) <= 8), NonInheritedFlags_does_not_grow);
}

ALWAYS_INLINE RenderStyle::RenderStyle(DefaultStyleTag)
{
    setBitDefaults();

    m_box.init();
    visual.init();
    m_background.init();
    surround.init();
    rareNonInheritedData.init();
    rareNonInheritedData.access()->m_deprecatedFlexibleBox.init();
    rareNonInheritedData.access()->m_flexibleBox.init();
    rareNonInheritedData.access()->m_marquee.init();
    rareNonInheritedData.access()->m_multiCol.init();
    rareNonInheritedData.access()->m_transform.init();
    rareNonInheritedData.access()->m_willChange.init();
    rareNonInheritedData.access()->m_filter.init();
    rareNonInheritedData.access()->m_grid.init();
    rareNonInheritedData.access()->m_gridItem.init();
    rareInheritedData.init();
    inherited.init();
    m_svgStyle.init();
}

ALWAYS_INLINE RenderStyle::RenderStyle(const RenderStyle& o)
    : RefCounted<RenderStyle>()
    , m_box(o.m_box)
    , visual(o.visual)
    , m_background(o.m_background)
    , surround(o.surround)
    , rareNonInheritedData(o.rareNonInheritedData)
    , rareInheritedData(o.rareInheritedData)
    , inherited(o.inherited)
    , m_svgStyle(o.m_svgStyle)
    , inherited_flags(o.inherited_flags)
    , noninherited_flags(o.noninherited_flags)
{
}

static StyleRecalcChange diffPseudoStyles(const RenderStyle* oldStyle, const RenderStyle* newStyle)
{
    // If the pseudoStyles have changed, we want any StyleRecalcChange that is not NoChange
    // because setStyle will do the right thing with anything else.
    if (!oldStyle->hasAnyPublicPseudoStyles())
        return NoChange;
    for (PseudoId pseudoId = FIRST_PUBLIC_PSEUDOID; pseudoId < FIRST_INTERNAL_PSEUDOID; pseudoId = static_cast<PseudoId>(pseudoId + 1)) {
        if (!oldStyle->hasPseudoStyle(pseudoId))
            continue;
        RenderStyle* newPseudoStyle = newStyle->getCachedPseudoStyle(pseudoId);
        if (!newPseudoStyle)
            return NoInherit;
        RenderStyle* oldPseudoStyle = oldStyle->getCachedPseudoStyle(pseudoId);
        if (oldPseudoStyle && *oldPseudoStyle != *newPseudoStyle)
            return NoInherit;
    }
    return NoChange;
}

StyleRecalcChange RenderStyle::stylePropagationDiff(const RenderStyle* oldStyle, const RenderStyle* newStyle)
{
    if ((!oldStyle && newStyle) || (oldStyle && !newStyle))
        return Reattach;

    if (!oldStyle && !newStyle)
        return NoChange;

    if (oldStyle->display() != newStyle->display()
        || oldStyle->hasPseudoStyle(FIRST_LETTER) != newStyle->hasPseudoStyle(FIRST_LETTER)
        || oldStyle->columnSpan() != newStyle->columnSpan()
        || !oldStyle->contentDataEquivalent(newStyle)
        || oldStyle->hasTextCombine() != newStyle->hasTextCombine())
        return Reattach;

    if (*oldStyle == *newStyle)
        return diffPseudoStyles(oldStyle, newStyle);

    if (oldStyle->inheritedNotEqual(newStyle)
        || oldStyle->hasExplicitlyInheritedProperties()
        || newStyle->hasExplicitlyInheritedProperties())
        return Inherit;

    return NoInherit;
}

void RenderStyle::inheritFrom(const RenderStyle* inheritParent, IsAtShadowBoundary isAtShadowBoundary)
{
    if (isAtShadowBoundary == AtShadowBoundary) {
        // Even if surrounding content is user-editable, shadow DOM should act as a single unit, and not necessarily be editable
        EUserModify currentUserModify = userModify();
        rareInheritedData = inheritParent->rareInheritedData;
        setUserModify(currentUserModify);
    } else
        rareInheritedData = inheritParent->rareInheritedData;
    inherited = inheritParent->inherited;
    inherited_flags = inheritParent->inherited_flags;
    if (m_svgStyle != inheritParent->m_svgStyle)
        m_svgStyle.access()->inheritFrom(inheritParent->m_svgStyle.get());
}

void RenderStyle::copyNonInheritedFrom(const RenderStyle* other)
{
    m_box = other->m_box;
    visual = other->visual;
    m_background = other->m_background;
    surround = other->surround;
    rareNonInheritedData = other->rareNonInheritedData;
    // The flags are copied one-by-one because noninherited_flags contains a bunch of stuff other than real style data.
    noninherited_flags._effectiveDisplay = other->noninherited_flags._effectiveDisplay;
    noninherited_flags._originalDisplay = other->noninherited_flags._originalDisplay;
    noninherited_flags._overflowX = other->noninherited_flags._overflowX;
    noninherited_flags._overflowY = other->noninherited_flags._overflowY;
    noninherited_flags._vertical_align = other->noninherited_flags._vertical_align;
    noninherited_flags._clear = other->noninherited_flags._clear;
    noninherited_flags._position = other->noninherited_flags._position;
    noninherited_flags._floating = other->noninherited_flags._floating;
    noninherited_flags._table_layout = other->noninherited_flags._table_layout;
    noninherited_flags._unicodeBidi = other->noninherited_flags._unicodeBidi;
    noninherited_flags._page_break_before = other->noninherited_flags._page_break_before;
    noninherited_flags._page_break_after = other->noninherited_flags._page_break_after;
    noninherited_flags._page_break_inside = other->noninherited_flags._page_break_inside;
    noninherited_flags.explicitInheritance = other->noninherited_flags.explicitInheritance;
    noninherited_flags.currentColor = other->noninherited_flags.currentColor;
    noninherited_flags.hasViewportUnits = other->noninherited_flags.hasViewportUnits;
    if (m_svgStyle != other->m_svgStyle)
        m_svgStyle.access()->copyNonInheritedFrom(other->m_svgStyle.get());
    ASSERT(zoom() == initialZoom());
}

bool RenderStyle::operator==(const RenderStyle& o) const
{
    // compare everything except the pseudoStyle pointer
    return inherited_flags == o.inherited_flags
        && noninherited_flags == o.noninherited_flags
        && m_box == o.m_box
        && visual == o.visual
        && m_background == o.m_background
        && surround == o.surround
        && rareNonInheritedData == o.rareNonInheritedData
        && rareInheritedData == o.rareInheritedData
        && inherited == o.inherited
        && m_svgStyle == o.m_svgStyle;
}

bool RenderStyle::isStyleAvailable() const
{
    return this != StyleResolver::styleNotYetAvailable();
}

bool RenderStyle::hasUniquePseudoStyle() const
{
    if (!m_cachedPseudoStyles || styleType() != NOPSEUDO)
        return false;

    for (size_t i = 0; i < m_cachedPseudoStyles->size(); ++i) {
        RenderStyle* pseudoStyle = m_cachedPseudoStyles->at(i).get();
        if (pseudoStyle->unique())
            return true;
    }

    return false;
}

RenderStyle* RenderStyle::getCachedPseudoStyle(PseudoId pid) const
{
    if (!m_cachedPseudoStyles || !m_cachedPseudoStyles->size())
        return 0;

    if (styleType() != NOPSEUDO)
        return 0;

    for (size_t i = 0; i < m_cachedPseudoStyles->size(); ++i) {
        RenderStyle* pseudoStyle = m_cachedPseudoStyles->at(i).get();
        if (pseudoStyle->styleType() == pid)
            return pseudoStyle;
    }

    return 0;
}

RenderStyle* RenderStyle::addCachedPseudoStyle(PassRefPtr<RenderStyle> pseudo)
{
    if (!pseudo)
        return 0;

    ASSERT(pseudo->styleType() > NOPSEUDO);

    RenderStyle* result = pseudo.get();

    if (!m_cachedPseudoStyles)
        m_cachedPseudoStyles = adoptPtr(new PseudoStyleCache);

    m_cachedPseudoStyles->append(pseudo);

    return result;
}

void RenderStyle::removeCachedPseudoStyle(PseudoId pid)
{
    if (!m_cachedPseudoStyles)
        return;
    for (size_t i = 0; i < m_cachedPseudoStyles->size(); ++i) {
        RenderStyle* pseudoStyle = m_cachedPseudoStyles->at(i).get();
        if (pseudoStyle->styleType() == pid) {
            m_cachedPseudoStyles->remove(i);
            return;
        }
    }
}

bool RenderStyle::inheritedNotEqual(const RenderStyle* other) const
{
    return inherited_flags != other->inherited_flags
           || inherited != other->inherited
           || m_svgStyle->inheritedNotEqual(other->m_svgStyle.get())
           || rareInheritedData != other->rareInheritedData;
}

bool RenderStyle::inheritedDataShared(const RenderStyle* other) const
{
    // This is a fast check that only looks if the data structures are shared.
    return inherited_flags == other->inherited_flags
        && inherited.get() == other->inherited.get()
        && m_svgStyle.get() == other->m_svgStyle.get()
        && rareInheritedData.get() == other->rareInheritedData.get();
}

static bool positionedObjectMovedOnly(const LengthBox& a, const LengthBox& b, const Length& width)
{
    // If any unit types are different, then we can't guarantee
    // that this was just a movement.
    if (a.left().type() != b.left().type()
        || a.right().type() != b.right().type()
        || a.top().type() != b.top().type()
        || a.bottom().type() != b.bottom().type())
        return false;

    // Only one unit can be non-auto in the horizontal direction and
    // in the vertical direction.  Otherwise the adjustment of values
    // is changing the size of the box.
    if (!a.left().isIntrinsicOrAuto() && !a.right().isIntrinsicOrAuto())
        return false;
    if (!a.top().isIntrinsicOrAuto() && !a.bottom().isIntrinsicOrAuto())
        return false;
    // If our width is auto and left or right is specified and changed then this
    // is not just a movement - we need to resize to our container.
    if (width.isIntrinsicOrAuto()
        && ((!a.left().isIntrinsicOrAuto() && a.left() != b.left())
            || (!a.right().isIntrinsicOrAuto() && a.right() != b.right())))
        return false;

    // One of the units is fixed or percent in both directions and stayed
    // that way in the new style.  Therefore all we are doing is moving.
    return true;
}

StyleDifference RenderStyle::visualInvalidationDiff(const RenderStyle& other, unsigned& changedContextSensitiveProperties) const
{
    // Note, we use .get() on each DataRef below because DataRef::operator== will do a deep
    // compare, which is duplicate work when we're going to compare each property inside
    // this function anyway.

    StyleDifference diff;
    if (m_svgStyle.get() != other.m_svgStyle.get())
        diff = m_svgStyle->diff(other.m_svgStyle.get());

    if ((!diff.needsFullLayout() || !diff.needsRepaint()) && diffNeedsFullLayoutAndRepaint(other)) {
        diff.setNeedsFullLayout();
        diff.setNeedsRepaintObject();
    }

    if (!diff.needsFullLayout() && diffNeedsFullLayout(other))
        diff.setNeedsFullLayout();

    if (!diff.needsFullLayout() && position() != StaticPosition && surround->offset != other.surround->offset) {
        // Optimize for the case where a positioned layer is moving but not changing size.
        if ((position() == AbsolutePosition || position() == FixedPosition)
            && positionedObjectMovedOnly(surround->offset, other.surround->offset, m_box->width())) {
            diff.setNeedsPositionedMovementLayout();
        } else {
            // FIXME: We would like to use SimplifiedLayout for relative positioning, but we can't quite do that yet.
            // We need to make sure SimplifiedLayout can operate correctly on RenderInlines (we will need
            // to add a selfNeedsSimplifiedLayout bit in order to not get confused and taint every line).
            diff.setNeedsFullLayout();
        }
    }

    if (diffNeedsRepaintLayer(other))
        diff.setNeedsRepaintLayer();
    else if (diffNeedsRepaintObject(other))
        diff.setNeedsRepaintObject();

    changedContextSensitiveProperties = computeChangedContextSensitiveProperties(other, diff);

    if (diff.hasNoChange() && diffNeedsRecompositeLayer(other))
        diff.setNeedsRecompositeLayer();

    // Cursors are not checked, since they will be set appropriately in response to mouse events,
    // so they don't need to cause any repaint or layout.

    // Animations don't need to be checked either. We always set the new style on the RenderObject, so we will get a chance to fire off
    // the resulting transition properly.

    return diff;
}

bool RenderStyle::diffNeedsFullLayoutAndRepaint(const RenderStyle& other) const
{
    // FIXME: Not all cases in this method need both full layout and repaint.
    // Should move cases into diffNeedsFullLayout() if
    // - don't need repaint at all;
    // - or the renderer knows how to exactly repaint caused by the layout change
    //   instead of forced full repaint.

    if (m_box.get() != other.m_box.get()) {
        if (m_box->width() != other.m_box->width()
            || m_box->minWidth() != other.m_box->minWidth()
            || m_box->maxWidth() != other.m_box->maxWidth()
            || m_box->height() != other.m_box->height()
            || m_box->minHeight() != other.m_box->minHeight()
            || m_box->maxHeight() != other.m_box->maxHeight())
            return true;

        if (m_box->verticalAlign() != other.m_box->verticalAlign())
            return true;

        if (m_box->boxSizing() != other.m_box->boxSizing())
            return true;
    }

    if (surround.get() != other.surround.get()) {
        if (surround->margin != other.surround->margin)
            return true;

        if (surround->padding != other.surround->padding)
            return true;

        // If our border widths change, then we need to layout. Other changes to borders only necessitate a repaint.
        if (borderLeftWidth() != other.borderLeftWidth()
            || borderTopWidth() != other.borderTopWidth()
            || borderBottomWidth() != other.borderBottomWidth()
            || borderRightWidth() != other.borderRightWidth())
            return true;
    }

    if (rareNonInheritedData.get() != other.rareNonInheritedData.get()) {
        if (rareNonInheritedData->m_appearance != other.rareNonInheritedData->m_appearance
            || rareNonInheritedData->marginBeforeCollapse != other.rareNonInheritedData->marginBeforeCollapse
            || rareNonInheritedData->marginAfterCollapse != other.rareNonInheritedData->marginAfterCollapse
            || rareNonInheritedData->lineClamp != other.rareNonInheritedData->lineClamp
            || rareNonInheritedData->textOverflow != other.rareNonInheritedData->textOverflow
            || rareNonInheritedData->m_wrapFlow != other.rareNonInheritedData->m_wrapFlow
            || rareNonInheritedData->m_wrapThrough != other.rareNonInheritedData->m_wrapThrough
            || rareNonInheritedData->m_shapeMargin != other.rareNonInheritedData->m_shapeMargin
            || rareNonInheritedData->m_order != other.rareNonInheritedData->m_order
            || rareNonInheritedData->m_alignContent != other.rareNonInheritedData->m_alignContent
            || rareNonInheritedData->m_alignItems != other.rareNonInheritedData->m_alignItems
            || rareNonInheritedData->m_alignSelf != other.rareNonInheritedData->m_alignSelf
            || rareNonInheritedData->m_justifyContent != other.rareNonInheritedData->m_justifyContent
            || rareNonInheritedData->m_grid.get() != other.rareNonInheritedData->m_grid.get()
            || rareNonInheritedData->m_gridItem.get() != other.rareNonInheritedData->m_gridItem.get()
            || rareNonInheritedData->m_textCombine != other.rareNonInheritedData->m_textCombine
            || rareNonInheritedData->hasFilters() != other.rareNonInheritedData->hasFilters())
            return true;

        if (rareNonInheritedData->m_deprecatedFlexibleBox.get() != other.rareNonInheritedData->m_deprecatedFlexibleBox.get()
            && *rareNonInheritedData->m_deprecatedFlexibleBox.get() != *other.rareNonInheritedData->m_deprecatedFlexibleBox.get())
            return true;

        if (rareNonInheritedData->m_flexibleBox.get() != other.rareNonInheritedData->m_flexibleBox.get()
            && *rareNonInheritedData->m_flexibleBox.get() != *other.rareNonInheritedData->m_flexibleBox.get())
            return true;

        // FIXME: We should add an optimized form of layout that just recomputes visual overflow.
        if (!rareNonInheritedData->shadowDataEquivalent(*other.rareNonInheritedData.get()))
            return true;

        if (!rareNonInheritedData->reflectionDataEquivalent(*other.rareNonInheritedData.get()))
            return true;

        if (rareNonInheritedData->m_multiCol.get() != other.rareNonInheritedData->m_multiCol.get()
            && *rareNonInheritedData->m_multiCol.get() != *other.rareNonInheritedData->m_multiCol.get())
            return true;

        // If the counter directives change, trigger a relayout to re-calculate counter values and rebuild the counter node tree.
        const CounterDirectiveMap* mapA = rareNonInheritedData->m_counterDirectives.get();
        const CounterDirectiveMap* mapB = other.rareNonInheritedData->m_counterDirectives.get();
        if (!(mapA == mapB || (mapA && mapB && *mapA == *mapB)))
            return true;

        // We only need do layout for opacity changes if adding or losing opacity could trigger a change
        // in us being a stacking context.
        if (hasAutoZIndex() != other.hasAutoZIndex() && rareNonInheritedData->hasOpacity() != other.rareNonInheritedData->hasOpacity()) {
            // FIXME: We would like to use SimplifiedLayout here, but we can't quite do that yet.
            // We need to make sure SimplifiedLayout can operate correctly on RenderInlines (we will need
            // to add a selfNeedsSimplifiedLayout bit in order to not get confused and taint every line).
            // In addition we need to solve the floating object issue when layers come and go. Right now
            // a full layout is necessary to keep floating object lists sane.
            return true;
        }
    }

    if (rareInheritedData.get() != other.rareInheritedData.get()) {
        if (rareInheritedData->highlight != other.rareInheritedData->highlight
            || rareInheritedData->indent != other.rareInheritedData->indent
            || rareInheritedData->m_textAlignLast != other.rareInheritedData->m_textAlignLast
            || rareInheritedData->m_textIndentLine != other.rareInheritedData->m_textIndentLine
            || rareInheritedData->m_effectiveZoom != other.rareInheritedData->m_effectiveZoom
            || rareInheritedData->wordBreak != other.rareInheritedData->wordBreak
            || rareInheritedData->overflowWrap != other.rareInheritedData->overflowWrap
            || rareInheritedData->lineBreak != other.rareInheritedData->lineBreak
            || rareInheritedData->textSecurity != other.rareInheritedData->textSecurity
            || rareInheritedData->hyphens != other.rareInheritedData->hyphens
            || rareInheritedData->hyphenationLimitBefore != other.rareInheritedData->hyphenationLimitBefore
            || rareInheritedData->hyphenationLimitAfter != other.rareInheritedData->hyphenationLimitAfter
            || rareInheritedData->hyphenationString != other.rareInheritedData->hyphenationString
            || rareInheritedData->locale != other.rareInheritedData->locale
            || rareInheritedData->m_rubyPosition != other.rareInheritedData->m_rubyPosition
            || rareInheritedData->textEmphasisMark != other.rareInheritedData->textEmphasisMark
            || rareInheritedData->textEmphasisPosition != other.rareInheritedData->textEmphasisPosition
            || rareInheritedData->textEmphasisCustomMark != other.rareInheritedData->textEmphasisCustomMark
            || rareInheritedData->m_textJustify != other.rareInheritedData->m_textJustify
            || rareInheritedData->m_textOrientation != other.rareInheritedData->m_textOrientation
            || rareInheritedData->m_tabSize != other.rareInheritedData->m_tabSize
            || rareInheritedData->m_lineBoxContain != other.rareInheritedData->m_lineBoxContain
            || rareInheritedData->listStyleImage != other.rareInheritedData->listStyleImage
            || rareInheritedData->textStrokeWidth != other.rareInheritedData->textStrokeWidth)
            return true;

        if (!rareInheritedData->shadowDataEquivalent(*other.rareInheritedData.get()))
            return true;

        if (!rareInheritedData->quotesDataEquivalent(*other.rareInheritedData.get()))
            return true;
    }

    if (inherited->textAutosizingMultiplier != other.inherited->textAutosizingMultiplier)
        return true;

    if (inherited.get() != other.inherited.get()) {
        if (inherited->line_height != other.inherited->line_height
            || inherited->font != other.inherited->font
            || inherited->horizontal_border_spacing != other.inherited->horizontal_border_spacing
            || inherited->vertical_border_spacing != other.inherited->vertical_border_spacing)
            return true;
    }

    if (inherited_flags._box_direction != other.inherited_flags._box_direction
        || inherited_flags.m_rtlOrdering != other.inherited_flags.m_rtlOrdering
        || inherited_flags._text_align != other.inherited_flags._text_align
        || inherited_flags._text_transform != other.inherited_flags._text_transform
        || inherited_flags._direction != other.inherited_flags._direction
        || inherited_flags._white_space != other.inherited_flags._white_space
        || inherited_flags.m_writingMode != other.inherited_flags.m_writingMode)
        return true;

    if (noninherited_flags._overflowX != other.noninherited_flags._overflowX
        || noninherited_flags._overflowY != other.noninherited_flags._overflowY
        || noninherited_flags._clear != other.noninherited_flags._clear
        || noninherited_flags._unicodeBidi != other.noninherited_flags._unicodeBidi
        || noninherited_flags._position != other.noninherited_flags._position
        || noninherited_flags._floating != other.noninherited_flags._floating
        || noninherited_flags._originalDisplay != other.noninherited_flags._originalDisplay
        || noninherited_flags._vertical_align != other.noninherited_flags._vertical_align)
        return true;

    if (noninherited_flags._effectiveDisplay >= FIRST_TABLE_DISPLAY && noninherited_flags._effectiveDisplay <= LAST_TABLE_DISPLAY) {
        if (inherited_flags._border_collapse != other.inherited_flags._border_collapse
            || inherited_flags._empty_cells != other.inherited_flags._empty_cells
            || inherited_flags._caption_side != other.inherited_flags._caption_side
            || noninherited_flags._table_layout != other.noninherited_flags._table_layout)
            return true;

        // In the collapsing border model, 'hidden' suppresses other borders, while 'none'
        // does not, so these style differences can be width differences.
        if (inherited_flags._border_collapse
            && ((borderTopStyle() == BHIDDEN && other.borderTopStyle() == BNONE)
                || (borderTopStyle() == BNONE && other.borderTopStyle() == BHIDDEN)
                || (borderBottomStyle() == BHIDDEN && other.borderBottomStyle() == BNONE)
                || (borderBottomStyle() == BNONE && other.borderBottomStyle() == BHIDDEN)
                || (borderLeftStyle() == BHIDDEN && other.borderLeftStyle() == BNONE)
                || (borderLeftStyle() == BNONE && other.borderLeftStyle() == BHIDDEN)
                || (borderRightStyle() == BHIDDEN && other.borderRightStyle() == BNONE)
                || (borderRightStyle() == BNONE && other.borderRightStyle() == BHIDDEN)))
            return true;
    } else if (noninherited_flags._effectiveDisplay == LIST_ITEM) {
        if (inherited_flags._list_style_type != other.inherited_flags._list_style_type
            || inherited_flags._list_style_position != other.inherited_flags._list_style_position)
            return true;
    }

    if ((visibility() == COLLAPSE) != (other.visibility() == COLLAPSE))
        return true;

    if (!m_background->outline().visuallyEqual(other.m_background->outline())) {
        // FIXME: We only really need to recompute the overflow but we don't have an optimized layout for it.
        return true;
    }

    // Movement of non-static-positioned object is special cased in RenderStyle::visualInvalidationDiff().

    return false;
}

bool RenderStyle::diffNeedsFullLayout(const RenderStyle& other) const
{
    return false;
}

bool RenderStyle::diffNeedsRepaintLayer(const RenderStyle& other) const
{
    if (position() != StaticPosition && (visual->clip != other.visual->clip || visual->hasClip != other.visual->hasClip))
        return true;

    if (rareNonInheritedData.get() != other.rareNonInheritedData.get()) {
        if (RuntimeEnabledFeatures::cssCompositingEnabled()
            && (rareNonInheritedData->m_effectiveBlendMode != other.rareNonInheritedData->m_effectiveBlendMode
                || rareNonInheritedData->m_isolation != other.rareNonInheritedData->m_isolation))
            return true;

        if (rareNonInheritedData->m_mask != other.rareNonInheritedData->m_mask
            || rareNonInheritedData->m_maskBoxImage != other.rareNonInheritedData->m_maskBoxImage)
            return true;
    }

    return false;
}

bool RenderStyle::diffNeedsRepaintObject(const RenderStyle& other) const
{
    if (inherited_flags._visibility != other.inherited_flags._visibility
        || inherited_flags.m_printColorAdjust != other.inherited_flags.m_printColorAdjust
        || inherited_flags._insideLink != other.inherited_flags._insideLink
        || !surround->border.visuallyEqual(other.surround->border)
        || !m_background->visuallyEqual(*other.m_background))
        return true;

    if (rareInheritedData.get() != other.rareInheritedData.get()) {
        if (rareInheritedData->userModify != other.rareInheritedData->userModify
            || rareInheritedData->userSelect != other.rareInheritedData->userSelect
            || rareInheritedData->m_imageRendering != other.rareInheritedData->m_imageRendering)
            return true;
    }

    if (rareNonInheritedData.get() != other.rareNonInheritedData.get()) {
        if (rareNonInheritedData->userDrag != other.rareNonInheritedData->userDrag
            || rareNonInheritedData->m_borderFit != other.rareNonInheritedData->m_borderFit
            || rareNonInheritedData->m_objectFit != other.rareNonInheritedData->m_objectFit
            || rareNonInheritedData->m_objectPosition != other.rareNonInheritedData->m_objectPosition
            || rareNonInheritedData->m_shapeOutside != other.rareNonInheritedData->m_shapeOutside
            || rareNonInheritedData->m_clipPath != other.rareNonInheritedData->m_clipPath)
            return true;
    }

    return false;
}

bool RenderStyle::diffNeedsRecompositeLayer(const RenderStyle& other) const
{
    if (rareNonInheritedData.get() != other.rareNonInheritedData.get()) {
        if (rareNonInheritedData->m_transformStyle3D != other.rareNonInheritedData->m_transformStyle3D
            || rareNonInheritedData->m_backfaceVisibility != other.rareNonInheritedData->m_backfaceVisibility
            || rareNonInheritedData->m_perspective != other.rareNonInheritedData->m_perspective
            || rareNonInheritedData->m_perspectiveOriginX != other.rareNonInheritedData->m_perspectiveOriginX
            || rareNonInheritedData->m_perspectiveOriginY != other.rareNonInheritedData->m_perspectiveOriginY
            || hasWillChangeCompositingHint() != other.hasWillChangeCompositingHint())
            return true;
    }

    return false;
}

unsigned RenderStyle::computeChangedContextSensitiveProperties(const RenderStyle& other, StyleDifference diff) const
{
    unsigned changedContextSensitiveProperties = ContextSensitivePropertyNone;

    // StyleAdjuster has ensured that zIndex is non-auto only if it's applicable.
    if (m_box->zIndex() != other.m_box->zIndex() || m_box->hasAutoZIndex() != other.m_box->hasAutoZIndex())
        changedContextSensitiveProperties |= ContextSensitivePropertyZIndex;

    if (rareNonInheritedData.get() != other.rareNonInheritedData.get()) {
        if (!transformDataEquivalent(other))
            changedContextSensitiveProperties |= ContextSensitivePropertyTransform;

        if (rareNonInheritedData->opacity != other.rareNonInheritedData->opacity)
            changedContextSensitiveProperties |= ContextSensitivePropertyOpacity;

        if (rareNonInheritedData->m_filter != other.rareNonInheritedData->m_filter)
            changedContextSensitiveProperties |= ContextSensitivePropertyFilter;
    }

    if (!diff.needsRepaint()) {
        if (inherited->color != other.inherited->color
            || inherited_flags.m_textUnderline != other.inherited_flags.m_textUnderline
            || visual->textDecoration != other.visual->textDecoration) {
            changedContextSensitiveProperties |= ContextSensitivePropertyTextOrColor;
        } else if (rareNonInheritedData.get() != other.rareNonInheritedData.get()) {
            if (rareNonInheritedData->m_textDecorationStyle != other.rareNonInheritedData->m_textDecorationStyle
                || rareNonInheritedData->m_textDecorationColor != other.rareNonInheritedData->m_textDecorationColor)
                changedContextSensitiveProperties |= ContextSensitivePropertyTextOrColor;
        } else if (rareInheritedData.get() != other.rareInheritedData.get()) {
            if (rareInheritedData->textFillColor() != other.rareInheritedData->textFillColor()
                || rareInheritedData->textStrokeColor() != other.rareInheritedData->textStrokeColor()
                || rareInheritedData->textEmphasisColor() != other.rareInheritedData->textEmphasisColor()
                || rareInheritedData->textEmphasisFill != other.rareInheritedData->textEmphasisFill
                || rareInheritedData->appliedTextDecorations != other.rareInheritedData->appliedTextDecorations)
                changedContextSensitiveProperties |= ContextSensitivePropertyTextOrColor;
        }
    }

    return changedContextSensitiveProperties;
}

void RenderStyle::setClip(const Length& top, const Length& right, const Length& bottom, const Length& left)
{
    StyleVisualData* data = visual.access();
    data->clip.m_top = top;
    data->clip.m_right = right;
    data->clip.m_bottom = bottom;
    data->clip.m_left = left;
}

void RenderStyle::addCursor(PassRefPtr<StyleImage> image, const IntPoint& hotSpot)
{
    if (!rareInheritedData.access()->cursorData)
        rareInheritedData.access()->cursorData = CursorList::create();
    rareInheritedData.access()->cursorData->append(CursorData(image, hotSpot));
}

void RenderStyle::setCursorList(PassRefPtr<CursorList> other)
{
    rareInheritedData.access()->cursorData = other;
}

void RenderStyle::setQuotes(PassRefPtr<QuotesData> q)
{
    rareInheritedData.access()->quotes = q;
}

void RenderStyle::clearCursorList()
{
    if (rareInheritedData->cursorData)
        rareInheritedData.access()->cursorData = nullptr;
}

void RenderStyle::addCallbackSelector(const String& selector)
{
    if (!rareNonInheritedData->m_callbackSelectors.contains(selector))
        rareNonInheritedData.access()->m_callbackSelectors.append(selector);
}

void RenderStyle::clearContent()
{
    if (rareNonInheritedData->m_content)
        rareNonInheritedData.access()->m_content = nullptr;
}

void RenderStyle::appendContent(PassOwnPtr<ContentData> contentData)
{
    OwnPtr<ContentData>& content = rareNonInheritedData.access()->m_content;
    ContentData* lastContent = content.get();
    while (lastContent && lastContent->next())
        lastContent = lastContent->next();

    if (lastContent)
        lastContent->setNext(contentData);
    else
        content = contentData;
}

void RenderStyle::setContent(PassRefPtr<StyleImage> image, bool add)
{
    if (!image)
        return;

    if (add) {
        appendContent(ContentData::create(image));
        return;
    }

    rareNonInheritedData.access()->m_content = ContentData::create(image);
}

void RenderStyle::setContent(const String& string, bool add)
{
    OwnPtr<ContentData>& content = rareNonInheritedData.access()->m_content;
    if (add) {
        ContentData* lastContent = content.get();
        while (lastContent && lastContent->next())
            lastContent = lastContent->next();

        if (lastContent) {
            // We attempt to merge with the last ContentData if possible.
            if (lastContent->isText()) {
                TextContentData* textContent = static_cast<TextContentData*>(lastContent);
                textContent->setText(textContent->text() + string);
            } else
                lastContent->setNext(ContentData::create(string));

            return;
        }
    }

    content = ContentData::create(string);
}

void RenderStyle::setContent(PassOwnPtr<CounterContent> counter, bool add)
{
    if (!counter)
        return;

    if (add) {
        appendContent(ContentData::create(counter));
        return;
    }

    rareNonInheritedData.access()->m_content = ContentData::create(counter);
}

void RenderStyle::setContent(QuoteType quote, bool add)
{
    if (add) {
        appendContent(ContentData::create(quote));
        return;
    }

    rareNonInheritedData.access()->m_content = ContentData::create(quote);
}

blink::WebBlendMode RenderStyle::blendMode() const
{
    if (RuntimeEnabledFeatures::cssCompositingEnabled())
        return static_cast<blink::WebBlendMode>(rareNonInheritedData->m_effectiveBlendMode);
    return blink::WebBlendModeNormal;
}

void RenderStyle::setBlendMode(blink::WebBlendMode v)
{
    if (RuntimeEnabledFeatures::cssCompositingEnabled())
        rareNonInheritedData.access()->m_effectiveBlendMode = v;
}

bool RenderStyle::hasBlendMode() const
{
    if (RuntimeEnabledFeatures::cssCompositingEnabled())
        return static_cast<blink::WebBlendMode>(rareNonInheritedData->m_effectiveBlendMode) != blink::WebBlendModeNormal;
    return false;
}

EIsolation RenderStyle::isolation() const
{
    if (RuntimeEnabledFeatures::cssCompositingEnabled())
        return static_cast<EIsolation>(rareNonInheritedData->m_isolation);
    return IsolationAuto;
}

void RenderStyle::setIsolation(EIsolation v)
{
    if (RuntimeEnabledFeatures::cssCompositingEnabled())
        rareNonInheritedData.access()->m_isolation = v;
}

bool RenderStyle::hasIsolation() const
{
    if (RuntimeEnabledFeatures::cssCompositingEnabled())
        return rareNonInheritedData->m_isolation != IsolationAuto;
    return false;
}

bool RenderStyle::hasWillChangeCompositingHint() const
{
    for (size_t i = 0; i < rareNonInheritedData->m_willChange->m_properties.size(); ++i) {
        switch (rareNonInheritedData->m_willChange->m_properties[i]) {
        case CSSPropertyOpacity:
        case CSSPropertyTransform:
        case CSSPropertyWebkitTransform:
        case CSSPropertyTop:
        case CSSPropertyLeft:
        case CSSPropertyBottom:
        case CSSPropertyRight:
            return true;
        default:
            break;
        }
    }
    return false;
}

inline bool requireTransformOrigin(const Vector<RefPtr<TransformOperation> >& transformOperations, RenderStyle::ApplyTransformOrigin applyOrigin)
{
    // transform-origin brackets the transform with translate operations.
    // Optimize for the case where the only transform is a translation, since the transform-origin is irrelevant
    // in that case.
    if (applyOrigin != RenderStyle::IncludeTransformOrigin)
        return false;

    unsigned size = transformOperations.size();
    for (unsigned i = 0; i < size; ++i) {
        TransformOperation::OperationType type = transformOperations[i]->type();
        if (type != TransformOperation::TranslateX
            && type != TransformOperation::TranslateY
            && type != TransformOperation::Translate
            && type != TransformOperation::TranslateZ
            && type != TransformOperation::Translate3D)
            return true;
    }

    return false;
}

void RenderStyle::applyTransform(TransformationMatrix& transform, const LayoutSize& borderBoxSize, ApplyTransformOrigin applyOrigin) const
{
    applyTransform(transform, FloatRect(FloatPoint(), borderBoxSize), applyOrigin);
}

void RenderStyle::applyTransform(TransformationMatrix& transform, const FloatRect& boundingBox, ApplyTransformOrigin applyOrigin) const
{
    const Vector<RefPtr<TransformOperation> >& transformOperations = rareNonInheritedData->m_transform->m_operations.operations();
    bool applyTransformOrigin = requireTransformOrigin(transformOperations, applyOrigin);

    float offsetX = transformOriginX().type() == Percent ? boundingBox.x() : 0;
    float offsetY = transformOriginY().type() == Percent ? boundingBox.y() : 0;

    if (applyTransformOrigin) {
        transform.translate3d(floatValueForLength(transformOriginX(), boundingBox.width()) + offsetX,
            floatValueForLength(transformOriginY(), boundingBox.height()) + offsetY,
            transformOriginZ());
    }

    unsigned size = transformOperations.size();
    for (unsigned i = 0; i < size; ++i)
        transformOperations[i]->apply(transform, boundingBox.size());

    if (applyTransformOrigin) {
        transform.translate3d(-floatValueForLength(transformOriginX(), boundingBox.width()) - offsetX,
            -floatValueForLength(transformOriginY(), boundingBox.height()) - offsetY,
            -transformOriginZ());
    }
}

void RenderStyle::setTextShadow(PassRefPtr<ShadowList> s)
{
    rareInheritedData.access()->textShadow = s;
}

void RenderStyle::setBoxShadow(PassRefPtr<ShadowList> s)
{
    rareNonInheritedData.access()->m_boxShadow = s;
}

static RoundedRect::Radii calcRadiiFor(const BorderData& border, IntSize size)
{
    return RoundedRect::Radii(
        IntSize(valueForLength(border.topLeft().width(), size.width()),
            valueForLength(border.topLeft().height(), size.height())),
        IntSize(valueForLength(border.topRight().width(), size.width()),
            valueForLength(border.topRight().height(), size.height())),
        IntSize(valueForLength(border.bottomLeft().width(), size.width()),
            valueForLength(border.bottomLeft().height(), size.height())),
        IntSize(valueForLength(border.bottomRight().width(), size.width()),
            valueForLength(border.bottomRight().height(), size.height())));
}

StyleImage* RenderStyle::listStyleImage() const { return rareInheritedData->listStyleImage.get(); }
void RenderStyle::setListStyleImage(PassRefPtr<StyleImage> v)
{
    if (rareInheritedData->listStyleImage != v)
        rareInheritedData.access()->listStyleImage = v;
}

Color RenderStyle::color() const { return inherited->color; }
Color RenderStyle::visitedLinkColor() const { return inherited->visitedLinkColor; }
void RenderStyle::setColor(const Color& v) { SET_VAR(inherited, color, v); }
void RenderStyle::setVisitedLinkColor(const Color& v) { SET_VAR(inherited, visitedLinkColor, v); }

short RenderStyle::horizontalBorderSpacing() const { return inherited->horizontal_border_spacing; }
short RenderStyle::verticalBorderSpacing() const { return inherited->vertical_border_spacing; }
void RenderStyle::setHorizontalBorderSpacing(short v) { SET_VAR(inherited, horizontal_border_spacing, v); }
void RenderStyle::setVerticalBorderSpacing(short v) { SET_VAR(inherited, vertical_border_spacing, v); }

RoundedRect RenderStyle::getRoundedBorderFor(const LayoutRect& borderRect, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    IntRect snappedBorderRect(pixelSnappedIntRect(borderRect));
    RoundedRect roundedRect(snappedBorderRect);
    if (hasBorderRadius()) {
        RoundedRect::Radii radii = calcRadiiFor(surround->border, snappedBorderRect.size());
        radii.scale(calcBorderRadiiConstraintScaleFor(borderRect, radii));
        roundedRect.includeLogicalEdges(radii, isHorizontalWritingMode(), includeLogicalLeftEdge, includeLogicalRightEdge);
    }
    return roundedRect;
}

RoundedRect RenderStyle::getRoundedInnerBorderFor(const LayoutRect& borderRect, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    bool horizontal = isHorizontalWritingMode();

    int leftWidth = (!horizontal || includeLogicalLeftEdge) ? borderLeftWidth() : 0;
    int rightWidth = (!horizontal || includeLogicalRightEdge) ? borderRightWidth() : 0;
    int topWidth = (horizontal || includeLogicalLeftEdge) ? borderTopWidth() : 0;
    int bottomWidth = (horizontal || includeLogicalRightEdge) ? borderBottomWidth() : 0;

    return getRoundedInnerBorderFor(borderRect, topWidth, bottomWidth, leftWidth, rightWidth, includeLogicalLeftEdge, includeLogicalRightEdge);
}

RoundedRect RenderStyle::getRoundedInnerBorderFor(const LayoutRect& borderRect,
    int topWidth, int bottomWidth, int leftWidth, int rightWidth, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    LayoutRect innerRect(borderRect.x() + leftWidth,
               borderRect.y() + topWidth,
               borderRect.width() - leftWidth - rightWidth,
               borderRect.height() - topWidth - bottomWidth);

    RoundedRect roundedRect(pixelSnappedIntRect(innerRect));

    if (hasBorderRadius()) {
        RoundedRect::Radii radii = getRoundedBorderFor(borderRect).radii();
        radii.shrink(topWidth, bottomWidth, leftWidth, rightWidth);
        roundedRect.includeLogicalEdges(radii, isHorizontalWritingMode(), includeLogicalLeftEdge, includeLogicalRightEdge);
    }
    return roundedRect;
}

static bool allLayersAreFixed(const FillLayer* layer)
{
    bool allFixed = true;

    for (const FillLayer* currLayer = layer; currLayer; currLayer = currLayer->next())
        allFixed &= (currLayer->image() && currLayer->attachment() == FixedBackgroundAttachment);

    return layer && allFixed;
}

bool RenderStyle::hasEntirelyFixedBackground() const
{
    return allLayersAreFixed(backgroundLayers());
}

const CounterDirectiveMap* RenderStyle::counterDirectives() const
{
    return rareNonInheritedData->m_counterDirectives.get();
}

CounterDirectiveMap& RenderStyle::accessCounterDirectives()
{
    OwnPtr<CounterDirectiveMap>& map = rareNonInheritedData.access()->m_counterDirectives;
    if (!map)
        map = adoptPtr(new CounterDirectiveMap);
    return *map;
}

const CounterDirectives RenderStyle::getCounterDirectives(const AtomicString& identifier) const
{
    if (const CounterDirectiveMap* directives = counterDirectives())
        return directives->get(identifier);
    return CounterDirectives();
}

const AtomicString& RenderStyle::hyphenString() const
{
    const AtomicString& hyphenationString = rareInheritedData.get()->hyphenationString;
    if (!hyphenationString.isNull())
        return hyphenationString;

    // FIXME: This should depend on locale.
    DEFINE_STATIC_LOCAL(AtomicString, hyphenMinusString, (&hyphenMinus, 1));
    DEFINE_STATIC_LOCAL(AtomicString, hyphenString, (&hyphen, 1));
    return font().primaryFontHasGlyphForCharacter(hyphen) ? hyphenString : hyphenMinusString;
}

const AtomicString& RenderStyle::textEmphasisMarkString() const
{
    switch (textEmphasisMark()) {
    case TextEmphasisMarkNone:
        return nullAtom;
    case TextEmphasisMarkCustom:
        return textEmphasisCustomMark();
    case TextEmphasisMarkDot: {
        DEFINE_STATIC_LOCAL(AtomicString, filledDotString, (&bullet, 1));
        DEFINE_STATIC_LOCAL(AtomicString, openDotString, (&whiteBullet, 1));
        return textEmphasisFill() == TextEmphasisFillFilled ? filledDotString : openDotString;
    }
    case TextEmphasisMarkCircle: {
        DEFINE_STATIC_LOCAL(AtomicString, filledCircleString, (&blackCircle, 1));
        DEFINE_STATIC_LOCAL(AtomicString, openCircleString, (&whiteCircle, 1));
        return textEmphasisFill() == TextEmphasisFillFilled ? filledCircleString : openCircleString;
    }
    case TextEmphasisMarkDoubleCircle: {
        DEFINE_STATIC_LOCAL(AtomicString, filledDoubleCircleString, (&fisheye, 1));
        DEFINE_STATIC_LOCAL(AtomicString, openDoubleCircleString, (&bullseye, 1));
        return textEmphasisFill() == TextEmphasisFillFilled ? filledDoubleCircleString : openDoubleCircleString;
    }
    case TextEmphasisMarkTriangle: {
        DEFINE_STATIC_LOCAL(AtomicString, filledTriangleString, (&blackUpPointingTriangle, 1));
        DEFINE_STATIC_LOCAL(AtomicString, openTriangleString, (&whiteUpPointingTriangle, 1));
        return textEmphasisFill() == TextEmphasisFillFilled ? filledTriangleString : openTriangleString;
    }
    case TextEmphasisMarkSesame: {
        DEFINE_STATIC_LOCAL(AtomicString, filledSesameString, (&sesameDot, 1));
        DEFINE_STATIC_LOCAL(AtomicString, openSesameString, (&whiteSesameDot, 1));
        return textEmphasisFill() == TextEmphasisFillFilled ? filledSesameString : openSesameString;
    }
    case TextEmphasisMarkAuto:
        ASSERT_NOT_REACHED();
        return nullAtom;
    }

    ASSERT_NOT_REACHED();
    return nullAtom;
}

CSSAnimationData& RenderStyle::accessAnimations()
{
    if (!rareNonInheritedData.access()->m_animations)
        rareNonInheritedData.access()->m_animations = CSSAnimationData::create();
    return *rareNonInheritedData->m_animations;
}

CSSTransitionData& RenderStyle::accessTransitions()
{
    if (!rareNonInheritedData.access()->m_transitions)
        rareNonInheritedData.access()->m_transitions = CSSTransitionData::create();
    return *rareNonInheritedData->m_transitions;
}

const Font& RenderStyle::font() const { return inherited->font; }
const FontMetrics& RenderStyle::fontMetrics() const { return inherited->font.fontMetrics(); }
const FontDescription& RenderStyle::fontDescription() const { return inherited->font.fontDescription(); }
float RenderStyle::specifiedFontSize() const { return fontDescription().specifiedSize(); }
float RenderStyle::computedFontSize() const { return fontDescription().computedSize(); }
int RenderStyle::fontSize() const { return fontDescription().computedPixelSize(); }
FontWeight RenderStyle::fontWeight() const { return fontDescription().weight(); }

TextDecoration RenderStyle::textDecorationsInEffect() const
{
    int decorations = 0;

    const Vector<AppliedTextDecoration>& applied = appliedTextDecorations();

    for (size_t i = 0; i < applied.size(); ++i)
        decorations |= applied[i].line();

    return static_cast<TextDecoration>(decorations);
}

const Vector<AppliedTextDecoration>& RenderStyle::appliedTextDecorations() const
{
    if (!inherited_flags.m_textUnderline && !rareInheritedData->appliedTextDecorations) {
        DEFINE_STATIC_LOCAL(Vector<AppliedTextDecoration>, empty, ());
        return empty;
    }
    if (inherited_flags.m_textUnderline) {
        DEFINE_STATIC_LOCAL(Vector<AppliedTextDecoration>, underline, (1, AppliedTextDecoration(TextDecorationUnderline)));
        return underline;
    }

    return rareInheritedData->appliedTextDecorations->vector();
}

float RenderStyle::wordSpacing() const { return fontDescription().wordSpacing(); }
float RenderStyle::letterSpacing() const { return fontDescription().letterSpacing(); }

bool RenderStyle::setFontDescription(const FontDescription& v)
{
    if (inherited->font.fontDescription() != v) {
        inherited.access()->font = Font(v);
        return true;
    }
    return false;
}

const Length& RenderStyle::specifiedLineHeight() const { return inherited->line_height; }
Length RenderStyle::lineHeight() const
{
    const Length& lh = inherited->line_height;
    // Unlike fontDescription().computedSize() and hence fontSize(), this is
    // recalculated on demand as we only store the specified line height.
    // FIXME: Should consider scaling the fixed part of any calc expressions
    // too, though this involves messily poking into CalcExpressionLength.
    float multiplier = textAutosizingMultiplier();
    if (multiplier > 1 && lh.isFixed())
        return Length(TextAutosizer::computeAutosizedFontSize(lh.value(), multiplier), Fixed);

    return lh;
}

void RenderStyle::setLineHeight(const Length& specifiedLineHeight) { SET_VAR(inherited, line_height, specifiedLineHeight); }

int RenderStyle::computedLineHeight() const
{
    const Length& lh = lineHeight();

    // Negative value means the line height is not set. Use the font's built-in spacing.
    if (lh.isNegative())
        return fontMetrics().lineSpacing();

    if (lh.isPercent())
        return minimumValueForLength(lh, fontSize());

    return lh.value();
}

void RenderStyle::setWordSpacing(float wordSpacing)
{
    FontSelector* currentFontSelector = font().fontSelector();
    FontDescription desc(fontDescription());
    desc.setWordSpacing(wordSpacing);
    setFontDescription(desc);
    font().update(currentFontSelector);
}

void RenderStyle::setLetterSpacing(float letterSpacing)
{
    FontSelector* currentFontSelector = font().fontSelector();
    FontDescription desc(fontDescription());
    desc.setLetterSpacing(letterSpacing);
    setFontDescription(desc);
    font().update(currentFontSelector);
}

void RenderStyle::setFontSize(float size)
{
    // size must be specifiedSize if Text Autosizing is enabled, but computedSize if text
    // zoom is enabled (if neither is enabled it's irrelevant as they're probably the same).

    ASSERT(std::isfinite(size));
    if (!std::isfinite(size) || size < 0)
        size = 0;
    else
        size = min(maximumAllowedFontSize, size);

    FontSelector* currentFontSelector = font().fontSelector();
    FontDescription desc(fontDescription());
    desc.setSpecifiedSize(size);
    desc.setComputedSize(size);

    float multiplier = textAutosizingMultiplier();
    if (multiplier > 1) {
        float autosizedFontSize = TextAutosizer::computeAutosizedFontSize(size, multiplier);
        desc.setComputedSize(min(maximumAllowedFontSize, autosizedFontSize));
    }

    setFontDescription(desc);
    font().update(currentFontSelector);
}

void RenderStyle::setFontWeight(FontWeight weight)
{
    FontSelector* currentFontSelector = font().fontSelector();
    FontDescription desc(fontDescription());
    desc.setWeight(weight);
    setFontDescription(desc);
    font().update(currentFontSelector);
}

void RenderStyle::addAppliedTextDecoration(const AppliedTextDecoration& decoration)
{
    RefPtr<AppliedTextDecorationList>& list = rareInheritedData.access()->appliedTextDecorations;

    if (!list)
        list = AppliedTextDecorationList::create();
    else if (!list->hasOneRef())
        list = list->copy();

    if (inherited_flags.m_textUnderline) {
        inherited_flags.m_textUnderline = false;
        list->append(AppliedTextDecoration(TextDecorationUnderline));
    }

    list->append(decoration);
}

void RenderStyle::applyTextDecorations()
{
    if (textDecoration() == TextDecorationNone)
        return;

    TextDecorationStyle style = textDecorationStyle();
    StyleColor styleColor = visitedDependentDecorationStyleColor();

    int decorations = textDecoration();

    if (decorations & TextDecorationUnderline) {
        // To save memory, we don't use AppliedTextDecoration objects in the
        // common case of a single simple underline.
        AppliedTextDecoration underline(TextDecorationUnderline, style, styleColor);

        if (!rareInheritedData->appliedTextDecorations && underline.isSimpleUnderline())
            inherited_flags.m_textUnderline = true;
        else
            addAppliedTextDecoration(underline);
    }
    if (decorations & TextDecorationOverline)
        addAppliedTextDecoration(AppliedTextDecoration(TextDecorationOverline, style, styleColor));
    if (decorations & TextDecorationLineThrough)
        addAppliedTextDecoration(AppliedTextDecoration(TextDecorationLineThrough, style, styleColor));
}

void RenderStyle::clearAppliedTextDecorations()
{
    inherited_flags.m_textUnderline = false;

    if (rareInheritedData->appliedTextDecorations)
        rareInheritedData.access()->appliedTextDecorations = nullptr;
}

void RenderStyle::getShadowExtent(const ShadowList* shadowList, LayoutUnit &top, LayoutUnit &right, LayoutUnit &bottom, LayoutUnit &left) const
{
    top = 0;
    right = 0;
    bottom = 0;
    left = 0;

    size_t shadowCount = shadowList ? shadowList->shadows().size() : 0;
    for (size_t i = 0; i < shadowCount; ++i) {
        const ShadowData& shadow = shadowList->shadows()[i];
        if (shadow.style() == Inset)
            continue;
        float blurAndSpread = shadow.blur() + shadow.spread();

        top = min<LayoutUnit>(top, shadow.y() - blurAndSpread);
        right = max<LayoutUnit>(right, shadow.x() + blurAndSpread);
        bottom = max<LayoutUnit>(bottom, shadow.y() + blurAndSpread);
        left = min<LayoutUnit>(left, shadow.x() - blurAndSpread);
    }
}

LayoutBoxExtent RenderStyle::getShadowInsetExtent(const ShadowList* shadowList) const
{
    LayoutUnit top = 0;
    LayoutUnit right = 0;
    LayoutUnit bottom = 0;
    LayoutUnit left = 0;

    size_t shadowCount = shadowList ? shadowList->shadows().size() : 0;
    for (size_t i = 0; i < shadowCount; ++i) {
        const ShadowData& shadow = shadowList->shadows()[i];
        if (shadow.style() == Normal)
            continue;
        float blurAndSpread = shadow.blur() + shadow.spread();
        top = max<LayoutUnit>(top, shadow.y() + blurAndSpread);
        right = min<LayoutUnit>(right, shadow.x() - blurAndSpread);
        bottom = min<LayoutUnit>(bottom, shadow.y() - blurAndSpread);
        left = max<LayoutUnit>(left, shadow.x() + blurAndSpread);
    }

    return LayoutBoxExtent(top, right, bottom, left);
}

void RenderStyle::getShadowHorizontalExtent(const ShadowList* shadowList, LayoutUnit &left, LayoutUnit &right) const
{
    left = 0;
    right = 0;

    size_t shadowCount = shadowList ? shadowList->shadows().size() : 0;
    for (size_t i = 0; i < shadowCount; ++i) {
        const ShadowData& shadow = shadowList->shadows()[i];
        if (shadow.style() == Inset)
            continue;
        float blurAndSpread = shadow.blur() + shadow.spread();

        left = min<LayoutUnit>(left, shadow.x() - blurAndSpread);
        right = max<LayoutUnit>(right, shadow.x() + blurAndSpread);
    }
}

void RenderStyle::getShadowVerticalExtent(const ShadowList* shadowList, LayoutUnit &top, LayoutUnit &bottom) const
{
    top = 0;
    bottom = 0;

    size_t shadowCount = shadowList ? shadowList->shadows().size() : 0;
    for (size_t i = 0; i < shadowCount; ++i) {
        const ShadowData& shadow = shadowList->shadows()[i];
        if (shadow.style() == Inset)
            continue;
        float blurAndSpread = shadow.blur() + shadow.spread();

        top = min<LayoutUnit>(top, shadow.y() - blurAndSpread);
        bottom = max<LayoutUnit>(bottom, shadow.y() + blurAndSpread);
    }
}

StyleColor RenderStyle::visitedDependentDecorationStyleColor() const
{
    bool isVisited = insideLink() == InsideVisitedLink;

    StyleColor styleColor = isVisited ? visitedLinkTextDecorationColor() : textDecorationColor();

    if (!styleColor.isCurrentColor())
        return styleColor;

    if (textStrokeWidth()) {
        // Prefer stroke color if possible, but not if it's fully transparent.
        StyleColor textStrokeStyleColor = isVisited ? visitedLinkTextStrokeColor() : textStrokeColor();
        if (!textStrokeStyleColor.isCurrentColor() && textStrokeStyleColor.color().alpha())
            return textStrokeStyleColor;
    }

    return isVisited ? visitedLinkTextFillColor() : textFillColor();
}

Color RenderStyle::visitedDependentDecorationColor() const
{
    bool isVisited = insideLink() == InsideVisitedLink;
    return visitedDependentDecorationStyleColor().resolve(isVisited ? visitedLinkColor() : color());
}

Color RenderStyle::colorIncludingFallback(int colorProperty, bool visitedLink) const
{
    StyleColor result(StyleColor::currentColor());
    EBorderStyle borderStyle = BNONE;
    switch (colorProperty) {
    case CSSPropertyBackgroundColor:
        result = visitedLink ? visitedLinkBackgroundColor() : backgroundColor();
        break;
    case CSSPropertyBorderLeftColor:
        result = visitedLink ? visitedLinkBorderLeftColor() : borderLeftColor();
        borderStyle = borderLeftStyle();
        break;
    case CSSPropertyBorderRightColor:
        result = visitedLink ? visitedLinkBorderRightColor() : borderRightColor();
        borderStyle = borderRightStyle();
        break;
    case CSSPropertyBorderTopColor:
        result = visitedLink ? visitedLinkBorderTopColor() : borderTopColor();
        borderStyle = borderTopStyle();
        break;
    case CSSPropertyBorderBottomColor:
        result = visitedLink ? visitedLinkBorderBottomColor() : borderBottomColor();
        borderStyle = borderBottomStyle();
        break;
    case CSSPropertyColor:
        result = visitedLink ? visitedLinkColor() : color();
        break;
    case CSSPropertyOutlineColor:
        result = visitedLink ? visitedLinkOutlineColor() : outlineColor();
        break;
    case CSSPropertyWebkitColumnRuleColor:
        result = visitedLink ? visitedLinkColumnRuleColor() : columnRuleColor();
        break;
    case CSSPropertyWebkitTextEmphasisColor:
        result = visitedLink ? visitedLinkTextEmphasisColor() : textEmphasisColor();
        break;
    case CSSPropertyWebkitTextFillColor:
        result = visitedLink ? visitedLinkTextFillColor() : textFillColor();
        break;
    case CSSPropertyWebkitTextStrokeColor:
        result = visitedLink ? visitedLinkTextStrokeColor() : textStrokeColor();
        break;
    case CSSPropertyFloodColor:
        result = floodColor();
        break;
    case CSSPropertyLightingColor:
        result = lightingColor();
        break;
    case CSSPropertyStopColor:
        result = stopColor();
        break;
    case CSSPropertyWebkitTapHighlightColor:
        result = tapHighlightColor();
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    if (!result.isCurrentColor())
        return result.color();

    // FIXME: Treating styled borders with initial color differently causes problems
    // See crbug.com/316559, crbug.com/276231
    if (!visitedLink && (borderStyle == INSET || borderStyle == OUTSET || borderStyle == RIDGE || borderStyle == GROOVE))
        return Color(238, 238, 238);
    return visitedLink ? visitedLinkColor() : color();
}

Color RenderStyle::visitedDependentColor(int colorProperty) const
{
    Color unvisitedColor = colorIncludingFallback(colorProperty, false);
    if (insideLink() != InsideVisitedLink)
        return unvisitedColor;

    Color visitedColor = colorIncludingFallback(colorProperty, true);

    // FIXME: Technically someone could explicitly specify the color transparent, but for now we'll just
    // assume that if the background color is transparent that it wasn't set. Note that it's weird that
    // we're returning unvisited info for a visited link, but given our restriction that the alpha values
    // have to match, it makes more sense to return the unvisited background color if specified than it
    // does to return black. This behavior matches what Firefox 4 does as well.
    if (colorProperty == CSSPropertyBackgroundColor && visitedColor == Color::transparent)
        return unvisitedColor;

    // Take the alpha from the unvisited color, but get the RGB values from the visited color.
    return Color(visitedColor.red(), visitedColor.green(), visitedColor.blue(), unvisitedColor.alpha());
}

const BorderValue& RenderStyle::borderBefore() const
{
    switch (writingMode()) {
    case TopToBottomWritingMode:
        return borderTop();
    case BottomToTopWritingMode:
        return borderBottom();
    case LeftToRightWritingMode:
        return borderLeft();
    case RightToLeftWritingMode:
        return borderRight();
    }
    ASSERT_NOT_REACHED();
    return borderTop();
}

const BorderValue& RenderStyle::borderAfter() const
{
    switch (writingMode()) {
    case TopToBottomWritingMode:
        return borderBottom();
    case BottomToTopWritingMode:
        return borderTop();
    case LeftToRightWritingMode:
        return borderRight();
    case RightToLeftWritingMode:
        return borderLeft();
    }
    ASSERT_NOT_REACHED();
    return borderBottom();
}

const BorderValue& RenderStyle::borderStart() const
{
    if (isHorizontalWritingMode())
        return isLeftToRightDirection() ? borderLeft() : borderRight();
    return isLeftToRightDirection() ? borderTop() : borderBottom();
}

const BorderValue& RenderStyle::borderEnd() const
{
    if (isHorizontalWritingMode())
        return isLeftToRightDirection() ? borderRight() : borderLeft();
    return isLeftToRightDirection() ? borderBottom() : borderTop();
}

unsigned short RenderStyle::borderBeforeWidth() const
{
    switch (writingMode()) {
    case TopToBottomWritingMode:
        return borderTopWidth();
    case BottomToTopWritingMode:
        return borderBottomWidth();
    case LeftToRightWritingMode:
        return borderLeftWidth();
    case RightToLeftWritingMode:
        return borderRightWidth();
    }
    ASSERT_NOT_REACHED();
    return borderTopWidth();
}

unsigned short RenderStyle::borderAfterWidth() const
{
    switch (writingMode()) {
    case TopToBottomWritingMode:
        return borderBottomWidth();
    case BottomToTopWritingMode:
        return borderTopWidth();
    case LeftToRightWritingMode:
        return borderRightWidth();
    case RightToLeftWritingMode:
        return borderLeftWidth();
    }
    ASSERT_NOT_REACHED();
    return borderBottomWidth();
}

unsigned short RenderStyle::borderStartWidth() const
{
    if (isHorizontalWritingMode())
        return isLeftToRightDirection() ? borderLeftWidth() : borderRightWidth();
    return isLeftToRightDirection() ? borderTopWidth() : borderBottomWidth();
}

unsigned short RenderStyle::borderEndWidth() const
{
    if (isHorizontalWritingMode())
        return isLeftToRightDirection() ? borderRightWidth() : borderLeftWidth();
    return isLeftToRightDirection() ? borderBottomWidth() : borderTopWidth();
}

void RenderStyle::setMarginStart(const Length& margin)
{
    if (isHorizontalWritingMode()) {
        if (isLeftToRightDirection())
            setMarginLeft(margin);
        else
            setMarginRight(margin);
    } else {
        if (isLeftToRightDirection())
            setMarginTop(margin);
        else
            setMarginBottom(margin);
    }
}

void RenderStyle::setMarginEnd(const Length& margin)
{
    if (isHorizontalWritingMode()) {
        if (isLeftToRightDirection())
            setMarginRight(margin);
        else
            setMarginLeft(margin);
    } else {
        if (isLeftToRightDirection())
            setMarginBottom(margin);
        else
            setMarginTop(margin);
    }
}

TextEmphasisMark RenderStyle::textEmphasisMark() const
{
    TextEmphasisMark mark = static_cast<TextEmphasisMark>(rareInheritedData->textEmphasisMark);
    if (mark != TextEmphasisMarkAuto)
        return mark;

    if (isHorizontalWritingMode())
        return TextEmphasisMarkDot;

    return TextEmphasisMarkSesame;
}

Color RenderStyle::initialTapHighlightColor()
{
    return RenderTheme::tapHighlightColor();
}

LayoutBoxExtent RenderStyle::imageOutsets(const NinePieceImage& image) const
{
    return LayoutBoxExtent(NinePieceImage::computeOutset(image.outset().top(), borderTopWidth()),
                           NinePieceImage::computeOutset(image.outset().right(), borderRightWidth()),
                           NinePieceImage::computeOutset(image.outset().bottom(), borderBottomWidth()),
                           NinePieceImage::computeOutset(image.outset().left(), borderLeftWidth()));
}

void RenderStyle::setBorderImageSource(PassRefPtr<StyleImage> image)
{
    if (surround->border.m_image.image() == image.get())
        return;
    surround.access()->border.m_image.setImage(image);
}

void RenderStyle::setBorderImageSlices(const LengthBox& slices)
{
    if (surround->border.m_image.imageSlices() == slices)
        return;
    surround.access()->border.m_image.setImageSlices(slices);
}

void RenderStyle::setBorderImageWidth(const BorderImageLengthBox& slices)
{
    if (surround->border.m_image.borderSlices() == slices)
        return;
    surround.access()->border.m_image.setBorderSlices(slices);
}

void RenderStyle::setBorderImageOutset(const BorderImageLengthBox& outset)
{
    if (surround->border.m_image.outset() == outset)
        return;
    surround.access()->border.m_image.setOutset(outset);
}

float calcBorderRadiiConstraintScaleFor(const FloatRect& rect, const FloatRoundedRect::Radii& radii)
{
    // Constrain corner radii using CSS3 rules:
    // http://www.w3.org/TR/css3-background/#the-border-radius

    float factor = 1;
    float radiiSum;

    // top
    radiiSum = radii.topLeft().width() + radii.topRight().width(); // Casts to avoid integer overflow.
    if (radiiSum > rect.width())
        factor = std::min(rect.width() / radiiSum, factor);

    // bottom
    radiiSum = radii.bottomLeft().width() + radii.bottomRight().width();
    if (radiiSum > rect.width())
        factor = std::min(rect.width() / radiiSum, factor);

    // left
    radiiSum = radii.topLeft().height() + radii.bottomLeft().height();
    if (radiiSum > rect.height())
        factor = std::min(rect.height() / radiiSum, factor);

    // right
    radiiSum = radii.topRight().height() + radii.bottomRight().height();
    if (radiiSum > rect.height())
        factor = std::min(rect.height() / radiiSum, factor);

    ASSERT(factor <= 1);
    return factor;
}

} // namespace WebCore

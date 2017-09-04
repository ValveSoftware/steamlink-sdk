/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
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

#include "core/style/ComputedStyle.h"

#include "core/animation/css/CSSAnimationData.h"
#include "core/animation/css/CSSTransitionData.h"
#include "core/css/CSSPaintValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPropertyEquality.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/TextAutosizer.h"
#include "core/style/AppliedTextDecoration.h"
#include "core/style/BorderEdge.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/style/ContentData.h"
#include "core/style/CursorData.h"
#include "core/style/DataEquivalency.h"
#include "core/style/QuotesData.h"
#include "core/style/ShadowList.h"
#include "core/style/StyleImage.h"
#include "core/style/StyleInheritedData.h"
#include "core/style/StyleInheritedVariables.h"
#include "core/style/StyleNonInheritedVariables.h"
#include "platform/LengthFunctions.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/FontSelector.h"
#include "platform/geometry/FloatRoundedRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/transforms/RotateTransformOperation.h"
#include "platform/transforms/ScaleTransformOperation.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include "wtf/MathExtras.h"
#include "wtf/PtrUtil.h"
#include "wtf/SaturatedArithmetic.h"
#include "wtf/SizeAssertions.h"
#include <algorithm>
#include <memory>

namespace blink {

struct SameSizeAsBorderValue {
  RGBA32 m_color;
  unsigned m_width;
};

ASSERT_SIZE(BorderValue, SameSizeAsBorderValue);

// Since different compilers/architectures pack ComputedStyle differently,
// re-create the same structure for an accurate size comparison.
struct SameSizeAsComputedStyle : public ComputedStyleBase,
                                 public RefCounted<ComputedStyle> {
  void* dataRefs[7];
  void* ownPtrs[1];
  void* dataRefSvgStyle;

  struct InheritedData {
    unsigned m_bitfields[2];
  } m_inheritedData;

  struct NonInheritedData {
    unsigned m_bitfields[3];
  } m_nonInheritedData;
};

ASSERT_SIZE(ComputedStyle, SameSizeAsComputedStyle);

PassRefPtr<ComputedStyle> ComputedStyle::create() {
  return adoptRef(new ComputedStyle());
}

PassRefPtr<ComputedStyle> ComputedStyle::createInitialStyle() {
  return adoptRef(new ComputedStyle(InitialStyle));
}

void ComputedStyle::invalidateInitialStyle() {
  mutableInitialStyle().setTapHighlightColor(initialTapHighlightColor());
}

PassRefPtr<ComputedStyle> ComputedStyle::createAnonymousStyleWithDisplay(
    const ComputedStyle& parentStyle,
    EDisplay display) {
  RefPtr<ComputedStyle> newStyle = ComputedStyle::create();
  newStyle->inheritFrom(parentStyle);
  newStyle->inheritUnicodeBidiFrom(parentStyle);
  newStyle->setDisplay(display);
  return newStyle;
}

PassRefPtr<ComputedStyle> ComputedStyle::clone(const ComputedStyle& other) {
  return adoptRef(new ComputedStyle(other));
}

ALWAYS_INLINE ComputedStyle::ComputedStyle()
    : ComputedStyleBase(),
      RefCounted<ComputedStyle>(),
      m_box(initialStyle().m_box),
      m_visual(initialStyle().m_visual),
      m_background(initialStyle().m_background),
      m_surround(initialStyle().m_surround),
      m_rareNonInheritedData(initialStyle().m_rareNonInheritedData),
      m_rareInheritedData(initialStyle().m_rareInheritedData),
      m_styleInheritedData(initialStyle().m_styleInheritedData),
      m_svgStyle(initialStyle().m_svgStyle) {
  setBitDefaults();  // Would it be faster to copy this from the default style?
  static_assert((sizeof(InheritedData) <= 8), "InheritedData should not grow");
  static_assert((sizeof(NonInheritedData) <= 12),
                "NonInheritedData should not grow");
}

ALWAYS_INLINE ComputedStyle::ComputedStyle(InitialStyleTag)
    : ComputedStyleBase(), RefCounted<ComputedStyle>() {
  setBitDefaults();

  m_box.init();
  m_visual.init();
  m_background.init();
  m_surround.init();
  m_rareNonInheritedData.init();
  m_rareNonInheritedData.access()->m_deprecatedFlexibleBox.init();
  m_rareNonInheritedData.access()->m_flexibleBox.init();
  m_rareNonInheritedData.access()->m_multiCol.init();
  m_rareNonInheritedData.access()->m_transform.init();
  m_rareNonInheritedData.access()->m_willChange.init();
  m_rareNonInheritedData.access()->m_filter.init();
  m_rareNonInheritedData.access()->m_backdropFilter.init();
  m_rareNonInheritedData.access()->m_grid.init();
  m_rareNonInheritedData.access()->m_gridItem.init();
  m_rareNonInheritedData.access()->m_scrollSnap.init();
  m_rareInheritedData.init();
  m_styleInheritedData.init();
  m_svgStyle.init();
}

ALWAYS_INLINE ComputedStyle::ComputedStyle(const ComputedStyle& o)
    : ComputedStyleBase(o),
      RefCounted<ComputedStyle>(),
      m_box(o.m_box),
      m_visual(o.m_visual),
      m_background(o.m_background),
      m_surround(o.m_surround),
      m_rareNonInheritedData(o.m_rareNonInheritedData),
      m_rareInheritedData(o.m_rareInheritedData),
      m_styleInheritedData(o.m_styleInheritedData),
      m_svgStyle(o.m_svgStyle),
      m_inheritedData(o.m_inheritedData),
      m_nonInheritedData(o.m_nonInheritedData) {}

static StyleRecalcChange diffPseudoStyles(const ComputedStyle& oldStyle,
                                          const ComputedStyle& newStyle) {
  // If the pseudoStyles have changed, ensure layoutObject triggers setStyle.
  if (!oldStyle.hasAnyPublicPseudoStyles() &&
      !newStyle.hasAnyPublicPseudoStyles())
    return NoChange;
  for (PseudoId pseudoId = FirstPublicPseudoId;
       pseudoId < FirstInternalPseudoId;
       pseudoId = static_cast<PseudoId>(pseudoId + 1)) {
    if (!oldStyle.hasPseudoStyle(pseudoId) &&
        !newStyle.hasPseudoStyle(pseudoId))
      continue;
    const ComputedStyle* newPseudoStyle =
        newStyle.getCachedPseudoStyle(pseudoId);
    if (!newPseudoStyle)
      return NoInherit;
    const ComputedStyle* oldPseudoStyle =
        oldStyle.getCachedPseudoStyle(pseudoId);
    if (oldPseudoStyle && *oldPseudoStyle != *newPseudoStyle)
      return NoInherit;
  }
  return NoChange;
}

StyleRecalcChange ComputedStyle::stylePropagationDiff(
    const ComputedStyle* oldStyle,
    const ComputedStyle* newStyle) {
  // If the style has changed from display none or to display none, then the
  // layout subtree needs to be reattached
  if ((!oldStyle && newStyle) || (oldStyle && !newStyle))
    return Reattach;

  if (!oldStyle && !newStyle)
    return NoChange;

  if (oldStyle->display() != newStyle->display() ||
      oldStyle->hasPseudoStyle(PseudoIdFirstLetter) !=
          newStyle->hasPseudoStyle(PseudoIdFirstLetter) ||
      !oldStyle->contentDataEquivalent(newStyle) ||
      oldStyle->hasTextCombine() != newStyle->hasTextCombine())
    return Reattach;

  bool independentEqual = oldStyle->independentInheritedEqual(*newStyle);
  bool nonIndependentEqual = oldStyle->nonIndependentInheritedEqual(*newStyle);
  if (!independentEqual || !nonIndependentEqual) {
    if (nonIndependentEqual && !oldStyle->hasExplicitlyInheritedProperties())
      return IndependentInherit;
    return Inherit;
  }

  if (!oldStyle->loadingCustomFontsEqual(*newStyle) ||
      oldStyle->alignItems() != newStyle->alignItems() ||
      oldStyle->justifyItems() != newStyle->justifyItems())
    return Inherit;

  if (*oldStyle == *newStyle)
    return diffPseudoStyles(*oldStyle, *newStyle);

  if (oldStyle->hasExplicitlyInheritedProperties())
    return Inherit;

  return NoInherit;
}

// TODO(sashab): Generate this function.
void ComputedStyle::propagateIndependentInheritedProperties(
    const ComputedStyle& parentStyle) {
  if (m_nonInheritedData.m_isPointerEventsInherited)
    setPointerEvents(parentStyle.pointerEvents());
  if (m_nonInheritedData.m_isVisibilityInherited)
    setVisibility(parentStyle.visibility());
}

StyleSelfAlignmentData resolvedSelfAlignment(
    const StyleSelfAlignmentData& value,
    ItemPosition normalValueBehavior) {
  // To avoid needing to copy the RareNonInheritedData, we repurpose the 'auto'
  // flag to not just mean 'auto' prior to running the StyleAdjuster but also
  // mean 'normal' after running it.
  if (value.position() == ItemPositionNormal ||
      value.position() == ItemPositionAuto)
    return {normalValueBehavior, OverflowAlignmentDefault};
  return value;
}

StyleSelfAlignmentData ComputedStyle::resolvedAlignItems(
    ItemPosition normalValueBehaviour) const {
  // We will return the behaviour of 'normal' value if needed, which is specific
  // of each layout model.
  return resolvedSelfAlignment(alignItems(), normalValueBehaviour);
}

StyleSelfAlignmentData ComputedStyle::resolvedAlignSelf(
    ItemPosition normalValueBehaviour,
    const ComputedStyle* parentStyle) const {
  // We will return the behaviour of 'normal' value if needed, which is specific
  // of each layout model.
  if (!parentStyle || alignSelfPosition() != ItemPositionAuto)
    return resolvedSelfAlignment(alignSelf(), normalValueBehaviour);

  // We shouldn't need to resolve any 'auto' value in post-adjusment
  // ComputedStyle, but some layout models can generate anonymous boxes that may
  // need 'auto' value resolution during layout.
  // The 'auto' keyword computes to the parent's align-items computed value.
  return parentStyle->resolvedAlignItems(normalValueBehaviour);
}

StyleSelfAlignmentData ComputedStyle::resolvedJustifyItems(
    ItemPosition normalValueBehaviour) const {
  // We will return the behaviour of 'normal' value if needed, which is specific
  // of each layout model.
  return resolvedSelfAlignment(justifyItems(), normalValueBehaviour);
}

StyleSelfAlignmentData ComputedStyle::resolvedJustifySelf(
    ItemPosition normalValueBehaviour,
    const ComputedStyle* parentStyle) const {
  // We will return the behaviour of 'normal' value if needed, which is specific
  // of each layout model.
  if (!parentStyle || justifySelfPosition() != ItemPositionAuto)
    return resolvedSelfAlignment(justifySelf(), normalValueBehaviour);

  // We shouldn't need to resolve any 'auto' value in post-adjusment
  // ComputedStyle, but some layout models can generate anonymous boxes that may
  // need 'auto' value resolution during layout.
  // The auto keyword computes to the parent's justify-items computed value.
  return parentStyle->resolvedJustifyItems(normalValueBehaviour);
}

static inline ContentPosition resolvedContentAlignmentPosition(
    const StyleContentAlignmentData& value,
    const StyleContentAlignmentData& normalValueBehavior) {
  return (value.position() == ContentPositionNormal &&
          value.distribution() == ContentDistributionDefault)
             ? normalValueBehavior.position()
             : value.position();
}

static inline ContentDistributionType resolvedContentAlignmentDistribution(
    const StyleContentAlignmentData& value,
    const StyleContentAlignmentData& normalValueBehavior) {
  return (value.position() == ContentPositionNormal &&
          value.distribution() == ContentDistributionDefault)
             ? normalValueBehavior.distribution()
             : value.distribution();
}

ContentPosition ComputedStyle::resolvedJustifyContentPosition(
    const StyleContentAlignmentData& normalValueBehavior) const {
  return resolvedContentAlignmentPosition(justifyContent(),
                                          normalValueBehavior);
}

ContentDistributionType ComputedStyle::resolvedJustifyContentDistribution(
    const StyleContentAlignmentData& normalValueBehavior) const {
  return resolvedContentAlignmentDistribution(justifyContent(),
                                              normalValueBehavior);
}

ContentPosition ComputedStyle::resolvedAlignContentPosition(
    const StyleContentAlignmentData& normalValueBehavior) const {
  return resolvedContentAlignmentPosition(alignContent(), normalValueBehavior);
}

ContentDistributionType ComputedStyle::resolvedAlignContentDistribution(
    const StyleContentAlignmentData& normalValueBehavior) const {
  return resolvedContentAlignmentDistribution(alignContent(),
                                              normalValueBehavior);
}

void ComputedStyle::inheritFrom(const ComputedStyle& inheritParent,
                                IsAtShadowBoundary isAtShadowBoundary) {
  ComputedStyleBase::inheritFrom(inheritParent, isAtShadowBoundary);
  if (isAtShadowBoundary == AtShadowBoundary) {
    // Even if surrounding content is user-editable, shadow DOM should act as a
    // single unit, and not necessarily be editable
    EUserModify currentUserModify = userModify();
    m_rareInheritedData = inheritParent.m_rareInheritedData;
    setUserModify(currentUserModify);
  } else {
    m_rareInheritedData = inheritParent.m_rareInheritedData;
  }
  m_styleInheritedData = inheritParent.m_styleInheritedData;
  m_inheritedData = inheritParent.m_inheritedData;
  if (m_svgStyle != inheritParent.m_svgStyle)
    m_svgStyle.access()->inheritFrom(inheritParent.m_svgStyle.get());
}

void ComputedStyle::copyNonInheritedFromCached(const ComputedStyle& other) {
  ComputedStyleBase::copyNonInheritedFromCached(other);
  m_box = other.m_box;
  m_visual = other.m_visual;
  m_background = other.m_background;
  m_surround = other.m_surround;
  m_rareNonInheritedData = other.m_rareNonInheritedData;

  // The flags are copied one-by-one because m_nonInheritedData.m_contains a
  // bunch of stuff other than real style data.
  // See comments for each skipped flag below.
  m_nonInheritedData.m_effectiveDisplay =
      other.m_nonInheritedData.m_effectiveDisplay;
  m_nonInheritedData.m_originalDisplay =
      other.m_nonInheritedData.m_originalDisplay;
  m_nonInheritedData.m_overflowAnchor =
      other.m_nonInheritedData.m_overflowAnchor;
  m_nonInheritedData.m_overflowX = other.m_nonInheritedData.m_overflowX;
  m_nonInheritedData.m_overflowY = other.m_nonInheritedData.m_overflowY;
  m_nonInheritedData.m_verticalAlign = other.m_nonInheritedData.m_verticalAlign;
  m_nonInheritedData.m_clear = other.m_nonInheritedData.m_clear;
  m_nonInheritedData.m_position = other.m_nonInheritedData.m_position;
  m_nonInheritedData.m_tableLayout = other.m_nonInheritedData.m_tableLayout;
  m_nonInheritedData.m_unicodeBidi = other.m_nonInheritedData.m_unicodeBidi;
  m_nonInheritedData.m_hasViewportUnits =
      other.m_nonInheritedData.m_hasViewportUnits;
  m_nonInheritedData.m_breakBefore = other.m_nonInheritedData.m_breakBefore;
  m_nonInheritedData.m_breakAfter = other.m_nonInheritedData.m_breakAfter;
  m_nonInheritedData.m_breakInside = other.m_nonInheritedData.m_breakInside;
  m_nonInheritedData.m_hasRemUnits = other.m_nonInheritedData.m_hasRemUnits;

  // Correctly set during selector matching:
  // m_nonInheritedData.m_styleType
  // m_nonInheritedData.m_pseudoBits

  // Set correctly while computing style for children:
  // m_nonInheritedData.m_explicitInheritance

  // unique() styles are not cacheable.
  DCHECK(!other.m_nonInheritedData.m_unique);

  // styles with non inherited properties that reference variables are not
  // cacheable.
  DCHECK(!other.m_nonInheritedData.m_variableReference);

  // The following flags are set during matching before we decide that we get a
  // match in the MatchedPropertiesCache which in turn calls this method. The
  // reason why we don't copy these flags is that they're already correctly set
  // and that they may differ between elements which have the same set of
  // matched properties. For instance, given the rule:
  //
  // :-webkit-any(:hover, :focus) { background-color: green }"
  //
  // A hovered element, and a focused element may use the same cached matched
  // properties here, but the affectedBy flags will be set differently based on
  // the matching order of the :-webkit-any components.
  //
  // m_nonInheritedData.m_emptyState
  // m_nonInheritedData.m_affectedByFocus
  // m_nonInheritedData.m_affectedByHover
  // m_nonInheritedData.m_affectedByActive
  // m_nonInheritedData.m_affectedByDrag
  // m_nonInheritedData.m_isLink

  // Any properties that are inherited on a style are also inherited on elements
  // that share this style.
  m_nonInheritedData.m_isPointerEventsInherited =
      other.m_nonInheritedData.m_isPointerEventsInherited;
  m_nonInheritedData.m_isVisibilityInherited =
      other.m_nonInheritedData.m_isVisibilityInherited;

  if (m_svgStyle != other.m_svgStyle)
    m_svgStyle.access()->copyNonInheritedFromCached(other.m_svgStyle.get());
  DCHECK_EQ(zoom(), initialZoom());
}

bool ComputedStyle::operator==(const ComputedStyle& o) const {
  return inheritedEqual(o) && nonInheritedEqual(o);
}

bool ComputedStyle::isStyleAvailable() const {
  return this != StyleResolver::styleNotYetAvailable();
}

bool ComputedStyle::hasUniquePseudoStyle() const {
  if (!m_cachedPseudoStyles || styleType() != PseudoIdNone)
    return false;

  for (size_t i = 0; i < m_cachedPseudoStyles->size(); ++i) {
    const ComputedStyle& pseudoStyle = *m_cachedPseudoStyles->at(i);
    if (pseudoStyle.unique())
      return true;
  }

  return false;
}

ComputedStyle* ComputedStyle::getCachedPseudoStyle(PseudoId pid) const {
  if (!m_cachedPseudoStyles || !m_cachedPseudoStyles->size())
    return 0;

  if (styleType() != PseudoIdNone)
    return 0;

  for (size_t i = 0; i < m_cachedPseudoStyles->size(); ++i) {
    ComputedStyle* pseudoStyle = m_cachedPseudoStyles->at(i).get();
    if (pseudoStyle->styleType() == pid)
      return pseudoStyle;
  }

  return 0;
}

ComputedStyle* ComputedStyle::addCachedPseudoStyle(
    PassRefPtr<ComputedStyle> pseudo) {
  if (!pseudo)
    return 0;

  ASSERT(pseudo->styleType() > PseudoIdNone);

  ComputedStyle* result = pseudo.get();

  if (!m_cachedPseudoStyles)
    m_cachedPseudoStyles = wrapUnique(new PseudoStyleCache);

  m_cachedPseudoStyles->append(pseudo);

  return result;
}

void ComputedStyle::removeCachedPseudoStyle(PseudoId pid) {
  if (!m_cachedPseudoStyles)
    return;
  for (size_t i = 0; i < m_cachedPseudoStyles->size(); ++i) {
    ComputedStyle* pseudoStyle = m_cachedPseudoStyles->at(i).get();
    if (pseudoStyle->styleType() == pid) {
      m_cachedPseudoStyles->remove(i);
      return;
    }
  }
}

bool ComputedStyle::inheritedEqual(const ComputedStyle& other) const {
  return independentInheritedEqual(other) &&
         nonIndependentInheritedEqual(other);
}

bool ComputedStyle::independentInheritedEqual(
    const ComputedStyle& other) const {
  return ComputedStyleBase::independentInheritedEqual(other) &&
         m_inheritedData.compareEqualIndependent(other.m_inheritedData);
}

bool ComputedStyle::nonIndependentInheritedEqual(
    const ComputedStyle& other) const {
  return ComputedStyleBase::nonIndependentInheritedEqual(other) &&
         m_inheritedData.compareEqualNonIndependent(other.m_inheritedData) &&
         m_styleInheritedData == other.m_styleInheritedData &&
         m_svgStyle->inheritedEqual(*other.m_svgStyle) &&
         m_rareInheritedData == other.m_rareInheritedData;
}

bool ComputedStyle::loadingCustomFontsEqual(const ComputedStyle& other) const {
  return font().loadingCustomFonts() == other.font().loadingCustomFonts();
}

bool ComputedStyle::nonInheritedEqual(const ComputedStyle& other) const {
  // compare everything except the pseudoStyle pointer
  return ComputedStyleBase::nonInheritedEqual(other) &&
         m_nonInheritedData == other.m_nonInheritedData &&
         m_box == other.m_box && m_visual == other.m_visual &&
         m_background == other.m_background && m_surround == other.m_surround &&
         m_rareNonInheritedData == other.m_rareNonInheritedData &&
         m_svgStyle->nonInheritedEqual(*other.m_svgStyle);
}

bool ComputedStyle::inheritedDataShared(const ComputedStyle& other) const {
  // This is a fast check that only looks if the data structures are shared.
  // TODO(sashab): Should ComputedStyleBase have an inheritedDataShared method?
  return ComputedStyleBase::inheritedEqual(other) &&
         m_inheritedData == other.m_inheritedData &&
         m_styleInheritedData.get() == other.m_styleInheritedData.get() &&
         m_svgStyle.get() == other.m_svgStyle.get() &&
         m_rareInheritedData.get() == other.m_rareInheritedData.get();
}

static bool dependenceOnContentHeightHasChanged(const ComputedStyle& a,
                                                const ComputedStyle& b) {
  // If top or bottom become auto/non-auto then it means we either have to solve
  // height based on the content or stop doing so
  // (http://www.w3.org/TR/CSS2/visudet.html#abs-non-replaced-height)
  // - either way requires a layout.
  return a.logicalTop().isAuto() != b.logicalTop().isAuto() ||
         a.logicalBottom().isAuto() != b.logicalBottom().isAuto();
}

StyleDifference ComputedStyle::visualInvalidationDiff(
    const ComputedStyle& other) const {
  // Note, we use .get() on each DataRef below because DataRef::operator== will
  // do a deep compare, which is duplicate work when we're going to compare each
  // property inside this function anyway.

  StyleDifference diff;
  if (m_svgStyle.get() != other.m_svgStyle.get())
    diff = m_svgStyle->diff(other.m_svgStyle.get());

  if ((!diff.needsFullLayout() || !diff.needsPaintInvalidation()) &&
      diffNeedsFullLayoutAndPaintInvalidation(other)) {
    diff.setNeedsFullLayout();
    diff.setNeedsPaintInvalidationObject();
  }

  if (!diff.needsFullLayout() && diffNeedsFullLayout(other))
    diff.setNeedsFullLayout();

  if (!diff.needsFullLayout() &&
      m_surround->margin != other.m_surround->margin) {
    // Relative-positioned elements collapse their margins so need a full
    // layout.
    if (hasOutOfFlowPosition())
      diff.setNeedsPositionedMovementLayout();
    else
      diff.setNeedsFullLayout();
  }

  if (!diff.needsFullLayout() && position() != StaticPosition &&
      m_surround->offset != other.m_surround->offset) {
    // Optimize for the case where a positioned layer is moving but not changing
    // size.
    if (dependenceOnContentHeightHasChanged(*this, other))
      diff.setNeedsFullLayout();
    else
      diff.setNeedsPositionedMovementLayout();
  }

  if (diffNeedsPaintInvalidationSubtree(other))
    diff.setNeedsPaintInvalidationSubtree();
  else if (diffNeedsPaintInvalidationObject(other))
    diff.setNeedsPaintInvalidationObject();

  updatePropertySpecificDifferences(other, diff);

  if (scrollAnchorDisablingPropertyChanged(other, diff))
    diff.setScrollAnchorDisablingPropertyChanged();

  // Cursors are not checked, since they will be set appropriately in response
  // to mouse events, so they don't need to cause any paint invalidation or
  // layout.

  // Animations don't need to be checked either. We always set the new style on
  // the layoutObject, so we will get a chance to fire off the resulting
  // transition properly.

  return diff;
}

bool ComputedStyle::scrollAnchorDisablingPropertyChanged(
    const ComputedStyle& other,
    StyleDifference& diff) const {
  if (m_nonInheritedData.m_position != other.m_nonInheritedData.m_position)
    return true;

  if (m_box.get() != other.m_box.get()) {
    if (m_box->width() != other.m_box->width() ||
        m_box->minWidth() != other.m_box->minWidth() ||
        m_box->maxWidth() != other.m_box->maxWidth() ||
        m_box->height() != other.m_box->height() ||
        m_box->minHeight() != other.m_box->minHeight() ||
        m_box->maxHeight() != other.m_box->maxHeight())
      return true;
  }

  if (m_surround.get() != other.m_surround.get()) {
    if (m_surround->margin != other.m_surround->margin ||
        m_surround->offset != other.m_surround->offset ||
        m_surround->padding != other.m_surround->padding)
      return true;
  }

  if (diff.transformChanged())
    return true;

  return false;
}

bool ComputedStyle::diffNeedsFullLayoutAndPaintInvalidation(
    const ComputedStyle& other) const {
  // FIXME: Not all cases in this method need both full layout and paint
  // invalidation.
  // Should move cases into diffNeedsFullLayout() if
  // - don't need paint invalidation at all;
  // - or the layoutObject knows how to exactly invalidate paints caused by the
  //   layout change instead of forced full paint invalidation.

  if (m_surround.get() != other.m_surround.get()) {
    // If our border widths change, then we need to layout. Other changes to
    // borders only necessitate a paint invalidation.
    if (borderLeftWidth() != other.borderLeftWidth() ||
        borderTopWidth() != other.borderTopWidth() ||
        borderBottomWidth() != other.borderBottomWidth() ||
        borderRightWidth() != other.borderRightWidth())
      return true;
  }

  if (m_rareNonInheritedData.get() != other.m_rareNonInheritedData.get()) {
    if (m_rareNonInheritedData->m_appearance !=
            other.m_rareNonInheritedData->m_appearance ||
        m_rareNonInheritedData->marginBeforeCollapse !=
            other.m_rareNonInheritedData->marginBeforeCollapse ||
        m_rareNonInheritedData->marginAfterCollapse !=
            other.m_rareNonInheritedData->marginAfterCollapse ||
        m_rareNonInheritedData->lineClamp !=
            other.m_rareNonInheritedData->lineClamp ||
        m_rareNonInheritedData->textOverflow !=
            other.m_rareNonInheritedData->textOverflow ||
        m_rareNonInheritedData->m_shapeMargin !=
            other.m_rareNonInheritedData->m_shapeMargin ||
        m_rareNonInheritedData->m_order !=
            other.m_rareNonInheritedData->m_order ||
        m_rareNonInheritedData->hasFilters() !=
            other.m_rareNonInheritedData->hasFilters())
      return true;

    if (m_rareNonInheritedData->m_grid.get() !=
            other.m_rareNonInheritedData->m_grid.get() &&
        *m_rareNonInheritedData->m_grid.get() !=
            *other.m_rareNonInheritedData->m_grid.get())
      return true;

    if (m_rareNonInheritedData->m_gridItem.get() !=
            other.m_rareNonInheritedData->m_gridItem.get() &&
        *m_rareNonInheritedData->m_gridItem.get() !=
            *other.m_rareNonInheritedData->m_gridItem.get())
      return true;

    if (m_rareNonInheritedData->m_deprecatedFlexibleBox.get() !=
            other.m_rareNonInheritedData->m_deprecatedFlexibleBox.get() &&
        *m_rareNonInheritedData->m_deprecatedFlexibleBox.get() !=
            *other.m_rareNonInheritedData->m_deprecatedFlexibleBox.get())
      return true;

    if (m_rareNonInheritedData->m_flexibleBox.get() !=
            other.m_rareNonInheritedData->m_flexibleBox.get() &&
        *m_rareNonInheritedData->m_flexibleBox.get() !=
            *other.m_rareNonInheritedData->m_flexibleBox.get())
      return true;

    if (m_rareNonInheritedData->m_multiCol.get() !=
            other.m_rareNonInheritedData->m_multiCol.get() &&
        *m_rareNonInheritedData->m_multiCol.get() !=
            *other.m_rareNonInheritedData->m_multiCol.get())
      return true;

    // If the counter directives change, trigger a relayout to re-calculate
    // counter values and rebuild the counter node tree.
    const CounterDirectiveMap* mapA =
        m_rareNonInheritedData->m_counterDirectives.get();
    const CounterDirectiveMap* mapB =
        other.m_rareNonInheritedData->m_counterDirectives.get();
    if (!(mapA == mapB || (mapA && mapB && *mapA == *mapB)))
      return true;

    // We only need do layout for opacity changes if adding or losing opacity
    // could trigger a change
    // in us being a stacking context.
    if (isStackingContext() != other.isStackingContext() &&
        m_rareNonInheritedData->hasOpacity() !=
            other.m_rareNonInheritedData->hasOpacity()) {
      // FIXME: We would like to use SimplifiedLayout here, but we can't quite
      // do that yet.  We need to make sure SimplifiedLayout can operate
      // correctly on LayoutInlines (we will need to add a
      // selfNeedsSimplifiedLayout bit in order to not get confused and taint
      // every line).  In addition we need to solve the floating object issue
      // when layers come and go. Right now a full layout is necessary to keep
      // floating object lists sane.
      return true;
    }
  }

  if (m_rareInheritedData.get() != other.m_rareInheritedData.get()) {
    if (m_rareInheritedData->highlight !=
            other.m_rareInheritedData->highlight ||
        m_rareInheritedData->indent != other.m_rareInheritedData->indent ||
        m_rareInheritedData->m_textAlignLast !=
            other.m_rareInheritedData->m_textAlignLast ||
        m_rareInheritedData->m_textIndentLine !=
            other.m_rareInheritedData->m_textIndentLine ||
        m_rareInheritedData->m_effectiveZoom !=
            other.m_rareInheritedData->m_effectiveZoom ||
        m_rareInheritedData->wordBreak !=
            other.m_rareInheritedData->wordBreak ||
        m_rareInheritedData->overflowWrap !=
            other.m_rareInheritedData->overflowWrap ||
        m_rareInheritedData->lineBreak !=
            other.m_rareInheritedData->lineBreak ||
        m_rareInheritedData->textSecurity !=
            other.m_rareInheritedData->textSecurity ||
        m_rareInheritedData->hyphens != other.m_rareInheritedData->hyphens ||
        m_rareInheritedData->hyphenationLimitBefore !=
            other.m_rareInheritedData->hyphenationLimitBefore ||
        m_rareInheritedData->hyphenationLimitAfter !=
            other.m_rareInheritedData->hyphenationLimitAfter ||
        m_rareInheritedData->hyphenationString !=
            other.m_rareInheritedData->hyphenationString ||
        m_rareInheritedData->m_respectImageOrientation !=
            other.m_rareInheritedData->m_respectImageOrientation ||
        m_rareInheritedData->m_rubyPosition !=
            other.m_rareInheritedData->m_rubyPosition ||
        m_rareInheritedData->textEmphasisMark !=
            other.m_rareInheritedData->textEmphasisMark ||
        m_rareInheritedData->textEmphasisPosition !=
            other.m_rareInheritedData->textEmphasisPosition ||
        m_rareInheritedData->textEmphasisCustomMark !=
            other.m_rareInheritedData->textEmphasisCustomMark ||
        m_rareInheritedData->m_textJustify !=
            other.m_rareInheritedData->m_textJustify ||
        m_rareInheritedData->m_textOrientation !=
            other.m_rareInheritedData->m_textOrientation ||
        m_rareInheritedData->m_textCombine !=
            other.m_rareInheritedData->m_textCombine ||
        m_rareInheritedData->m_tabSize !=
            other.m_rareInheritedData->m_tabSize ||
        m_rareInheritedData->m_textSizeAdjust !=
            other.m_rareInheritedData->m_textSizeAdjust ||
        m_rareInheritedData->listStyleImage !=
            other.m_rareInheritedData->listStyleImage ||
        m_rareInheritedData->m_snapHeightUnit !=
            other.m_rareInheritedData->m_snapHeightUnit ||
        m_rareInheritedData->m_snapHeightPosition !=
            other.m_rareInheritedData->m_snapHeightPosition ||
        m_rareInheritedData->textStrokeWidth !=
            other.m_rareInheritedData->textStrokeWidth)
      return true;

    if (!m_rareInheritedData->shadowDataEquivalent(
            *other.m_rareInheritedData.get()))
      return true;

    if (!m_rareInheritedData->quotesDataEquivalent(
            *other.m_rareInheritedData.get()))
      return true;
  }

  if (m_styleInheritedData->textAutosizingMultiplier !=
      other.m_styleInheritedData->textAutosizingMultiplier)
    return true;

  if (m_styleInheritedData->font.loadingCustomFonts() !=
      other.m_styleInheritedData->font.loadingCustomFonts())
    return true;

  if (m_styleInheritedData.get() != other.m_styleInheritedData.get()) {
    if (m_styleInheritedData->line_height !=
            other.m_styleInheritedData->line_height ||
        m_styleInheritedData->font != other.m_styleInheritedData->font ||
        m_styleInheritedData->horizontal_border_spacing !=
            other.m_styleInheritedData->horizontal_border_spacing ||
        m_styleInheritedData->vertical_border_spacing !=
            other.m_styleInheritedData->vertical_border_spacing)
      return true;
  }

  if (m_inheritedData.m_boxDirection != other.m_inheritedData.m_boxDirection ||
      m_inheritedData.m_rtlOrdering != other.m_inheritedData.m_rtlOrdering ||
      m_inheritedData.m_textAlign != other.m_inheritedData.m_textAlign ||
      m_inheritedData.m_textTransform !=
          other.m_inheritedData.m_textTransform ||
      m_inheritedData.m_direction != other.m_inheritedData.m_direction ||
      m_inheritedData.m_whiteSpace != other.m_inheritedData.m_whiteSpace ||
      m_inheritedData.m_writingMode != other.m_inheritedData.m_writingMode)
    return true;

  if (m_nonInheritedData.m_overflowX != other.m_nonInheritedData.m_overflowX ||
      m_nonInheritedData.m_overflowY != other.m_nonInheritedData.m_overflowY ||
      m_nonInheritedData.m_clear != other.m_nonInheritedData.m_clear ||
      m_nonInheritedData.m_unicodeBidi !=
          other.m_nonInheritedData.m_unicodeBidi ||
      floating() != other.floating() ||
      m_nonInheritedData.m_originalDisplay !=
          other.m_nonInheritedData.m_originalDisplay)
    return true;

  if (isDisplayTableType(display())) {
    if (m_inheritedData.m_borderCollapse !=
            other.m_inheritedData.m_borderCollapse ||
        emptyCells() != other.emptyCells() ||
        m_inheritedData.m_captionSide != other.m_inheritedData.m_captionSide ||
        m_nonInheritedData.m_tableLayout !=
            other.m_nonInheritedData.m_tableLayout)
      return true;

    // In the collapsing border model, 'hidden' suppresses other borders, while
    // 'none' does not, so these style differences can be width differences.
    if (m_inheritedData.m_borderCollapse &&
        ((borderTopStyle() == BorderStyleHidden &&
          other.borderTopStyle() == BorderStyleNone) ||
         (borderTopStyle() == BorderStyleNone &&
          other.borderTopStyle() == BorderStyleHidden) ||
         (borderBottomStyle() == BorderStyleHidden &&
          other.borderBottomStyle() == BorderStyleNone) ||
         (borderBottomStyle() == BorderStyleNone &&
          other.borderBottomStyle() == BorderStyleHidden) ||
         (borderLeftStyle() == BorderStyleHidden &&
          other.borderLeftStyle() == BorderStyleNone) ||
         (borderLeftStyle() == BorderStyleNone &&
          other.borderLeftStyle() == BorderStyleHidden) ||
         (borderRightStyle() == BorderStyleHidden &&
          other.borderRightStyle() == BorderStyleNone) ||
         (borderRightStyle() == BorderStyleNone &&
          other.borderRightStyle() == BorderStyleHidden)))
      return true;
  } else if (display() == EDisplay::ListItem) {
    if (m_inheritedData.m_listStyleType !=
            other.m_inheritedData.m_listStyleType ||
        m_inheritedData.m_listStylePosition !=
            other.m_inheritedData.m_listStylePosition)
      return true;
  }

  if ((visibility() == EVisibility::Collapse) !=
      (other.visibility() == EVisibility::Collapse))
    return true;

  if (hasPseudoStyle(PseudoIdScrollbar) !=
      other.hasPseudoStyle(PseudoIdScrollbar))
    return true;

  // Movement of non-static-positioned object is special cased in
  // ComputedStyle::visualInvalidationDiff().

  return false;
}

bool ComputedStyle::diffNeedsFullLayout(const ComputedStyle& other) const {
  if (m_box.get() != other.m_box.get()) {
    if (m_box->width() != other.m_box->width() ||
        m_box->minWidth() != other.m_box->minWidth() ||
        m_box->maxWidth() != other.m_box->maxWidth() ||
        m_box->height() != other.m_box->height() ||
        m_box->minHeight() != other.m_box->minHeight() ||
        m_box->maxHeight() != other.m_box->maxHeight())
      return true;

    if (m_box->verticalAlign() != other.m_box->verticalAlign())
      return true;

    if (m_box->boxSizing() != other.m_box->boxSizing())
      return true;
  }

  if (m_nonInheritedData.m_verticalAlign !=
          other.m_nonInheritedData.m_verticalAlign ||
      m_nonInheritedData.m_position != other.m_nonInheritedData.m_position)
    return true;

  if (m_surround.get() != other.m_surround.get()) {
    if (m_surround->padding != other.m_surround->padding)
      return true;
  }

  if (m_rareNonInheritedData.get() != other.m_rareNonInheritedData.get()) {
    if (m_rareNonInheritedData->m_alignContent !=
            other.m_rareNonInheritedData->m_alignContent ||
        m_rareNonInheritedData->m_alignItems !=
            other.m_rareNonInheritedData->m_alignItems ||
        m_rareNonInheritedData->m_alignSelf !=
            other.m_rareNonInheritedData->m_alignSelf ||
        m_rareNonInheritedData->m_justifyContent !=
            other.m_rareNonInheritedData->m_justifyContent ||
        m_rareNonInheritedData->m_justifyItems !=
            other.m_rareNonInheritedData->m_justifyItems ||
        m_rareNonInheritedData->m_justifySelf !=
            other.m_rareNonInheritedData->m_justifySelf ||
        m_rareNonInheritedData->m_contain !=
            other.m_rareNonInheritedData->m_contain)
      return true;
  }

  return false;
}

bool ComputedStyle::diffNeedsPaintInvalidationSubtree(
    const ComputedStyle& other) const {
  if (position() != StaticPosition &&
      (m_visual->clip != other.m_visual->clip ||
       m_visual->hasAutoClip != other.m_visual->hasAutoClip))
    return true;

  if (m_rareNonInheritedData.get() != other.m_rareNonInheritedData.get()) {
    if (m_rareNonInheritedData->m_effectiveBlendMode !=
            other.m_rareNonInheritedData->m_effectiveBlendMode ||
        m_rareNonInheritedData->m_isolation !=
            other.m_rareNonInheritedData->m_isolation)
      return true;

    if (m_rareNonInheritedData->m_mask !=
            other.m_rareNonInheritedData->m_mask ||
        m_rareNonInheritedData->m_maskBoxImage !=
            other.m_rareNonInheritedData->m_maskBoxImage)
      return true;
  }

  return false;
}

bool ComputedStyle::diffNeedsPaintInvalidationObject(
    const ComputedStyle& other) const {
  if (visibility() != other.visibility() ||
      m_inheritedData.m_printColorAdjust !=
          other.m_inheritedData.m_printColorAdjust ||
      m_inheritedData.m_insideLink != other.m_inheritedData.m_insideLink ||
      !m_surround->border.visuallyEqual(other.m_surround->border) ||
      *m_background != *other.m_background)
    return true;

  if (m_rareInheritedData.get() != other.m_rareInheritedData.get()) {
    if (m_rareInheritedData->userModify !=
            other.m_rareInheritedData->userModify ||
        m_rareInheritedData->userSelect !=
            other.m_rareInheritedData->userSelect ||
        m_rareInheritedData->m_imageRendering !=
            other.m_rareInheritedData->m_imageRendering)
      return true;
  }

  if (m_rareNonInheritedData.get() != other.m_rareNonInheritedData.get()) {
    if (m_rareNonInheritedData->userDrag !=
            other.m_rareNonInheritedData->userDrag ||
        m_rareNonInheritedData->m_objectFit !=
            other.m_rareNonInheritedData->m_objectFit ||
        m_rareNonInheritedData->m_objectPosition !=
            other.m_rareNonInheritedData->m_objectPosition ||
        !m_rareNonInheritedData->shadowDataEquivalent(
            *other.m_rareNonInheritedData.get()) ||
        !m_rareNonInheritedData->shapeOutsideDataEquivalent(
            *other.m_rareNonInheritedData.get()) ||
        !m_rareNonInheritedData->clipPathDataEquivalent(
            *other.m_rareNonInheritedData.get()) ||
        !m_rareNonInheritedData->m_outline.visuallyEqual(
            other.m_rareNonInheritedData->m_outline) ||
        (visitedLinkBorderLeftColor() != other.visitedLinkBorderLeftColor() &&
         borderLeftWidth()) ||
        (visitedLinkBorderRightColor() != other.visitedLinkBorderRightColor() &&
         borderRightWidth()) ||
        (visitedLinkBorderBottomColor() !=
             other.visitedLinkBorderBottomColor() &&
         borderBottomWidth()) ||
        (visitedLinkBorderTopColor() != other.visitedLinkBorderTopColor() &&
         borderTopWidth()) ||
        (visitedLinkOutlineColor() != other.visitedLinkOutlineColor() &&
         outlineWidth()) ||
        (visitedLinkBackgroundColor() != other.visitedLinkBackgroundColor()))
      return true;
  }

  if (resize() != other.resize())
    return true;

  if (m_rareNonInheritedData->m_paintImages) {
    for (const auto& image : *m_rareNonInheritedData->m_paintImages) {
      if (diffNeedsPaintInvalidationObjectForPaintImage(image, other))
        return true;
    }
  }

  return false;
}

bool ComputedStyle::diffNeedsPaintInvalidationObjectForPaintImage(
    const StyleImage* image,
    const ComputedStyle& other) const {
  CSSPaintValue* value = toCSSPaintValue(image->cssValue());

  // NOTE: If the invalidation properties vectors are null, we are invalid as
  // we haven't yet been painted (and can't provide the invalidation
  // properties yet).
  if (!value->nativeInvalidationProperties() ||
      !value->customInvalidationProperties())
    return true;

  for (CSSPropertyID propertyID : *value->nativeInvalidationProperties()) {
    // TODO(ikilpatrick): remove isInterpolableProperty check once
    // CSSPropertyEquality::propertiesEqual correctly handles all properties.
    if (!CSSPropertyMetadata::isInterpolableProperty(propertyID) ||
        !CSSPropertyEquality::propertiesEqual(propertyID, *this, other))
      return true;
  }

  if (inheritedVariables() || nonInheritedVariables() ||
      other.inheritedVariables() || other.nonInheritedVariables()) {
    for (const AtomicString& property :
         *value->customInvalidationProperties()) {
      if (!dataEquivalent(getVariable(property), other.getVariable(property)))
        return true;
    }
  }

  return false;
}

void ComputedStyle::updatePropertySpecificDifferences(
    const ComputedStyle& other,
    StyleDifference& diff) const {
  if (m_box->zIndex() != other.m_box->zIndex() ||
      isStackingContext() != other.isStackingContext())
    diff.setZIndexChanged();

  if (m_rareNonInheritedData.get() != other.m_rareNonInheritedData.get()) {
    if (!transformDataEquivalent(other))
      diff.setTransformChanged();

    if (m_rareNonInheritedData->opacity !=
        other.m_rareNonInheritedData->opacity)
      diff.setOpacityChanged();

    if (m_rareNonInheritedData->m_filter !=
        other.m_rareNonInheritedData->m_filter)
      diff.setFilterChanged();

    if (!m_rareNonInheritedData->shadowDataEquivalent(
            *other.m_rareNonInheritedData.get()))
      diff.setNeedsRecomputeOverflow();

    if (m_rareNonInheritedData->m_backdropFilter !=
        other.m_rareNonInheritedData->m_backdropFilter)
      diff.setBackdropFilterChanged();

    if (!m_rareNonInheritedData->reflectionDataEquivalent(
            *other.m_rareNonInheritedData.get()))
      diff.setFilterChanged();

    if (!m_rareNonInheritedData->m_outline.visuallyEqual(
            other.m_rareNonInheritedData->m_outline))
      diff.setNeedsRecomputeOverflow();
  }

  if (!m_surround->border.visualOverflowEqual(other.m_surround->border))
    diff.setNeedsRecomputeOverflow();

  if (!diff.needsPaintInvalidation()) {
    if (m_styleInheritedData->color != other.m_styleInheritedData->color ||
        m_styleInheritedData->visitedLinkColor !=
            other.m_styleInheritedData->visitedLinkColor ||
        m_inheritedData.m_textUnderline !=
            other.m_inheritedData.m_textUnderline ||
        m_visual->textDecoration != other.m_visual->textDecoration) {
      diff.setTextDecorationOrColorChanged();
    } else if (m_rareNonInheritedData.get() !=
                   other.m_rareNonInheritedData.get() &&
               (m_rareNonInheritedData->m_textDecorationStyle !=
                    other.m_rareNonInheritedData->m_textDecorationStyle ||
                m_rareNonInheritedData->m_textDecorationColor !=
                    other.m_rareNonInheritedData->m_textDecorationColor ||
                m_rareNonInheritedData->m_visitedLinkTextDecorationColor !=
                    other.m_rareNonInheritedData
                        ->m_visitedLinkTextDecorationColor)) {
      diff.setTextDecorationOrColorChanged();
    } else if (m_rareInheritedData.get() != other.m_rareInheritedData.get() &&
               (m_rareInheritedData->textFillColor() !=
                    other.m_rareInheritedData->textFillColor() ||
                m_rareInheritedData->textStrokeColor() !=
                    other.m_rareInheritedData->textStrokeColor() ||
                m_rareInheritedData->textEmphasisColor() !=
                    other.m_rareInheritedData->textEmphasisColor() ||
                m_rareInheritedData->visitedLinkTextFillColor() !=
                    other.m_rareInheritedData->visitedLinkTextFillColor() ||
                m_rareInheritedData->visitedLinkTextStrokeColor() !=
                    other.m_rareInheritedData->visitedLinkTextStrokeColor() ||
                m_rareInheritedData->visitedLinkTextEmphasisColor() !=
                    other.m_rareInheritedData->visitedLinkTextEmphasisColor() ||
                m_rareInheritedData->textEmphasisFill !=
                    other.m_rareInheritedData->textEmphasisFill ||
                m_rareInheritedData->m_textDecorationSkip !=
                    other.m_rareInheritedData->m_textDecorationSkip ||
                m_rareInheritedData->appliedTextDecorations !=
                    other.m_rareInheritedData->appliedTextDecorations)) {
      diff.setTextDecorationOrColorChanged();
    }
  }
}

void ComputedStyle::addPaintImage(StyleImage* image) {
  if (!m_rareNonInheritedData.access()->m_paintImages) {
    m_rareNonInheritedData.access()->m_paintImages =
        makeUnique<Vector<Persistent<StyleImage>>>();
  }
  m_rareNonInheritedData.access()->m_paintImages->append(image);
}

void ComputedStyle::addCursor(StyleImage* image,
                              bool hotSpotSpecified,
                              const IntPoint& hotSpot) {
  if (!m_rareInheritedData.access()->cursorData)
    m_rareInheritedData.access()->cursorData = new CursorList;
  m_rareInheritedData.access()->cursorData->append(
      CursorData(image, hotSpotSpecified, hotSpot));
}

void ComputedStyle::setCursorList(CursorList* other) {
  m_rareInheritedData.access()->cursorData = other;
}

void ComputedStyle::setQuotes(PassRefPtr<QuotesData> q) {
  m_rareInheritedData.access()->quotes = q;
}

void ComputedStyle::clearCursorList() {
  if (m_rareInheritedData->cursorData)
    m_rareInheritedData.access()->cursorData = nullptr;
}

static bool hasPropertyThatCreatesStackingContext(
    const Vector<CSSPropertyID>& properties) {
  for (CSSPropertyID property : properties) {
    switch (property) {
      case CSSPropertyOpacity:
      case CSSPropertyTransform:
      case CSSPropertyAliasWebkitTransform:
      case CSSPropertyTransformStyle:
      case CSSPropertyAliasWebkitTransformStyle:
      case CSSPropertyPerspective:
      case CSSPropertyAliasWebkitPerspective:
      case CSSPropertyTranslate:
      case CSSPropertyRotate:
      case CSSPropertyScale:
      case CSSPropertyOffsetPath:
      case CSSPropertyOffsetPosition:
      case CSSPropertyWebkitMask:
      case CSSPropertyWebkitMaskBoxImage:
      case CSSPropertyClipPath:
      case CSSPropertyAliasWebkitClipPath:
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

void ComputedStyle::updateIsStackingContext(bool isDocumentElement,
                                            bool isInTopLayer) {
  if (isStackingContext())
    return;

  if (isDocumentElement || isInTopLayer || styleType() == PseudoIdBackdrop ||
      hasOpacity() || hasTransformRelatedProperty() || hasMask() ||
      clipPath() || boxReflect() || hasFilterInducingProperty() ||
      hasBackdropFilter() || hasBlendMode() || hasIsolation() ||
      hasViewportConstrainedPosition() ||
      hasPropertyThatCreatesStackingContext(willChangeProperties()) ||
      containsPaint()) {
    setIsStackingContext(true);
  }
}

void ComputedStyle::addCallbackSelector(const String& selector) {
  if (!m_rareNonInheritedData->m_callbackSelectors.contains(selector))
    m_rareNonInheritedData.access()->m_callbackSelectors.append(selector);
}

void ComputedStyle::setContent(ContentData* contentData) {
  SET_VAR(m_rareNonInheritedData, m_content, contentData);
}

bool ComputedStyle::hasWillChangeCompositingHint() const {
  for (size_t i = 0;
       i < m_rareNonInheritedData->m_willChange->m_properties.size(); ++i) {
    switch (m_rareNonInheritedData->m_willChange->m_properties[i]) {
      case CSSPropertyOpacity:
      case CSSPropertyTransform:
      case CSSPropertyAliasWebkitTransform:
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

bool ComputedStyle::hasWillChangeTransformHint() const {
  for (const auto& property :
       m_rareNonInheritedData->m_willChange->m_properties) {
    switch (property) {
      case CSSPropertyTransform:
      case CSSPropertyAliasWebkitTransform:
      case CSSPropertyPerspective:
      case CSSPropertyTranslate:
      case CSSPropertyScale:
      case CSSPropertyRotate:
        return true;
      default:
        break;
    }
  }
  return false;
}

bool ComputedStyle::requireTransformOrigin(
    ApplyTransformOrigin applyOrigin,
    ApplyMotionPath applyMotionPath) const {
  // transform-origin brackets the transform with translate operations.
  // Optimize for the case where the only transform is a translation, since the
  // transform-origin is irrelevant in that case.
  if (applyOrigin != IncludeTransformOrigin)
    return false;

  if (applyMotionPath == IncludeMotionPath)
    return true;

  for (const auto& operation : transform().operations()) {
    TransformOperation::OperationType type = operation->type();
    if (type != TransformOperation::TranslateX &&
        type != TransformOperation::TranslateY &&
        type != TransformOperation::Translate &&
        type != TransformOperation::TranslateZ &&
        type != TransformOperation::Translate3D)
      return true;
  }

  return scale() || rotate();
}

void ComputedStyle::applyTransform(
    TransformationMatrix& result,
    const LayoutSize& borderBoxSize,
    ApplyTransformOrigin applyOrigin,
    ApplyMotionPath applyMotionPath,
    ApplyIndependentTransformProperties applyIndependentTransformProperties)
    const {
  applyTransform(result, FloatRect(FloatPoint(), FloatSize(borderBoxSize)),
                 applyOrigin, applyMotionPath,
                 applyIndependentTransformProperties);
}

void ComputedStyle::applyTransform(
    TransformationMatrix& result,
    const FloatRect& boundingBox,
    ApplyTransformOrigin applyOrigin,
    ApplyMotionPath applyMotionPath,
    ApplyIndependentTransformProperties applyIndependentTransformProperties)
    const {
  if (!hasOffset())
    applyMotionPath = ExcludeMotionPath;
  bool applyTransformOrigin =
      requireTransformOrigin(applyOrigin, applyMotionPath);

  float originX = 0;
  float originY = 0;
  float originZ = 0;

  const FloatSize& boxSize = boundingBox.size();
  if (applyTransformOrigin ||
      // We need to calculate originX and originY for applying motion path.
      applyMotionPath == IncludeMotionPath) {
    float offsetX = transformOriginX().type() == Percent ? boundingBox.x() : 0;
    originX =
        floatValueForLength(transformOriginX(), boxSize.width()) + offsetX;
    float offsetY = transformOriginY().type() == Percent ? boundingBox.y() : 0;
    originY =
        floatValueForLength(transformOriginY(), boxSize.height()) + offsetY;
    if (applyTransformOrigin) {
      originZ = transformOriginZ();
      result.translate3d(originX, originY, originZ);
    }
  }

  if (applyIndependentTransformProperties ==
      IncludeIndependentTransformProperties) {
    if (translate())
      translate()->apply(result, boxSize);

    if (rotate())
      rotate()->apply(result, boxSize);

    if (scale())
      scale()->apply(result, boxSize);
  }

  if (applyMotionPath == IncludeMotionPath)
    applyMotionPathTransform(originX, originY, boundingBox, result);

  for (const auto& operation : transform().operations())
    operation->apply(result, boxSize);

  if (applyTransformOrigin) {
    result.translate3d(-originX, -originY, -originZ);
  }
}

void ComputedStyle::applyMotionPathTransform(
    float originX,
    float originY,
    const FloatRect& boundingBox,
    TransformationMatrix& transform) const {
  const StyleMotionData& motionData =
      m_rareNonInheritedData->m_transform->m_motion;
  // TODO(ericwilligers): crbug.com/638055 Apply offset-position.
  if (!motionData.m_path) {
    return;
  }
  const StylePath& motionPath = *motionData.m_path;
  float pathLength = motionPath.length();
  float distance = floatValueForLength(motionData.m_distance, pathLength);
  float computedDistance;
  if (motionPath.isClosed() && pathLength > 0) {
    computedDistance = fmod(distance, pathLength);
    if (computedDistance < 0)
      computedDistance += pathLength;
  } else {
    computedDistance = clampTo<float>(distance, 0, pathLength);
  }

  FloatPoint point;
  float angle;
  motionPath.path().pointAndNormalAtLength(computedDistance, point, angle);

  if (motionData.m_rotation.type == OffsetRotationFixed)
    angle = 0;

  float originShiftX = 0;
  float originShiftY = 0;
  if (RuntimeEnabledFeatures::cssOffsetPositionAnchorEnabled()) {
    // TODO(ericwilligers): crbug.com/638055 Support offset-anchor: auto.
    const LengthPoint& anchor = offsetAnchor();
    originShiftX = floatValueForLength(anchor.x(), boundingBox.width()) -
                   floatValueForLength(transformOriginX(), boundingBox.width());
    originShiftY =
        floatValueForLength(anchor.y(), boundingBox.height()) -
        floatValueForLength(transformOriginY(), boundingBox.height());
  }

  transform.translate(point.x() - originX + originShiftX,
                      point.y() - originY + originShiftY);
  transform.rotate(angle + motionData.m_rotation.angle);

  if (RuntimeEnabledFeatures::cssOffsetPositionAnchorEnabled()) {
    transform.translate(-originShiftX, -originShiftY);
  }
}

void ComputedStyle::setTextShadow(PassRefPtr<ShadowList> s) {
  m_rareInheritedData.access()->textShadow = s;
}

void ComputedStyle::setBoxShadow(PassRefPtr<ShadowList> s) {
  m_rareNonInheritedData.access()->m_boxShadow = s;
}

static FloatRoundedRect::Radii calcRadiiFor(const BorderData& border,
                                            LayoutSize size) {
  return FloatRoundedRect::Radii(
      FloatSize(
          floatValueForLength(border.topLeft().width(), size.width().toFloat()),
          floatValueForLength(border.topLeft().height(),
                              size.height().toFloat())),
      FloatSize(floatValueForLength(border.topRight().width(),
                                    size.width().toFloat()),
                floatValueForLength(border.topRight().height(),
                                    size.height().toFloat())),
      FloatSize(floatValueForLength(border.bottomLeft().width(),
                                    size.width().toFloat()),
                floatValueForLength(border.bottomLeft().height(),
                                    size.height().toFloat())),
      FloatSize(floatValueForLength(border.bottomRight().width(),
                                    size.width().toFloat()),
                floatValueForLength(border.bottomRight().height(),
                                    size.height().toFloat())));
}

StyleImage* ComputedStyle::listStyleImage() const {
  return m_rareInheritedData->listStyleImage.get();
}
void ComputedStyle::setListStyleImage(StyleImage* v) {
  if (m_rareInheritedData->listStyleImage != v)
    m_rareInheritedData.access()->listStyleImage = v;
}

Color ComputedStyle::color() const {
  return m_styleInheritedData->color;
}
Color ComputedStyle::visitedLinkColor() const {
  return m_styleInheritedData->visitedLinkColor;
}
void ComputedStyle::setColor(const Color& v) {
  SET_VAR(m_styleInheritedData, color, v);
}
void ComputedStyle::setVisitedLinkColor(const Color& v) {
  SET_VAR(m_styleInheritedData, visitedLinkColor, v);
}

short ComputedStyle::horizontalBorderSpacing() const {
  return m_styleInheritedData->horizontal_border_spacing;
}
short ComputedStyle::verticalBorderSpacing() const {
  return m_styleInheritedData->vertical_border_spacing;
}
void ComputedStyle::setHorizontalBorderSpacing(short v) {
  SET_VAR(m_styleInheritedData, horizontal_border_spacing, v);
}
void ComputedStyle::setVerticalBorderSpacing(short v) {
  SET_VAR(m_styleInheritedData, vertical_border_spacing, v);
}

FloatRoundedRect ComputedStyle::getRoundedBorderFor(
    const LayoutRect& borderRect,
    bool includeLogicalLeftEdge,
    bool includeLogicalRightEdge) const {
  FloatRoundedRect roundedRect(pixelSnappedIntRect(borderRect));
  if (hasBorderRadius()) {
    FloatRoundedRect::Radii radii =
        calcRadiiFor(m_surround->border, borderRect.size());
    roundedRect.includeLogicalEdges(radii, isHorizontalWritingMode(),
                                    includeLogicalLeftEdge,
                                    includeLogicalRightEdge);
    roundedRect.constrainRadii();
  }
  return roundedRect;
}

FloatRoundedRect ComputedStyle::getRoundedInnerBorderFor(
    const LayoutRect& borderRect,
    bool includeLogicalLeftEdge,
    bool includeLogicalRightEdge) const {
  bool horizontal = isHorizontalWritingMode();

  int leftWidth =
      (!horizontal || includeLogicalLeftEdge) ? borderLeftWidth() : 0;
  int rightWidth =
      (!horizontal || includeLogicalRightEdge) ? borderRightWidth() : 0;
  int topWidth = (horizontal || includeLogicalLeftEdge) ? borderTopWidth() : 0;
  int bottomWidth =
      (horizontal || includeLogicalRightEdge) ? borderBottomWidth() : 0;

  return getRoundedInnerBorderFor(
      borderRect,
      LayoutRectOutsets(-topWidth, -rightWidth, -bottomWidth, -leftWidth),
      includeLogicalLeftEdge, includeLogicalRightEdge);
}

FloatRoundedRect ComputedStyle::getRoundedInnerBorderFor(
    const LayoutRect& borderRect,
    const LayoutRectOutsets insets,
    bool includeLogicalLeftEdge,
    bool includeLogicalRightEdge) const {
  LayoutRect innerRect(borderRect);
  innerRect.expand(insets);

  FloatRoundedRect roundedRect(pixelSnappedIntRect(innerRect));

  if (hasBorderRadius()) {
    FloatRoundedRect::Radii radii = getRoundedBorderFor(borderRect).getRadii();
    // Insets use negative values.
    radii.shrink(-insets.top().toFloat(), -insets.bottom().toFloat(),
                 -insets.left().toFloat(), -insets.right().toFloat());
    roundedRect.includeLogicalEdges(radii, isHorizontalWritingMode(),
                                    includeLogicalLeftEdge,
                                    includeLogicalRightEdge);
  }
  return roundedRect;
}

static bool allLayersAreFixed(const FillLayer& layer) {
  for (const FillLayer* currLayer = &layer; currLayer;
       currLayer = currLayer->next()) {
    if (!currLayer->image() ||
        currLayer->attachment() != FixedBackgroundAttachment)
      return false;
  }

  return true;
}

bool ComputedStyle::hasEntirelyFixedBackground() const {
  return allLayersAreFixed(backgroundLayers());
}

const CounterDirectiveMap* ComputedStyle::counterDirectives() const {
  return m_rareNonInheritedData->m_counterDirectives.get();
}

CounterDirectiveMap& ComputedStyle::accessCounterDirectives() {
  std::unique_ptr<CounterDirectiveMap>& map =
      m_rareNonInheritedData.access()->m_counterDirectives;
  if (!map)
    map = wrapUnique(new CounterDirectiveMap);
  return *map;
}

const CounterDirectives ComputedStyle::getCounterDirectives(
    const AtomicString& identifier) const {
  if (const CounterDirectiveMap* directives = counterDirectives())
    return directives->get(identifier);
  return CounterDirectives();
}

void ComputedStyle::clearIncrementDirectives() {
  if (!counterDirectives())
    return;

  // This makes us copy even if we may not be removing any items.
  CounterDirectiveMap& map = accessCounterDirectives();
  typedef CounterDirectiveMap::iterator Iterator;

  Iterator end = map.end();
  for (Iterator it = map.begin(); it != end; ++it)
    it->value.clearIncrement();
}

void ComputedStyle::clearResetDirectives() {
  if (!counterDirectives())
    return;

  // This makes us copy even if we may not be removing any items.
  CounterDirectiveMap& map = accessCounterDirectives();
  typedef CounterDirectiveMap::iterator Iterator;

  Iterator end = map.end();
  for (Iterator it = map.begin(); it != end; ++it)
    it->value.clearReset();
}

Hyphenation* ComputedStyle::getHyphenation() const {
  return getHyphens() == HyphensAuto
             ? getFontDescription().localeOrDefault().getHyphenation()
             : nullptr;
}

const AtomicString& ComputedStyle::hyphenString() const {
  const AtomicString& hyphenationString =
      m_rareInheritedData.get()->hyphenationString;
  if (!hyphenationString.isNull())
    return hyphenationString;

  // FIXME: This should depend on locale.
  DEFINE_STATIC_LOCAL(AtomicString, hyphenMinusString,
                      (&hyphenMinusCharacter, 1));
  DEFINE_STATIC_LOCAL(AtomicString, hyphenString, (&hyphenCharacter, 1));
  const SimpleFontData* primaryFont = font().primaryFont();
  DCHECK(primaryFont);
  return primaryFont && primaryFont->glyphForCharacter(hyphenCharacter)
             ? hyphenString
             : hyphenMinusString;
}

const AtomicString& ComputedStyle::textEmphasisMarkString() const {
  switch (getTextEmphasisMark()) {
    case TextEmphasisMarkNone:
      return nullAtom;
    case TextEmphasisMarkCustom:
      return textEmphasisCustomMark();
    case TextEmphasisMarkDot: {
      DEFINE_STATIC_LOCAL(AtomicString, filledDotString, (&bulletCharacter, 1));
      DEFINE_STATIC_LOCAL(AtomicString, openDotString,
                          (&whiteBulletCharacter, 1));
      return getTextEmphasisFill() == TextEmphasisFillFilled ? filledDotString
                                                             : openDotString;
    }
    case TextEmphasisMarkCircle: {
      DEFINE_STATIC_LOCAL(AtomicString, filledCircleString,
                          (&blackCircleCharacter, 1));
      DEFINE_STATIC_LOCAL(AtomicString, openCircleString,
                          (&whiteCircleCharacter, 1));
      return getTextEmphasisFill() == TextEmphasisFillFilled
                 ? filledCircleString
                 : openCircleString;
    }
    case TextEmphasisMarkDoubleCircle: {
      DEFINE_STATIC_LOCAL(AtomicString, filledDoubleCircleString,
                          (&fisheyeCharacter, 1));
      DEFINE_STATIC_LOCAL(AtomicString, openDoubleCircleString,
                          (&bullseyeCharacter, 1));
      return getTextEmphasisFill() == TextEmphasisFillFilled
                 ? filledDoubleCircleString
                 : openDoubleCircleString;
    }
    case TextEmphasisMarkTriangle: {
      DEFINE_STATIC_LOCAL(AtomicString, filledTriangleString,
                          (&blackUpPointingTriangleCharacter, 1));
      DEFINE_STATIC_LOCAL(AtomicString, openTriangleString,
                          (&whiteUpPointingTriangleCharacter, 1));
      return getTextEmphasisFill() == TextEmphasisFillFilled
                 ? filledTriangleString
                 : openTriangleString;
    }
    case TextEmphasisMarkSesame: {
      DEFINE_STATIC_LOCAL(AtomicString, filledSesameString,
                          (&sesameDotCharacter, 1));
      DEFINE_STATIC_LOCAL(AtomicString, openSesameString,
                          (&whiteSesameDotCharacter, 1));
      return getTextEmphasisFill() == TextEmphasisFillFilled
                 ? filledSesameString
                 : openSesameString;
    }
    case TextEmphasisMarkAuto:
      ASSERT_NOT_REACHED();
      return nullAtom;
  }

  ASSERT_NOT_REACHED();
  return nullAtom;
}

CSSAnimationData& ComputedStyle::accessAnimations() {
  if (!m_rareNonInheritedData.access()->m_animations)
    m_rareNonInheritedData.access()->m_animations = CSSAnimationData::create();
  return *m_rareNonInheritedData->m_animations;
}

CSSTransitionData& ComputedStyle::accessTransitions() {
  if (!m_rareNonInheritedData.access()->m_transitions)
    m_rareNonInheritedData.access()->m_transitions =
        CSSTransitionData::create();
  return *m_rareNonInheritedData->m_transitions;
}

const Font& ComputedStyle::font() const {
  return m_styleInheritedData->font;
}
const FontDescription& ComputedStyle::getFontDescription() const {
  return m_styleInheritedData->font.getFontDescription();
}
float ComputedStyle::specifiedFontSize() const {
  return getFontDescription().specifiedSize();
}
float ComputedStyle::computedFontSize() const {
  return getFontDescription().computedSize();
}
int ComputedStyle::fontSize() const {
  return getFontDescription().computedPixelSize();
}
float ComputedStyle::fontSizeAdjust() const {
  return getFontDescription().sizeAdjust();
}
bool ComputedStyle::hasFontSizeAdjust() const {
  return getFontDescription().hasSizeAdjust();
}
FontWeight ComputedStyle::fontWeight() const {
  return getFontDescription().weight();
}
FontStretch ComputedStyle::fontStretch() const {
  return getFontDescription().stretch();
}

TextDecoration ComputedStyle::textDecorationsInEffect() const {
  int decorations = 0;

  const Vector<AppliedTextDecoration>& applied = appliedTextDecorations();

  for (size_t i = 0; i < applied.size(); ++i)
    decorations |= applied[i].line();

  return static_cast<TextDecoration>(decorations);
}

const Vector<AppliedTextDecoration>& ComputedStyle::appliedTextDecorations()
    const {
  if (!m_inheritedData.m_textUnderline &&
      !m_rareInheritedData->appliedTextDecorations) {
    DEFINE_STATIC_LOCAL(Vector<AppliedTextDecoration>, empty, ());
    return empty;
  }
  if (m_inheritedData.m_textUnderline) {
    DEFINE_STATIC_LOCAL(Vector<AppliedTextDecoration>, underline,
                        (1, AppliedTextDecoration(TextDecorationUnderline)));
    return underline;
  }

  return m_rareInheritedData->appliedTextDecorations->vector();
}

StyleInheritedVariables* ComputedStyle::inheritedVariables() const {
  return m_rareInheritedData->variables.get();
}

StyleNonInheritedVariables* ComputedStyle::nonInheritedVariables() const {
  return m_rareNonInheritedData->m_variables.get();
}

StyleInheritedVariables& ComputedStyle::mutableInheritedVariables() {
  RefPtr<StyleInheritedVariables>& variables =
      m_rareInheritedData.access()->variables;
  if (!variables)
    variables = StyleInheritedVariables::create();
  else if (!variables->hasOneRef())
    variables = variables->copy();
  return *variables;
}

StyleNonInheritedVariables& ComputedStyle::mutableNonInheritedVariables() {
  std::unique_ptr<StyleNonInheritedVariables>& variables =
      m_rareNonInheritedData.access()->m_variables;
  if (!variables)
    variables = StyleNonInheritedVariables::create();
  return *variables;
}

void ComputedStyle::setUnresolvedInheritedVariable(
    const AtomicString& name,
    PassRefPtr<CSSVariableData> value) {
  DCHECK(value && value->needsVariableResolution());
  mutableInheritedVariables().setVariable(name, std::move(value));
}

void ComputedStyle::setUnresolvedNonInheritedVariable(
    const AtomicString& name,
    PassRefPtr<CSSVariableData> value) {
  DCHECK(value && value->needsVariableResolution());
  mutableNonInheritedVariables().setVariable(name, std::move(value));
}

void ComputedStyle::setResolvedUnregisteredVariable(
    const AtomicString& name,
    PassRefPtr<CSSVariableData> value) {
  DCHECK(value && !value->needsVariableResolution());
  mutableInheritedVariables().setVariable(name, std::move(value));
}

void ComputedStyle::setResolvedInheritedVariable(
    const AtomicString& name,
    PassRefPtr<CSSVariableData> value,
    const CSSValue* parsedValue) {
  DCHECK(!!value == !!parsedValue);
  DCHECK(!(value && value->needsVariableResolution()));

  StyleInheritedVariables& variables = mutableInheritedVariables();
  variables.setVariable(name, std::move(value));
  variables.setRegisteredVariable(name, parsedValue);
}

void ComputedStyle::setResolvedNonInheritedVariable(
    const AtomicString& name,
    PassRefPtr<CSSVariableData> value,
    const CSSValue* parsedValue) {
  DCHECK(!!value == !!parsedValue);
  DCHECK(!(value && value->needsVariableResolution()));

  StyleNonInheritedVariables& variables = mutableNonInheritedVariables();
  variables.setVariable(name, std::move(value));
  variables.setRegisteredVariable(name, parsedValue);
}

void ComputedStyle::removeInheritedVariable(const AtomicString& name) {
  mutableInheritedVariables().removeVariable(name);
}

void ComputedStyle::removeNonInheritedVariable(const AtomicString& name) {
  mutableNonInheritedVariables().removeVariable(name);
}

CSSVariableData* ComputedStyle::getVariable(const AtomicString& name) const {
  if (inheritedVariables()) {
    if (CSSVariableData* variable = inheritedVariables()->getVariable(name))
      return variable;
  }
  if (nonInheritedVariables()) {
    if (CSSVariableData* variable = nonInheritedVariables()->getVariable(name))
      return variable;
  }
  return nullptr;
}

float ComputedStyle::wordSpacing() const {
  return getFontDescription().wordSpacing();
}
float ComputedStyle::letterSpacing() const {
  return getFontDescription().letterSpacing();
}

bool ComputedStyle::setFontDescription(const FontDescription& v) {
  if (m_styleInheritedData->font.getFontDescription() != v) {
    m_styleInheritedData.access()->font = Font(v);
    return true;
  }
  return false;
}

void ComputedStyle::setFont(const Font& font) {
  m_styleInheritedData.access()->font = font;
}

bool ComputedStyle::hasIdenticalAscentDescentAndLineGap(
    const ComputedStyle& other) const {
  const SimpleFontData* fontData = font().primaryFont();
  const SimpleFontData* otherFontData = other.font().primaryFont();
  return fontData && otherFontData &&
         fontData->getFontMetrics().hasIdenticalAscentDescentAndLineGap(
             otherFontData->getFontMetrics());
}

const Length& ComputedStyle::specifiedLineHeight() const {
  return m_styleInheritedData->line_height;
}
Length ComputedStyle::lineHeight() const {
  const Length& lh = m_styleInheritedData->line_height;
  // Unlike getFontDescription().computedSize() and hence fontSize(), this is
  // recalculated on demand as we only store the specified line height.
  // FIXME: Should consider scaling the fixed part of any calc expressions
  // too, though this involves messily poking into CalcExpressionLength.
  if (lh.isFixed()) {
    float multiplier = textAutosizingMultiplier();
    return Length(
        TextAutosizer::computeAutosizedFontSize(lh.value(), multiplier), Fixed);
  }

  return lh;
}

void ComputedStyle::setLineHeight(const Length& specifiedLineHeight) {
  SET_VAR(m_styleInheritedData, line_height, specifiedLineHeight);
}

int ComputedStyle::computedLineHeight() const {
  const Length& lh = lineHeight();

  // Negative value means the line height is not set. Use the font's built-in
  // spacing, if avalible.
  if (lh.isNegative() && font().primaryFont())
    return font().primaryFont()->getFontMetrics().lineSpacing();

  if (lh.isPercentOrCalc())
    return minimumValueForLength(lh, LayoutUnit(computedFontSize())).toInt();

  return std::min(lh.value(), LayoutUnit::max().toFloat());
}

void ComputedStyle::setWordSpacing(float wordSpacing) {
  FontSelector* currentFontSelector = font().getFontSelector();
  FontDescription desc(getFontDescription());
  desc.setWordSpacing(wordSpacing);
  setFontDescription(desc);
  font().update(currentFontSelector);
}

void ComputedStyle::setLetterSpacing(float letterSpacing) {
  FontSelector* currentFontSelector = font().getFontSelector();
  FontDescription desc(getFontDescription());
  desc.setLetterSpacing(letterSpacing);
  setFontDescription(desc);
  font().update(currentFontSelector);
}

void ComputedStyle::setTextAutosizingMultiplier(float multiplier) {
  SET_VAR(m_styleInheritedData, textAutosizingMultiplier, multiplier);

  float size = specifiedFontSize();

  ASSERT(std::isfinite(size));
  if (!std::isfinite(size) || size < 0)
    size = 0;
  else
    size = std::min(maximumAllowedFontSize, size);

  FontSelector* currentFontSelector = font().getFontSelector();
  FontDescription desc(getFontDescription());
  desc.setSpecifiedSize(size);
  desc.setComputedSize(size);

  float autosizedFontSize =
      TextAutosizer::computeAutosizedFontSize(size, multiplier);
  desc.setComputedSize(std::min(maximumAllowedFontSize, autosizedFontSize));

  setFontDescription(desc);
  font().update(currentFontSelector);
}

void ComputedStyle::addAppliedTextDecoration(
    const AppliedTextDecoration& decoration) {
  RefPtr<AppliedTextDecorationList>& list =
      m_rareInheritedData.access()->appliedTextDecorations;

  if (!list)
    list = AppliedTextDecorationList::create();
  else if (!list->hasOneRef())
    list = list->copy();

  if (m_inheritedData.m_textUnderline) {
    m_inheritedData.m_textUnderline = false;
    list->append(AppliedTextDecoration(TextDecorationUnderline));
  }

  list->append(decoration);
}

void ComputedStyle::applyTextDecorations() {
  if (getTextDecoration() == TextDecorationNone)
    return;

  TextDecorationStyle style = getTextDecorationStyle();
  StyleColor styleColor =
      decorationColorIncludingFallback(insideLink() == InsideVisitedLink);

  int decorations = getTextDecoration();

  if (decorations & TextDecorationUnderline) {
    // To save memory, we don't use AppliedTextDecoration objects in the
    // common case of a single simple underline.
    AppliedTextDecoration underline(TextDecorationUnderline, style, styleColor);

    if (!m_rareInheritedData->appliedTextDecorations &&
        underline.isSimpleUnderline())
      m_inheritedData.m_textUnderline = true;
    else
      addAppliedTextDecoration(underline);
  }
  if (decorations & TextDecorationOverline)
    addAppliedTextDecoration(
        AppliedTextDecoration(TextDecorationOverline, style, styleColor));
  if (decorations & TextDecorationLineThrough)
    addAppliedTextDecoration(
        AppliedTextDecoration(TextDecorationLineThrough, style, styleColor));
}

void ComputedStyle::clearAppliedTextDecorations() {
  m_inheritedData.m_textUnderline = false;

  if (m_rareInheritedData->appliedTextDecorations)
    m_rareInheritedData.access()->appliedTextDecorations = nullptr;
}

void ComputedStyle::restoreParentTextDecorations(
    const ComputedStyle& parentStyle) {
  m_inheritedData.m_textUnderline = parentStyle.m_inheritedData.m_textUnderline;
  if (m_rareInheritedData->appliedTextDecorations !=
      parentStyle.m_rareInheritedData->appliedTextDecorations)
    m_rareInheritedData.access()->appliedTextDecorations =
        parentStyle.m_rareInheritedData->appliedTextDecorations;
}

void ComputedStyle::clearMultiCol() {
  m_rareNonInheritedData.access()->m_multiCol = nullptr;
  m_rareNonInheritedData.access()->m_multiCol.init();
}

StyleColor ComputedStyle::decorationColorIncludingFallback(
    bool visitedLink) const {
  StyleColor styleColor =
      visitedLink ? visitedLinkTextDecorationColor() : textDecorationColor();

  if (!styleColor.isCurrentColor())
    return styleColor;

  if (textStrokeWidth()) {
    // Prefer stroke color if possible, but not if it's fully transparent.
    StyleColor textStrokeStyleColor =
        visitedLink ? visitedLinkTextStrokeColor() : textStrokeColor();
    if (!textStrokeStyleColor.isCurrentColor() &&
        textStrokeStyleColor.getColor().alpha())
      return textStrokeStyleColor;
  }

  return visitedLink ? visitedLinkTextFillColor() : textFillColor();
}

Color ComputedStyle::colorIncludingFallback(int colorProperty,
                                            bool visitedLink) const {
  StyleColor result(StyleColor::currentColor());
  EBorderStyle borderStyle = BorderStyleNone;
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
      result =
          visitedLink ? visitedLinkBorderBottomColor() : borderBottomColor();
      borderStyle = borderBottomStyle();
      break;
    case CSSPropertyColor:
      result = visitedLink ? visitedLinkColor() : color();
      break;
    case CSSPropertyOutlineColor:
      result = visitedLink ? visitedLinkOutlineColor() : outlineColor();
      break;
    case CSSPropertyColumnRuleColor:
      result = visitedLink ? visitedLinkColumnRuleColor() : columnRuleColor();
      break;
    case CSSPropertyWebkitTextEmphasisColor:
      result =
          visitedLink ? visitedLinkTextEmphasisColor() : textEmphasisColor();
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
    case CSSPropertyTextDecorationColor:
      result = decorationColorIncludingFallback(visitedLink);
      break;
    default:
      ASSERT_NOT_REACHED();
      break;
  }

  if (!result.isCurrentColor())
    return result.getColor();

  // FIXME: Treating styled borders with initial color differently causes
  // problems, see crbug.com/316559, crbug.com/276231
  if (!visitedLink &&
      (borderStyle == BorderStyleInset || borderStyle == BorderStyleOutset ||
       borderStyle == BorderStyleRidge || borderStyle == BorderStyleGroove))
    return Color(238, 238, 238);
  return visitedLink ? visitedLinkColor() : color();
}

Color ComputedStyle::visitedDependentColor(int colorProperty) const {
  Color unvisitedColor = colorIncludingFallback(colorProperty, false);
  if (insideLink() != InsideVisitedLink)
    return unvisitedColor;

  Color visitedColor = colorIncludingFallback(colorProperty, true);

  // FIXME: Technically someone could explicitly specify the color transparent,
  // but for now we'll just assume that if the background color is transparent
  // that it wasn't set. Note that it's weird that we're returning unvisited
  // info for a visited link, but given our restriction that the alpha values
  // have to match, it makes more sense to return the unvisited background color
  // if specified than it does to return black. This behavior matches what
  // Firefox 4 does as well.
  if (colorProperty == CSSPropertyBackgroundColor &&
      visitedColor == Color::transparent)
    return unvisitedColor;

  // Take the alpha from the unvisited color, but get the RGB values from the
  // visited color.
  return Color(visitedColor.red(), visitedColor.green(), visitedColor.blue(),
               unvisitedColor.alpha());
}

const BorderValue& ComputedStyle::borderBefore() const {
  switch (getWritingMode()) {
    case TopToBottomWritingMode:
      return borderTop();
    case LeftToRightWritingMode:
      return borderLeft();
    case RightToLeftWritingMode:
      return borderRight();
  }
  ASSERT_NOT_REACHED();
  return borderTop();
}

const BorderValue& ComputedStyle::borderAfter() const {
  switch (getWritingMode()) {
    case TopToBottomWritingMode:
      return borderBottom();
    case LeftToRightWritingMode:
      return borderRight();
    case RightToLeftWritingMode:
      return borderLeft();
  }
  ASSERT_NOT_REACHED();
  return borderBottom();
}

const BorderValue& ComputedStyle::borderStart() const {
  if (isHorizontalWritingMode())
    return isLeftToRightDirection() ? borderLeft() : borderRight();
  return isLeftToRightDirection() ? borderTop() : borderBottom();
}

const BorderValue& ComputedStyle::borderEnd() const {
  if (isHorizontalWritingMode())
    return isLeftToRightDirection() ? borderRight() : borderLeft();
  return isLeftToRightDirection() ? borderBottom() : borderTop();
}

int ComputedStyle::borderBeforeWidth() const {
  switch (getWritingMode()) {
    case TopToBottomWritingMode:
      return borderTopWidth();
    case LeftToRightWritingMode:
      return borderLeftWidth();
    case RightToLeftWritingMode:
      return borderRightWidth();
  }
  ASSERT_NOT_REACHED();
  return borderTopWidth();
}

int ComputedStyle::borderAfterWidth() const {
  switch (getWritingMode()) {
    case TopToBottomWritingMode:
      return borderBottomWidth();
    case LeftToRightWritingMode:
      return borderRightWidth();
    case RightToLeftWritingMode:
      return borderLeftWidth();
  }
  ASSERT_NOT_REACHED();
  return borderBottomWidth();
}

int ComputedStyle::borderStartWidth() const {
  if (isHorizontalWritingMode())
    return isLeftToRightDirection() ? borderLeftWidth() : borderRightWidth();
  return isLeftToRightDirection() ? borderTopWidth() : borderBottomWidth();
}

int ComputedStyle::borderEndWidth() const {
  if (isHorizontalWritingMode())
    return isLeftToRightDirection() ? borderRightWidth() : borderLeftWidth();
  return isLeftToRightDirection() ? borderBottomWidth() : borderTopWidth();
}

int ComputedStyle::borderOverWidth() const {
  return isHorizontalWritingMode() ? borderTopWidth() : borderRightWidth();
}

int ComputedStyle::borderUnderWidth() const {
  return isHorizontalWritingMode() ? borderBottomWidth() : borderLeftWidth();
}

void ComputedStyle::setMarginStart(const Length& margin) {
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

void ComputedStyle::setMarginEnd(const Length& margin) {
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

void ComputedStyle::setOffsetPath(PassRefPtr<StylePath> path) {
  m_rareNonInheritedData.access()->m_transform.access()->m_motion.m_path = path;
}

int ComputedStyle::outlineOutsetExtent() const {
  if (!hasOutline())
    return 0;
  if (outlineStyleIsAuto()) {
    return GraphicsContext::focusRingOutsetExtent(
        outlineOffset(), std::ceil(getOutlineStrokeWidthForFocusRing()));
  }
  return std::max(0, saturatedAddition(outlineWidth(), outlineOffset()));
}

float ComputedStyle::getOutlineStrokeWidthForFocusRing() const {
#if OS(MACOSX)
  return outlineWidth();
#else
  // Draw an outline with thickness in proportion to the zoom level, but never
  // less than 1 pixel so that it remains visible.
  return std::max(effectiveZoom(), 1.f);
#endif
}

bool ComputedStyle::columnRuleEquivalent(
    const ComputedStyle* otherStyle) const {
  return columnRuleStyle() == otherStyle->columnRuleStyle() &&
         columnRuleWidth() == otherStyle->columnRuleWidth() &&
         visitedDependentColor(CSSPropertyColumnRuleColor) ==
             otherStyle->visitedDependentColor(CSSPropertyColumnRuleColor);
}

TextEmphasisMark ComputedStyle::getTextEmphasisMark() const {
  TextEmphasisMark mark =
      static_cast<TextEmphasisMark>(m_rareInheritedData->textEmphasisMark);
  if (mark != TextEmphasisMarkAuto)
    return mark;

  if (isHorizontalWritingMode())
    return TextEmphasisMarkDot;

  return TextEmphasisMarkSesame;
}

Color ComputedStyle::initialTapHighlightColor() {
  return LayoutTheme::tapHighlightColor();
}

const FilterOperations& ComputedStyle::initialFilter() {
  DEFINE_STATIC_LOCAL(FilterOperationsWrapper, ops,
                      (FilterOperationsWrapper::create()));
  return ops.operations();
}

const FilterOperations& ComputedStyle::initialBackdropFilter() {
  DEFINE_STATIC_LOCAL(FilterOperationsWrapper, ops,
                      (FilterOperationsWrapper::create()));
  return ops.operations();
}

LayoutRectOutsets ComputedStyle::imageOutsets(
    const NinePieceImage& image) const {
  return LayoutRectOutsets(
      NinePieceImage::computeOutset(image.outset().top(), borderTopWidth()),
      NinePieceImage::computeOutset(image.outset().right(), borderRightWidth()),
      NinePieceImage::computeOutset(image.outset().bottom(),
                                    borderBottomWidth()),
      NinePieceImage::computeOutset(image.outset().left(), borderLeftWidth()));
}

void ComputedStyle::setBorderImageSource(StyleImage* image) {
  if (m_surround->border.m_image.image() == image)
    return;
  m_surround.access()->border.m_image.setImage(image);
}

void ComputedStyle::setBorderImageSlices(const LengthBox& slices) {
  if (m_surround->border.m_image.imageSlices() == slices)
    return;
  m_surround.access()->border.m_image.setImageSlices(slices);
}

void ComputedStyle::setBorderImageSlicesFill(bool fill) {
  if (m_surround->border.m_image.fill() == fill)
    return;
  m_surround.access()->border.m_image.setFill(fill);
}

void ComputedStyle::setBorderImageWidth(const BorderImageLengthBox& slices) {
  if (m_surround->border.m_image.borderSlices() == slices)
    return;
  m_surround.access()->border.m_image.setBorderSlices(slices);
}

void ComputedStyle::setBorderImageOutset(const BorderImageLengthBox& outset) {
  if (m_surround->border.m_image.outset() == outset)
    return;
  m_surround.access()->border.m_image.setOutset(outset);
}

bool ComputedStyle::borderObscuresBackground() const {
  if (!hasBorder())
    return false;

  // Bail if we have any border-image for now. We could look at the image alpha
  // to improve this.
  if (borderImage().image())
    return false;

  BorderEdge edges[4];
  getBorderEdgeInfo(edges);

  for (int i = BSTop; i <= BSLeft; ++i) {
    const BorderEdge& currEdge = edges[i];
    if (!currEdge.obscuresBackground())
      return false;
  }

  return true;
}

void ComputedStyle::getBorderEdgeInfo(BorderEdge edges[],
                                      bool includeLogicalLeftEdge,
                                      bool includeLogicalRightEdge) const {
  bool horizontal = isHorizontalWritingMode();

  edges[BSTop] = BorderEdge(
      borderTopWidth(), visitedDependentColor(CSSPropertyBorderTopColor),
      borderTopStyle(), horizontal || includeLogicalLeftEdge);

  edges[BSRight] = BorderEdge(
      borderRightWidth(), visitedDependentColor(CSSPropertyBorderRightColor),
      borderRightStyle(), !horizontal || includeLogicalRightEdge);

  edges[BSBottom] = BorderEdge(
      borderBottomWidth(), visitedDependentColor(CSSPropertyBorderBottomColor),
      borderBottomStyle(), horizontal || includeLogicalRightEdge);

  edges[BSLeft] = BorderEdge(
      borderLeftWidth(), visitedDependentColor(CSSPropertyBorderLeftColor),
      borderLeftStyle(), !horizontal || includeLogicalLeftEdge);
}

void ComputedStyle::copyChildDependentFlagsFrom(const ComputedStyle& other) {
  setEmptyState(other.emptyState());
  if (other.hasExplicitlyInheritedProperties())
    setHasExplicitlyInheritedProperties();
}

bool ComputedStyle::shadowListHasCurrentColor(const ShadowList* shadowList) {
  if (!shadowList)
    return false;
  for (size_t i = shadowList->shadows().size(); i--;) {
    if (shadowList->shadows()[i].color().isCurrentColor())
      return true;
  }
  return false;
}

static inline Vector<GridTrackSize> initialGridAutoTracks() {
  Vector<GridTrackSize> trackSizeList;
  trackSizeList.reserveInitialCapacity(1);
  trackSizeList.uncheckedAppend(GridTrackSize(Length(Auto)));
  return trackSizeList;
}

Vector<GridTrackSize> ComputedStyle::initialGridAutoColumns() {
  return initialGridAutoTracks();
}

Vector<GridTrackSize> ComputedStyle::initialGridAutoRows() {
  return initialGridAutoTracks();
}

int adjustForAbsoluteZoom(int value, float zoomFactor) {
  if (zoomFactor == 1)
    return value;
  // Needed because computeLengthInt truncates (rather than rounds) when scaling
  // up.
  float fvalue = value;
  if (zoomFactor > 1) {
    if (value < 0)
      fvalue -= 0.5f;
    else
      fvalue += 0.5f;
  }

  return roundForImpreciseConversion<int>(fvalue / zoomFactor);
}

}  // namespace blink

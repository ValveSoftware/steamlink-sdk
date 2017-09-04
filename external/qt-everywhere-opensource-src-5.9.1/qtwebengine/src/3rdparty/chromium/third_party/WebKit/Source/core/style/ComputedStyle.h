/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef ComputedStyle_h
#define ComputedStyle_h

#include "core/CSSPropertyNames.h"
#include "core/ComputedStyleBase.h"
#include "core/CoreExport.h"
#include "core/style/BorderValue.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/style/CounterDirectives.h"
#include "core/style/DataRef.h"
#include "core/style/LineClampValue.h"
#include "core/style/NinePieceImage.h"
#include "core/style/SVGComputedStyle.h"
#include "core/style/StyleBackgroundData.h"
#include "core/style/StyleBoxData.h"
#include "core/style/StyleContentAlignmentData.h"
#include "core/style/StyleDeprecatedFlexibleBoxData.h"
#include "core/style/StyleDifference.h"
#include "core/style/StyleFilterData.h"
#include "core/style/StyleFlexibleBoxData.h"
#include "core/style/StyleGridData.h"
#include "core/style/StyleGridItemData.h"
#include "core/style/StyleInheritedData.h"
#include "core/style/StyleMultiColData.h"
#include "core/style/StyleOffsetRotation.h"
#include "core/style/StyleRareInheritedData.h"
#include "core/style/StyleRareNonInheritedData.h"
#include "core/style/StyleReflection.h"
#include "core/style/StyleSelfAlignmentData.h"
#include "core/style/StyleSurroundData.h"
#include "core/style/StyleTransformData.h"
#include "core/style/StyleVisualData.h"
#include "core/style/StyleWillChangeData.h"
#include "core/style/TransformOrigin.h"
#include "platform/Length.h"
#include "platform/LengthBox.h"
#include "platform/LengthPoint.h"
#include "platform/LengthSize.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ThemeTypes.h"
#include "platform/fonts/FontDescription.h"
#include "platform/geometry/FloatRoundedRect.h"
#include "platform/geometry/LayoutRectOutsets.h"
#include "platform/graphics/Color.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/text/TextDirection.h"
#include "platform/text/UnicodeBidi.h"
#include "platform/transforms/TransformOperations.h"
#include "wtf/Forward.h"
#include "wtf/LeakAnnotations.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include <memory>

template <typename T, typename U>
inline bool compareEqual(const T& t, const U& u) {
  return t == static_cast<T>(u);
}

template <typename T>
inline bool compareEqual(const T& a, const T& b) {
  return a == b;
}

#define SET_VAR(group, variable, value)      \
  if (!compareEqual(group->variable, value)) \
  group.access()->variable = value

#define SET_NESTED_VAR(group, base, variable, value) \
  if (!compareEqual(group->base->variable, value))   \
  group.access()->base.access()->variable = value

#define SET_VAR_WITH_SETTER(group, getter, setter, value) \
  if (!compareEqual(group->getter(), value))              \
  group.access()->setter(value)

#define SET_BORDERVALUE_COLOR(group, variable, value) \
  if (!compareEqual(group->variable.color(), value))  \
  group.access()->variable.setColor(value)

namespace blink {

using std::max;

class FilterOperations;

class AppliedTextDecoration;
class BorderData;
struct BorderEdge;
class CSSAnimationData;
class CSSTransitionData;
class CSSVariableData;
class Font;
class Hyphenation;
class RotateTransformOperation;
class ScaleTransformOperation;
class ShadowList;
class ShapeValue;
class StyleImage;
class StyleInheritedData;
class StylePath;
class StyleResolver;
class TransformationMatrix;
class TranslateTransformOperation;

class ContentData;

typedef Vector<RefPtr<ComputedStyle>, 4> PseudoStyleCache;

// ComputedStyle stores the final style for an element and provides the
// interface between the style engine and the rest of Blink.
//
// It contains all the resolved styles for an element, and is densely packed and
// optimized for memory and performance. Enums and small fields are packed in
// bit fields, while large fields are stored in pointers and shared where not
// modified from their parent value (see the DataRef class).
//
// Currently, ComputedStyle is hand-written and ComputedStyleBase is generated.
// Over time, methods will be moved to ComputedStyleBase and the generator will
// be expanded to handle more and more types of properties. Eventually, all
// methods will be on ComputedStyleBase (with custom methods defined in a class
// such as ComputedStyleBase.cpp) and ComputedStyle will be removed.
class CORE_EXPORT ComputedStyle : public ComputedStyleBase,
                                  public RefCounted<ComputedStyle> {
  // Used by Web Animations CSS. Sets the color styles.
  friend class AnimatedStyleBuilder;
  // Used by Web Animations CSS. Gets visited and unvisited colors separately.
  friend class CSSAnimatableValueFactory;
  // Used by CSS animations. We can't allow them to animate based off visited
  // colors.
  friend class CSSPropertyEquality;
  // Editing has to only reveal unvisited info.
  friend class ApplyStyleCommand;
  // Editing has to only reveal unvisited info.
  friend class EditingStyle;
  // Needs to be able to see visited and unvisited colors for devtools.
  friend class ComputedStyleCSSValueMapping;
  // Sets color styles
  friend class StyleBuilderFunctions;
  // Saves Border/Background information for later comparison.
  friend class CachedUAStyle;
  // Accesses visited and unvisited colors.
  friend class ColorPropertyFunctions;

  // FIXME: When we stop resolving currentColor at style time, these can be
  // removed.
  friend class CSSToStyleMap;
  friend class FilterOperationResolver;
  friend class StyleBuilderConverter;
  friend class StyleResolverState;
  friend class StyleResolver;

 protected:
  // non-inherited attributes
  DataRef<StyleBoxData> m_box;
  DataRef<StyleVisualData> m_visual;
  DataRef<StyleBackgroundData> m_background;
  DataRef<StyleSurroundData> m_surround;
  DataRef<StyleRareNonInheritedData> m_rareNonInheritedData;

  // inherited attributes
  DataRef<StyleRareInheritedData> m_rareInheritedData;
  DataRef<StyleInheritedData> m_styleInheritedData;

  // list of associated pseudo styles
  std::unique_ptr<PseudoStyleCache> m_cachedPseudoStyles;

  DataRef<SVGComputedStyle> m_svgStyle;

  // !START SYNC!: Keep this in sync with the copy constructor in
  // ComputedStyle.cpp and implicitlyInherited() in StyleResolver.cpp

  // inherit
  struct InheritedData {
    bool operator==(const InheritedData& other) const {
      return compareEqualIndependent(other) &&
             compareEqualNonIndependent(other);
    }

    bool operator!=(const InheritedData& other) const {
      return !(*this == other);
    }

    inline bool compareEqualIndependent(const InheritedData& other) const {
      // These must match the properties tagged 'independent' in
      // CSSProperties.in.
      // TODO(sashab): Generate this function.
      return (m_pointerEvents == other.m_pointerEvents);
    }

    inline bool compareEqualNonIndependent(const InheritedData& other) const {
      return (m_captionSide == other.m_captionSide) &&
             (m_listStyleType == other.m_listStyleType) &&
             (m_listStylePosition == other.m_listStylePosition) &&
             (m_textAlign == other.m_textAlign) &&
             (m_textTransform == other.m_textTransform) &&
             (m_textUnderline == other.m_textUnderline) &&
             (m_cursorStyle == other.m_cursorStyle) &&
             (m_direction == other.m_direction) &&
             (m_whiteSpace == other.m_whiteSpace) &&
             (m_borderCollapse == other.m_borderCollapse) &&
             (m_boxDirection == other.m_boxDirection) &&
             (m_rtlOrdering == other.m_rtlOrdering) &&
             (m_printColorAdjust == other.m_printColorAdjust) &&
             (m_insideLink == other.m_insideLink) &&
             (m_writingMode == other.m_writingMode);
    }

    unsigned m_captionSide : 2;        // ECaptionSide
    unsigned m_listStyleType : 7;      // EListStyleType
    unsigned m_listStylePosition : 1;  // EListStylePosition
    unsigned m_textAlign : 4;          // ETextAlign
    unsigned m_textTransform : 2;      // ETextTransform
    unsigned m_textUnderline : 1;
    unsigned m_cursorStyle : 6;     // ECursor
    unsigned m_direction : 1;       // TextDirection
    unsigned m_whiteSpace : 3;      // EWhiteSpace
    unsigned m_borderCollapse : 1;  // EBorderCollapse
    unsigned m_boxDirection : 1;  // EBoxDirection (CSS3 box_direction property,
                                  // flexible box layout module)
    // 32 bits

    // non CSS2 inherited
    unsigned m_rtlOrdering : 1;  // Order
    unsigned m_printColorAdjust : PrintColorAdjustBits;
    unsigned m_pointerEvents : 4;  // EPointerEvents
    unsigned m_insideLink : 2;     // EInsideLink

    // CSS Text Layout Module Level 3: Vertical writing support
    unsigned m_writingMode : 2;  // WritingMode
                                 // 42 bits
  } m_inheritedData;

  // don't inherit
  struct NonInheritedData {
    // Compare computed styles, differences in inherited bits or other flags
    // should not cause an inequality.
    bool operator==(const NonInheritedData& other) const {
      return m_effectiveDisplay == other.m_effectiveDisplay &&
             m_originalDisplay == other.m_originalDisplay &&
             m_overflowAnchor == other.m_overflowAnchor &&
             m_overflowX == other.m_overflowX &&
             m_overflowY == other.m_overflowY &&
             m_verticalAlign == other.m_verticalAlign &&
             m_clear == other.m_clear && m_position == other.m_position &&
             m_tableLayout == other.m_tableLayout &&
             m_unicodeBidi == other.m_unicodeBidi
             // hasViewportUnits
             && m_breakBefore == other.m_breakBefore &&
             m_breakAfter == other.m_breakAfter &&
             m_breakInside == other.m_breakInside;
      // styleType
      // pseudoBits
      // explicitInheritance
      // unique
      // emptyState
      // affectedByFocus
      // affectedByHover
      // affectedByActive
      // affectedByDrag
      // isLink
      // isInherited flags
    }

    bool operator!=(const NonInheritedData& other) const {
      return !(*this == other);
    }

    unsigned m_effectiveDisplay : 5;  // EDisplay
    unsigned m_originalDisplay : 5;   // EDisplay
    unsigned m_overflowAnchor : 2;    // EOverflowAnchor
    unsigned m_overflowX : 3;         // EOverflow
    unsigned m_overflowY : 3;         // EOverflow
    unsigned m_verticalAlign : 4;     // EVerticalAlign
    unsigned m_clear : 2;             // EClear
    unsigned m_position : 3;          // EPosition
    unsigned m_tableLayout : 1;       // ETableLayout
    unsigned m_unicodeBidi : 3;       // EUnicodeBidi

    // This is set if we used viewport units when resolving a length.
    // It is mutable so we can pass around const ComputedStyles to resolve
    // lengths.
    mutable unsigned m_hasViewportUnits : 1;

    // 32 bits

    unsigned m_breakBefore : 4;  // EBreak
    unsigned m_breakAfter : 4;   // EBreak
    unsigned m_breakInside : 2;  // EBreak

    unsigned m_styleType : 6;  // PseudoId
    unsigned m_pseudoBits : 8;
    unsigned m_explicitInheritance : 1;  // Explicitly inherits a non-inherited
                                         // property
    unsigned m_variableReference : 1;  // A non-inherited property references a
                                       // variable or @apply is used.
    unsigned m_unique : 1;             // Style can not be shared.

    unsigned m_emptyState : 1;

    unsigned m_affectedByFocus : 1;
    unsigned m_affectedByHover : 1;
    unsigned m_affectedByActive : 1;
    unsigned m_affectedByDrag : 1;

    // 64 bits

    unsigned m_isLink : 1;

    mutable unsigned m_hasRemUnits : 1;

    // For each independent inherited property, store a 1 if the stored
    // value was inherited from its parent, or 0 if it is explicitly set on
    // this element.
    // Eventually, all properties will have a bit in here to store whether
    // they were inherited from their parent or not.
    // Although two ComputedStyles are equal if their nonInheritedData is
    // equal regardless of the isInherited flags, this struct is stored next
    // to the existing flags to take advantage of packing as much as possible.
    // TODO(sashab): Move these flags closer to inheritedData so that it's
    // clear which inherited properties have a flag stored and which don't.
    // Keep this list of fields in sync with:
    // - setBitDefaults()
    // - The ComputedStyle setter, which must take an extra boolean parameter
    //   and set this - propagateIndependentInheritedProperties() in
    //   ComputedStyle.cpp
    // - The compareEqual() methods in the corresponding class
    // InheritedFlags
    unsigned m_isPointerEventsInherited : 1;
    unsigned m_isVisibilityInherited : 1;

    // If you add more style bits here, you will also need to update
    // ComputedStyle::copyNonInheritedFromCached() 68 bits
  } m_nonInheritedData;

  // !END SYNC!

  void setBitDefaults() {
    ComputedStyleBase::setBitDefaults();
    m_inheritedData.m_captionSide = static_cast<unsigned>(initialCaptionSide());
    m_inheritedData.m_listStyleType =
        static_cast<unsigned>(initialListStyleType());
    m_inheritedData.m_listStylePosition =
        static_cast<unsigned>(initialListStylePosition());
    m_inheritedData.m_textAlign = static_cast<unsigned>(initialTextAlign());
    m_inheritedData.m_textTransform = initialTextTransform();
    m_inheritedData.m_textUnderline = false;
    m_inheritedData.m_cursorStyle = static_cast<unsigned>(initialCursor());
    m_inheritedData.m_direction = initialDirection();
    m_inheritedData.m_whiteSpace = initialWhiteSpace();
    m_inheritedData.m_borderCollapse = initialBorderCollapse();
    m_inheritedData.m_rtlOrdering = initialRTLOrdering();
    m_inheritedData.m_boxDirection = initialBoxDirection();
    m_inheritedData.m_printColorAdjust = initialPrintColorAdjust();
    m_inheritedData.m_pointerEvents = initialPointerEvents();
    m_inheritedData.m_insideLink = NotInsideLink;
    m_inheritedData.m_writingMode = initialWritingMode();

    m_nonInheritedData.m_effectiveDisplay =
        m_nonInheritedData.m_originalDisplay =
            static_cast<unsigned>(initialDisplay());
    m_nonInheritedData.m_overflowAnchor = initialOverflowAnchor();
    m_nonInheritedData.m_overflowX = initialOverflowX();
    m_nonInheritedData.m_overflowY = initialOverflowY();
    m_nonInheritedData.m_verticalAlign = initialVerticalAlign();
    m_nonInheritedData.m_clear = initialClear();
    m_nonInheritedData.m_position = initialPosition();
    m_nonInheritedData.m_tableLayout = initialTableLayout();
    m_nonInheritedData.m_unicodeBidi = initialUnicodeBidi();
    m_nonInheritedData.m_breakBefore = initialBreakBefore();
    m_nonInheritedData.m_breakAfter = initialBreakAfter();
    m_nonInheritedData.m_breakInside = initialBreakInside();
    m_nonInheritedData.m_styleType = PseudoIdNone;
    m_nonInheritedData.m_pseudoBits = 0;
    m_nonInheritedData.m_explicitInheritance = false;
    m_nonInheritedData.m_variableReference = false;
    m_nonInheritedData.m_unique = false;
    m_nonInheritedData.m_emptyState = false;
    m_nonInheritedData.m_hasViewportUnits = false;
    m_nonInheritedData.m_affectedByFocus = false;
    m_nonInheritedData.m_affectedByHover = false;
    m_nonInheritedData.m_affectedByActive = false;
    m_nonInheritedData.m_affectedByDrag = false;
    m_nonInheritedData.m_isLink = false;
    m_nonInheritedData.m_hasRemUnits = false;

    // All independently inherited properties default to being inherited.
    m_nonInheritedData.m_isPointerEventsInherited = true;
    m_nonInheritedData.m_isVisibilityInherited = true;
  }

 private:
  // TODO(sashab): Move these to the bottom of ComputedStyle.
  ALWAYS_INLINE ComputedStyle();

  enum InitialStyleTag { InitialStyle };
  ALWAYS_INLINE explicit ComputedStyle(InitialStyleTag);
  ALWAYS_INLINE ComputedStyle(const ComputedStyle&);

  static PassRefPtr<ComputedStyle> createInitialStyle();
  static inline ComputedStyle& mutableInitialStyle() {
    LEAK_SANITIZER_DISABLED_SCOPE;
    DEFINE_STATIC_REF(ComputedStyle, s_initialStyle,
                      (ComputedStyle::createInitialStyle()));
    return *s_initialStyle;
  }

 public:
  static PassRefPtr<ComputedStyle> create();
  static PassRefPtr<ComputedStyle> createAnonymousStyleWithDisplay(
      const ComputedStyle& parentStyle,
      EDisplay);
  static PassRefPtr<ComputedStyle> clone(const ComputedStyle&);
  static const ComputedStyle& initialStyle() { return mutableInitialStyle(); }
  static void invalidateInitialStyle();

  // Computes how the style change should be propagated down the tree.
  static StyleRecalcChange stylePropagationDiff(const ComputedStyle* oldStyle,
                                                const ComputedStyle* newStyle);

  // Copies the values of any independent inherited properties from the parent
  // that are not explicitly set in this style.
  void propagateIndependentInheritedProperties(
      const ComputedStyle& parentStyle);

  ContentPosition resolvedJustifyContentPosition(
      const StyleContentAlignmentData& normalValueBehavior) const;
  ContentDistributionType resolvedJustifyContentDistribution(
      const StyleContentAlignmentData& normalValueBehavior) const;
  ContentPosition resolvedAlignContentPosition(
      const StyleContentAlignmentData& normalValueBehavior) const;
  ContentDistributionType resolvedAlignContentDistribution(
      const StyleContentAlignmentData& normalValueBehavior) const;
  StyleSelfAlignmentData resolvedAlignItems(
      ItemPosition normalValueBehaviour) const;
  StyleSelfAlignmentData resolvedAlignSelf(
      ItemPosition normalValueBehaviour,
      const ComputedStyle* parentStyle = nullptr) const;
  StyleSelfAlignmentData resolvedJustifyItems(
      ItemPosition normalValueBehaviour) const;
  StyleSelfAlignmentData resolvedJustifySelf(
      ItemPosition normalValueBehaviour,
      const ComputedStyle* parentStyle = nullptr) const;

  StyleDifference visualInvalidationDiff(const ComputedStyle&) const;

  void inheritFrom(const ComputedStyle& inheritParent,
                   IsAtShadowBoundary = NotAtShadowBoundary);
  void copyNonInheritedFromCached(const ComputedStyle&);

  PseudoId styleType() const {
    return static_cast<PseudoId>(m_nonInheritedData.m_styleType);
  }
  void setStyleType(PseudoId styleType) {
    m_nonInheritedData.m_styleType = styleType;
  }

  ComputedStyle* getCachedPseudoStyle(PseudoId) const;
  ComputedStyle* addCachedPseudoStyle(PassRefPtr<ComputedStyle>);
  void removeCachedPseudoStyle(PseudoId);

  const PseudoStyleCache* cachedPseudoStyles() const {
    return m_cachedPseudoStyles.get();
  }

  /**
     * ComputedStyle properties
     *
     * Each property stored in ComputedStyle is made up of fields. Fields have
     * initial value functions, getters and setters. A field is preferably a
     * basic data type or enum, but can be any type. A set of fields should be
     * preceded by the property the field is stored for.
     *
     * Field method naming should be done like so:
     *   // name-of-property
     *   static int initialNameOfProperty();
     *   int nameOfProperty() const;
     *   void setNameOfProperty(int);
     * If the property has multiple fields, add the field name to the end of the
     * method name.
     *
     * Avoid nested types by splitting up fields where possible, e.g.:
     *  int getBorderTopWidth();
     *  int getBorderBottomWidth();
     *  int getBorderLeftWidth();
     *  int getBorderRightWidth();
     * is preferable to:
     *  BorderWidths getBorderWidths();
     *
     * Utility functions should go in a separate section at the end of the
     * class, and be kept to a minimum.
     */

  // Non-Inherited properties.

  // Content alignment properties.
  static StyleContentAlignmentData initialContentAlignment() {
    return StyleContentAlignmentData(ContentPositionNormal,
                                     ContentDistributionDefault,
                                     OverflowAlignmentDefault);
  }

  // align-content (aka -webkit-align-content)
  const StyleContentAlignmentData& alignContent() const {
    return m_rareNonInheritedData->m_alignContent;
  }
  void setAlignContent(const StyleContentAlignmentData& data) {
    SET_VAR(m_rareNonInheritedData, m_alignContent, data);
  }

  // justify-content (aka -webkit-justify-content)
  const StyleContentAlignmentData& justifyContent() const {
    return m_rareNonInheritedData->m_justifyContent;
  }
  void setJustifyContent(const StyleContentAlignmentData& data) {
    SET_VAR(m_rareNonInheritedData, m_justifyContent, data);
  }

  // Default-Alignment properties.
  static StyleSelfAlignmentData initialDefaultAlignment() {
    return StyleSelfAlignmentData(RuntimeEnabledFeatures::cssGridLayoutEnabled()
                                      ? ItemPositionNormal
                                      : ItemPositionStretch,
                                  OverflowAlignmentDefault);
  }

  // align-items (aka -webkit-align-items)
  const StyleSelfAlignmentData& alignItems() const {
    return m_rareNonInheritedData->m_alignItems;
  }
  void setAlignItems(const StyleSelfAlignmentData& data) {
    SET_VAR(m_rareNonInheritedData, m_alignItems, data);
  }

  // justify-items
  const StyleSelfAlignmentData& justifyItems() const {
    return m_rareNonInheritedData->m_justifyItems;
  }
  void setJustifyItems(const StyleSelfAlignmentData& data) {
    SET_VAR(m_rareNonInheritedData, m_justifyItems, data);
  }

  // Self-Alignment properties.
  static StyleSelfAlignmentData initialSelfAlignment() {
    return StyleSelfAlignmentData(ItemPositionAuto, OverflowAlignmentDefault);
  }

  // align-self (aka -webkit-align-self)
  const StyleSelfAlignmentData& alignSelf() const {
    return m_rareNonInheritedData->m_alignSelf;
  }
  void setAlignSelf(const StyleSelfAlignmentData& data) {
    SET_VAR(m_rareNonInheritedData, m_alignSelf, data);
  }

  // justify-self
  const StyleSelfAlignmentData& justifySelf() const {
    return m_rareNonInheritedData->m_justifySelf;
  }
  void setJustifySelf(const StyleSelfAlignmentData& data) {
    SET_VAR(m_rareNonInheritedData, m_justifySelf, data);
  }

  // Filter properties.

  // backdrop-filter
  static const FilterOperations& initialBackdropFilter();
  const FilterOperations& backdropFilter() const {
    return m_rareNonInheritedData->m_backdropFilter->m_operations;
  }
  FilterOperations& mutableBackdropFilter() {
    return m_rareNonInheritedData.access()
        ->m_backdropFilter.access()
        ->m_operations;
  }
  bool hasBackdropFilter() const {
    return !m_rareNonInheritedData->m_backdropFilter->m_operations.operations()
                .isEmpty();
  }
  void setBackdropFilter(const FilterOperations& ops) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_backdropFilter, m_operations, ops);
  }

  // filter (aka -webkit-filter)
  static const FilterOperations& initialFilter();
  FilterOperations& mutableFilter() {
    return m_rareNonInheritedData.access()->m_filter.access()->m_operations;
  }
  const FilterOperations& filter() const {
    return m_rareNonInheritedData->m_filter->m_operations;
  }
  bool hasFilter() const {
    return !m_rareNonInheritedData->m_filter->m_operations.operations()
                .isEmpty();
  }
  void setFilter(const FilterOperations& ops) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_filter, m_operations, ops);
  }

  // backface-visibility (aka -webkit-backface-visibility)
  static EBackfaceVisibility initialBackfaceVisibility() {
    return BackfaceVisibilityVisible;
  }
  EBackfaceVisibility backfaceVisibility() const {
    return static_cast<EBackfaceVisibility>(
        m_rareNonInheritedData->m_backfaceVisibility);
  }
  void setBackfaceVisibility(EBackfaceVisibility b) {
    SET_VAR(m_rareNonInheritedData, m_backfaceVisibility, b);
  }

  // Background properties.
  // background-color
  static Color initialBackgroundColor() { return Color::transparent; }
  void setBackgroundColor(const StyleColor& v) {
    SET_VAR(m_background, m_color, v);
  }

  // background-image
  bool hasBackgroundImage() const {
    return m_background->background().hasImage();
  }
  bool hasFixedBackgroundImage() const {
    return m_background->background().hasFixedImage();
  }
  bool hasEntirelyFixedBackground() const;

  // background-clip
  EFillBox backgroundClip() const {
    return static_cast<EFillBox>(m_background->background().clip());
  }

  // Border properties.
  // -webkit-border-image
  static NinePieceImage initialNinePieceImage() { return NinePieceImage(); }
  const NinePieceImage& borderImage() const {
    return m_surround->border.image();
  }
  void setBorderImage(const NinePieceImage& b) {
    SET_VAR(m_surround, border.m_image, b);
  }

  // border-image-slice
  const LengthBox& borderImageSlices() const {
    return m_surround->border.image().imageSlices();
  }
  void setBorderImageSlices(const LengthBox&);

  // border-image-source
  static StyleImage* initialBorderImageSource() { return 0; }
  StyleImage* borderImageSource() const {
    return m_surround->border.image().image();
  }
  void setBorderImageSource(StyleImage*);

  // border-image-width
  const BorderImageLengthBox& borderImageWidth() const {
    return m_surround->border.image().borderSlices();
  }
  void setBorderImageWidth(const BorderImageLengthBox&);

  // border-image-outset
  const BorderImageLengthBox& borderImageOutset() const {
    return m_surround->border.image().outset();
  }
  void setBorderImageOutset(const BorderImageLengthBox&);

  // Border width properties.
  static unsigned initialBorderWidth() { return 3; }

  // border-top-width
  int borderTopWidth() const { return m_surround->border.borderTopWidth(); }
  void setBorderTopWidth(unsigned v) {
    SET_VAR(m_surround, border.m_top.m_width, v);
  }

  // border-bottom-width
  int borderBottomWidth() const {
    return m_surround->border.borderBottomWidth();
  }
  void setBorderBottomWidth(unsigned v) {
    SET_VAR(m_surround, border.m_bottom.m_width, v);
  }

  // border-left-width
  int borderLeftWidth() const { return m_surround->border.borderLeftWidth(); }
  void setBorderLeftWidth(unsigned v) {
    SET_VAR(m_surround, border.m_left.m_width, v);
  }

  // border-right-width
  int borderRightWidth() const { return m_surround->border.borderRightWidth(); }
  void setBorderRightWidth(unsigned v) {
    SET_VAR(m_surround, border.m_right.m_width, v);
  }

  // Border style properties.
  static EBorderStyle initialBorderStyle() { return BorderStyleNone; }

  // border-top-style
  EBorderStyle borderTopStyle() const {
    return m_surround->border.top().style();
  }
  void setBorderTopStyle(EBorderStyle v) {
    SET_VAR(m_surround, border.m_top.m_style, v);
  }

  // border-right-style
  EBorderStyle borderRightStyle() const {
    return m_surround->border.right().style();
  }
  void setBorderRightStyle(EBorderStyle v) {
    SET_VAR(m_surround, border.m_right.m_style, v);
  }

  // border-left-style
  EBorderStyle borderLeftStyle() const {
    return m_surround->border.left().style();
  }
  void setBorderLeftStyle(EBorderStyle v) {
    SET_VAR(m_surround, border.m_left.m_style, v);
  }

  // border-bottom-style
  EBorderStyle borderBottomStyle() const {
    return m_surround->border.bottom().style();
  }
  void setBorderBottomStyle(EBorderStyle v) {
    SET_VAR(m_surround, border.m_bottom.m_style, v);
  }

  // Border color properties.
  // border-left-color
  void setBorderLeftColor(const StyleColor& v) {
    SET_BORDERVALUE_COLOR(m_surround, border.m_left, v);
  }

  // border-right-color
  void setBorderRightColor(const StyleColor& v) {
    SET_BORDERVALUE_COLOR(m_surround, border.m_right, v);
  }

  // border-top-color
  void setBorderTopColor(const StyleColor& v) {
    SET_BORDERVALUE_COLOR(m_surround, border.m_top, v);
  }

  // border-bottom-color
  void setBorderBottomColor(const StyleColor& v) {
    SET_BORDERVALUE_COLOR(m_surround, border.m_bottom, v);
  }

  // Border radius properties.
  static LengthSize initialBorderRadius() {
    return LengthSize(Length(0, Fixed), Length(0, Fixed));
  }

  // border-top-left-radius (aka -webkit-border-top-left-radius)
  const LengthSize& borderTopLeftRadius() const {
    return m_surround->border.topLeft();
  }
  void setBorderTopLeftRadius(const LengthSize& s) {
    SET_VAR(m_surround, border.m_topLeft, s);
  }

  // border-top-right-radius (aka -webkit-border-top-right-radius)
  const LengthSize& borderTopRightRadius() const {
    return m_surround->border.topRight();
  }
  void setBorderTopRightRadius(const LengthSize& s) {
    SET_VAR(m_surround, border.m_topRight, s);
  }

  // border-bottom-left-radius (aka -webkit-border-bottom-left-radius)
  const LengthSize& borderBottomLeftRadius() const {
    return m_surround->border.bottomLeft();
  }
  void setBorderBottomLeftRadius(const LengthSize& s) {
    SET_VAR(m_surround, border.m_bottomLeft, s);
  }

  // border-bottom-right-radius (aka -webkit-border-bottom-right-radius)
  const LengthSize& borderBottomRightRadius() const {
    return m_surround->border.bottomRight();
  }
  void setBorderBottomRightRadius(const LengthSize& s) {
    SET_VAR(m_surround, border.m_bottomRight, s);
  }

  // Offset properties.
  static Length initialOffset() { return Length(); }

  // left
  const Length& left() const { return m_surround->offset.left(); }
  void setLeft(const Length& v) { SET_VAR(m_surround, offset.m_left, v); }

  // right
  const Length& right() const { return m_surround->offset.right(); }
  void setRight(const Length& v) { SET_VAR(m_surround, offset.m_right, v); }

  // top
  const Length& top() const { return m_surround->offset.top(); }
  void setTop(const Length& v) { SET_VAR(m_surround, offset.m_top, v); }

  // bottom
  const Length& bottom() const { return m_surround->offset.bottom(); }
  void setBottom(const Length& v) { SET_VAR(m_surround, offset.m_bottom, v); }

  // box-shadow (aka -webkit-box-shadow)
  static ShadowList* initialBoxShadow() { return 0; }
  ShadowList* boxShadow() const {
    return m_rareNonInheritedData->m_boxShadow.get();
  }
  void setBoxShadow(PassRefPtr<ShadowList>);

  // box-sizing (aka -webkit-box-sizing)
  static EBoxSizing initialBoxSizing() { return BoxSizingContentBox; }
  EBoxSizing boxSizing() const { return m_box->boxSizing(); }
  void setBoxSizing(EBoxSizing s) { SET_VAR(m_box, m_boxSizing, s); }

  // clear
  static EClear initialClear() { return ClearNone; }
  EClear clear() const {
    return static_cast<EClear>(m_nonInheritedData.m_clear);
  }
  void setClear(EClear v) { m_nonInheritedData.m_clear = v; }

  // Page break properties.
  // break-after (shorthand for page-break-after and -webkit-column-break-after)
  static EBreak initialBreakAfter() { return BreakAuto; }
  EBreak breakAfter() const {
    return static_cast<EBreak>(m_nonInheritedData.m_breakAfter);
  }
  void setBreakAfter(EBreak b) {
    DCHECK_LE(b, BreakValueLastAllowedForBreakAfterAndBefore);
    m_nonInheritedData.m_breakAfter = b;
  }

  // break-before (shorthand for page-break-before and
  // -webkit-column-break-before)
  static EBreak initialBreakBefore() { return BreakAuto; }
  EBreak breakBefore() const {
    return static_cast<EBreak>(m_nonInheritedData.m_breakBefore);
  }
  void setBreakBefore(EBreak b) {
    DCHECK_LE(b, BreakValueLastAllowedForBreakAfterAndBefore);
    m_nonInheritedData.m_breakBefore = b;
  }

  // break-inside (shorthand for page-break-inside and
  // -webkit-column-break-inside)
  static EBreak initialBreakInside() { return BreakAuto; }
  EBreak breakInside() const {
    return static_cast<EBreak>(m_nonInheritedData.m_breakInside);
  }
  void setBreakInside(EBreak b) {
    DCHECK_LE(b, BreakValueLastAllowedForBreakInside);
    m_nonInheritedData.m_breakInside = b;
  }

  // clip
  static LengthBox initialClip() { return LengthBox(); }
  const LengthBox& clip() const { return m_visual->clip; }
  void setClip(const LengthBox& box) {
    SET_VAR(m_visual, hasAutoClip, false);
    SET_VAR(m_visual, clip, box);
  }
  bool hasAutoClip() const { return m_visual->hasAutoClip; }
  void setHasAutoClip() {
    SET_VAR(m_visual, hasAutoClip, true);
    SET_VAR(m_visual, clip, ComputedStyle::initialClip());
  }

  // Column properties.
  // column-count (aka -webkit-column-count)
  static unsigned short initialColumnCount() { return 1; }
  unsigned short columnCount() const {
    return m_rareNonInheritedData->m_multiCol->m_count;
  }
  void setColumnCount(unsigned short c) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_autoCount, false);
    SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_count, c);
  }
  bool hasAutoColumnCount() const {
    return m_rareNonInheritedData->m_multiCol->m_autoCount;
  }
  void setHasAutoColumnCount() {
    SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_autoCount, true);
    SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_count,
                   initialColumnCount());
  }

  // column-fill
  static ColumnFill initialColumnFill() { return ColumnFillBalance; }
  ColumnFill getColumnFill() const {
    return static_cast<ColumnFill>(m_rareNonInheritedData->m_multiCol->m_fill);
  }
  void setColumnFill(ColumnFill columnFill) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_fill, columnFill);
  }

  // column-gap (aka -webkit-column-gap)
  float columnGap() const { return m_rareNonInheritedData->m_multiCol->m_gap; }
  void setColumnGap(float f) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_normalGap, false);
    SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_gap, f);
  }
  bool hasNormalColumnGap() const {
    return m_rareNonInheritedData->m_multiCol->m_normalGap;
  }
  void setHasNormalColumnGap() {
    SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_normalGap, true);
    SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_gap, 0);
  }

  // column-rule-color (aka -webkit-column-rule-color)
  void setColumnRuleColor(const StyleColor& c) {
    SET_BORDERVALUE_COLOR(m_rareNonInheritedData.access()->m_multiCol, m_rule,
                          c);
  }

  // column-rule-style (aka -webkit-column-rule-style)
  EBorderStyle columnRuleStyle() const {
    return m_rareNonInheritedData->m_multiCol->m_rule.style();
  }
  void setColumnRuleStyle(EBorderStyle b) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_rule.m_style, b);
  }

  // column-rule-width (aka -webkit-column-rule-width)
  static unsigned short initialColumnRuleWidth() { return 3; }
  unsigned short columnRuleWidth() const {
    return m_rareNonInheritedData->m_multiCol->ruleWidth();
  }
  void setColumnRuleWidth(unsigned short w) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_rule.m_width, w);
  }

  // column-span (aka -webkit-column-span)
  static ColumnSpan initialColumnSpan() { return ColumnSpanNone; }
  ColumnSpan getColumnSpan() const {
    return static_cast<ColumnSpan>(
        m_rareNonInheritedData->m_multiCol->m_columnSpan);
  }
  void setColumnSpan(ColumnSpan columnSpan) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_columnSpan,
                   columnSpan);
  }

  // column-width (aka -webkit-column-width)
  float columnWidth() const {
    return m_rareNonInheritedData->m_multiCol->m_width;
  }
  void setColumnWidth(float f) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_autoWidth, false);
    SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_width, f);
  }
  bool hasAutoColumnWidth() const {
    return m_rareNonInheritedData->m_multiCol->m_autoWidth;
  }
  void setHasAutoColumnWidth() {
    SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_autoWidth, true);
    SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_width, 0);
  }

  // contain
  static Containment initialContain() { return ContainsNone; }
  Containment contain() const {
    return static_cast<Containment>(m_rareNonInheritedData->m_contain);
  }
  void setContain(Containment contain) {
    SET_VAR(m_rareNonInheritedData, m_contain, contain);
  }

  // content
  ContentData* contentData() const {
    return m_rareNonInheritedData->m_content.get();
  }
  void setContent(ContentData*);

  // display
  static EDisplay initialDisplay() { return EDisplay::Inline; }
  EDisplay display() const {
    return static_cast<EDisplay>(m_nonInheritedData.m_effectiveDisplay);
  }
  EDisplay originalDisplay() const {
    return static_cast<EDisplay>(m_nonInheritedData.m_originalDisplay);
  }
  void setDisplay(EDisplay v) {
    m_nonInheritedData.m_effectiveDisplay = static_cast<unsigned>(v);
  }
  void setOriginalDisplay(EDisplay v) {
    m_nonInheritedData.m_originalDisplay = static_cast<unsigned>(v);
  }

  // Flex properties.
  // flex-basis (aka -webkit-flex-basis)
  static Length initialFlexBasis() { return Length(Auto); }
  const Length& flexBasis() const {
    return m_rareNonInheritedData->m_flexibleBox->m_flexBasis;
  }
  void setFlexBasis(const Length& length) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_flexibleBox, m_flexBasis, length);
  }

  // flex-direction (aka -webkit-flex-direction)
  static EFlexDirection initialFlexDirection() { return FlowRow; }
  EFlexDirection flexDirection() const {
    return static_cast<EFlexDirection>(
        m_rareNonInheritedData->m_flexibleBox->m_flexDirection);
  }
  void setFlexDirection(EFlexDirection direction) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_flexibleBox, m_flexDirection,
                   direction);
  }

  // flex-grow (aka -webkit-flex-grow)
  static float initialFlexGrow() { return 0; }
  float flexGrow() const {
    return m_rareNonInheritedData->m_flexibleBox->m_flexGrow;
  }
  void setFlexGrow(float f) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_flexibleBox, m_flexGrow, f);
  }

  // flex-shrink (aka -webkit-flex-shrink)
  static float initialFlexShrink() { return 1; }
  float flexShrink() const {
    return m_rareNonInheritedData->m_flexibleBox->m_flexShrink;
  }
  void setFlexShrink(float f) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_flexibleBox, m_flexShrink, f);
  }

  // flex-wrap (aka -webkit-flex-wrap)
  static EFlexWrap initialFlexWrap() { return FlexNoWrap; }
  EFlexWrap flexWrap() const {
    return static_cast<EFlexWrap>(
        m_rareNonInheritedData->m_flexibleBox->m_flexWrap);
  }
  void setFlexWrap(EFlexWrap w) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_flexibleBox, m_flexWrap, w);
  }

  // -webkit-box-flex
  static float initialBoxFlex() { return 0.0f; }
  float boxFlex() const {
    return m_rareNonInheritedData->m_deprecatedFlexibleBox->flex;
  }
  void setBoxFlex(float f) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_deprecatedFlexibleBox, flex, f);
  }

  // -webkit-box-flex-group
  static unsigned initialBoxFlexGroup() { return 1; }
  unsigned boxFlexGroup() const {
    return m_rareNonInheritedData->m_deprecatedFlexibleBox->flexGroup;
  }
  void setBoxFlexGroup(unsigned fg) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_deprecatedFlexibleBox, flexGroup,
                   fg);
  }

  // -webkit-box-align
  // For valid values of box-align see
  // http://www.w3.org/TR/2009/WD-css3-flexbox-20090723/#alignment
  static EBoxAlignment initialBoxAlign() { return BSTRETCH; }
  EBoxAlignment boxAlign() const {
    return static_cast<EBoxAlignment>(
        m_rareNonInheritedData->m_deprecatedFlexibleBox->align);
  }
  void setBoxAlign(EBoxAlignment a) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_deprecatedFlexibleBox, align, a);
  }

  // -webkit-box-decoration-break
  static EBoxDecorationBreak initialBoxDecorationBreak() {
    return BoxDecorationBreakSlice;
  }
  EBoxDecorationBreak boxDecorationBreak() const {
    return m_box->boxDecorationBreak();
  }
  void setBoxDecorationBreak(EBoxDecorationBreak b) {
    SET_VAR(m_box, m_boxDecorationBreak, b);
  }

  // -webkit-box-lines
  static EBoxLines initialBoxLines() { return SINGLE; }
  EBoxLines boxLines() const {
    return static_cast<EBoxLines>(
        m_rareNonInheritedData->m_deprecatedFlexibleBox->lines);
  }
  void setBoxLines(EBoxLines lines) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_deprecatedFlexibleBox, lines,
                   lines);
  }

  // -webkit-box-ordinal-group
  static unsigned initialBoxOrdinalGroup() { return 1; }
  unsigned boxOrdinalGroup() const {
    return m_rareNonInheritedData->m_deprecatedFlexibleBox->ordinalGroup;
  }
  void setBoxOrdinalGroup(unsigned og) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_deprecatedFlexibleBox,
                   ordinalGroup, og);
  }

  // -webkit-box-orient
  static EBoxOrient initialBoxOrient() { return HORIZONTAL; }
  EBoxOrient boxOrient() const {
    return static_cast<EBoxOrient>(
        m_rareNonInheritedData->m_deprecatedFlexibleBox->orient);
  }
  void setBoxOrient(EBoxOrient o) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_deprecatedFlexibleBox, orient, o);
  }

  // -webkit-box-pack
  static EBoxPack initialBoxPack() { return BoxPackStart; }
  EBoxPack boxPack() const {
    return static_cast<EBoxPack>(
        m_rareNonInheritedData->m_deprecatedFlexibleBox->pack);
  }
  void setBoxPack(EBoxPack p) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_deprecatedFlexibleBox, pack, p);
  }

  // -webkit-box-reflect
  static StyleReflection* initialBoxReflect() { return 0; }
  StyleReflection* boxReflect() const {
    return m_rareNonInheritedData->m_boxReflect.get();
  }
  void setBoxReflect(PassRefPtr<StyleReflection> reflect) {
    if (m_rareNonInheritedData->m_boxReflect != reflect)
      m_rareNonInheritedData.access()->m_boxReflect = reflect;
  }

  // Grid properties.
  static Vector<GridTrackSize> initialGridAutoRepeatTracks() {
    return Vector<GridTrackSize>(); /* none */
  }
  static size_t initialGridAutoRepeatInsertionPoint() { return 0; }
  static AutoRepeatType initialGridAutoRepeatType() { return NoAutoRepeat; }
  static NamedGridLinesMap initialNamedGridColumnLines() {
    return NamedGridLinesMap();
  }
  static NamedGridLinesMap initialNamedGridRowLines() {
    return NamedGridLinesMap();
  }
  static OrderedNamedGridLines initialOrderedNamedGridColumnLines() {
    return OrderedNamedGridLines();
  }
  static OrderedNamedGridLines initialOrderedNamedGridRowLines() {
    return OrderedNamedGridLines();
  }
  static NamedGridAreaMap initialNamedGridArea() { return NamedGridAreaMap(); }
  static size_t initialNamedGridAreaCount() { return 0; }

  // grid-auto-columns
  static Vector<GridTrackSize> initialGridAutoColumns();
  const Vector<GridTrackSize>& gridAutoColumns() const {
    return m_rareNonInheritedData->m_grid->m_gridAutoColumns;
  }
  void setGridAutoColumns(const Vector<GridTrackSize>& trackSizeList) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridAutoColumns,
                   trackSizeList);
  }

  // grid-auto-flow
  static GridAutoFlow initialGridAutoFlow() { return AutoFlowRow; }
  void setGridAutoFlow(GridAutoFlow flow) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridAutoFlow, flow);
  }

  // grid-auto-rows
  static Vector<GridTrackSize> initialGridAutoRows();
  const Vector<GridTrackSize>& gridAutoRows() const {
    return m_rareNonInheritedData->m_grid->m_gridAutoRows;
  }
  void setGridAutoRows(const Vector<GridTrackSize>& trackSizeList) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridAutoRows,
                   trackSizeList);
  }

  // grid-column-gap
  static Length initialGridColumnGap() { return Length(Fixed); }
  const Length& gridColumnGap() const {
    return m_rareNonInheritedData->m_grid->m_gridColumnGap;
  }
  void setGridColumnGap(const Length& v) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridColumnGap, v);
  }

  // grid-column-start
  static GridPosition initialGridColumnStart() {
    return GridPosition(); /* auto */
  }
  const GridPosition& gridColumnStart() const {
    return m_rareNonInheritedData->m_gridItem->m_gridColumnStart;
  }
  void setGridColumnStart(const GridPosition& columnStartPosition) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_gridItem, m_gridColumnStart,
                   columnStartPosition);
  }

  // grid-column-end
  static GridPosition initialGridColumnEnd() {
    return GridPosition(); /* auto */
  }
  const GridPosition& gridColumnEnd() const {
    return m_rareNonInheritedData->m_gridItem->m_gridColumnEnd;
  }
  void setGridColumnEnd(const GridPosition& columnEndPosition) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_gridItem, m_gridColumnEnd,
                   columnEndPosition);
  }

  // grid-row-gap
  static Length initialGridRowGap() { return Length(Fixed); }
  const Length& gridRowGap() const {
    return m_rareNonInheritedData->m_grid->m_gridRowGap;
  }
  void setGridRowGap(const Length& v) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridRowGap, v);
  }

  // grid-row-start
  static GridPosition initialGridRowStart() {
    return GridPosition(); /* auto */
  }
  const GridPosition& gridRowStart() const {
    return m_rareNonInheritedData->m_gridItem->m_gridRowStart;
  }
  void setGridRowStart(const GridPosition& rowStartPosition) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_gridItem, m_gridRowStart,
                   rowStartPosition);
  }

  // grid-row-end
  static GridPosition initialGridRowEnd() { return GridPosition(); /* auto */ }
  const GridPosition& gridRowEnd() const {
    return m_rareNonInheritedData->m_gridItem->m_gridRowEnd;
  }
  void setGridRowEnd(const GridPosition& rowEndPosition) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_gridItem, m_gridRowEnd,
                   rowEndPosition);
  }

  // grid-template-columns
  static Vector<GridTrackSize> initialGridTemplateColumns() {
    return Vector<GridTrackSize>(); /* none */
  }
  const Vector<GridTrackSize>& gridTemplateColumns() const {
    return m_rareNonInheritedData->m_grid->m_gridTemplateColumns;
  }
  void setGridTemplateColumns(const Vector<GridTrackSize>& lengths) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridTemplateColumns,
                   lengths);
  }

  // grid-template-rows
  static Vector<GridTrackSize> initialGridTemplateRows() {
    return Vector<GridTrackSize>(); /* none */
  }
  const Vector<GridTrackSize>& gridTemplateRows() const {
    return m_rareNonInheritedData->m_grid->m_gridTemplateRows;
  }
  void setGridTemplateRows(const Vector<GridTrackSize>& lengths) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridTemplateRows, lengths);
  }

  // Width/height properties.
  static Length initialSize() { return Length(); }
  static Length initialMaxSize() { return Length(MaxSizeNone); }
  static Length initialMinSize() { return Length(); }

  // width
  const Length& width() const { return m_box->width(); }
  void setWidth(const Length& v) { SET_VAR(m_box, m_width, v); }

  // height
  const Length& height() const { return m_box->height(); }
  void setHeight(const Length& v) { SET_VAR(m_box, m_height, v); }

  // max-width
  const Length& maxWidth() const { return m_box->maxWidth(); }
  void setMaxWidth(const Length& v) { SET_VAR(m_box, m_maxWidth, v); }

  // max-height
  const Length& maxHeight() const { return m_box->maxHeight(); }
  void setMaxHeight(const Length& v) { SET_VAR(m_box, m_maxHeight, v); }

  // min-width
  const Length& minWidth() const { return m_box->minWidth(); }
  void setMinWidth(const Length& v) { SET_VAR(m_box, m_minWidth, v); }

  // min-height
  const Length& minHeight() const { return m_box->minHeight(); }
  void setMinHeight(const Length& v) { SET_VAR(m_box, m_minHeight, v); }

  // image-orientation
  static RespectImageOrientationEnum initialRespectImageOrientation() {
    return DoNotRespectImageOrientation;
  }
  RespectImageOrientationEnum respectImageOrientation() const {
    return static_cast<RespectImageOrientationEnum>(
        m_rareInheritedData->m_respectImageOrientation);
  }
  void setRespectImageOrientation(RespectImageOrientationEnum v) {
    SET_VAR(m_rareInheritedData, m_respectImageOrientation, v);
  }

  // image-rendering
  static EImageRendering initialImageRendering() { return ImageRenderingAuto; }
  EImageRendering imageRendering() const {
    return static_cast<EImageRendering>(m_rareInheritedData->m_imageRendering);
  }
  void setImageRendering(EImageRendering v) {
    SET_VAR(m_rareInheritedData, m_imageRendering, v);
  }

  // isolation
  static EIsolation initialIsolation() { return IsolationAuto; }
  EIsolation isolation() const {
    return static_cast<EIsolation>(m_rareNonInheritedData->m_isolation);
  }
  void setIsolation(EIsolation v) {
    m_rareNonInheritedData.access()->m_isolation = v;
  }

  // Margin properties.
  static Length initialMargin() { return Length(Fixed); }

  // margin-top
  const Length& marginTop() const { return m_surround->margin.top(); }
  void setMarginTop(const Length& v) { SET_VAR(m_surround, margin.m_top, v); }

  // margin-bottom
  const Length& marginBottom() const { return m_surround->margin.bottom(); }
  void setMarginBottom(const Length& v) {
    SET_VAR(m_surround, margin.m_bottom, v);
  }

  // margin-left
  const Length& marginLeft() const { return m_surround->margin.left(); }
  void setMarginLeft(const Length& v) { SET_VAR(m_surround, margin.m_left, v); }

  // margin-right
  const Length& marginRight() const { return m_surround->margin.right(); }
  void setMarginRight(const Length& v) {
    SET_VAR(m_surround, margin.m_right, v);
  }

  // -webkit-margin-before-collapse (aka -webkit-margin-top-collapse)
  static EMarginCollapse initialMarginBeforeCollapse() {
    return MarginCollapseCollapse;
  }
  EMarginCollapse marginAfterCollapse() const {
    return static_cast<EMarginCollapse>(
        m_rareNonInheritedData->marginAfterCollapse);
  }
  void setMarginBeforeCollapse(EMarginCollapse c) {
    SET_VAR(m_rareNonInheritedData, marginBeforeCollapse, c);
  }

  // -webkit-margin-after-collapse (aka -webkit-margin-bottom-collapse)
  static EMarginCollapse initialMarginAfterCollapse() {
    return MarginCollapseCollapse;
  }
  EMarginCollapse marginBeforeCollapse() const {
    return static_cast<EMarginCollapse>(
        m_rareNonInheritedData->marginBeforeCollapse);
  }
  void setMarginAfterCollapse(EMarginCollapse c) {
    SET_VAR(m_rareNonInheritedData, marginAfterCollapse, c);
  }

  // mix-blend-mode
  static WebBlendMode initialBlendMode() { return WebBlendModeNormal; }
  WebBlendMode blendMode() const {
    return static_cast<WebBlendMode>(
        m_rareNonInheritedData->m_effectiveBlendMode);
  }
  void setBlendMode(WebBlendMode v) {
    m_rareNonInheritedData.access()->m_effectiveBlendMode = v;
  }

  // object-fit
  static ObjectFit initialObjectFit() { return ObjectFitFill; }
  ObjectFit getObjectFit() const {
    return static_cast<ObjectFit>(m_rareNonInheritedData->m_objectFit);
  }
  void setObjectFit(ObjectFit f) {
    SET_VAR(m_rareNonInheritedData, m_objectFit, f);
  }

  // object-position
  static LengthPoint initialObjectPosition() {
    return LengthPoint(Length(50.0, Percent), Length(50.0, Percent));
  }
  LengthPoint objectPosition() const {
    return m_rareNonInheritedData->m_objectPosition;
  }
  void setObjectPosition(LengthPoint position) {
    SET_VAR(m_rareNonInheritedData, m_objectPosition, position);
  }

  // offset-anchor
  static LengthPoint initialOffsetAnchor() {
    return LengthPoint(Length(50.0, Percent), Length(50.0, Percent));
  }
  const LengthPoint& offsetAnchor() const {
    return m_rareNonInheritedData->m_transform->m_motion.m_anchor;
  }
  void setOffsetAnchor(const LengthPoint& offsetAnchor) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_transform, m_motion.m_anchor,
                   offsetAnchor);
  }

  // offset-distance
  static Length initialOffsetDistance() { return Length(0, Fixed); }
  const Length& offsetDistance() const {
    return m_rareNonInheritedData->m_transform->m_motion.m_distance;
  }
  void setOffsetDistance(const Length& offsetDistance) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_transform, m_motion.m_distance,
                   offsetDistance);
  }

  // offset-path
  static StylePath* initialOffsetPath() { return nullptr; }
  StylePath* offsetPath() const {
    return m_rareNonInheritedData->m_transform->m_motion.m_path.get();
  }
  void setOffsetPath(PassRefPtr<StylePath>);

  // offset-position
  static LengthPoint initialOffsetPosition() {
    return LengthPoint(Length(Auto), Length(Auto));
  }
  const LengthPoint& offsetPosition() const {
    return m_rareNonInheritedData->m_transform->m_motion.m_position;
  }
  void setOffsetPosition(const LengthPoint& offsetPosition) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_transform, m_motion.m_position,
                   offsetPosition);
  }

  // offset-rotate
  static StyleOffsetRotation initialOffsetRotate() {
    return initialOffsetRotation();
  }
  const StyleOffsetRotation& offsetRotate() const { return offsetRotation(); }
  void setOffsetRotate(const StyleOffsetRotation& offsetRotate) {
    setOffsetRotation(offsetRotate);
  }

  // offset-rotation
  static StyleOffsetRotation initialOffsetRotation() {
    return StyleOffsetRotation(0, OffsetRotationAuto);
  }
  const StyleOffsetRotation& offsetRotation() const {
    return m_rareNonInheritedData->m_transform->m_motion.m_rotation;
  }
  void setOffsetRotation(const StyleOffsetRotation& offsetRotation) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_transform, m_motion.m_rotation,
                   offsetRotation);
  }

  // opacity (aka -webkit-opacity)
  static float initialOpacity() { return 1.0f; }
  float opacity() const { return m_rareNonInheritedData->opacity; }
  void setOpacity(float f) {
    float v = clampTo<float>(f, 0, 1);
    SET_VAR(m_rareNonInheritedData, opacity, v);
  }

  // order (aka -webkit-order)
  static int initialOrder() { return 0; }
  int order() const { return m_rareNonInheritedData->m_order; }
  // We restrict the smallest value to int min + 2 because we use int min and
  // int min + 1 as special values in a hash set.
  void setOrder(int o) {
    SET_VAR(m_rareNonInheritedData, m_order,
            max(std::numeric_limits<int>::min() + 2, o));
  }

  // Outline properties.
  // outline-color
  void setOutlineColor(const StyleColor& v) {
    SET_BORDERVALUE_COLOR(m_rareNonInheritedData, m_outline, v);
  }

  // outline-style
  EBorderStyle outlineStyle() const {
    return m_rareNonInheritedData->m_outline.style();
  }
  void setOutlineStyle(EBorderStyle v) {
    SET_VAR(m_rareNonInheritedData, m_outline.m_style, v);
  }
  static OutlineIsAuto initialOutlineStyleIsAuto() { return OutlineIsAutoOff; }
  OutlineIsAuto outlineStyleIsAuto() const {
    return static_cast<OutlineIsAuto>(
        m_rareNonInheritedData->m_outline.isAuto());
  }
  void setOutlineStyleIsAuto(OutlineIsAuto isAuto) {
    SET_VAR(m_rareNonInheritedData, m_outline.m_isAuto, isAuto);
  }

  // outline-width
  static unsigned short initialOutlineWidth() { return 3; }
  int outlineWidth() const {
    if (m_rareNonInheritedData->m_outline.style() == BorderStyleNone)
      return 0;
    return m_rareNonInheritedData->m_outline.width();
  }
  void setOutlineWidth(unsigned short v) {
    SET_VAR(m_rareNonInheritedData, m_outline.m_width, v);
  }

  // outline-offset
  static int initialOutlineOffset() { return 0; }
  int outlineOffset() const {
    if (m_rareNonInheritedData->m_outline.style() == BorderStyleNone)
      return 0;
    return m_rareNonInheritedData->m_outline.offset();
  }
  void setOutlineOffset(int v) {
    SET_VAR(m_rareNonInheritedData, m_outline.m_offset, v);
  }

  // Overflow properties.
  // overflow-anchor
  static EOverflowAnchor initialOverflowAnchor() { return AnchorAuto; }
  EOverflowAnchor overflowAnchor() const {
    return static_cast<EOverflowAnchor>(m_nonInheritedData.m_overflowAnchor);
  }
  void setOverflowAnchor(EOverflowAnchor v) {
    m_nonInheritedData.m_overflowAnchor = v;
  }

  // overflow-x
  static EOverflow initialOverflowX() { return OverflowVisible; }
  EOverflow overflowX() const {
    return static_cast<EOverflow>(m_nonInheritedData.m_overflowX);
  }
  void setOverflowX(EOverflow v) { m_nonInheritedData.m_overflowX = v; }

  // overflow-y
  static EOverflow initialOverflowY() { return OverflowVisible; }
  EOverflow overflowY() const {
    return static_cast<EOverflow>(m_nonInheritedData.m_overflowY);
  }
  void setOverflowY(EOverflow v) { m_nonInheritedData.m_overflowY = v; }

  // Padding properties.
  static Length initialPadding() { return Length(Fixed); }

  // padding-bottom
  const Length& paddingBottom() const { return m_surround->padding.bottom(); }
  void setPaddingBottom(const Length& v) {
    SET_VAR(m_surround, padding.m_bottom, v);
  }

  // padding-left
  const Length& paddingLeft() const { return m_surround->padding.left(); }
  void setPaddingLeft(const Length& v) {
    SET_VAR(m_surround, padding.m_left, v);
  }

  // padding-right
  const Length& paddingRight() const { return m_surround->padding.right(); }
  void setPaddingRight(const Length& v) {
    SET_VAR(m_surround, padding.m_right, v);
  }

  // padding-top
  const Length& paddingTop() const { return m_surround->padding.top(); }
  void setPaddingTop(const Length& v) { SET_VAR(m_surround, padding.m_top, v); }

  // perspective (aka -webkit-perspective)
  static float initialPerspective() { return 0; }
  float perspective() const { return m_rareNonInheritedData->m_perspective; }
  void setPerspective(float p) {
    SET_VAR(m_rareNonInheritedData, m_perspective, p);
  }

  // perspective-origin (aka -webkit-perspective-origin)
  static LengthPoint initialPerspectiveOrigin() {
    return LengthPoint(Length(50.0, Percent), Length(50.0, Percent));
  }
  const LengthPoint& perspectiveOrigin() const {
    return m_rareNonInheritedData->m_perspectiveOrigin;
  }
  void setPerspectiveOrigin(const LengthPoint& p) {
    SET_VAR(m_rareNonInheritedData, m_perspectiveOrigin, p);
  }

  // -webkit-perspective-origin-x
  static Length initialPerspectiveOriginX() { return Length(50.0, Percent); }
  const Length& perspectiveOriginX() const { return perspectiveOrigin().x(); }
  void setPerspectiveOriginX(const Length& v) {
    setPerspectiveOrigin(LengthPoint(v, perspectiveOriginY()));
  }

  // -webkit-perspective-origin-y
  static Length initialPerspectiveOriginY() { return Length(50.0, Percent); }
  const Length& perspectiveOriginY() const { return perspectiveOrigin().y(); }
  void setPerspectiveOriginY(const Length& v) {
    setPerspectiveOrigin(LengthPoint(perspectiveOriginX(), v));
  }

  // position
  static EPosition initialPosition() { return StaticPosition; }
  EPosition position() const {
    return static_cast<EPosition>(m_nonInheritedData.m_position);
  }
  void setPosition(EPosition v) { m_nonInheritedData.m_position = v; }

  // resize
  static EResize initialResize() { return RESIZE_NONE; }
  EResize resize() const {
    return static_cast<EResize>(m_rareNonInheritedData->m_resize);
  }
  void setResize(EResize r) { SET_VAR(m_rareNonInheritedData, m_resize, r); }

  // Transform properties.
  // transform (aka -webkit-transform)
  static EmptyTransformOperations initialTransform() {
    return EmptyTransformOperations();
  }
  const TransformOperations& transform() const {
    return m_rareNonInheritedData->m_transform->m_operations;
  }
  void setTransform(const TransformOperations& ops) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_transform, m_operations, ops);
  }

  // transform-origin (aka -webkit-transform-origin)
  static TransformOrigin initialTransformOrigin() {
    return TransformOrigin(Length(50.0, Percent), Length(50.0, Percent), 0);
  }
  const TransformOrigin& transformOrigin() const {
    return m_rareNonInheritedData->m_transform->m_origin;
  }
  void setTransformOrigin(const TransformOrigin& o) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_transform, m_origin, o);
  }

  // transform-style (aka -webkit-transform-style)
  static ETransformStyle3D initialTransformStyle3D() {
    return TransformStyle3DFlat;
  }
  ETransformStyle3D transformStyle3D() const {
    return static_cast<ETransformStyle3D>(
        m_rareNonInheritedData->m_transformStyle3D);
  }
  void setTransformStyle3D(ETransformStyle3D b) {
    SET_VAR(m_rareNonInheritedData, m_transformStyle3D, b);
  }

  // -webkit-transform-origin-x
  static Length initialTransformOriginX() { return Length(50.0, Percent); }
  const Length& transformOriginX() const { return transformOrigin().x(); }
  void setTransformOriginX(const Length& v) {
    setTransformOrigin(
        TransformOrigin(v, transformOriginY(), transformOriginZ()));
  }

  // -webkit-transform-origin-y
  static Length initialTransformOriginY() { return Length(50.0, Percent); }
  const Length& transformOriginY() const { return transformOrigin().y(); }
  void setTransformOriginY(const Length& v) {
    setTransformOrigin(
        TransformOrigin(transformOriginX(), v, transformOriginZ()));
  }

  // -webkit-transform-origin-z
  static float initialTransformOriginZ() { return 0; }
  float transformOriginZ() const { return transformOrigin().z(); }
  void setTransformOriginZ(float f) {
    setTransformOrigin(
        TransformOrigin(transformOriginX(), transformOriginY(), f));
  }

  // Independent transform properties.
  // translate
  static PassRefPtr<TranslateTransformOperation> initialTranslate() {
    return nullptr;
  }
  TranslateTransformOperation* translate() const {
    return m_rareNonInheritedData->m_transform->m_translate.get();
  }
  void setTranslate(PassRefPtr<TranslateTransformOperation> v) {
    m_rareNonInheritedData.access()->m_transform.access()->m_translate = v;
  }

  // rotate
  static PassRefPtr<RotateTransformOperation> initialRotate() {
    return nullptr;
  }
  RotateTransformOperation* rotate() const {
    return m_rareNonInheritedData->m_transform->m_rotate.get();
  }
  void setRotate(PassRefPtr<RotateTransformOperation> v) {
    m_rareNonInheritedData.access()->m_transform.access()->m_rotate = v;
  }

  // scale
  static PassRefPtr<ScaleTransformOperation> initialScale() { return nullptr; }
  ScaleTransformOperation* scale() const {
    return m_rareNonInheritedData->m_transform->m_scale.get();
  }
  void setScale(PassRefPtr<ScaleTransformOperation> v) {
    m_rareNonInheritedData.access()->m_transform.access()->m_scale = v;
  }

  // Scroll properties.
  // scroll-behavior
  static ScrollBehavior initialScrollBehavior() { return ScrollBehaviorAuto; }
  ScrollBehavior getScrollBehavior() const {
    return static_cast<ScrollBehavior>(
        m_rareNonInheritedData->m_scrollBehavior);
  }
  void setScrollBehavior(ScrollBehavior b) {
    SET_VAR(m_rareNonInheritedData, m_scrollBehavior, b);
  }

  // scroll-snap-coordinate
  static Vector<LengthPoint> initialScrollSnapCoordinate() {
    return Vector<LengthPoint>();
  }
  const Vector<LengthPoint>& scrollSnapCoordinate() const {
    return m_rareNonInheritedData->m_scrollSnap->m_coordinates;
  }
  void setScrollSnapCoordinate(const Vector<LengthPoint>& b) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_scrollSnap, m_coordinates, b);
  }

  // scroll-snap-destination
  static LengthPoint initialScrollSnapDestination() {
    return LengthPoint(Length(0, Fixed), Length(0, Fixed));
  }
  const LengthPoint& scrollSnapDestination() const {
    return m_rareNonInheritedData->m_scrollSnap->m_destination;
  }
  void setScrollSnapDestination(const LengthPoint& b) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_scrollSnap, m_destination, b);
  }

  // scroll-snap-points-x
  static ScrollSnapPoints initialScrollSnapPointsX() {
    return ScrollSnapPoints();
  }
  const ScrollSnapPoints& scrollSnapPointsX() const {
    return m_rareNonInheritedData->m_scrollSnap->m_xPoints;
  }
  void setScrollSnapPointsX(const ScrollSnapPoints& b) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_scrollSnap, m_xPoints, b);
  }

  // scroll-snap-points-y
  static ScrollSnapPoints initialScrollSnapPointsY() {
    return ScrollSnapPoints();
  }
  const ScrollSnapPoints& scrollSnapPointsY() const {
    return m_rareNonInheritedData->m_scrollSnap->m_yPoints;
  }
  void setScrollSnapPointsY(const ScrollSnapPoints& b) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_scrollSnap, m_yPoints, b);
  }

  // scroll-snap-type
  static ScrollSnapType initialScrollSnapType() { return ScrollSnapTypeNone; }
  ScrollSnapType getScrollSnapType() const {
    return static_cast<ScrollSnapType>(
        m_rareNonInheritedData->m_scrollSnapType);
  }
  void setScrollSnapType(ScrollSnapType b) {
    SET_VAR(m_rareNonInheritedData, m_scrollSnapType, b);
  }

  // shape-image-threshold (aka -webkit-shape-image-threshold)
  static float initialShapeImageThreshold() { return 0; }
  float shapeImageThreshold() const {
    return m_rareNonInheritedData->m_shapeImageThreshold;
  }
  void setShapeImageThreshold(float shapeImageThreshold) {
    float clampedShapeImageThreshold =
        clampTo<float>(shapeImageThreshold, 0, 1);
    SET_VAR(m_rareNonInheritedData, m_shapeImageThreshold,
            clampedShapeImageThreshold);
  }

  // shape-margin (aka -webkit-shape-margin)
  static Length initialShapeMargin() { return Length(0, Fixed); }
  const Length& shapeMargin() const {
    return m_rareNonInheritedData->m_shapeMargin;
  }
  void setShapeMargin(const Length& shapeMargin) {
    SET_VAR(m_rareNonInheritedData, m_shapeMargin, shapeMargin);
  }

  // shape-outside (aka -webkit-shape-outside)
  static ShapeValue* initialShapeOutside() { return 0; }
  ShapeValue* shapeOutside() const {
    return m_rareNonInheritedData->m_shapeOutside.get();
  }
  void setShapeOutside(ShapeValue* value) {
    if (m_rareNonInheritedData->m_shapeOutside == value)
      return;
    m_rareNonInheritedData.access()->m_shapeOutside = value;
  }

  // size
  const FloatSize& pageSize() const {
    return m_rareNonInheritedData->m_pageSize;
  }
  PageSizeType getPageSizeType() const {
    return static_cast<PageSizeType>(m_rareNonInheritedData->m_pageSizeType);
  }
  void setPageSize(const FloatSize& s) {
    SET_VAR(m_rareNonInheritedData, m_pageSize, s);
  }
  void setPageSizeType(PageSizeType t) {
    SET_VAR(m_rareNonInheritedData, m_pageSizeType, t);
  }
  // table-layout
  static ETableLayout initialTableLayout() { return TableLayoutAuto; }
  ETableLayout tableLayout() const {
    return static_cast<ETableLayout>(m_nonInheritedData.m_tableLayout);
  }
  void setTableLayout(ETableLayout v) { m_nonInheritedData.m_tableLayout = v; }

  // Text decoration properties.
  // text-decoration-line
  static TextDecoration initialTextDecoration() { return TextDecorationNone; }
  TextDecoration getTextDecoration() const {
    return static_cast<TextDecoration>(m_visual->textDecoration);
  }
  void setTextDecoration(TextDecoration v) {
    SET_VAR(m_visual, textDecoration, v);
  }

  // text-decoration-color
  void setTextDecorationColor(const StyleColor& c) {
    SET_VAR(m_rareNonInheritedData, m_textDecorationColor, c);
  }

  // text-decoration-style
  static TextDecorationStyle initialTextDecorationStyle() {
    return TextDecorationStyleSolid;
  }
  TextDecorationStyle getTextDecorationStyle() const {
    return static_cast<TextDecorationStyle>(
        m_rareNonInheritedData->m_textDecorationStyle);
  }
  void setTextDecorationStyle(TextDecorationStyle v) {
    SET_VAR(m_rareNonInheritedData, m_textDecorationStyle, v);
  }

  // text-underline-position
  static TextUnderlinePosition initialTextUnderlinePosition() {
    return TextUnderlinePositionAuto;
  }
  TextUnderlinePosition getTextUnderlinePosition() const {
    return static_cast<TextUnderlinePosition>(
        m_rareInheritedData->m_textUnderlinePosition);
  }
  void setTextUnderlinePosition(TextUnderlinePosition v) {
    SET_VAR(m_rareInheritedData, m_textUnderlinePosition, v);
  }

  // text-decoration-skip
  static TextDecorationSkip initialTextDecorationSkip() {
    return TextDecorationSkipObjects;
  }
  TextDecorationSkip getTextDecorationSkip() const {
    return static_cast<TextDecorationSkip>(
        m_rareInheritedData->m_textDecorationSkip);
  }
  void setTextDecorationSkip(TextDecorationSkip v) {
    SET_VAR(m_rareInheritedData, m_textDecorationSkip, v);
  }

  // text-overflow
  static TextOverflow initialTextOverflow() { return TextOverflowClip; }
  TextOverflow getTextOverflow() const {
    return static_cast<TextOverflow>(m_rareNonInheritedData->textOverflow);
  }
  void setTextOverflow(TextOverflow overflow) {
    SET_VAR(m_rareNonInheritedData, textOverflow, overflow);
  }

  // touch-action
  static TouchAction initialTouchAction() { return TouchActionAuto; }
  TouchAction getTouchAction() const {
    return static_cast<TouchAction>(m_rareNonInheritedData->m_touchAction);
  }
  void setTouchAction(TouchAction t) {
    SET_VAR(m_rareNonInheritedData, m_touchAction, t);
  }

  // unicode-bidi
  static EUnicodeBidi initialUnicodeBidi() { return UBNormal; }
  EUnicodeBidi unicodeBidi() const {
    return static_cast<EUnicodeBidi>(m_nonInheritedData.m_unicodeBidi);
  }
  void setUnicodeBidi(EUnicodeBidi b) { m_nonInheritedData.m_unicodeBidi = b; }

  // vertical-align
  static EVerticalAlign initialVerticalAlign() { return VerticalAlignBaseline; }
  EVerticalAlign verticalAlign() const {
    return static_cast<EVerticalAlign>(m_nonInheritedData.m_verticalAlign);
  }
  const Length& getVerticalAlignLength() const {
    return m_box->verticalAlign();
  }
  void setVerticalAlign(EVerticalAlign v) {
    m_nonInheritedData.m_verticalAlign = v;
  }
  void setVerticalAlignLength(const Length& length) {
    setVerticalAlign(VerticalAlignLength);
    SET_VAR(m_box, m_verticalAlign, length);
  }

  // visibility
  // TODO(sashab): Move this to ComputedStyleBase.
  void setVisibilityIsInherited(bool isInherited) {
    m_nonInheritedData.m_isVisibilityInherited = isInherited;
  }

  // will-change
  const Vector<CSSPropertyID>& willChangeProperties() const {
    return m_rareNonInheritedData->m_willChange->m_properties;
  }
  bool willChangeContents() const {
    return m_rareNonInheritedData->m_willChange->m_contents;
  }
  bool willChangeScrollPosition() const {
    return m_rareNonInheritedData->m_willChange->m_scrollPosition;
  }
  bool subtreeWillChangeContents() const {
    return m_rareInheritedData->m_subtreeWillChangeContents;
  }
  void setWillChangeProperties(const Vector<CSSPropertyID>& properties) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_willChange, m_properties,
                   properties);
  }
  void setWillChangeContents(bool b) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_willChange, m_contents, b);
  }
  void setWillChangeScrollPosition(bool b) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_willChange, m_scrollPosition, b);
  }
  void setSubtreeWillChangeContents(bool b) {
    SET_VAR(m_rareInheritedData, m_subtreeWillChangeContents, b);
  }

  // z-index
  int zIndex() const { return m_box->zIndex(); }
  bool hasAutoZIndex() const { return m_box->hasAutoZIndex(); }
  void setZIndex(int v) {
    SET_VAR(m_box, m_hasAutoZIndex, false);
    SET_VAR(m_box, m_zIndex, v);
  }
  void setHasAutoZIndex() {
    SET_VAR(m_box, m_hasAutoZIndex, true);
    SET_VAR(m_box, m_zIndex, 0);
  }

  // zoom
  static float initialZoom() { return 1.0f; }
  float zoom() const { return m_visual->m_zoom; }
  float effectiveZoom() const { return m_rareInheritedData->m_effectiveZoom; }
  bool setZoom(float);
  bool setEffectiveZoom(float);

  // -webkit-app-region
  DraggableRegionMode getDraggableRegionMode() const {
    return m_rareNonInheritedData->m_draggableRegionMode;
  }
  void setDraggableRegionMode(DraggableRegionMode v) {
    SET_VAR(m_rareNonInheritedData, m_draggableRegionMode, v);
  }

  // -webkit-appearance
  static ControlPart initialAppearance() { return NoControlPart; }
  ControlPart appearance() const {
    return static_cast<ControlPart>(m_rareNonInheritedData->m_appearance);
  }
  void setAppearance(ControlPart a) {
    SET_VAR(m_rareNonInheritedData, m_appearance, a);
  }

  // -webkit-clip-path
  static ClipPathOperation* initialClipPath() { return 0; }
  ClipPathOperation* clipPath() const {
    return m_rareNonInheritedData->m_clipPath.get();
  }
  void setClipPath(PassRefPtr<ClipPathOperation> operation) {
    if (m_rareNonInheritedData->m_clipPath != operation)
      m_rareNonInheritedData.access()->m_clipPath = operation;
  }

  // Mask properties.
  // -webkit-mask-box-image-outset
  const BorderImageLengthBox& maskBoxImageOutset() const {
    return m_rareNonInheritedData->m_maskBoxImage.outset();
  }
  void setMaskBoxImageOutset(const BorderImageLengthBox& outset) {
    m_rareNonInheritedData.access()->m_maskBoxImage.setOutset(outset);
  }

  // -webkit-mask-box-image-slice
  const LengthBox& maskBoxImageSlices() const {
    return m_rareNonInheritedData->m_maskBoxImage.imageSlices();
  }
  void setMaskBoxImageSlices(const LengthBox& slices) {
    m_rareNonInheritedData.access()->m_maskBoxImage.setImageSlices(slices);
  }

  // -webkit-mask-box-image-source
  static StyleImage* initialMaskBoxImageSource() { return 0; }
  StyleImage* maskBoxImageSource() const {
    return m_rareNonInheritedData->m_maskBoxImage.image();
  }
  void setMaskBoxImageSource(StyleImage* v) {
    m_rareNonInheritedData.access()->m_maskBoxImage.setImage(v);
  }

  // -webkit-mask-box-image-width
  const BorderImageLengthBox& maskBoxImageWidth() const {
    return m_rareNonInheritedData->m_maskBoxImage.borderSlices();
  }
  void setMaskBoxImageWidth(const BorderImageLengthBox& slices) {
    m_rareNonInheritedData.access()->m_maskBoxImage.setBorderSlices(slices);
  }

  // Inherited properties.

  // border-collapse
  static EBorderCollapse initialBorderCollapse() {
    return BorderCollapseSeparate;
  }
  EBorderCollapse borderCollapse() const {
    return static_cast<EBorderCollapse>(m_inheritedData.m_borderCollapse);
  }
  void setBorderCollapse(EBorderCollapse collapse) {
    m_inheritedData.m_borderCollapse = collapse;
  }

  // Border-spacing properties.
  // -webkit-border-horizontal-spacing
  static short initialHorizontalBorderSpacing() { return 0; }
  short horizontalBorderSpacing() const;
  void setHorizontalBorderSpacing(short);

  // -webkit-border-vertical-spacing
  static short initialVerticalBorderSpacing() { return 0; }
  short verticalBorderSpacing() const;
  void setVerticalBorderSpacing(short);

  // caption-side (aka -epub-caption-side)
  static ECaptionSide initialCaptionSide() { return ECaptionSide::Top; }
  ECaptionSide captionSide() const {
    return static_cast<ECaptionSide>(m_inheritedData.m_captionSide);
  }
  void setCaptionSide(ECaptionSide v) {
    m_inheritedData.m_captionSide = static_cast<unsigned>(v);
  }

  // cursor
  static ECursor initialCursor() { return ECursor::Auto; }
  ECursor cursor() const {
    return static_cast<ECursor>(m_inheritedData.m_cursorStyle);
  }
  void setCursor(ECursor c) {
    m_inheritedData.m_cursorStyle = static_cast<unsigned>(c);
  }

  // direction
  static TextDirection initialDirection() { return LTR; }
  TextDirection direction() const {
    return static_cast<TextDirection>(m_inheritedData.m_direction);
  }
  void setDirection(TextDirection v) { m_inheritedData.m_direction = v; }

  // color
  static Color initialColor() { return Color::black; }
  void setColor(const Color&);

  // hyphens
  static Hyphens initialHyphens() { return HyphensManual; }
  Hyphens getHyphens() const {
    return static_cast<Hyphens>(m_rareInheritedData->hyphens);
  }
  void setHyphens(Hyphens h) { SET_VAR(m_rareInheritedData, hyphens, h); }

  // -webkit-hyphenate-character
  static const AtomicString& initialHyphenationString() { return nullAtom; }
  const AtomicString& hyphenationString() const {
    return m_rareInheritedData->hyphenationString;
  }
  void setHyphenationString(const AtomicString& h) {
    SET_VAR(m_rareInheritedData, hyphenationString, h);
  }

  // line-height
  static Length initialLineHeight() { return Length(-100.0, Percent); }
  Length lineHeight() const;
  void setLineHeight(const Length& specifiedLineHeight);

  // List style properties.
  // list-style-type
  static EListStyleType initialListStyleType() { return EListStyleType::Disc; }
  EListStyleType listStyleType() const {
    return static_cast<EListStyleType>(m_inheritedData.m_listStyleType);
  }
  void setListStyleType(EListStyleType v) {
    m_inheritedData.m_listStyleType = static_cast<unsigned>(v);
  }

  // list-style-position
  static EListStylePosition initialListStylePosition() {
    return EListStylePosition::Outside;
  }
  EListStylePosition listStylePosition() const {
    return static_cast<EListStylePosition>(m_inheritedData.m_listStylePosition);
  }
  void setListStylePosition(EListStylePosition v) {
    m_inheritedData.m_listStylePosition = static_cast<unsigned>(v);
  }

  // list-style-image
  static StyleImage* initialListStyleImage() { return 0; }
  StyleImage* listStyleImage() const;
  void setListStyleImage(StyleImage*);

  // orphans
  static short initialOrphans() { return 2; }
  short orphans() const { return m_rareInheritedData->orphans; }
  void setOrphans(short o) { SET_VAR(m_rareInheritedData, orphans, o); }

  // widows
  static short initialWidows() { return 2; }
  short widows() const { return m_rareInheritedData->widows; }
  void setWidows(short w) { SET_VAR(m_rareInheritedData, widows, w); }

  // overflow-wrap (aka word-wrap)
  static EOverflowWrap initialOverflowWrap() { return NormalOverflowWrap; }
  EOverflowWrap overflowWrap() const {
    return static_cast<EOverflowWrap>(m_rareInheritedData->overflowWrap);
  }
  void setOverflowWrap(EOverflowWrap b) {
    SET_VAR(m_rareInheritedData, overflowWrap, b);
  }

  // pointer-events
  static EPointerEvents initialPointerEvents() { return PE_AUTO; }
  EPointerEvents pointerEvents() const {
    return static_cast<EPointerEvents>(m_inheritedData.m_pointerEvents);
  }
  void setPointerEvents(EPointerEvents p) {
    m_inheritedData.m_pointerEvents = p;
  }
  void setPointerEventsIsInherited(bool isInherited) {
    m_nonInheritedData.m_isPointerEventsInherited = isInherited;
  }

  // quotes
  static QuotesData* initialQuotes() { return 0; }
  QuotesData* quotes() const { return m_rareInheritedData->quotes.get(); }
  void setQuotes(PassRefPtr<QuotesData>);

  // snap-height
  uint8_t snapHeightPosition() const {
    return m_rareInheritedData->m_snapHeightPosition;
  }
  uint8_t snapHeightUnit() const {
    return m_rareInheritedData->m_snapHeightUnit;
  }
  void setSnapHeightPosition(uint8_t position) {
    SET_VAR(m_rareInheritedData, m_snapHeightPosition, position);
  }
  void setSnapHeightUnit(uint8_t unit) {
    SET_VAR(m_rareInheritedData, m_snapHeightUnit, unit);
  }

  // speak
  static ESpeak initialSpeak() { return SpeakNormal; }
  ESpeak speak() const {
    return static_cast<ESpeak>(m_rareInheritedData->speak);
  }
  void setSpeak(ESpeak s) { SET_VAR(m_rareInheritedData, speak, s); }

  // tab-size
  static TabSize initialTabSize() { return TabSize(8); }
  TabSize getTabSize() const { return m_rareInheritedData->m_tabSize; }
  void setTabSize(TabSize size) {
    SET_VAR(m_rareInheritedData, m_tabSize, size);
  }

  // text-align
  static ETextAlign initialTextAlign() { return ETextAlign::Start; }
  ETextAlign textAlign() const {
    return static_cast<ETextAlign>(m_inheritedData.m_textAlign);
  }
  void setTextAlign(ETextAlign v) {
    m_inheritedData.m_textAlign = static_cast<unsigned>(v);
  }

  // text-align-last
  static TextAlignLast initialTextAlignLast() { return TextAlignLastAuto; }
  TextAlignLast getTextAlignLast() const {
    return static_cast<TextAlignLast>(m_rareInheritedData->m_textAlignLast);
  }
  void setTextAlignLast(TextAlignLast v) {
    SET_VAR(m_rareInheritedData, m_textAlignLast, v);
  }

  // text-combine-upright (aka -webkit-text-combine, -epub-text-combine)
  static TextCombine initialTextCombine() { return TextCombineNone; }
  TextCombine getTextCombine() const {
    return static_cast<TextCombine>(m_rareInheritedData->m_textCombine);
  }
  void setTextCombine(TextCombine v) {
    SET_VAR(m_rareInheritedData, m_textCombine, v);
  }

  // text-indent
  static Length initialTextIndent() { return Length(Fixed); }
  static TextIndentLine initialTextIndentLine() { return TextIndentFirstLine; }
  static TextIndentType initialTextIndentType() { return TextIndentNormal; }
  const Length& textIndent() const { return m_rareInheritedData->indent; }
  TextIndentLine getTextIndentLine() const {
    return static_cast<TextIndentLine>(m_rareInheritedData->m_textIndentLine);
  }
  TextIndentType getTextIndentType() const {
    return static_cast<TextIndentType>(m_rareInheritedData->m_textIndentType);
  }
  void setTextIndent(const Length& v) {
    SET_VAR(m_rareInheritedData, indent, v);
  }
  void setTextIndentLine(TextIndentLine v) {
    SET_VAR(m_rareInheritedData, m_textIndentLine, v);
  }
  void setTextIndentType(TextIndentType v) {
    SET_VAR(m_rareInheritedData, m_textIndentType, v);
  }

  // text-justify
  static TextJustify initialTextJustify() { return TextJustifyAuto; }
  TextJustify getTextJustify() const {
    return static_cast<TextJustify>(m_rareInheritedData->m_textJustify);
  }
  void setTextJustify(TextJustify v) {
    SET_VAR(m_rareInheritedData, m_textJustify, v);
  }

  // text-orientation (aka -webkit-text-orientation, -epub-text-orientation)
  static TextOrientation initialTextOrientation() {
    return TextOrientationMixed;
  }
  TextOrientation getTextOrientation() const {
    return static_cast<TextOrientation>(m_rareInheritedData->m_textOrientation);
  }
  bool setTextOrientation(TextOrientation);

  // text-shadow
  static ShadowList* initialTextShadow() { return 0; }
  ShadowList* textShadow() const {
    return m_rareInheritedData->textShadow.get();
  }
  void setTextShadow(PassRefPtr<ShadowList>);

  // text-size-adjust (aka -webkit-text-size-adjust)
  static TextSizeAdjust initialTextSizeAdjust() {
    return TextSizeAdjust::adjustAuto();
  }
  TextSizeAdjust getTextSizeAdjust() const {
    return m_rareInheritedData->m_textSizeAdjust;
  }
  void setTextSizeAdjust(TextSizeAdjust sizeAdjust) {
    SET_VAR(m_rareInheritedData, m_textSizeAdjust, sizeAdjust);
  }

  // text-transform (aka -epub-text-transform)
  static ETextTransform initialTextTransform() { return TTNONE; }
  ETextTransform textTransform() const {
    return static_cast<ETextTransform>(m_inheritedData.m_textTransform);
  }
  void setTextTransform(ETextTransform v) {
    m_inheritedData.m_textTransform = v;
  }

  // white-space inherited
  static EWhiteSpace initialWhiteSpace() { return NORMAL; }
  EWhiteSpace whiteSpace() const {
    return static_cast<EWhiteSpace>(m_inheritedData.m_whiteSpace);
  }
  void setWhiteSpace(EWhiteSpace v) { m_inheritedData.m_whiteSpace = v; }

  // word-break inherited (aka -epub-word-break)
  static EWordBreak initialWordBreak() { return NormalWordBreak; }
  EWordBreak wordBreak() const {
    return static_cast<EWordBreak>(m_rareInheritedData->wordBreak);
  }
  void setWordBreak(EWordBreak b) {
    SET_VAR(m_rareInheritedData, wordBreak, b);
  }

  // -webkit-line-break
  static LineBreak initialLineBreak() { return LineBreakAuto; }
  LineBreak getLineBreak() const {
    return static_cast<LineBreak>(m_rareInheritedData->lineBreak);
  }
  void setLineBreak(LineBreak b) { SET_VAR(m_rareInheritedData, lineBreak, b); }

  // writing-mode (aka -webkit-writing-mode, -epub-writing-mode)
  static WritingMode initialWritingMode() { return TopToBottomWritingMode; }
  WritingMode getWritingMode() const {
    return static_cast<WritingMode>(m_inheritedData.m_writingMode);
  }
  bool setWritingMode(WritingMode v) {
    if (v == getWritingMode())
      return false;

    m_inheritedData.m_writingMode = v;
    return true;
  }

  // Text emphasis properties.
  static TextEmphasisFill initialTextEmphasisFill() {
    return TextEmphasisFillFilled;
  }
  static TextEmphasisMark initialTextEmphasisMark() {
    return TextEmphasisMarkNone;
  }
  static const AtomicString& initialTextEmphasisCustomMark() {
    return nullAtom;
  }
  TextEmphasisFill getTextEmphasisFill() const {
    return static_cast<TextEmphasisFill>(m_rareInheritedData->textEmphasisFill);
  }
  TextEmphasisMark getTextEmphasisMark() const;
  const AtomicString& textEmphasisCustomMark() const {
    return m_rareInheritedData->textEmphasisCustomMark;
  }
  const AtomicString& textEmphasisMarkString() const;
  void setTextEmphasisFill(TextEmphasisFill fill) {
    SET_VAR(m_rareInheritedData, textEmphasisFill, fill);
  }
  void setTextEmphasisMark(TextEmphasisMark mark) {
    SET_VAR(m_rareInheritedData, textEmphasisMark, mark);
  }
  void setTextEmphasisCustomMark(const AtomicString& mark) {
    SET_VAR(m_rareInheritedData, textEmphasisCustomMark, mark);
  }

  // -webkit-text-emphasis-color (aka -epub-text-emphasis-color)
  void setTextEmphasisColor(const StyleColor& c) {
    SET_VAR_WITH_SETTER(m_rareInheritedData, textEmphasisColor,
                        setTextEmphasisColor, c);
  }

  // -webkit-text-emphasis-position
  static TextEmphasisPosition initialTextEmphasisPosition() {
    return TextEmphasisPositionOver;
  }
  TextEmphasisPosition getTextEmphasisPosition() const {
    return static_cast<TextEmphasisPosition>(
        m_rareInheritedData->textEmphasisPosition);
  }
  void setTextEmphasisPosition(TextEmphasisPosition position) {
    SET_VAR(m_rareInheritedData, textEmphasisPosition, position);
  }

  // -webkit-box-direction
  static EBoxDirection initialBoxDirection() { return BNORMAL; }
  EBoxDirection boxDirection() const {
    return static_cast<EBoxDirection>(m_inheritedData.m_boxDirection);
  }
  void setBoxDirection(EBoxDirection d) { m_inheritedData.m_boxDirection = d; }

  // -webkit-highlight
  static const AtomicString& initialHighlight() { return nullAtom; }
  const AtomicString& highlight() const {
    return m_rareInheritedData->highlight;
  }
  void setHighlight(const AtomicString& h) {
    SET_VAR(m_rareInheritedData, highlight, h);
  }

  // -webkit-line-clamp
  static LineClampValue initialLineClamp() { return LineClampValue(); }
  const LineClampValue& lineClamp() const {
    return m_rareNonInheritedData->lineClamp;
  }
  void setLineClamp(LineClampValue c) {
    SET_VAR(m_rareNonInheritedData, lineClamp, c);
  }

  // -webkit-print-color-adjust
  static PrintColorAdjust initialPrintColorAdjust() {
    return PrintColorAdjustEconomy;
  }
  PrintColorAdjust getPrintColorAdjust() const {
    return static_cast<PrintColorAdjust>(m_inheritedData.m_printColorAdjust);
  }
  void setPrintColorAdjust(PrintColorAdjust value) {
    m_inheritedData.m_printColorAdjust = value;
  }

  // -webkit-rtl-ordering
  static Order initialRTLOrdering() { return LogicalOrder; }
  Order rtlOrdering() const {
    return static_cast<Order>(m_inheritedData.m_rtlOrdering);
  }
  void setRTLOrdering(Order o) { m_inheritedData.m_rtlOrdering = o; }

  // -webkit-ruby-position
  static RubyPosition initialRubyPosition() { return RubyPositionBefore; }
  RubyPosition getRubyPosition() const {
    return static_cast<RubyPosition>(m_rareInheritedData->m_rubyPosition);
  }
  void setRubyPosition(RubyPosition position) {
    SET_VAR(m_rareInheritedData, m_rubyPosition, position);
  }

  // -webkit-tap-highlight-color
  static Color initialTapHighlightColor();
  Color tapHighlightColor() const {
    return m_rareInheritedData->tapHighlightColor;
  }
  void setTapHighlightColor(const Color& c) {
    SET_VAR(m_rareInheritedData, tapHighlightColor, c);
  }

  // -webkit-text-fill-color
  void setTextFillColor(const StyleColor& c) {
    SET_VAR_WITH_SETTER(m_rareInheritedData, textFillColor, setTextFillColor,
                        c);
  }

  // -webkit-text-security
  static ETextSecurity initialTextSecurity() { return TSNONE; }
  ETextSecurity textSecurity() const {
    return static_cast<ETextSecurity>(m_rareInheritedData->textSecurity);
  }
  void setTextSecurity(ETextSecurity aTextSecurity) {
    SET_VAR(m_rareInheritedData, textSecurity, aTextSecurity);
  }

  // -webkit-text-stroke-color
  void setTextStrokeColor(const StyleColor& c) {
    SET_VAR_WITH_SETTER(m_rareInheritedData, textStrokeColor,
                        setTextStrokeColor, c);
  }

  // -webkit-text-stroke-width
  static float initialTextStrokeWidth() { return 0; }
  float textStrokeWidth() const { return m_rareInheritedData->textStrokeWidth; }
  void setTextStrokeWidth(float w) {
    SET_VAR(m_rareInheritedData, textStrokeWidth, w);
  }

  // -webkit-user-drag
  static EUserDrag initialUserDrag() { return DRAG_AUTO; }
  EUserDrag userDrag() const {
    return static_cast<EUserDrag>(m_rareNonInheritedData->userDrag);
  }
  void setUserDrag(EUserDrag d) {
    SET_VAR(m_rareNonInheritedData, userDrag, d);
  }

  // -webkit-user-modify
  static EUserModify initialUserModify() { return READ_ONLY; }
  EUserModify userModify() const {
    return static_cast<EUserModify>(m_rareInheritedData->userModify);
  }
  void setUserModify(EUserModify u) {
    SET_VAR(m_rareInheritedData, userModify, u);
  }

  // -webkit-user-select
  static EUserSelect initialUserSelect() { return SELECT_TEXT; }
  EUserSelect userSelect() const {
    return static_cast<EUserSelect>(m_rareInheritedData->userSelect);
  }
  void setUserSelect(EUserSelect s) {
    SET_VAR(m_rareInheritedData, userSelect, s);
  }

  // Font properties.
  const Font& font() const;
  void setFont(const Font&);
  const FontDescription& getFontDescription() const;
  bool setFontDescription(const FontDescription&);
  bool hasIdenticalAscentDescentAndLineGap(const ComputedStyle& other) const;

  // font-size
  int fontSize() const;
  float specifiedFontSize() const;
  float computedFontSize() const;

  // font-size-adjust
  float fontSizeAdjust() const;
  bool hasFontSizeAdjust() const;

  // font-weight
  FontWeight fontWeight() const;

  // font-stretch
  FontStretch fontStretch() const;

  // -webkit-locale
  const AtomicString& locale() const {
    return LayoutLocale::localeString(getFontDescription().locale());
  }

  // FIXME: Remove letter-spacing/word-spacing and replace them with respective
  // FontBuilder calls.  letter-spacing
  static float initialLetterWordSpacing() { return 0.0f; }
  float letterSpacing() const;
  void setLetterSpacing(float);

  // word-spacing
  float wordSpacing() const;
  void setWordSpacing(float);

  // SVG properties.
  const SVGComputedStyle& svgStyle() const { return *m_svgStyle.get(); }
  SVGComputedStyle& accessSVGStyle() { return *m_svgStyle.access(); }

  // baseline-shift
  EBaselineShift baselineShift() const { return svgStyle().baselineShift(); }
  const Length& baselineShiftValue() const {
    return svgStyle().baselineShiftValue();
  }
  void setBaselineShiftValue(const Length& value) {
    SVGComputedStyle& svgStyle = accessSVGStyle();
    svgStyle.setBaselineShift(BS_LENGTH);
    svgStyle.setBaselineShiftValue(value);
  }

  // cx
  void setCx(const Length& cx) { accessSVGStyle().setCx(cx); }

  // cy
  void setCy(const Length& cy) { accessSVGStyle().setCy(cy); }

  // d
  void setD(PassRefPtr<StylePath> d) { accessSVGStyle().setD(std::move(d)); }

  // x
  void setX(const Length& x) { accessSVGStyle().setX(x); }

  // y
  void setY(const Length& y) { accessSVGStyle().setY(y); }

  // r
  void setR(const Length& r) { accessSVGStyle().setR(r); }

  // rx
  void setRx(const Length& rx) { accessSVGStyle().setRx(rx); }

  // ry
  void setRy(const Length& ry) { accessSVGStyle().setRy(ry); }

  // fill-opacity
  float fillOpacity() const { return svgStyle().fillOpacity(); }
  void setFillOpacity(float f) { accessSVGStyle().setFillOpacity(f); }

  // Fill utiltiy functions.
  const SVGPaintType& fillPaintType() const {
    return svgStyle().fillPaintType();
  }
  Color fillPaintColor() const { return svgStyle().fillPaintColor(); }

  // stop-color
  void setStopColor(const Color& c) { accessSVGStyle().setStopColor(c); }

  // flood-color
  void setFloodColor(const Color& c) { accessSVGStyle().setFloodColor(c); }

  // lighting-color
  void setLightingColor(const Color& c) {
    accessSVGStyle().setLightingColor(c);
  }

  // flood-opacity
  float floodOpacity() const { return svgStyle().floodOpacity(); }
  void setFloodOpacity(float f) { accessSVGStyle().setFloodOpacity(f); }

  // stop-opacity
  float stopOpacity() const { return svgStyle().stopOpacity(); }
  void setStopOpacity(float f) { accessSVGStyle().setStopOpacity(f); }

  // stroke
  const SVGPaintType& strokePaintType() const {
    return svgStyle().strokePaintType();
  }
  Color strokePaintColor() const { return svgStyle().strokePaintColor(); }

  // stroke-dasharray
  SVGDashArray* strokeDashArray() const { return svgStyle().strokeDashArray(); }
  void setStrokeDashArray(PassRefPtr<SVGDashArray> array) {
    accessSVGStyle().setStrokeDashArray(std::move(array));
  }

  // stroke-dashoffset
  const Length& strokeDashOffset() const {
    return svgStyle().strokeDashOffset();
  }
  void setStrokeDashOffset(const Length& d) {
    accessSVGStyle().setStrokeDashOffset(d);
  }

  // stroke-miterlimit
  float strokeMiterLimit() const { return svgStyle().strokeMiterLimit(); }
  void setStrokeMiterLimit(float f) { accessSVGStyle().setStrokeMiterLimit(f); }

  // stroke-opacity
  float strokeOpacity() const { return svgStyle().strokeOpacity(); }
  void setStrokeOpacity(float f) { accessSVGStyle().setStrokeOpacity(f); }

  // stroke-width
  const UnzoomedLength& strokeWidth() const { return svgStyle().strokeWidth(); }
  void setStrokeWidth(const UnzoomedLength& w) {
    accessSVGStyle().setStrokeWidth(w);
  }

  // Comparison operators
  bool operator==(const ComputedStyle& other) const;
  bool operator!=(const ComputedStyle& other) const {
    return !(*this == other);
  }

  bool inheritedEqual(const ComputedStyle&) const;
  bool nonInheritedEqual(const ComputedStyle&) const;
  inline bool independentInheritedEqual(const ComputedStyle&) const;
  inline bool nonIndependentInheritedEqual(const ComputedStyle&) const;
  bool loadingCustomFontsEqual(const ComputedStyle&) const;
  bool inheritedDataShared(const ComputedStyle&) const;

  bool hasChildDependentFlags() const {
    return emptyState() || hasExplicitlyInheritedProperties();
  }
  void copyChildDependentFlagsFrom(const ComputedStyle&);

  // Counters.
  const CounterDirectiveMap* counterDirectives() const;
  CounterDirectiveMap& accessCounterDirectives();
  const CounterDirectives getCounterDirectives(
      const AtomicString& identifier) const;
  void clearIncrementDirectives();
  void clearResetDirectives();

  // Variables.
  static StyleInheritedVariables* initialInheritedVariables() {
    return nullptr;
  }
  static StyleNonInheritedVariables* initialNonInheritedVariables() {
    return nullptr;
  }

  StyleInheritedVariables* inheritedVariables() const;
  StyleNonInheritedVariables* nonInheritedVariables() const;

  void setUnresolvedInheritedVariable(const AtomicString&,
                                      PassRefPtr<CSSVariableData>);
  void setUnresolvedNonInheritedVariable(const AtomicString&,
                                         PassRefPtr<CSSVariableData>);

  void setResolvedUnregisteredVariable(const AtomicString&,
                                       PassRefPtr<CSSVariableData>);
  void setResolvedInheritedVariable(const AtomicString&,
                                    PassRefPtr<CSSVariableData>,
                                    const CSSValue*);
  void setResolvedNonInheritedVariable(const AtomicString&,
                                       PassRefPtr<CSSVariableData>,
                                       const CSSValue*);

  void removeInheritedVariable(const AtomicString&);
  void removeNonInheritedVariable(const AtomicString&);

  // Handles both inherited and non-inherited variables
  CSSVariableData* getVariable(const AtomicString&) const;

  void setHasVariableReferenceFromNonInheritedProperty() {
    m_nonInheritedData.m_variableReference = true;
  }
  bool hasVariableReferenceFromNonInheritedProperty() const {
    return m_nonInheritedData.m_variableReference;
  }

  // Animations.
  CSSAnimationData& accessAnimations();
  const CSSAnimationData* animations() const {
    return m_rareNonInheritedData->m_animations.get();
  }

  // Transitions.
  const CSSTransitionData* transitions() const {
    return m_rareNonInheritedData->m_transitions.get();
  }
  CSSTransitionData& accessTransitions();

  // Callback selectors.
  const Vector<String>& callbackSelectors() const {
    return m_rareNonInheritedData->m_callbackSelectors;
  }
  void addCallbackSelector(const String& selector);

  // Non-property flags.
  bool hasViewportUnits() const {
    return m_nonInheritedData.m_hasViewportUnits;
  }
  void setHasViewportUnits(bool hasViewportUnits = true) const {
    m_nonInheritedData.m_hasViewportUnits = hasViewportUnits;
  }

  bool hasRemUnits() const { return m_nonInheritedData.m_hasRemUnits; }
  void setHasRemUnits() const { m_nonInheritedData.m_hasRemUnits = true; }

  bool affectedByFocus() const { return m_nonInheritedData.m_affectedByFocus; }
  void setAffectedByFocus() { m_nonInheritedData.m_affectedByFocus = true; }

  bool affectedByHover() const { return m_nonInheritedData.m_affectedByHover; }
  void setAffectedByHover() { m_nonInheritedData.m_affectedByHover = true; }

  bool affectedByActive() const {
    return m_nonInheritedData.m_affectedByActive;
  }
  void setAffectedByActive() { m_nonInheritedData.m_affectedByActive = true; }

  bool affectedByDrag() const { return m_nonInheritedData.m_affectedByDrag; }
  void setAffectedByDrag() { m_nonInheritedData.m_affectedByDrag = true; }

  bool emptyState() const { return m_nonInheritedData.m_emptyState; }
  void setEmptyState(bool b) {
    setUnique();
    m_nonInheritedData.m_emptyState = b;
  }

  bool hasInlineTransform() const {
    return m_rareNonInheritedData->m_hasInlineTransform;
  }
  void setHasInlineTransform(bool b) {
    SET_VAR(m_rareNonInheritedData, m_hasInlineTransform, b);
  }

  bool hasCompositorProxy() const {
    return m_rareNonInheritedData->m_hasCompositorProxy;
  }
  void setHasCompositorProxy(bool b) {
    SET_VAR(m_rareNonInheritedData, m_hasCompositorProxy, b);
  }

  bool isLink() const { return m_nonInheritedData.m_isLink; }
  void setIsLink(bool b) { m_nonInheritedData.m_isLink = b; }

  EInsideLink insideLink() const {
    return static_cast<EInsideLink>(m_inheritedData.m_insideLink);
  }
  void setInsideLink(EInsideLink insideLink) {
    m_inheritedData.m_insideLink = insideLink;
  }

  bool hasExplicitlyInheritedProperties() const {
    return m_nonInheritedData.m_explicitInheritance;
  }
  void setHasExplicitlyInheritedProperties() {
    m_nonInheritedData.m_explicitInheritance = true;
  }

  bool requiresAcceleratedCompositingForExternalReasons(bool b) {
    return m_rareNonInheritedData
        ->m_requiresAcceleratedCompositingForExternalReasons;
  }
  void setRequiresAcceleratedCompositingForExternalReasons(bool b) {
    SET_VAR(m_rareNonInheritedData,
            m_requiresAcceleratedCompositingForExternalReasons, b);
  }

  bool hasAuthorBackground() const {
    return m_rareNonInheritedData->m_hasAuthorBackground;
  };
  void setHasAuthorBackground(bool authorBackground) {
    SET_VAR(m_rareNonInheritedData, m_hasAuthorBackground, authorBackground);
  }

  bool hasAuthorBorder() const {
    return m_rareNonInheritedData->m_hasAuthorBorder;
  };
  void setHasAuthorBorder(bool authorBorder) {
    SET_VAR(m_rareNonInheritedData, m_hasAuthorBorder, authorBorder);
  }

  // A stacking context is painted atomically and defines a stacking order,
  // whereas a containing stacking context defines in which order the stacking
  // contexts below are painted.
  // See CSS 2.1, Appendix E (https://www.w3.org/TR/CSS21/zindex.html) for more
  // details.
  bool isStackingContext() const {
    return m_rareNonInheritedData->m_isStackingContext;
  }
  void setIsStackingContext(bool b) {
    SET_VAR(m_rareNonInheritedData, m_isStackingContext, b);
  }

  // A unique style is one that has matches something that makes it impossible
  // to share.
  bool unique() const { return m_nonInheritedData.m_unique; }
  void setUnique() { m_nonInheritedData.m_unique = true; }

  float textAutosizingMultiplier() const {
    return m_styleInheritedData->textAutosizingMultiplier;
  }
  void setTextAutosizingMultiplier(float);

  bool selfOrAncestorHasDirAutoAttribute() const {
    return m_rareInheritedData->m_selfOrAncestorHasDirAutoAttribute;
  }
  void setSelfOrAncestorHasDirAutoAttribute(bool v) {
    SET_VAR(m_rareInheritedData, m_selfOrAncestorHasDirAutoAttribute, v);
  }

  // Animation flags.
  bool hasCurrentOpacityAnimation() const {
    return m_rareNonInheritedData->m_hasCurrentOpacityAnimation;
  }
  void setHasCurrentOpacityAnimation(bool b = true) {
    SET_VAR(m_rareNonInheritedData, m_hasCurrentOpacityAnimation, b);
  }

  bool hasCurrentTransformAnimation() const {
    return m_rareNonInheritedData->m_hasCurrentTransformAnimation;
  }
  void setHasCurrentTransformAnimation(bool b = true) {
    SET_VAR(m_rareNonInheritedData, m_hasCurrentTransformAnimation, b);
  }

  bool hasCurrentFilterAnimation() const {
    return m_rareNonInheritedData->m_hasCurrentFilterAnimation;
  }
  void setHasCurrentFilterAnimation(bool b = true) {
    SET_VAR(m_rareNonInheritedData, m_hasCurrentFilterAnimation, b);
  }

  bool hasCurrentBackdropFilterAnimation() const {
    return m_rareNonInheritedData->m_hasCurrentBackdropFilterAnimation;
  }
  void setHasCurrentBackdropFilterAnimation(bool b = true) {
    SET_VAR(m_rareNonInheritedData, m_hasCurrentBackdropFilterAnimation, b);
  }

  bool isRunningOpacityAnimationOnCompositor() const {
    return m_rareNonInheritedData->m_runningOpacityAnimationOnCompositor;
  }
  void setIsRunningOpacityAnimationOnCompositor(bool b = true) {
    SET_VAR(m_rareNonInheritedData, m_runningOpacityAnimationOnCompositor, b);
  }

  bool isRunningTransformAnimationOnCompositor() const {
    return m_rareNonInheritedData->m_runningTransformAnimationOnCompositor;
  }
  void setIsRunningTransformAnimationOnCompositor(bool b = true) {
    SET_VAR(m_rareNonInheritedData, m_runningTransformAnimationOnCompositor, b);
  }

  bool isRunningFilterAnimationOnCompositor() const {
    return m_rareNonInheritedData->m_runningFilterAnimationOnCompositor;
  }
  void setIsRunningFilterAnimationOnCompositor(bool b = true) {
    SET_VAR(m_rareNonInheritedData, m_runningFilterAnimationOnCompositor, b);
  }

  bool isRunningBackdropFilterAnimationOnCompositor() const {
    return m_rareNonInheritedData->m_runningBackdropFilterAnimationOnCompositor;
  }
  void setIsRunningBackdropFilterAnimationOnCompositor(bool b = true) {
    SET_VAR(m_rareNonInheritedData,
            m_runningBackdropFilterAnimationOnCompositor, b);
  }

  // Column utility functions.
  void clearMultiCol();
  bool specifiesColumns() const {
    return !hasAutoColumnCount() || !hasAutoColumnWidth();
  }
  bool columnRuleIsTransparent() const {
    return m_rareNonInheritedData->m_multiCol->m_rule.isTransparent();
  }
  bool columnRuleEquivalent(const ComputedStyle* otherStyle) const;
  void inheritColumnPropertiesFrom(const ComputedStyle& parent) {
    m_rareNonInheritedData.access()->m_multiCol =
        parent.m_rareNonInheritedData->m_multiCol;
  }

  // Flex utility functions.
  bool isColumnFlexDirection() const {
    return flexDirection() == FlowColumn ||
           flexDirection() == FlowColumnReverse;
  }
  bool isReverseFlexDirection() const {
    return flexDirection() == FlowRowReverse ||
           flexDirection() == FlowColumnReverse;
  }
  bool hasBoxReflect() const { return boxReflect(); }
  bool reflectionDataEquivalent(const ComputedStyle* otherStyle) const {
    return m_rareNonInheritedData->reflectionDataEquivalent(
        *otherStyle->m_rareNonInheritedData);
  }

  // Mask utility functions.
  bool hasMask() const {
    return m_rareNonInheritedData->m_mask.hasImage() ||
           m_rareNonInheritedData->m_maskBoxImage.hasImage();
  }
  StyleImage* maskImage() const {
    return m_rareNonInheritedData->m_mask.image();
  }
  FillLayer& accessMaskLayers() {
    return m_rareNonInheritedData.access()->m_mask;
  }
  const FillLayer& maskLayers() const { return m_rareNonInheritedData->m_mask; }
  const NinePieceImage& maskBoxImage() const {
    return m_rareNonInheritedData->m_maskBoxImage;
  }
  bool maskBoxImageSlicesFill() const {
    return m_rareNonInheritedData->m_maskBoxImage.fill();
  }
  void adjustMaskLayers() {
    if (maskLayers().next()) {
      accessMaskLayers().cullEmptyLayers();
      accessMaskLayers().fillUnsetProperties();
    }
  }
  void setMaskBoxImage(const NinePieceImage& b) {
    SET_VAR(m_rareNonInheritedData, m_maskBoxImage, b);
  }
  void setMaskBoxImageSlicesFill(bool fill) {
    m_rareNonInheritedData.access()->m_maskBoxImage.setFill(fill);
  }

  // Text-combine utility functions.
  bool hasTextCombine() const { return getTextCombine() != TextCombineNone; }

  // Grid utility functions.
  const Vector<GridTrackSize>& gridAutoRepeatColumns() const {
    return m_rareNonInheritedData->m_grid->m_gridAutoRepeatColumns;
  }
  const Vector<GridTrackSize>& gridAutoRepeatRows() const {
    return m_rareNonInheritedData->m_grid->m_gridAutoRepeatRows;
  }
  size_t gridAutoRepeatColumnsInsertionPoint() const {
    return m_rareNonInheritedData->m_grid->m_autoRepeatColumnsInsertionPoint;
  }
  size_t gridAutoRepeatRowsInsertionPoint() const {
    return m_rareNonInheritedData->m_grid->m_autoRepeatRowsInsertionPoint;
  }
  AutoRepeatType gridAutoRepeatColumnsType() const {
    return m_rareNonInheritedData->m_grid->m_autoRepeatColumnsType;
  }
  AutoRepeatType gridAutoRepeatRowsType() const {
    return m_rareNonInheritedData->m_grid->m_autoRepeatRowsType;
  }
  const NamedGridLinesMap& namedGridColumnLines() const {
    return m_rareNonInheritedData->m_grid->m_namedGridColumnLines;
  }
  const NamedGridLinesMap& namedGridRowLines() const {
    return m_rareNonInheritedData->m_grid->m_namedGridRowLines;
  }
  const OrderedNamedGridLines& orderedNamedGridColumnLines() const {
    return m_rareNonInheritedData->m_grid->m_orderedNamedGridColumnLines;
  }
  const OrderedNamedGridLines& orderedNamedGridRowLines() const {
    return m_rareNonInheritedData->m_grid->m_orderedNamedGridRowLines;
  }
  const NamedGridLinesMap& autoRepeatNamedGridColumnLines() const {
    return m_rareNonInheritedData->m_grid->m_autoRepeatNamedGridColumnLines;
  }
  const NamedGridLinesMap& autoRepeatNamedGridRowLines() const {
    return m_rareNonInheritedData->m_grid->m_autoRepeatNamedGridRowLines;
  }
  const OrderedNamedGridLines& autoRepeatOrderedNamedGridColumnLines() const {
    return m_rareNonInheritedData->m_grid
        ->m_autoRepeatOrderedNamedGridColumnLines;
  }
  const OrderedNamedGridLines& autoRepeatOrderedNamedGridRowLines() const {
    return m_rareNonInheritedData->m_grid->m_autoRepeatOrderedNamedGridRowLines;
  }
  const NamedGridAreaMap& namedGridArea() const {
    return m_rareNonInheritedData->m_grid->m_namedGridArea;
  }
  size_t namedGridAreaRowCount() const {
    return m_rareNonInheritedData->m_grid->m_namedGridAreaRowCount;
  }
  size_t namedGridAreaColumnCount() const {
    return m_rareNonInheritedData->m_grid->m_namedGridAreaColumnCount;
  }
  GridAutoFlow getGridAutoFlow() const {
    return static_cast<GridAutoFlow>(
        m_rareNonInheritedData->m_grid->m_gridAutoFlow);
  }
  bool isGridAutoFlowDirectionRow() const {
    return (m_rareNonInheritedData->m_grid->m_gridAutoFlow &
            InternalAutoFlowDirectionRow) == InternalAutoFlowDirectionRow;
  }
  bool isGridAutoFlowDirectionColumn() const {
    return (m_rareNonInheritedData->m_grid->m_gridAutoFlow &
            InternalAutoFlowDirectionColumn) == InternalAutoFlowDirectionColumn;
  }
  bool isGridAutoFlowAlgorithmSparse() const {
    return (m_rareNonInheritedData->m_grid->m_gridAutoFlow &
            InternalAutoFlowAlgorithmSparse) == InternalAutoFlowAlgorithmSparse;
  }
  bool isGridAutoFlowAlgorithmDense() const {
    return (m_rareNonInheritedData->m_grid->m_gridAutoFlow &
            InternalAutoFlowAlgorithmDense) == InternalAutoFlowAlgorithmDense;
  }
  void setGridAutoRepeatColumns(const Vector<GridTrackSize>& trackSizes) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridAutoRepeatColumns,
                   trackSizes);
  }
  void setGridAutoRepeatRows(const Vector<GridTrackSize>& trackSizes) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridAutoRepeatRows,
                   trackSizes);
  }
  void setGridAutoRepeatColumnsInsertionPoint(const size_t insertionPoint) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid,
                   m_autoRepeatColumnsInsertionPoint, insertionPoint);
  }
  void setGridAutoRepeatRowsInsertionPoint(const size_t insertionPoint) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid,
                   m_autoRepeatRowsInsertionPoint, insertionPoint);
  }
  void setGridAutoRepeatColumnsType(const AutoRepeatType autoRepeatType) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_autoRepeatColumnsType,
                   autoRepeatType);
  }
  void setGridAutoRepeatRowsType(const AutoRepeatType autoRepeatType) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_autoRepeatRowsType,
                   autoRepeatType);
  }
  void setNamedGridColumnLines(const NamedGridLinesMap& namedGridColumnLines) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_namedGridColumnLines,
                   namedGridColumnLines);
  }
  void setNamedGridRowLines(const NamedGridLinesMap& namedGridRowLines) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_namedGridRowLines,
                   namedGridRowLines);
  }
  void setOrderedNamedGridColumnLines(
      const OrderedNamedGridLines& orderedNamedGridColumnLines) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid,
                   m_orderedNamedGridColumnLines, orderedNamedGridColumnLines);
  }
  void setOrderedNamedGridRowLines(
      const OrderedNamedGridLines& orderedNamedGridRowLines) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_orderedNamedGridRowLines,
                   orderedNamedGridRowLines);
  }
  void setAutoRepeatNamedGridColumnLines(
      const NamedGridLinesMap& namedGridColumnLines) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid,
                   m_autoRepeatNamedGridColumnLines, namedGridColumnLines);
  }
  void setAutoRepeatNamedGridRowLines(
      const NamedGridLinesMap& namedGridRowLines) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid,
                   m_autoRepeatNamedGridRowLines, namedGridRowLines);
  }
  void setAutoRepeatOrderedNamedGridColumnLines(
      const OrderedNamedGridLines& orderedNamedGridColumnLines) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid,
                   m_autoRepeatOrderedNamedGridColumnLines,
                   orderedNamedGridColumnLines);
  }
  void setAutoRepeatOrderedNamedGridRowLines(
      const OrderedNamedGridLines& orderedNamedGridRowLines) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid,
                   m_autoRepeatOrderedNamedGridRowLines,
                   orderedNamedGridRowLines);
  }
  void setNamedGridArea(const NamedGridAreaMap& namedGridArea) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_namedGridArea,
                   namedGridArea);
  }
  void setNamedGridAreaRowCount(size_t rowCount) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_namedGridAreaRowCount,
                   rowCount);
  }
  void setNamedGridAreaColumnCount(size_t columnCount) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_namedGridAreaColumnCount,
                   columnCount);
  }

  // align-content utility functions.
  ContentPosition alignContentPosition() const {
    return m_rareNonInheritedData->m_alignContent.position();
  }
  ContentDistributionType alignContentDistribution() const {
    return m_rareNonInheritedData->m_alignContent.distribution();
  }
  OverflowAlignment alignContentOverflowAlignment() const {
    return m_rareNonInheritedData->m_alignContent.overflow();
  }
  void setAlignContentPosition(ContentPosition position) {
    m_rareNonInheritedData.access()->m_alignContent.setPosition(position);
  }
  void setAlignContentDistribution(ContentDistributionType distribution) {
    m_rareNonInheritedData.access()->m_alignContent.setDistribution(
        distribution);
  }
  void setAlignContentOverflow(OverflowAlignment overflow) {
    m_rareNonInheritedData.access()->m_alignContent.setOverflow(overflow);
  }

  // justify-content utility functions.
  ContentPosition justifyContentPosition() const {
    return m_rareNonInheritedData->m_justifyContent.position();
  }
  ContentDistributionType justifyContentDistribution() const {
    return m_rareNonInheritedData->m_justifyContent.distribution();
  }
  OverflowAlignment justifyContentOverflowAlignment() const {
    return m_rareNonInheritedData->m_justifyContent.overflow();
  }
  void setJustifyContentPosition(ContentPosition position) {
    m_rareNonInheritedData.access()->m_justifyContent.setPosition(position);
  }
  void setJustifyContentDistribution(ContentDistributionType distribution) {
    m_rareNonInheritedData.access()->m_justifyContent.setDistribution(
        distribution);
  }
  void setJustifyContentOverflow(OverflowAlignment overflow) {
    m_rareNonInheritedData.access()->m_justifyContent.setOverflow(overflow);
  }

  // align-items utility functions.
  ItemPosition alignItemsPosition() const {
    return m_rareNonInheritedData->m_alignItems.position();
  }
  OverflowAlignment alignItemsOverflowAlignment() const {
    return m_rareNonInheritedData->m_alignItems.overflow();
  }
  void setAlignItemsPosition(ItemPosition position) {
    m_rareNonInheritedData.access()->m_alignItems.setPosition(position);
  }
  void setAlignItemsOverflow(OverflowAlignment overflow) {
    m_rareNonInheritedData.access()->m_alignItems.setOverflow(overflow);
  }

  // justify-items utility functions.
  ItemPosition justifyItemsPosition() const {
    return m_rareNonInheritedData->m_justifyItems.position();
  }
  OverflowAlignment justifyItemsOverflowAlignment() const {
    return m_rareNonInheritedData->m_justifyItems.overflow();
  }
  ItemPositionType justifyItemsPositionType() const {
    return m_rareNonInheritedData->m_justifyItems.positionType();
  }
  void setJustifyItemsPosition(ItemPosition position) {
    m_rareNonInheritedData.access()->m_justifyItems.setPosition(position);
  }
  void setJustifyItemsOverflow(OverflowAlignment overflow) {
    m_rareNonInheritedData.access()->m_justifyItems.setOverflow(overflow);
  }
  void setJustifyItemsPositionType(ItemPositionType positionType) {
    m_rareNonInheritedData.access()->m_justifyItems.setPositionType(
        positionType);
  }

  // align-self utility functions.
  ItemPosition alignSelfPosition() const {
    return m_rareNonInheritedData->m_alignSelf.position();
  }
  OverflowAlignment alignSelfOverflowAlignment() const {
    return m_rareNonInheritedData->m_alignSelf.overflow();
  }
  void setAlignSelfPosition(ItemPosition position) {
    m_rareNonInheritedData.access()->m_alignSelf.setPosition(position);
  }
  void setAlignSelfOverflow(OverflowAlignment overflow) {
    m_rareNonInheritedData.access()->m_alignSelf.setOverflow(overflow);
  }

  // justify-self utility functions.
  ItemPosition justifySelfPosition() const {
    return m_rareNonInheritedData->m_justifySelf.position();
  }
  OverflowAlignment justifySelfOverflowAlignment() const {
    return m_rareNonInheritedData->m_justifySelf.overflow();
  }
  void setJustifySelfPosition(ItemPosition position) {
    m_rareNonInheritedData.access()->m_justifySelf.setPosition(position);
  }
  void setJustifySelfOverflow(OverflowAlignment overflow) {
    m_rareNonInheritedData.access()->m_justifySelf.setOverflow(overflow);
  }

  // Writing mode utility functions.
  bool isHorizontalWritingMode() const {
    return blink::isHorizontalWritingMode(getWritingMode());
  }
  bool isFlippedLinesWritingMode() const {
    return blink::isFlippedLinesWritingMode(getWritingMode());
  }
  bool isFlippedBlocksWritingMode() const {
    return blink::isFlippedBlocksWritingMode(getWritingMode());
  }

  // Will-change utility functions.
  bool hasWillChangeCompositingHint() const;
  bool hasWillChangeOpacityHint() const {
    return willChangeProperties().contains(CSSPropertyOpacity);
  }
  bool hasWillChangeTransformHint() const;

  // Hyphen utility functions.
  Hyphenation* getHyphenation() const;
  const AtomicString& hyphenString() const;

  // Line-height utility functions.
  const Length& specifiedLineHeight() const;
  int computedLineHeight() const;

  // Width/height utility functions.
  const Length& logicalWidth() const {
    return isHorizontalWritingMode() ? width() : height();
  }
  const Length& logicalHeight() const {
    return isHorizontalWritingMode() ? height() : width();
  }
  void setLogicalWidth(const Length& v) {
    if (isHorizontalWritingMode()) {
      SET_VAR(m_box, m_width, v);
    } else {
      SET_VAR(m_box, m_height, v);
    }
  }

  void setLogicalHeight(const Length& v) {
    if (isHorizontalWritingMode()) {
      SET_VAR(m_box, m_height, v);
    } else {
      SET_VAR(m_box, m_width, v);
    }
  }
  const Length& logicalMaxWidth() const {
    return isHorizontalWritingMode() ? maxWidth() : maxHeight();
  }
  const Length& logicalMaxHeight() const {
    return isHorizontalWritingMode() ? maxHeight() : maxWidth();
  }
  const Length& logicalMinWidth() const {
    return isHorizontalWritingMode() ? minWidth() : minHeight();
  }
  const Length& logicalMinHeight() const {
    return isHorizontalWritingMode() ? minHeight() : minWidth();
  }

  // Margin utility functions.
  bool hasMargin() const { return m_surround->margin.nonZero(); }
  bool hasMarginBeforeQuirk() const { return marginBefore().quirk(); }
  bool hasMarginAfterQuirk() const { return marginAfter().quirk(); }
  const Length& marginBefore() const {
    return m_surround->margin.before(getWritingMode());
  }
  const Length& marginAfter() const {
    return m_surround->margin.after(getWritingMode());
  }
  const Length& marginStart() const {
    return m_surround->margin.start(getWritingMode(), direction());
  }
  const Length& marginEnd() const {
    return m_surround->margin.end(getWritingMode(), direction());
  }
  const Length& marginOver() const {
    return m_surround->margin.over(getWritingMode());
  }
  const Length& marginUnder() const {
    return m_surround->margin.under(getWritingMode());
  }
  const Length& marginStartUsing(const ComputedStyle* otherStyle) const {
    return m_surround->margin.start(otherStyle->getWritingMode(),
                                    otherStyle->direction());
  }
  const Length& marginEndUsing(const ComputedStyle* otherStyle) const {
    return m_surround->margin.end(otherStyle->getWritingMode(),
                                  otherStyle->direction());
  }
  const Length& marginBeforeUsing(const ComputedStyle* otherStyle) const {
    return m_surround->margin.before(otherStyle->getWritingMode());
  }
  const Length& marginAfterUsing(const ComputedStyle* otherStyle) const {
    return m_surround->margin.after(otherStyle->getWritingMode());
  }
  void setMarginStart(const Length&);
  void setMarginEnd(const Length&);

  // Padding utility functions.
  const LengthBox& paddingBox() const { return m_surround->padding; }
  const Length& paddingBefore() const {
    return m_surround->padding.before(getWritingMode());
  }
  const Length& paddingAfter() const {
    return m_surround->padding.after(getWritingMode());
  }
  const Length& paddingStart() const {
    return m_surround->padding.start(getWritingMode(), direction());
  }
  const Length& paddingEnd() const {
    return m_surround->padding.end(getWritingMode(), direction());
  }
  const Length& paddingOver() const {
    return m_surround->padding.over(getWritingMode());
  }
  const Length& paddingUnder() const {
    return m_surround->padding.under(getWritingMode());
  }
  bool hasPadding() const { return m_surround->padding.nonZero(); }
  void resetPadding() { SET_VAR(m_surround, padding, LengthBox(Fixed)); }
  void setPaddingBox(const LengthBox& b) { SET_VAR(m_surround, padding, b); }

  // Border utility functions
  LayoutRectOutsets imageOutsets(const NinePieceImage&) const;
  bool hasBorderImageOutsets() const {
    return borderImage().hasImage() && borderImage().outset().nonZero();
  }
  LayoutRectOutsets borderImageOutsets() const {
    return imageOutsets(borderImage());
  }
  bool borderImageSlicesFill() const {
    return m_surround->border.image().fill();
  }

  void setBorderImageSlicesFill(bool);
  const BorderData& border() const { return m_surround->border; }
  const BorderValue& borderLeft() const { return m_surround->border.left(); }
  const BorderValue& borderRight() const { return m_surround->border.right(); }
  const BorderValue& borderTop() const { return m_surround->border.top(); }
  const BorderValue& borderBottom() const {
    return m_surround->border.bottom();
  }
  const BorderValue& borderBefore() const;
  const BorderValue& borderAfter() const;
  const BorderValue& borderStart() const;
  const BorderValue& borderEnd() const;
  int borderAfterWidth() const;
  int borderBeforeWidth() const;
  int borderEndWidth() const;
  int borderStartWidth() const;
  int borderOverWidth() const;
  int borderUnderWidth() const;

  bool hasBorderFill() const { return m_surround->border.hasBorderFill(); }
  bool hasBorder() const { return m_surround->border.hasBorder(); }
  bool hasBorderDecoration() const { return hasBorder() || hasBorderFill(); }
  bool hasBorderRadius() const { return m_surround->border.hasBorderRadius(); }

  void resetBorder() {
    resetBorderImage();
    resetBorderTop();
    resetBorderRight();
    resetBorderBottom();
    resetBorderLeft();
    resetBorderTopLeftRadius();
    resetBorderTopRightRadius();
    resetBorderBottomLeftRadius();
    resetBorderBottomRightRadius();
  }
  void resetBorderTop() { SET_VAR(m_surround, border.m_top, BorderValue()); }
  void resetBorderRight() {
    SET_VAR(m_surround, border.m_right, BorderValue());
  }
  void resetBorderBottom() {
    SET_VAR(m_surround, border.m_bottom, BorderValue());
  }
  void resetBorderLeft() { SET_VAR(m_surround, border.m_left, BorderValue()); }
  void resetBorderImage() {
    SET_VAR(m_surround, border.m_image, NinePieceImage());
  }
  void resetBorderTopLeftRadius() {
    SET_VAR(m_surround, border.m_topLeft, initialBorderRadius());
  }
  void resetBorderTopRightRadius() {
    SET_VAR(m_surround, border.m_topRight, initialBorderRadius());
  }
  void resetBorderBottomLeftRadius() {
    SET_VAR(m_surround, border.m_bottomLeft, initialBorderRadius());
  }
  void resetBorderBottomRightRadius() {
    SET_VAR(m_surround, border.m_bottomRight, initialBorderRadius());
  }

  void setBorderRadius(const LengthSize& s) {
    setBorderTopLeftRadius(s);
    setBorderTopRightRadius(s);
    setBorderBottomLeftRadius(s);
    setBorderBottomRightRadius(s);
  }
  void setBorderRadius(const IntSize& s) {
    setBorderRadius(
        LengthSize(Length(s.width(), Fixed), Length(s.height(), Fixed)));
  }

  FloatRoundedRect getRoundedBorderFor(
      const LayoutRect& borderRect,
      bool includeLogicalLeftEdge = true,
      bool includeLogicalRightEdge = true) const;
  FloatRoundedRect getRoundedInnerBorderFor(
      const LayoutRect& borderRect,
      bool includeLogicalLeftEdge = true,
      bool includeLogicalRightEdge = true) const;
  FloatRoundedRect getRoundedInnerBorderFor(const LayoutRect& borderRect,
                                            const LayoutRectOutsets insets,
                                            bool includeLogicalLeftEdge,
                                            bool includeLogicalRightEdge) const;

  // Float utility functions.
  bool isFloating() const { return floating() != EFloat::None; }

  // Mix-blend-mode utility functions.
  bool hasBlendMode() const { return blendMode() != WebBlendModeNormal; }

  // Motion utility functions.
  bool hasOffset() const {
    return (offsetPosition().x() != Length(Auto)) || offsetPath();
  }

  // Direction utility functions.
  bool isLeftToRightDirection() const { return direction() == LTR; }

  // Perspective utility functions.
  bool hasPerspective() const {
    return m_rareNonInheritedData->m_perspective > 0;
  }

  // Page size utility functions.
  void resetPageSizeType() {
    SET_VAR(m_rareNonInheritedData, m_pageSizeType, PAGE_SIZE_AUTO);
  }

  // Outline utility functions.
  bool hasOutline() const {
    return outlineWidth() > 0 && outlineStyle() > BorderStyleHidden;
  }
  int outlineOutsetExtent() const;
  float getOutlineStrokeWidthForFocusRing() const;

  // Position utility functions.
  bool hasOutOfFlowPosition() const {
    return position() == AbsolutePosition || position() == FixedPosition;
  }
  bool hasInFlowPosition() const {
    return position() == RelativePosition || position() == StickyPosition;
  }
  bool hasViewportConstrainedPosition() const {
    return position() == FixedPosition || position() == StickyPosition;
  }

  // Clip utility functions.
  const Length& clipLeft() const { return m_visual->clip.left(); }
  const Length& clipRight() const { return m_visual->clip.right(); }
  const Length& clipTop() const { return m_visual->clip.top(); }
  const Length& clipBottom() const { return m_visual->clip.bottom(); }

  // Offset utility functions.
  // Accessors for positioned object edges that take into account writing mode.
  const Length& logicalLeft() const {
    return m_surround->offset.logicalLeft(getWritingMode());
  }
  const Length& logicalRight() const {
    return m_surround->offset.logicalRight(getWritingMode());
  }
  const Length& logicalTop() const {
    return m_surround->offset.before(getWritingMode());
  }
  const Length& logicalBottom() const {
    return m_surround->offset.after(getWritingMode());
  }

  // Whether or not a positioned element requires normal flow x/y to be computed
  // to determine its position.
  bool hasAutoLeftAndRight() const {
    return left().isAuto() && right().isAuto();
  }
  bool hasAutoTopAndBottom() const {
    return top().isAuto() && bottom().isAuto();
  }
  bool hasStaticInlinePosition(bool horizontal) const {
    return horizontal ? hasAutoLeftAndRight() : hasAutoTopAndBottom();
  }
  bool hasStaticBlockPosition(bool horizontal) const {
    return horizontal ? hasAutoTopAndBottom() : hasAutoLeftAndRight();
  }

  // Content utility functions.
  bool contentDataEquivalent(const ComputedStyle* otherStyle) const {
    return const_cast<ComputedStyle*>(this)
        ->m_rareNonInheritedData->contentDataEquivalent(
            *const_cast<ComputedStyle*>(otherStyle)->m_rareNonInheritedData);
  }

  // Contain utility functions.
  bool containsPaint() const {
    return m_rareNonInheritedData->m_contain & ContainsPaint;
  }
  bool containsStyle() const {
    return m_rareNonInheritedData->m_contain & ContainsStyle;
  }
  bool containsLayout() const {
    return m_rareNonInheritedData->m_contain & ContainsLayout;
  }
  bool containsSize() const {
    return m_rareNonInheritedData->m_contain & ContainsSize;
  }

  // Display utility functions.
  bool isDisplayReplacedType() const {
    return isDisplayReplacedType(display());
  }
  bool isDisplayInlineType() const { return isDisplayInlineType(display()); }
  bool isOriginalDisplayInlineType() const {
    return isDisplayInlineType(originalDisplay());
  }
  bool isDisplayFlexibleOrGridBox() const {
    return isDisplayFlexibleBox(display()) || isDisplayGridBox(display());
  }
  bool isDisplayFlexibleBox() const { return isDisplayFlexibleBox(display()); }

  // Isolation utility functions.
  bool hasIsolation() const { return isolation() != IsolationAuto; }

  // Content utility functions.
  bool hasContent() const { return contentData(); }

  // Cursor utility functions.
  CursorList* cursors() const { return m_rareInheritedData->cursorData.get(); }
  void addCursor(StyleImage*,
                 bool hotSpotSpecified,
                 const IntPoint& hotSpot = IntPoint());
  void setCursorList(CursorList*);
  void clearCursorList();

  // Text decoration utility functions.
  void applyTextDecorations();
  void clearAppliedTextDecorations();
  void restoreParentTextDecorations(const ComputedStyle& parentStyle);
  const Vector<AppliedTextDecoration>& appliedTextDecorations() const;
  TextDecoration textDecorationsInEffect() const;

  // Overflow utility functions.

  EOverflow overflowInlineDirection() const {
    return isHorizontalWritingMode() ? overflowX() : overflowY();
  }
  EOverflow overflowBlockDirection() const {
    return isHorizontalWritingMode() ? overflowY() : overflowX();
  }

  // It's sufficient to just check one direction, since it's illegal to have
  // visible on only one overflow value.
  bool isOverflowVisible() const {
    DCHECK(overflowX() != OverflowVisible || overflowX() == overflowY());
    return overflowX() == OverflowVisible;
  }
  bool isOverflowPaged() const {
    return overflowY() == OverflowPagedX || overflowY() == OverflowPagedY;
  }

  // Visibility utility functions.
  bool visibleToHitTesting() const {
    return visibility() == EVisibility::Visible && pointerEvents() != PE_NONE;
  }

  // Animation utility functions.
  bool shouldCompositeForCurrentAnimations() const {
    return hasCurrentOpacityAnimation() || hasCurrentTransformAnimation() ||
           hasCurrentFilterAnimation() || hasCurrentBackdropFilterAnimation();
  }
  bool isRunningAnimationOnCompositor() const {
    return isRunningOpacityAnimationOnCompositor() ||
           isRunningTransformAnimationOnCompositor() ||
           isRunningFilterAnimationOnCompositor() ||
           isRunningBackdropFilterAnimationOnCompositor();
  }

  // Opacity utility functions.
  bool hasOpacity() const { return opacity() < 1.0f; }

  // Table layout utility functions.
  bool isFixedTableLayout() const {
    return tableLayout() == TableLayoutFixed && !logicalWidth().isAuto();
  }

  // Filter/transform utility functions.
  bool has3DTransform() const {
    return m_rareNonInheritedData->m_transform->has3DTransform();
  }
  bool hasTransform() const {
    return hasTransformOperations() || hasOffset() ||
           hasCurrentTransformAnimation() || translate() || rotate() || scale();
  }
  bool hasTransformOperations() const {
    return !m_rareNonInheritedData->m_transform->m_operations.operations()
                .isEmpty();
  }
  ETransformStyle3D usedTransformStyle3D() const {
    return hasGroupingProperty() ? TransformStyle3DFlat : transformStyle3D();
  }
  bool transformDataEquivalent(const ComputedStyle& otherStyle) const {
    return m_rareNonInheritedData->m_transform ==
           otherStyle.m_rareNonInheritedData->m_transform;
  }
  bool preserves3D() const {
    return usedTransformStyle3D() != TransformStyle3DFlat;
  }
  enum ApplyTransformOrigin { IncludeTransformOrigin, ExcludeTransformOrigin };
  enum ApplyMotionPath { IncludeMotionPath, ExcludeMotionPath };
  enum ApplyIndependentTransformProperties {
    IncludeIndependentTransformProperties,
    ExcludeIndependentTransformProperties
  };
  void applyTransform(TransformationMatrix&,
                      const LayoutSize& borderBoxSize,
                      ApplyTransformOrigin,
                      ApplyMotionPath,
                      ApplyIndependentTransformProperties) const;
  void applyTransform(TransformationMatrix&,
                      const FloatRect& boundingBox,
                      ApplyTransformOrigin,
                      ApplyMotionPath,
                      ApplyIndependentTransformProperties) const;

  // Returns |true| if any property that renders using filter operations is
  // used (including, but not limited to, 'filter' and 'box-reflect').
  bool hasFilterInducingProperty() const {
    return hasFilter() || hasBoxReflect();
  }

  // Returns |true| if opacity should be considered to have non-initial value
  // for the purpose of creating stacking contexts.
  bool hasNonInitialOpacity() const {
    return hasOpacity() || hasWillChangeOpacityHint() ||
           hasCurrentOpacityAnimation();
  }

  // Returns whether this style contains any grouping property as defined by
  // [css-transforms].  The main purpose of this is to adjust the used value of
  // transform-style property.
  // Note: We currently don't include every grouping property on the spec to
  // maintain backward compatibility.  [css-transforms]
  // https://drafts.csswg.org/css-transforms/#grouping-property-values
  bool hasGroupingProperty() const {
    return !isOverflowVisible() || hasFilterInducingProperty() ||
           hasNonInitialOpacity();
  }

  // Return true if any transform related property (currently
  // transform/motionPath, transformStyle3D, perspective, or
  // will-change:transform) indicates that we are transforming.
  // will-change:transform should result in the same rendering behavior as
  // having a transform, including the creation of a containing block for fixed
  // position descendants.
  bool hasTransformRelatedProperty() const {
    return hasTransform() || preserves3D() || hasPerspective() ||
           hasWillChangeTransformHint();
  }

  // Paint utility functions.
  void addPaintImage(StyleImage*);

  // FIXME: reflections should belong to this helper function but they are
  // currently handled through their self-painting layers. So the layout code
  // doesn't account for them.
  bool hasVisualOverflowingEffect() const {
    return boxShadow() || hasBorderImageOutsets() || hasOutline();
  }

  // Stacking contexts and positioned elements[1] are stacked (sorted in
  // negZOrderList
  // and posZOrderList) in their enclosing stacking contexts.
  //
  // [1] According to CSS2.1, Appendix E.2.8
  // (https://www.w3.org/TR/CSS21/zindex.html),
  // positioned elements with 'z-index: auto' are "treated as if it created a
  // new stacking context" and z-ordered together with other elements with
  // 'z-index: 0'.  The difference of them from normal stacking contexts is that
  // they don't determine the stacking of the elements underneath them.  (Note:
  // There are also other elements treated as stacking context during painting,
  // but not managed in stacks. See ObjectPainter::paintAllPhasesAtomically().)
  void updateIsStackingContext(bool isDocumentElement, bool isInTopLayer);
  bool isStacked() const {
    return isStackingContext() || position() != StaticPosition;
  }

  // Pseudo-styles
  bool hasAnyPublicPseudoStyles() const;
  bool hasPseudoStyle(PseudoId) const;
  void setHasPseudoStyle(PseudoId);
  bool hasUniquePseudoStyle() const;
  bool hasPseudoElementStyle() const;

  // Note: canContainAbsolutePositionObjects should return true if
  // canContainFixedPositionObjects.  We currently never use this value
  // directly, always OR'ing it with canContainFixedPositionObjects.
  bool canContainAbsolutePositionObjects() const {
    return position() != StaticPosition;
  }
  bool canContainFixedPositionObjects() const {
    return hasTransformRelatedProperty() || containsPaint();
  }

  // Whitespace utility functions.
  static bool autoWrap(EWhiteSpace ws) {
    // Nowrap and pre don't automatically wrap.
    return ws != NOWRAP && ws != PRE;
  }

  bool autoWrap() const { return autoWrap(whiteSpace()); }

  static bool preserveNewline(EWhiteSpace ws) {
    // Normal and nowrap do not preserve newlines.
    return ws != NORMAL && ws != NOWRAP;
  }

  bool preserveNewline() const { return preserveNewline(whiteSpace()); }

  static bool collapseWhiteSpace(EWhiteSpace ws) {
    // Pre and prewrap do not collapse whitespace.
    return ws != PRE && ws != PRE_WRAP;
  }

  bool collapseWhiteSpace() const { return collapseWhiteSpace(whiteSpace()); }

  bool isCollapsibleWhiteSpace(UChar c) const {
    switch (c) {
      case ' ':
      case '\t':
        return collapseWhiteSpace();
      case '\n':
        return !preserveNewline();
    }
    return false;
  }
  bool breakOnlyAfterWhiteSpace() const {
    return whiteSpace() == PRE_WRAP ||
           getLineBreak() == LineBreakAfterWhiteSpace;
  }

  bool breakWords() const {
    return (wordBreak() == BreakWordBreak ||
            overflowWrap() == BreakOverflowWrap) &&
           whiteSpace() != PRE && whiteSpace() != NOWRAP;
  }

  // Text direction utility functions.
  bool shouldPlaceBlockDirectionScrollbarOnLogicalLeft() const {
    return !isLeftToRightDirection() && isHorizontalWritingMode();
  }
  bool hasInlinePaginationAxis() const {
    // If the pagination axis is parallel with the writing mode inline axis,
    // columns may be laid out along the inline axis, just like for regular
    // multicol. Otherwise, we need to lay out along the block axis.
    if (isOverflowPaged())
      return (overflowY() == OverflowPagedX) == isHorizontalWritingMode();
    return false;
  }

  // Border utility functions.
  bool borderObscuresBackground() const;
  void getBorderEdgeInfo(BorderEdge edges[],
                         bool includeLogicalLeftEdge = true,
                         bool includeLogicalRightEdge = true) const;

  bool hasBoxDecorations() const {
    return hasBorderDecoration() || hasBorderRadius() || hasOutline() ||
           hasAppearance() || boxShadow() || hasFilterInducingProperty() ||
           hasBackdropFilter() || resize() != RESIZE_NONE;
  }

  // "Box decoration background" includes all box decorations and backgrounds
  // that are painted as the background of the object. It includes borders,
  // box-shadows, background-color and background-image, etc.
  bool hasBoxDecorationBackground() const {
    return hasBackground() || hasBorderDecoration() || hasAppearance() ||
           boxShadow();
  }

  // Background utility functions.
  FillLayer& accessBackgroundLayers() {
    return m_background.access()->m_background;
  }
  const FillLayer& backgroundLayers() const {
    return m_background->background();
  }
  void adjustBackgroundLayers() {
    if (backgroundLayers().next()) {
      accessBackgroundLayers().cullEmptyLayers();
      accessBackgroundLayers().fillUnsetProperties();
    }
  }
  bool hasBackgroundRelatedColorReferencingCurrentColor() const {
    if (backgroundColor().isCurrentColor() ||
        visitedLinkBackgroundColor().isCurrentColor())
      return true;
    if (!boxShadow())
      return false;
    return shadowListHasCurrentColor(boxShadow());
  }
  bool hasBackground() const {
    Color color = visitedDependentColor(CSSPropertyBackgroundColor);
    if (color.alpha())
      return true;
    return hasBackgroundImage();
  }

  // Color utility functions.
  // TODO(sashab): Rename this to just getColor(), and add a comment explaining
  // how it works.
  Color visitedDependentColor(int colorProperty) const;

  // -webkit-appearance utility functions.
  bool hasAppearance() const { return appearance() != NoControlPart; }

  // Other utility functions.
  bool isStyleAvailable() const;
  bool isSharable() const;

 private:
  void setVisitedLinkColor(const Color&);
  void setVisitedLinkBackgroundColor(const StyleColor& v) {
    SET_VAR(m_rareNonInheritedData, m_visitedLinkBackgroundColor, v);
  }
  void setVisitedLinkBorderLeftColor(const StyleColor& v) {
    SET_VAR(m_rareNonInheritedData, m_visitedLinkBorderLeftColor, v);
  }
  void setVisitedLinkBorderRightColor(const StyleColor& v) {
    SET_VAR(m_rareNonInheritedData, m_visitedLinkBorderRightColor, v);
  }
  void setVisitedLinkBorderBottomColor(const StyleColor& v) {
    SET_VAR(m_rareNonInheritedData, m_visitedLinkBorderBottomColor, v);
  }
  void setVisitedLinkBorderTopColor(const StyleColor& v) {
    SET_VAR(m_rareNonInheritedData, m_visitedLinkBorderTopColor, v);
  }
  void setVisitedLinkOutlineColor(const StyleColor& v) {
    SET_VAR(m_rareNonInheritedData, m_visitedLinkOutlineColor, v);
  }
  void setVisitedLinkColumnRuleColor(const StyleColor& v) {
    SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol,
                   m_visitedLinkColumnRuleColor, v);
  }
  void setVisitedLinkTextDecorationColor(const StyleColor& v) {
    SET_VAR(m_rareNonInheritedData, m_visitedLinkTextDecorationColor, v);
  }
  void setVisitedLinkTextEmphasisColor(const StyleColor& v) {
    SET_VAR_WITH_SETTER(m_rareInheritedData, visitedLinkTextEmphasisColor,
                        setVisitedLinkTextEmphasisColor, v);
  }
  void setVisitedLinkTextFillColor(const StyleColor& v) {
    SET_VAR_WITH_SETTER(m_rareInheritedData, visitedLinkTextFillColor,
                        setVisitedLinkTextFillColor, v);
  }
  void setVisitedLinkTextStrokeColor(const StyleColor& v) {
    SET_VAR_WITH_SETTER(m_rareInheritedData, visitedLinkTextStrokeColor,
                        setVisitedLinkTextStrokeColor, v);
  }

  void inheritUnicodeBidiFrom(const ComputedStyle& parent) {
    m_nonInheritedData.m_unicodeBidi = parent.m_nonInheritedData.m_unicodeBidi;
  }

  static bool isDisplayFlexibleBox(EDisplay display) {
    return display == EDisplay::Flex || display == EDisplay::InlineFlex;
  }

  static bool isDisplayGridBox(EDisplay display) {
    return display == EDisplay::Grid || display == EDisplay::InlineGrid;
  }

  static bool isDisplayReplacedType(EDisplay display) {
    return display == EDisplay::InlineBlock || display == EDisplay::InlineBox ||
           display == EDisplay::InlineFlex ||
           display == EDisplay::InlineTable || display == EDisplay::InlineGrid;
  }

  static bool isDisplayInlineType(EDisplay display) {
    return display == EDisplay::Inline || isDisplayReplacedType(display);
  }

  static bool isDisplayTableType(EDisplay display) {
    return display == EDisplay::Table || display == EDisplay::InlineTable ||
           display == EDisplay::TableRowGroup ||
           display == EDisplay::TableHeaderGroup ||
           display == EDisplay::TableFooterGroup ||
           display == EDisplay::TableRow ||
           display == EDisplay::TableColumnGroup ||
           display == EDisplay::TableColumn || display == EDisplay::TableCell ||
           display == EDisplay::TableCaption;
  }

  // Color accessors are all private to make sure callers use
  // visitedDependentColor instead to access them.
  StyleColor borderLeftColor() const {
    return m_surround->border.left().color();
  }
  StyleColor borderRightColor() const {
    return m_surround->border.right().color();
  }
  StyleColor borderTopColor() const { return m_surround->border.top().color(); }
  StyleColor borderBottomColor() const {
    return m_surround->border.bottom().color();
  }
  StyleColor backgroundColor() const { return m_background->color(); }
  Color color() const;
  StyleColor columnRuleColor() const {
    return m_rareNonInheritedData->m_multiCol->m_rule.color();
  }
  StyleColor outlineColor() const {
    return m_rareNonInheritedData->m_outline.color();
  }
  StyleColor textEmphasisColor() const {
    return m_rareInheritedData->textEmphasisColor();
  }
  StyleColor textFillColor() const {
    return m_rareInheritedData->textFillColor();
  }
  StyleColor textStrokeColor() const {
    return m_rareInheritedData->textStrokeColor();
  }
  Color visitedLinkColor() const;
  StyleColor visitedLinkBackgroundColor() const {
    return m_rareNonInheritedData->m_visitedLinkBackgroundColor;
  }
  StyleColor visitedLinkBorderLeftColor() const {
    return m_rareNonInheritedData->m_visitedLinkBorderLeftColor;
  }
  StyleColor visitedLinkBorderRightColor() const {
    return m_rareNonInheritedData->m_visitedLinkBorderRightColor;
  }
  StyleColor visitedLinkBorderBottomColor() const {
    return m_rareNonInheritedData->m_visitedLinkBorderBottomColor;
  }
  StyleColor visitedLinkBorderTopColor() const {
    return m_rareNonInheritedData->m_visitedLinkBorderTopColor;
  }
  StyleColor visitedLinkOutlineColor() const {
    return m_rareNonInheritedData->m_visitedLinkOutlineColor;
  }
  StyleColor visitedLinkColumnRuleColor() const {
    return m_rareNonInheritedData->m_multiCol->m_visitedLinkColumnRuleColor;
  }
  StyleColor textDecorationColor() const {
    return m_rareNonInheritedData->m_textDecorationColor;
  }
  StyleColor visitedLinkTextDecorationColor() const {
    return m_rareNonInheritedData->m_visitedLinkTextDecorationColor;
  }
  StyleColor visitedLinkTextEmphasisColor() const {
    return m_rareInheritedData->visitedLinkTextEmphasisColor();
  }
  StyleColor visitedLinkTextFillColor() const {
    return m_rareInheritedData->visitedLinkTextFillColor();
  }
  StyleColor visitedLinkTextStrokeColor() const {
    return m_rareInheritedData->visitedLinkTextStrokeColor();
  }

  StyleColor decorationColorIncludingFallback(bool visitedLink) const;
  Color colorIncludingFallback(int colorProperty, bool visitedLink) const;

  Color stopColor() const { return svgStyle().stopColor(); }
  Color floodColor() const { return svgStyle().floodColor(); }
  Color lightingColor() const { return svgStyle().lightingColor(); }

  void addAppliedTextDecoration(const AppliedTextDecoration&);
  void applyMotionPathTransform(float originX,
                                float originY,
                                const FloatRect& boundingBox,
                                TransformationMatrix&) const;

  bool scrollAnchorDisablingPropertyChanged(const ComputedStyle& other,
                                            StyleDifference&) const;
  bool diffNeedsFullLayoutAndPaintInvalidation(
      const ComputedStyle& other) const;
  bool diffNeedsFullLayout(const ComputedStyle& other) const;
  bool diffNeedsPaintInvalidationSubtree(const ComputedStyle& other) const;
  bool diffNeedsPaintInvalidationObject(const ComputedStyle& other) const;
  bool diffNeedsPaintInvalidationObjectForPaintImage(
      const StyleImage*,
      const ComputedStyle& other) const;
  void updatePropertySpecificDifferences(const ComputedStyle& other,
                                         StyleDifference&) const;

  bool requireTransformOrigin(ApplyTransformOrigin applyOrigin,
                              ApplyMotionPath) const;
  static bool shadowListHasCurrentColor(const ShadowList*);

  StyleInheritedVariables& mutableInheritedVariables();
  StyleNonInheritedVariables& mutableNonInheritedVariables();
};

// FIXME: Reduce/remove the dependency on zoom adjusted int values.
// The float or LayoutUnit versions of layout values should be used.
int adjustForAbsoluteZoom(int value, float zoomFactor);

inline int adjustForAbsoluteZoom(int value, const ComputedStyle* style) {
  float zoomFactor = style->effectiveZoom();
  if (zoomFactor == 1)
    return value;
  return adjustForAbsoluteZoom(value, zoomFactor);
}

inline float adjustFloatForAbsoluteZoom(float value,
                                        const ComputedStyle& style) {
  return value / style.effectiveZoom();
}

inline double adjustDoubleForAbsoluteZoom(double value,
                                          const ComputedStyle& style) {
  return value / style.effectiveZoom();
}

inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit value,
                                                  const ComputedStyle& style) {
  return LayoutUnit(value / style.effectiveZoom());
}

inline float adjustScrollForAbsoluteZoom(float scrollOffset, float zoomFactor) {
  return scrollOffset / zoomFactor;
}

inline float adjustScrollForAbsoluteZoom(float scrollOffset,
                                         const ComputedStyle& style) {
  return adjustScrollForAbsoluteZoom(scrollOffset, style.effectiveZoom());
}

inline bool ComputedStyle::setZoom(float f) {
  if (compareEqual(m_visual->m_zoom, f))
    return false;
  m_visual.access()->m_zoom = f;
  setEffectiveZoom(effectiveZoom() * zoom());
  return true;
}

inline bool ComputedStyle::setEffectiveZoom(float f) {
  // Clamp the effective zoom value to a smaller (but hopeful still large
  // enough) range, to avoid overflow in derived computations.
  float clampedEffectiveZoom = clampTo<float>(f, 1e-6, 1e6);
  if (compareEqual(m_rareInheritedData->m_effectiveZoom, clampedEffectiveZoom))
    return false;
  m_rareInheritedData.access()->m_effectiveZoom = clampedEffectiveZoom;
  return true;
}

inline bool ComputedStyle::isSharable() const {
  if (unique())
    return false;
  if (hasUniquePseudoStyle())
    return false;
  return true;
}

inline bool ComputedStyle::setTextOrientation(TextOrientation textOrientation) {
  if (compareEqual(m_rareInheritedData->m_textOrientation, textOrientation))
    return false;

  m_rareInheritedData.access()->m_textOrientation = textOrientation;
  return true;
}

inline bool ComputedStyle::hasAnyPublicPseudoStyles() const {
  return PublicPseudoIdMask & m_nonInheritedData.m_pseudoBits;
}

inline bool ComputedStyle::hasPseudoStyle(PseudoId pseudo) const {
  ASSERT(pseudo > PseudoIdNone);
  ASSERT(pseudo < FirstInternalPseudoId);
  return (1 << (pseudo - 1)) & m_nonInheritedData.m_pseudoBits;
}

inline void ComputedStyle::setHasPseudoStyle(PseudoId pseudo) {
  ASSERT(pseudo > PseudoIdNone);
  ASSERT(pseudo < FirstInternalPseudoId);
  m_nonInheritedData.m_pseudoBits |= 1 << (pseudo - 1);
}

inline bool ComputedStyle::hasPseudoElementStyle() const {
  return m_nonInheritedData.m_pseudoBits & ElementPseudoIdMask;
}

}  // namespace blink

#endif  // ComputedStyle_h

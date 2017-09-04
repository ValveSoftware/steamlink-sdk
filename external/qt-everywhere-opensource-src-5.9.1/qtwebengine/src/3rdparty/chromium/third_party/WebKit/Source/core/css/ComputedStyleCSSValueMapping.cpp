/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2011 Sencha, Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "core/css/ComputedStyleCSSValueMapping.h"

#include "core/StylePropertyShorthand.h"
#include "core/animation/css/CSSAnimationData.h"
#include "core/animation/css/CSSTransitionData.h"
#include "core/css/BasicShapeFunctions.h"
#include "core/css/CSSBasicShapeValues.h"
#include "core/css/CSSBorderImage.h"
#include "core/css/CSSBorderImageSliceValue.h"
#include "core/css/CSSColorValue.h"
#include "core/css/CSSCounterValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSFontFamilyValue.h"
#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSGridTemplateAreasValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/CSSPathValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSStringValue.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/CSSURIValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"
#include "core/css/PropertyRegistry.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutGrid.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ContentData.h"
#include "core/style/CursorData.h"
#include "core/style/QuotesData.h"
#include "core/style/ShadowList.h"
#include "core/style/StyleInheritedVariables.h"
#include "core/style/StyleNonInheritedVariables.h"
#include "platform/LengthFunctions.h"

namespace blink {

inline static bool isFlexOrGrid(const ComputedStyle* style) {
  return style && style->isDisplayFlexibleOrGridBox();
}

inline static CSSPrimitiveValue* zoomAdjustedPixelValue(
    double value,
    const ComputedStyle& style) {
  return CSSPrimitiveValue::create(adjustFloatForAbsoluteZoom(value, style),
                                   CSSPrimitiveValue::UnitType::Pixels);
}

inline static CSSValue* zoomAdjustedPixelValueOrAuto(
    const Length& length,
    const ComputedStyle& style) {
  if (length.isAuto())
    return CSSIdentifierValue::create(CSSValueAuto);
  return zoomAdjustedPixelValue(length.value(), style);
}

static CSSValue* zoomAdjustedPixelValueForLength(const Length& length,
                                                 const ComputedStyle& style) {
  if (length.isFixed())
    return zoomAdjustedPixelValue(length.value(), style);
  return CSSValue::create(length, style.effectiveZoom());
}

static CSSValue* pixelValueForUnzoomedLength(
    const UnzoomedLength& unzoomedLength,
    const ComputedStyle& style) {
  const Length& length = unzoomedLength.length();
  if (length.isFixed())
    return CSSPrimitiveValue::create(length.value(),
                                     CSSPrimitiveValue::UnitType::Pixels);
  return CSSValue::create(length, style.effectiveZoom());
}

static CSSValueList* createPositionListForLayer(CSSPropertyID propertyID,
                                                const FillLayer& layer,
                                                const ComputedStyle& style) {
  CSSValueList* positionList = CSSValueList::createSpaceSeparated();
  if (layer.isBackgroundXOriginSet()) {
    DCHECK(propertyID == CSSPropertyBackgroundPosition ||
           propertyID == CSSPropertyWebkitMaskPosition);
    positionList->append(
        *CSSIdentifierValue::create(layer.backgroundXOrigin()));
  }
  positionList->append(
      *zoomAdjustedPixelValueForLength(layer.xPosition(), style));
  if (layer.isBackgroundYOriginSet()) {
    ASSERT(propertyID == CSSPropertyBackgroundPosition ||
           propertyID == CSSPropertyWebkitMaskPosition);
    positionList->append(
        *CSSIdentifierValue::create(layer.backgroundYOrigin()));
  }
  positionList->append(
      *zoomAdjustedPixelValueForLength(layer.yPosition(), style));
  return positionList;
}

CSSValue* ComputedStyleCSSValueMapping::currentColorOrValidColor(
    const ComputedStyle& style,
    const StyleColor& color) {
  // This function does NOT look at visited information, so that computed style
  // doesn't expose that.
  return CSSColorValue::create(color.resolve(style.color()).rgb());
}

static CSSValue* valueForFillSize(const FillSize& fillSize,
                                  const ComputedStyle& style) {
  if (fillSize.type == Contain)
    return CSSIdentifierValue::create(CSSValueContain);

  if (fillSize.type == Cover)
    return CSSIdentifierValue::create(CSSValueCover);

  if (fillSize.size.height().isAuto())
    return zoomAdjustedPixelValueForLength(fillSize.size.width(), style);

  CSSValueList* list = CSSValueList::createSpaceSeparated();
  list->append(*zoomAdjustedPixelValueForLength(fillSize.size.width(), style));
  list->append(*zoomAdjustedPixelValueForLength(fillSize.size.height(), style));
  return list;
}

static CSSValue* valueForFillRepeat(EFillRepeat xRepeat, EFillRepeat yRepeat) {
  // For backwards compatibility, if both values are equal, just return one of
  // them. And if the two values are equivalent to repeat-x or repeat-y, just
  // return the shorthand.
  if (xRepeat == yRepeat)
    return CSSIdentifierValue::create(xRepeat);
  if (xRepeat == RepeatFill && yRepeat == NoRepeatFill)
    return CSSIdentifierValue::create(CSSValueRepeatX);
  if (xRepeat == NoRepeatFill && yRepeat == RepeatFill)
    return CSSIdentifierValue::create(CSSValueRepeatY);

  CSSValueList* list = CSSValueList::createSpaceSeparated();
  list->append(*CSSIdentifierValue::create(xRepeat));
  list->append(*CSSIdentifierValue::create(yRepeat));
  return list;
}

static CSSValue* valueForFillSourceType(EMaskSourceType type) {
  switch (type) {
    case MaskAlpha:
      return CSSIdentifierValue::create(CSSValueAlpha);
    case MaskLuminance:
      return CSSIdentifierValue::create(CSSValueLuminance);
  }

  ASSERT_NOT_REACHED();

  return nullptr;
}

static CSSValue* valueForPositionOffset(const ComputedStyle& style,
                                        CSSPropertyID propertyID,
                                        const LayoutObject* layoutObject) {
  Length offset, opposite;
  switch (propertyID) {
    case CSSPropertyLeft:
      offset = style.left();
      opposite = style.right();
      break;
    case CSSPropertyRight:
      offset = style.right();
      opposite = style.left();
      break;
    case CSSPropertyTop:
      offset = style.top();
      opposite = style.bottom();
      break;
    case CSSPropertyBottom:
      offset = style.bottom();
      opposite = style.top();
      break;
    default:
      return nullptr;
  }

  if (offset.isPercentOrCalc() && layoutObject && layoutObject->isBox() &&
      layoutObject->isPositioned()) {
    LayoutUnit containingBlockSize =
        (propertyID == CSSPropertyLeft || propertyID == CSSPropertyRight)
            ? toLayoutBox(layoutObject)->containingBlockLogicalWidthForContent()
            : toLayoutBox(layoutObject)
                  ->containingBlockLogicalHeightForGetComputedStyle();
    return zoomAdjustedPixelValue(valueForLength(offset, containingBlockSize),
                                  style);
  }

  if (offset.isAuto() && layoutObject) {
    // If the property applies to a positioned element and the resolved value of
    // the display property is not none, the resolved value is the used value.
    if (layoutObject->isInFlowPositioned()) {
      // If e.g. left is auto and right is not auto, then left's computed value
      // is negative right. So we get the opposite length unit and see if it is
      // auto.
      if (opposite.isAuto())
        return CSSPrimitiveValue::create(0,
                                         CSSPrimitiveValue::UnitType::Pixels);

      if (opposite.isPercentOrCalc()) {
        if (layoutObject->isBox()) {
          LayoutUnit containingBlockSize =
              (propertyID == CSSPropertyLeft || propertyID == CSSPropertyRight)
                  ? toLayoutBox(layoutObject)
                        ->containingBlockLogicalWidthForContent()
                  : toLayoutBox(layoutObject)
                        ->containingBlockLogicalHeightForGetComputedStyle();
          return zoomAdjustedPixelValue(
              -floatValueForLength(opposite, containingBlockSize), style);
        }
        // FIXME:  fall back to auto for position:relative, display:inline
        return CSSIdentifierValue::create(CSSValueAuto);
      }

      // Length doesn't provide operator -, so multiply by -1.
      opposite *= -1.f;
      return zoomAdjustedPixelValueForLength(opposite, style);
    }

    if (layoutObject->isOutOfFlowPositioned() && layoutObject->isBox()) {
      // For fixed and absolute positioned elements, the top, left, bottom, and
      // right are defined relative to the corresponding sides of the containing
      // block.
      LayoutBlock* container = layoutObject->containingBlock();
      const LayoutBox* layoutBox = toLayoutBox(layoutObject);

      // clientOffset is the distance from this object's border edge to the
      // container's padding edge. Thus it includes margins which we subtract
      // below.
      const LayoutSize clientOffset =
          layoutBox->locationOffset() -
          LayoutSize(container->clientLeft(), container->clientTop());
      LayoutUnit position;

      switch (propertyID) {
        case CSSPropertyLeft:
          position = clientOffset.width() - layoutBox->marginLeft();
          break;
        case CSSPropertyTop:
          position = clientOffset.height() - layoutBox->marginTop();
          break;
        case CSSPropertyRight:
          position = container->clientWidth() - layoutBox->marginRight() -
                     (layoutBox->offsetWidth() + clientOffset.width());
          break;
        case CSSPropertyBottom:
          position = container->clientHeight() - layoutBox->marginBottom() -
                     (layoutBox->offsetHeight() + clientOffset.height());
          break;
        default:
          ASSERT_NOT_REACHED();
      }
      return zoomAdjustedPixelValue(position, style);
    }
  }

  if (offset.isAuto())
    return CSSIdentifierValue::create(CSSValueAuto);

  return zoomAdjustedPixelValueForLength(offset, style);
}

static CSSBorderImageSliceValue* valueForNinePieceImageSlice(
    const NinePieceImage& image) {
  // Create the slices.
  CSSPrimitiveValue* top = nullptr;
  CSSPrimitiveValue* right = nullptr;
  CSSPrimitiveValue* bottom = nullptr;
  CSSPrimitiveValue* left = nullptr;

  // TODO(alancutter): Make this code aware of calc lengths.
  if (image.imageSlices().top().isPercentOrCalc())
    top = CSSPrimitiveValue::create(image.imageSlices().top().value(),
                                    CSSPrimitiveValue::UnitType::Percentage);
  else
    top = CSSPrimitiveValue::create(image.imageSlices().top().value(),
                                    CSSPrimitiveValue::UnitType::Number);

  if (image.imageSlices().right() == image.imageSlices().top() &&
      image.imageSlices().bottom() == image.imageSlices().top() &&
      image.imageSlices().left() == image.imageSlices().top()) {
    right = top;
    bottom = top;
    left = top;
  } else {
    if (image.imageSlices().right().isPercentOrCalc())
      right =
          CSSPrimitiveValue::create(image.imageSlices().right().value(),
                                    CSSPrimitiveValue::UnitType::Percentage);
    else
      right = CSSPrimitiveValue::create(image.imageSlices().right().value(),
                                        CSSPrimitiveValue::UnitType::Number);

    if (image.imageSlices().bottom() == image.imageSlices().top() &&
        image.imageSlices().right() == image.imageSlices().left()) {
      bottom = top;
      left = right;
    } else {
      if (image.imageSlices().bottom().isPercentOrCalc())
        bottom =
            CSSPrimitiveValue::create(image.imageSlices().bottom().value(),
                                      CSSPrimitiveValue::UnitType::Percentage);
      else
        bottom = CSSPrimitiveValue::create(image.imageSlices().bottom().value(),
                                           CSSPrimitiveValue::UnitType::Number);

      if (image.imageSlices().left() == image.imageSlices().right()) {
        left = right;
      } else {
        if (image.imageSlices().left().isPercentOrCalc())
          left = CSSPrimitiveValue::create(
              image.imageSlices().left().value(),
              CSSPrimitiveValue::UnitType::Percentage);
        else
          left = CSSPrimitiveValue::create(image.imageSlices().left().value(),
                                           CSSPrimitiveValue::UnitType::Number);
      }
    }
  }

  return CSSBorderImageSliceValue::create(
      CSSQuadValue::create(top, right, bottom, left,
                           CSSQuadValue::SerializeAsQuad),
      image.fill());
}

static CSSValue* valueForBorderImageLength(
    const BorderImageLength& borderImageLength,
    const ComputedStyle& style) {
  if (borderImageLength.isNumber())
    return CSSPrimitiveValue::create(borderImageLength.number(),
                                     CSSPrimitiveValue::UnitType::Number);
  return CSSValue::create(borderImageLength.length(), style.effectiveZoom());
}

static CSSQuadValue* valueForNinePieceImageQuad(const BorderImageLengthBox& box,
                                                const ComputedStyle& style) {
  // Create the slices.
  CSSValue* top = nullptr;
  CSSValue* right = nullptr;
  CSSValue* bottom = nullptr;
  CSSValue* left = nullptr;

  top = valueForBorderImageLength(box.top(), style);

  if (box.right() == box.top() && box.bottom() == box.top() &&
      box.left() == box.top()) {
    right = top;
    bottom = top;
    left = top;
  } else {
    right = valueForBorderImageLength(box.right(), style);

    if (box.bottom() == box.top() && box.right() == box.left()) {
      bottom = top;
      left = right;
    } else {
      bottom = valueForBorderImageLength(box.bottom(), style);

      if (box.left() == box.right())
        left = right;
      else
        left = valueForBorderImageLength(box.left(), style);
    }
  }
  return CSSQuadValue::create(top, right, bottom, left,
                              CSSQuadValue::SerializeAsQuad);
}

static CSSValueID valueForRepeatRule(int rule) {
  switch (rule) {
    case RepeatImageRule:
      return CSSValueRepeat;
    case RoundImageRule:
      return CSSValueRound;
    case SpaceImageRule:
      return CSSValueSpace;
    default:
      return CSSValueStretch;
  }
}

static CSSValue* valueForNinePieceImageRepeat(const NinePieceImage& image) {
  CSSIdentifierValue* horizontalRepeat = nullptr;
  CSSIdentifierValue* verticalRepeat = nullptr;

  horizontalRepeat =
      CSSIdentifierValue::create(valueForRepeatRule(image.horizontalRule()));
  if (image.horizontalRule() == image.verticalRule()) {
    verticalRepeat = horizontalRepeat;
  } else {
    verticalRepeat =
        CSSIdentifierValue::create(valueForRepeatRule(image.verticalRule()));
  }
  return CSSValuePair::create(horizontalRepeat, verticalRepeat,
                              CSSValuePair::DropIdenticalValues);
}

static CSSValue* valueForNinePieceImage(const NinePieceImage& image,
                                        const ComputedStyle& style) {
  if (!image.hasImage())
    return CSSIdentifierValue::create(CSSValueNone);

  // Image first.
  CSSValue* imageValue = nullptr;
  if (image.image())
    imageValue = image.image()->computedCSSValue();

  // Create the image slice.
  CSSBorderImageSliceValue* imageSlices = valueForNinePieceImageSlice(image);

  // Create the border area slices.
  CSSValue* borderSlices =
      valueForNinePieceImageQuad(image.borderSlices(), style);

  // Create the border outset.
  CSSValue* outset = valueForNinePieceImageQuad(image.outset(), style);

  // Create the repeat rules.
  CSSValue* repeat = valueForNinePieceImageRepeat(image);

  return createBorderImageValue(imageValue, imageSlices, borderSlices, outset,
                                repeat);
}

static CSSValue* valueForReflection(const StyleReflection* reflection,
                                    const ComputedStyle& style) {
  if (!reflection)
    return CSSIdentifierValue::create(CSSValueNone);

  CSSPrimitiveValue* offset = nullptr;
  // TODO(alancutter): Make this work correctly for calc lengths.
  if (reflection->offset().isPercentOrCalc())
    offset = CSSPrimitiveValue::create(reflection->offset().percent(),
                                       CSSPrimitiveValue::UnitType::Percentage);
  else
    offset = zoomAdjustedPixelValue(reflection->offset().value(), style);

  CSSIdentifierValue* direction = nullptr;
  switch (reflection->direction()) {
    case ReflectionBelow:
      direction = CSSIdentifierValue::create(CSSValueBelow);
      break;
    case ReflectionAbove:
      direction = CSSIdentifierValue::create(CSSValueAbove);
      break;
    case ReflectionLeft:
      direction = CSSIdentifierValue::create(CSSValueLeft);
      break;
    case ReflectionRight:
      direction = CSSIdentifierValue::create(CSSValueRight);
      break;
  }

  return CSSReflectValue::create(
      direction, offset, valueForNinePieceImage(reflection->mask(), style));
}

static CSSValueList* valueForItemPositionWithOverflowAlignment(
    const StyleSelfAlignmentData& data) {
  CSSValueList* result = CSSValueList::createSpaceSeparated();
  if (data.positionType() == LegacyPosition)
    result->append(*CSSIdentifierValue::create(CSSValueLegacy));
  // To avoid needing to copy the RareNonInheritedData, we repurpose the 'auto'
  // flag to not just mean 'auto' prior to running the StyleAdjuster but also
  // mean 'normal' after running it.
  result->append(*CSSIdentifierValue::create(
      data.position() == ItemPositionAuto
          ? ComputedStyle::initialDefaultAlignment().position()
          : data.position()));
  if (data.position() >= ItemPositionCenter &&
      data.overflow() != OverflowAlignmentDefault)
    result->append(*CSSIdentifierValue::create(data.overflow()));
  ASSERT(result->length() <= 2);
  return result;
}

static CSSValueList* valuesForGridShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layoutObject,
    Node* styledNode,
    bool allowVisitedStyle) {
  CSSValueList* list = CSSValueList::createSlashSeparated();
  for (size_t i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value = ComputedStyleCSSValueMapping::get(
        shorthand.properties()[i], style, layoutObject, styledNode,
        allowVisitedStyle);
    ASSERT(value);
    list->append(*value);
  }
  return list;
}

static CSSValueList* valuesForShorthandProperty(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layoutObject,
    Node* styledNode,
    bool allowVisitedStyle) {
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  for (size_t i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value = ComputedStyleCSSValueMapping::get(
        shorthand.properties()[i], style, layoutObject, styledNode,
        allowVisitedStyle);
    ASSERT(value);
    list->append(*value);
  }
  return list;
}

static CSSValue* expandNoneLigaturesValue() {
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  list->append(*CSSIdentifierValue::create(CSSValueNoCommonLigatures));
  list->append(*CSSIdentifierValue::create(CSSValueNoDiscretionaryLigatures));
  list->append(*CSSIdentifierValue::create(CSSValueNoHistoricalLigatures));
  list->append(*CSSIdentifierValue::create(CSSValueNoContextual));
  return list;
}

static CSSValue* valuesForFontVariantProperty(const ComputedStyle& style,
                                              const LayoutObject* layoutObject,
                                              Node* styledNode,
                                              bool allowVisitedStyle) {
  enum VariantShorthandCases { AllNormal, NoneLigatures, ConcatenateNonNormal };
  VariantShorthandCases shorthandCase = AllNormal;
  for (size_t i = 0; i < fontVariantShorthand().length(); ++i) {
    const CSSValue* value = ComputedStyleCSSValueMapping::get(
        fontVariantShorthand().properties()[i], style, layoutObject, styledNode,
        allowVisitedStyle);

    if (shorthandCase == AllNormal && value->isIdentifierValue() &&
        toCSSIdentifierValue(value)->getValueID() == CSSValueNone &&
        fontVariantShorthand().properties()[i] ==
            CSSPropertyFontVariantLigatures) {
      shorthandCase = NoneLigatures;
    } else if (!(value->isIdentifierValue() &&
                 toCSSIdentifierValue(value)->getValueID() == CSSValueNormal)) {
      shorthandCase = ConcatenateNonNormal;
      break;
    }
  }

  switch (shorthandCase) {
    case AllNormal:
      return CSSIdentifierValue::create(CSSValueNormal);
    case NoneLigatures:
      return CSSIdentifierValue::create(CSSValueNone);
    case ConcatenateNonNormal: {
      CSSValueList* list = CSSValueList::createSpaceSeparated();
      for (size_t i = 0; i < fontVariantShorthand().length(); ++i) {
        const CSSValue* value = ComputedStyleCSSValueMapping::get(
            fontVariantShorthand().properties()[i], style, layoutObject,
            styledNode, allowVisitedStyle);
        ASSERT(value);
        if (value->isIdentifierValue() &&
            toCSSIdentifierValue(value)->getValueID() == CSSValueNone) {
          list->append(*expandNoneLigaturesValue());
        } else if (!(value->isIdentifierValue() &&
                     toCSSIdentifierValue(value)->getValueID() ==
                         CSSValueNormal)) {
          list->append(*value);
        }
      }
      return list;
    }
    default:
      NOTREACHED();
      return nullptr;
  }
}

static CSSValueList* valuesForBackgroundShorthand(
    const ComputedStyle& style,
    const LayoutObject* layoutObject,
    Node* styledNode,
    bool allowVisitedStyle) {
  CSSValueList* ret = CSSValueList::createCommaSeparated();
  const FillLayer* currLayer = &style.backgroundLayers();
  for (; currLayer; currLayer = currLayer->next()) {
    CSSValueList* list = CSSValueList::createSlashSeparated();
    CSSValueList* beforeSlash = CSSValueList::createSpaceSeparated();
    if (!currLayer->next()) {  // color only for final layer
      const CSSValue* value = ComputedStyleCSSValueMapping::get(
          CSSPropertyBackgroundColor, style, layoutObject, styledNode,
          allowVisitedStyle);
      ASSERT(value);
      beforeSlash->append(*value);
    }
    beforeSlash->append(currLayer->image()
                            ? *currLayer->image()->computedCSSValue()
                            : *CSSIdentifierValue::create(CSSValueNone));
    beforeSlash->append(
        *valueForFillRepeat(currLayer->repeatX(), currLayer->repeatY()));
    beforeSlash->append(*CSSIdentifierValue::create(currLayer->attachment()));
    beforeSlash->append(*createPositionListForLayer(
        CSSPropertyBackgroundPosition, *currLayer, style));
    list->append(*beforeSlash);
    CSSValueList* afterSlash = CSSValueList::createSpaceSeparated();
    afterSlash->append(*valueForFillSize(currLayer->size(), style));
    afterSlash->append(*CSSIdentifierValue::create(currLayer->origin()));
    afterSlash->append(*CSSIdentifierValue::create(currLayer->clip()));
    list->append(*afterSlash);
    ret->append(*list);
  }
  return ret;
}

static CSSValueList*
valueForContentPositionAndDistributionWithOverflowAlignment(
    const StyleContentAlignmentData& data,
    CSSValueID normalBehaviorValueID) {
  CSSValueList* result = CSSValueList::createSpaceSeparated();
  if (data.distribution() != ContentDistributionDefault)
    result->append(*CSSIdentifierValue::create(data.distribution()));
  if (data.distribution() == ContentDistributionDefault ||
      data.position() != ContentPositionNormal) {
    if (!RuntimeEnabledFeatures::cssGridLayoutEnabled() &&
        data.position() == ContentPositionNormal)
      result->append(*CSSIdentifierValue::create(normalBehaviorValueID));
    else
      result->append(*CSSIdentifierValue::create(data.position()));
  }
  if ((data.position() >= ContentPositionCenter ||
       data.distribution() != ContentDistributionDefault) &&
      data.overflow() != OverflowAlignmentDefault)
    result->append(*CSSIdentifierValue::create(data.overflow()));
  ASSERT(result->length() > 0);
  ASSERT(result->length() <= 3);
  return result;
}

static CSSValue* valueForLineHeight(const ComputedStyle& style) {
  Length length = style.lineHeight();
  if (length.isNegative())
    return CSSIdentifierValue::create(CSSValueNormal);

  return zoomAdjustedPixelValue(
      floatValueForLength(length, style.getFontDescription().computedSize()),
      style);
}

static CSSValue* valueForPosition(const LengthPoint& position,
                                  const ComputedStyle& style) {
  DCHECK((position.x() == Auto) == (position.y() == Auto));
  if (position.x() == Auto) {
    return CSSIdentifierValue::create(CSSValueAuto);
  }
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  list->append(*zoomAdjustedPixelValueForLength(position.x(), style));
  list->append(*zoomAdjustedPixelValueForLength(position.y(), style));
  return list;
}

static CSSValueID identifierForFamily(const AtomicString& family) {
  if (family == FontFamilyNames::webkit_cursive)
    return CSSValueCursive;
  if (family == FontFamilyNames::webkit_fantasy)
    return CSSValueFantasy;
  if (family == FontFamilyNames::webkit_monospace)
    return CSSValueMonospace;
  if (family == FontFamilyNames::webkit_pictograph)
    return CSSValueWebkitPictograph;
  if (family == FontFamilyNames::webkit_sans_serif)
    return CSSValueSansSerif;
  if (family == FontFamilyNames::webkit_serif)
    return CSSValueSerif;
  return CSSValueInvalid;
}

static CSSValue* valueForFamily(const AtomicString& family) {
  if (CSSValueID familyIdentifier = identifierForFamily(family))
    return CSSIdentifierValue::create(familyIdentifier);
  return CSSFontFamilyValue::create(family.getString());
}

static CSSValueList* valueForFontFamily(const ComputedStyle& style) {
  const FontFamily& firstFamily = style.getFontDescription().family();
  CSSValueList* list = CSSValueList::createCommaSeparated();
  for (const FontFamily* family = &firstFamily; family; family = family->next())
    list->append(*valueForFamily(family->family()));
  return list;
}

static CSSPrimitiveValue* valueForFontSize(const ComputedStyle& style) {
  return zoomAdjustedPixelValue(style.getFontDescription().computedSize(),
                                style);
}

static CSSIdentifierValue* valueForFontStretch(const ComputedStyle& style) {
  return CSSIdentifierValue::create(style.getFontDescription().stretch());
}

static CSSIdentifierValue* valueForFontStyle(const ComputedStyle& style) {
  return CSSIdentifierValue::create(style.getFontDescription().style());
}

static CSSIdentifierValue* valueForFontWeight(const ComputedStyle& style) {
  return CSSIdentifierValue::create(style.getFontDescription().weight());
}

static CSSIdentifierValue* valueForFontVariantCaps(const ComputedStyle& style) {
  FontDescription::FontVariantCaps variantCaps =
      style.getFontDescription().variantCaps();
  switch (variantCaps) {
    case FontDescription::CapsNormal:
      return CSSIdentifierValue::create(CSSValueNormal);
    case FontDescription::SmallCaps:
      return CSSIdentifierValue::create(CSSValueSmallCaps);
    case FontDescription::AllSmallCaps:
      return CSSIdentifierValue::create(CSSValueAllSmallCaps);
    case FontDescription::PetiteCaps:
      return CSSIdentifierValue::create(CSSValuePetiteCaps);
    case FontDescription::AllPetiteCaps:
      return CSSIdentifierValue::create(CSSValueAllPetiteCaps);
    case FontDescription::Unicase:
      return CSSIdentifierValue::create(CSSValueUnicase);
    case FontDescription::TitlingCaps:
      return CSSIdentifierValue::create(CSSValueTitlingCaps);
    default:
      NOTREACHED();
      return nullptr;
  }
}

static CSSValue* valueForFontVariantLigatures(const ComputedStyle& style) {
  FontDescription::LigaturesState commonLigaturesState =
      style.getFontDescription().commonLigaturesState();
  FontDescription::LigaturesState discretionaryLigaturesState =
      style.getFontDescription().discretionaryLigaturesState();
  FontDescription::LigaturesState historicalLigaturesState =
      style.getFontDescription().historicalLigaturesState();
  FontDescription::LigaturesState contextualLigaturesState =
      style.getFontDescription().contextualLigaturesState();
  if (commonLigaturesState == FontDescription::NormalLigaturesState &&
      discretionaryLigaturesState == FontDescription::NormalLigaturesState &&
      historicalLigaturesState == FontDescription::NormalLigaturesState &&
      contextualLigaturesState == FontDescription::NormalLigaturesState)
    return CSSIdentifierValue::create(CSSValueNormal);

  if (commonLigaturesState == FontDescription::DisabledLigaturesState &&
      discretionaryLigaturesState == FontDescription::DisabledLigaturesState &&
      historicalLigaturesState == FontDescription::DisabledLigaturesState &&
      contextualLigaturesState == FontDescription::DisabledLigaturesState)
    return CSSIdentifierValue::create(CSSValueNone);

  CSSValueList* valueList = CSSValueList::createSpaceSeparated();
  if (commonLigaturesState != FontDescription::NormalLigaturesState)
    valueList->append(*CSSIdentifierValue::create(
        commonLigaturesState == FontDescription::DisabledLigaturesState
            ? CSSValueNoCommonLigatures
            : CSSValueCommonLigatures));
  if (discretionaryLigaturesState != FontDescription::NormalLigaturesState)
    valueList->append(*CSSIdentifierValue::create(
        discretionaryLigaturesState == FontDescription::DisabledLigaturesState
            ? CSSValueNoDiscretionaryLigatures
            : CSSValueDiscretionaryLigatures));
  if (historicalLigaturesState != FontDescription::NormalLigaturesState)
    valueList->append(*CSSIdentifierValue::create(
        historicalLigaturesState == FontDescription::DisabledLigaturesState
            ? CSSValueNoHistoricalLigatures
            : CSSValueHistoricalLigatures));
  if (contextualLigaturesState != FontDescription::NormalLigaturesState)
    valueList->append(*CSSIdentifierValue::create(
        contextualLigaturesState == FontDescription::DisabledLigaturesState
            ? CSSValueNoContextual
            : CSSValueContextual));
  return valueList;
}

static CSSValue* valueForFontVariantNumeric(const ComputedStyle& style) {
  FontVariantNumeric variantNumeric =
      style.getFontDescription().variantNumeric();
  if (variantNumeric.isAllNormal())
    return CSSIdentifierValue::create(CSSValueNormal);

  CSSValueList* valueList = CSSValueList::createSpaceSeparated();
  if (variantNumeric.numericFigureValue() != FontVariantNumeric::NormalFigure)
    valueList->append(*CSSIdentifierValue::create(
        variantNumeric.numericFigureValue() == FontVariantNumeric::LiningNums
            ? CSSValueLiningNums
            : CSSValueOldstyleNums));
  if (variantNumeric.numericSpacingValue() !=
      FontVariantNumeric::NormalSpacing) {
    valueList->append(
        *CSSIdentifierValue::create(variantNumeric.numericSpacingValue() ==
                                            FontVariantNumeric::ProportionalNums
                                        ? CSSValueProportionalNums
                                        : CSSValueTabularNums));
  }
  if (variantNumeric.numericFractionValue() !=
      FontVariantNumeric::NormalFraction)
    valueList->append(*CSSIdentifierValue::create(
        variantNumeric.numericFractionValue() ==
                FontVariantNumeric::DiagonalFractions
            ? CSSValueDiagonalFractions
            : CSSValueStackedFractions));
  if (variantNumeric.ordinalValue() == FontVariantNumeric::OrdinalOn)
    valueList->append(*CSSIdentifierValue::create(CSSValueOrdinal));
  if (variantNumeric.slashedZeroValue() == FontVariantNumeric::SlashedZeroOn)
    valueList->append(*CSSIdentifierValue::create(CSSValueSlashedZero));

  return valueList;
}

static CSSValue* specifiedValueForGridTrackBreadth(
    const GridLength& trackBreadth,
    const ComputedStyle& style) {
  if (!trackBreadth.isLength())
    return CSSPrimitiveValue::create(trackBreadth.flex(),
                                     CSSPrimitiveValue::UnitType::Fraction);

  const Length& trackBreadthLength = trackBreadth.length();
  if (trackBreadthLength.isAuto())
    return CSSIdentifierValue::create(CSSValueAuto);
  return zoomAdjustedPixelValueForLength(trackBreadthLength, style);
}

static CSSValue* specifiedValueForGridTrackSize(const GridTrackSize& trackSize,
                                                const ComputedStyle& style) {
  switch (trackSize.type()) {
    case LengthTrackSizing:
      return specifiedValueForGridTrackBreadth(trackSize.minTrackBreadth(),
                                               style);
    case MinMaxTrackSizing: {
      if (trackSize.minTrackBreadth().isAuto() &&
          trackSize.maxTrackBreadth().isFlex()) {
        return CSSPrimitiveValue::create(trackSize.maxTrackBreadth().flex(),
                                         CSSPrimitiveValue::UnitType::Fraction);
      }

      auto* minMaxTrackBreadths = CSSFunctionValue::create(CSSValueMinmax);
      minMaxTrackBreadths->append(*specifiedValueForGridTrackBreadth(
          trackSize.minTrackBreadth(), style));
      minMaxTrackBreadths->append(*specifiedValueForGridTrackBreadth(
          trackSize.maxTrackBreadth(), style));
      return minMaxTrackBreadths;
    }
    case FitContentTrackSizing: {
      auto* fitContentTrackBreadth =
          CSSFunctionValue::create(CSSValueFitContent);
      fitContentTrackBreadth->append(*specifiedValueForGridTrackBreadth(
          trackSize.fitContentTrackBreadth(), style));
      return fitContentTrackBreadth;
    }
  }
  ASSERT_NOT_REACHED();
  return nullptr;
}

class OrderedNamedLinesCollector {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(OrderedNamedLinesCollector);

 public:
  OrderedNamedLinesCollector(const ComputedStyle& style,
                             bool isRowAxis,
                             size_t autoRepeatTracksCount)
      : m_orderedNamedGridLines(isRowAxis ? style.orderedNamedGridColumnLines()
                                          : style.orderedNamedGridRowLines()),
        m_orderedNamedAutoRepeatGridLines(
            isRowAxis ? style.autoRepeatOrderedNamedGridColumnLines()
                      : style.autoRepeatOrderedNamedGridRowLines()),
        m_insertionPoint(isRowAxis ? style.gridAutoRepeatColumnsInsertionPoint()
                                   : style.gridAutoRepeatRowsInsertionPoint()),
        m_autoRepeatTotalTracks(autoRepeatTracksCount),
        m_autoRepeatTrackListLength(isRowAxis
                                        ? style.gridAutoRepeatColumns().size()
                                        : style.gridAutoRepeatRows().size()) {}

  bool isEmpty() const {
    return m_orderedNamedGridLines.isEmpty() &&
           m_orderedNamedAutoRepeatGridLines.isEmpty();
  }
  void collectLineNamesForIndex(CSSGridLineNamesValue&, size_t index) const;

 private:
  enum NamedLinesType { NamedLines, AutoRepeatNamedLines };
  void appendLines(CSSGridLineNamesValue&, size_t index, NamedLinesType) const;

  const OrderedNamedGridLines& m_orderedNamedGridLines;
  const OrderedNamedGridLines& m_orderedNamedAutoRepeatGridLines;
  size_t m_insertionPoint;
  size_t m_autoRepeatTotalTracks;
  size_t m_autoRepeatTrackListLength;
};

void OrderedNamedLinesCollector::appendLines(
    CSSGridLineNamesValue& lineNamesValue,
    size_t index,
    NamedLinesType type) const {
  auto iter = type == NamedLines
                  ? m_orderedNamedGridLines.find(index)
                  : m_orderedNamedAutoRepeatGridLines.find(index);
  auto endIter = type == NamedLines ? m_orderedNamedGridLines.end()
                                    : m_orderedNamedAutoRepeatGridLines.end();
  if (iter == endIter)
    return;

  for (auto lineName : iter->value)
    lineNamesValue.append(*CSSCustomIdentValue::create(AtomicString(lineName)));
}

void OrderedNamedLinesCollector::collectLineNamesForIndex(
    CSSGridLineNamesValue& lineNamesValue,
    size_t i) const {
  DCHECK(!isEmpty());
  if (m_orderedNamedAutoRepeatGridLines.isEmpty() || i < m_insertionPoint) {
    appendLines(lineNamesValue, i, NamedLines);
    return;
  }

  DCHECK(m_autoRepeatTotalTracks);

  if (i > m_insertionPoint + m_autoRepeatTotalTracks) {
    appendLines(lineNamesValue, i - (m_autoRepeatTotalTracks - 1), NamedLines);
    return;
  }

  if (i == m_insertionPoint) {
    appendLines(lineNamesValue, i, NamedLines);
    appendLines(lineNamesValue, 0, AutoRepeatNamedLines);
    return;
  }

  if (i == m_insertionPoint + m_autoRepeatTotalTracks) {
    appendLines(lineNamesValue, m_autoRepeatTrackListLength,
                AutoRepeatNamedLines);
    appendLines(lineNamesValue, m_insertionPoint + 1, NamedLines);
    return;
  }

  size_t autoRepeatIndexInFirstRepetition =
      (i - m_insertionPoint) % m_autoRepeatTrackListLength;
  if (!autoRepeatIndexInFirstRepetition && i > m_insertionPoint)
    appendLines(lineNamesValue, m_autoRepeatTrackListLength,
                AutoRepeatNamedLines);
  appendLines(lineNamesValue, autoRepeatIndexInFirstRepetition,
              AutoRepeatNamedLines);
}

static void addValuesForNamedGridLinesAtIndex(
    OrderedNamedLinesCollector& collector,
    size_t i,
    CSSValueList& list) {
  if (collector.isEmpty())
    return;

  CSSGridLineNamesValue* lineNames = CSSGridLineNamesValue::create();
  collector.collectLineNamesForIndex(*lineNames, i);
  if (lineNames->length())
    list.append(*lineNames);
}

static CSSValue* valueForGridTrackSizeList(GridTrackSizingDirection direction,
                                           const ComputedStyle& style) {
  const Vector<GridTrackSize>& autoTrackSizes =
      direction == ForColumns ? style.gridAutoColumns() : style.gridAutoRows();

  CSSValueList* list = CSSValueList::createSpaceSeparated();
  for (auto& trackSize : autoTrackSizes)
    list->append(*specifiedValueForGridTrackSize(trackSize, style));
  return list;
}

static CSSValue* valueForGridTrackList(GridTrackSizingDirection direction,
                                       const LayoutObject* layoutObject,
                                       const ComputedStyle& style) {
  bool isRowAxis = direction == ForColumns;
  const Vector<GridTrackSize>& trackSizes =
      isRowAxis ? style.gridTemplateColumns() : style.gridTemplateRows();
  const Vector<GridTrackSize>& autoRepeatTrackSizes =
      isRowAxis ? style.gridAutoRepeatColumns() : style.gridAutoRepeatRows();
  bool isLayoutGrid = layoutObject && layoutObject->isLayoutGrid();

  // Handle the 'none' case.
  bool trackListIsEmpty =
      trackSizes.isEmpty() && autoRepeatTrackSizes.isEmpty();
  if (isLayoutGrid && trackListIsEmpty) {
    // For grids we should consider every listed track, whether implicitly or
    // explicitly created. Empty grids have a sole grid line per axis.
    auto& positions = isRowAxis ? toLayoutGrid(layoutObject)->columnPositions()
                                : toLayoutGrid(layoutObject)->rowPositions();
    trackListIsEmpty = positions.size() == 1;
  }

  if (trackListIsEmpty)
    return CSSIdentifierValue::create(CSSValueNone);

  size_t autoRepeatTotalTracks =
      isLayoutGrid
          ? toLayoutGrid(layoutObject)->autoRepeatCountForDirection(direction)
          : 0;
  OrderedNamedLinesCollector collector(style, isRowAxis, autoRepeatTotalTracks);
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  size_t insertionIndex;
  if (isLayoutGrid) {
    const auto* grid = toLayoutGrid(layoutObject);
    Vector<LayoutUnit> computedTrackSizes =
        grid->trackSizesForComputedStyle(direction);
    size_t numTracks = computedTrackSizes.size();

    for (size_t i = 0; i < numTracks; ++i) {
      addValuesForNamedGridLinesAtIndex(collector, i, *list);
      list->append(*zoomAdjustedPixelValue(computedTrackSizes[i], style));
    }
    addValuesForNamedGridLinesAtIndex(collector, numTracks + 1, *list);

    insertionIndex = numTracks;
  } else {
    for (size_t i = 0; i < trackSizes.size(); ++i) {
      addValuesForNamedGridLinesAtIndex(collector, i, *list);
      list->append(*specifiedValueForGridTrackSize(trackSizes[i], style));
    }
    insertionIndex = trackSizes.size();
  }
  // Those are the trailing <string>* allowed in the syntax.
  addValuesForNamedGridLinesAtIndex(collector, insertionIndex, *list);
  return list;
}

static CSSValue* valueForGridPosition(const GridPosition& position) {
  if (position.isAuto())
    return CSSIdentifierValue::create(CSSValueAuto);

  if (position.isNamedGridArea())
    return CSSCustomIdentValue::create(position.namedGridLine());

  CSSValueList* list = CSSValueList::createSpaceSeparated();
  if (position.isSpan()) {
    list->append(*CSSIdentifierValue::create(CSSValueSpan));
    list->append(*CSSPrimitiveValue::create(
        position.spanPosition(), CSSPrimitiveValue::UnitType::Number));
  } else {
    list->append(*CSSPrimitiveValue::create(
        position.integerPosition(), CSSPrimitiveValue::UnitType::Number));
  }

  if (!position.namedGridLine().isNull())
    list->append(*CSSCustomIdentValue::create(position.namedGridLine()));
  return list;
}

static LayoutRect sizingBox(const LayoutObject* layoutObject) {
  if (!layoutObject->isBox())
    return LayoutRect();

  const LayoutBox* box = toLayoutBox(layoutObject);
  return box->style()->boxSizing() == BoxSizingBorderBox
             ? box->borderBoxRect()
             : box->computedCSSContentBoxRect();
}

static CSSValue* renderTextDecorationFlagsToCSSValue(int textDecoration) {
  // Blink value is ignored.
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  if (textDecoration & TextDecorationUnderline)
    list->append(*CSSIdentifierValue::create(CSSValueUnderline));
  if (textDecoration & TextDecorationOverline)
    list->append(*CSSIdentifierValue::create(CSSValueOverline));
  if (textDecoration & TextDecorationLineThrough)
    list->append(*CSSIdentifierValue::create(CSSValueLineThrough));

  if (!list->length())
    return CSSIdentifierValue::create(CSSValueNone);
  return list;
}

static CSSValue* valueForTextDecorationStyle(
    TextDecorationStyle textDecorationStyle) {
  switch (textDecorationStyle) {
    case TextDecorationStyleSolid:
      return CSSIdentifierValue::create(CSSValueSolid);
    case TextDecorationStyleDouble:
      return CSSIdentifierValue::create(CSSValueDouble);
    case TextDecorationStyleDotted:
      return CSSIdentifierValue::create(CSSValueDotted);
    case TextDecorationStyleDashed:
      return CSSIdentifierValue::create(CSSValueDashed);
    case TextDecorationStyleWavy:
      return CSSIdentifierValue::create(CSSValueWavy);
  }

  ASSERT_NOT_REACHED();
  return CSSInitialValue::create();
}

static CSSValue* valueForTextDecorationSkip(
    TextDecorationSkip textDecorationSkip) {
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  if (textDecorationSkip & TextDecorationSkipObjects)
    list->append(*CSSIdentifierValue::create(CSSValueObjects));
  if (textDecorationSkip & TextDecorationSkipInk)
    list->append(*CSSIdentifierValue::create(CSSValueInk));

  DCHECK(list->length());
  return list;
}

static CSSValue* touchActionFlagsToCSSValue(TouchAction touchAction) {
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  if (touchAction == TouchActionAuto) {
    list->append(*CSSIdentifierValue::create(CSSValueAuto));
  } else if (touchAction == TouchActionNone) {
    list->append(*CSSIdentifierValue::create(CSSValueNone));
  } else if (touchAction == TouchActionManipulation) {
    list->append(*CSSIdentifierValue::create(CSSValueManipulation));
  } else {
    if ((touchAction & TouchActionPanX) == TouchActionPanX)
      list->append(*CSSIdentifierValue::create(CSSValuePanX));
    else if (touchAction & TouchActionPanLeft)
      list->append(*CSSIdentifierValue::create(CSSValuePanLeft));
    else if (touchAction & TouchActionPanRight)
      list->append(*CSSIdentifierValue::create(CSSValuePanRight));
    if ((touchAction & TouchActionPanY) == TouchActionPanY)
      list->append(*CSSIdentifierValue::create(CSSValuePanY));
    else if (touchAction & TouchActionPanUp)
      list->append(*CSSIdentifierValue::create(CSSValuePanUp));
    else if (touchAction & TouchActionPanDown)
      list->append(*CSSIdentifierValue::create(CSSValuePanDown));

    if ((touchAction & TouchActionPinchZoom) == TouchActionPinchZoom)
      list->append(*CSSIdentifierValue::create(CSSValuePinchZoom));
  }

  ASSERT(list->length());
  return list;
}

static CSSValue* valueForWillChange(
    const Vector<CSSPropertyID>& willChangeProperties,
    bool willChangeContents,
    bool willChangeScrollPosition) {
  CSSValueList* list = CSSValueList::createCommaSeparated();
  if (willChangeContents)
    list->append(*CSSIdentifierValue::create(CSSValueContents));
  if (willChangeScrollPosition)
    list->append(*CSSIdentifierValue::create(CSSValueScrollPosition));
  for (size_t i = 0; i < willChangeProperties.size(); ++i)
    list->append(*CSSCustomIdentValue::create(willChangeProperties[i]));
  if (!list->length())
    list->append(*CSSIdentifierValue::create(CSSValueAuto));
  return list;
}

static CSSValue* valueForAnimationDelay(const CSSTimingData* timingData) {
  CSSValueList* list = CSSValueList::createCommaSeparated();
  if (timingData) {
    for (size_t i = 0; i < timingData->delayList().size(); ++i)
      list->append(*CSSPrimitiveValue::create(
          timingData->delayList()[i], CSSPrimitiveValue::UnitType::Seconds));
  } else {
    list->append(*CSSPrimitiveValue::create(
        CSSTimingData::initialDelay(), CSSPrimitiveValue::UnitType::Seconds));
  }
  return list;
}

static CSSValue* valueForAnimationDirection(
    Timing::PlaybackDirection direction) {
  switch (direction) {
    case Timing::PlaybackDirection::NORMAL:
      return CSSIdentifierValue::create(CSSValueNormal);
    case Timing::PlaybackDirection::ALTERNATE_NORMAL:
      return CSSIdentifierValue::create(CSSValueAlternate);
    case Timing::PlaybackDirection::REVERSE:
      return CSSIdentifierValue::create(CSSValueReverse);
    case Timing::PlaybackDirection::ALTERNATE_REVERSE:
      return CSSIdentifierValue::create(CSSValueAlternateReverse);
    default:
      ASSERT_NOT_REACHED();
      return nullptr;
  }
}

static CSSValue* valueForAnimationDuration(const CSSTimingData* timingData) {
  CSSValueList* list = CSSValueList::createCommaSeparated();
  if (timingData) {
    for (size_t i = 0; i < timingData->durationList().size(); ++i)
      list->append(*CSSPrimitiveValue::create(
          timingData->durationList()[i], CSSPrimitiveValue::UnitType::Seconds));
  } else {
    list->append(
        *CSSPrimitiveValue::create(CSSTimingData::initialDuration(),
                                   CSSPrimitiveValue::UnitType::Seconds));
  }
  return list;
}

static CSSValue* valueForAnimationFillMode(Timing::FillMode fillMode) {
  switch (fillMode) {
    case Timing::FillMode::NONE:
      return CSSIdentifierValue::create(CSSValueNone);
    case Timing::FillMode::FORWARDS:
      return CSSIdentifierValue::create(CSSValueForwards);
    case Timing::FillMode::BACKWARDS:
      return CSSIdentifierValue::create(CSSValueBackwards);
    case Timing::FillMode::BOTH:
      return CSSIdentifierValue::create(CSSValueBoth);
    default:
      ASSERT_NOT_REACHED();
      return nullptr;
  }
}

static CSSValue* valueForAnimationIterationCount(double iterationCount) {
  if (iterationCount == std::numeric_limits<double>::infinity())
    return CSSIdentifierValue::create(CSSValueInfinite);
  return CSSPrimitiveValue::create(iterationCount,
                                   CSSPrimitiveValue::UnitType::Number);
}

static CSSValue* valueForAnimationPlayState(EAnimPlayState playState) {
  if (playState == AnimPlayStatePlaying)
    return CSSIdentifierValue::create(CSSValueRunning);
  ASSERT(playState == AnimPlayStatePaused);
  return CSSIdentifierValue::create(CSSValuePaused);
}

static CSSValue* createTimingFunctionValue(
    const TimingFunction* timingFunction) {
  switch (timingFunction->getType()) {
    case TimingFunction::Type::CUBIC_BEZIER: {
      const CubicBezierTimingFunction* bezierTimingFunction =
          toCubicBezierTimingFunction(timingFunction);
      if (bezierTimingFunction->getEaseType() !=
          CubicBezierTimingFunction::EaseType::CUSTOM) {
        CSSValueID valueId = CSSValueInvalid;
        switch (bezierTimingFunction->getEaseType()) {
          case CubicBezierTimingFunction::EaseType::EASE:
            valueId = CSSValueEase;
            break;
          case CubicBezierTimingFunction::EaseType::EASE_IN:
            valueId = CSSValueEaseIn;
            break;
          case CubicBezierTimingFunction::EaseType::EASE_OUT:
            valueId = CSSValueEaseOut;
            break;
          case CubicBezierTimingFunction::EaseType::EASE_IN_OUT:
            valueId = CSSValueEaseInOut;
            break;
          default:
            ASSERT_NOT_REACHED();
            return nullptr;
        }
        return CSSIdentifierValue::create(valueId);
      }
      return CSSCubicBezierTimingFunctionValue::create(
          bezierTimingFunction->x1(), bezierTimingFunction->y1(),
          bezierTimingFunction->x2(), bezierTimingFunction->y2());
    }

    case TimingFunction::Type::STEPS: {
      const StepsTimingFunction* stepsTimingFunction =
          toStepsTimingFunction(timingFunction);
      StepsTimingFunction::StepPosition position =
          stepsTimingFunction->getStepPosition();
      int steps = stepsTimingFunction->numberOfSteps();
      DCHECK(position == StepsTimingFunction::StepPosition::START ||
             position == StepsTimingFunction::StepPosition::END);

      if (steps > 1)
        return CSSStepsTimingFunctionValue::create(steps, position);
      CSSValueID valueId = position == StepsTimingFunction::StepPosition::START
                               ? CSSValueStepStart
                               : CSSValueStepEnd;
      return CSSIdentifierValue::create(valueId);
    }

    default:
      return CSSIdentifierValue::create(CSSValueLinear);
  }
}

static CSSValue* valueForAnimationTimingFunction(
    const CSSTimingData* timingData) {
  CSSValueList* list = CSSValueList::createCommaSeparated();
  if (timingData) {
    for (size_t i = 0; i < timingData->timingFunctionList().size(); ++i)
      list->append(*createTimingFunctionValue(
          timingData->timingFunctionList()[i].get()));
  } else {
    list->append(*createTimingFunctionValue(
        CSSTimingData::initialTimingFunction().get()));
  }
  return list;
}

static CSSValueList* valuesForBorderRadiusCorner(LengthSize radius,
                                                 const ComputedStyle& style) {
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  if (radius.width().type() == Percent)
    list->append(*CSSPrimitiveValue::create(
        radius.width().percent(), CSSPrimitiveValue::UnitType::Percentage));
  else
    list->append(*zoomAdjustedPixelValueForLength(radius.width(), style));
  if (radius.height().type() == Percent)
    list->append(*CSSPrimitiveValue::create(
        radius.height().percent(), CSSPrimitiveValue::UnitType::Percentage));
  else
    list->append(*zoomAdjustedPixelValueForLength(radius.height(), style));
  return list;
}

static const CSSValue& valueForBorderRadiusCorner(LengthSize radius,
                                                  const ComputedStyle& style) {
  CSSValueList& list = *valuesForBorderRadiusCorner(radius, style);
  if (list.item(0).equals(list.item(1)))
    return list.item(0);
  return list;
}

static CSSFunctionValue* valueForMatrixTransform(TransformationMatrix transform,
                                                 const ComputedStyle& style) {
  CSSFunctionValue* transformValue = nullptr;
  transform.zoom(1 / style.effectiveZoom());
  if (transform.isAffine()) {
    transformValue = CSSFunctionValue::create(CSSValueMatrix);

    transformValue->append(*CSSPrimitiveValue::create(
        transform.a(), CSSPrimitiveValue::UnitType::Number));
    transformValue->append(*CSSPrimitiveValue::create(
        transform.b(), CSSPrimitiveValue::UnitType::Number));
    transformValue->append(*CSSPrimitiveValue::create(
        transform.c(), CSSPrimitiveValue::UnitType::Number));
    transformValue->append(*CSSPrimitiveValue::create(
        transform.d(), CSSPrimitiveValue::UnitType::Number));
    transformValue->append(*CSSPrimitiveValue::create(
        transform.e(), CSSPrimitiveValue::UnitType::Number));
    transformValue->append(*CSSPrimitiveValue::create(
        transform.f(), CSSPrimitiveValue::UnitType::Number));
  } else {
    transformValue = CSSFunctionValue::create(CSSValueMatrix3d);

    transformValue->append(*CSSPrimitiveValue::create(
        transform.m11(), CSSPrimitiveValue::UnitType::Number));
    transformValue->append(*CSSPrimitiveValue::create(
        transform.m12(), CSSPrimitiveValue::UnitType::Number));
    transformValue->append(*CSSPrimitiveValue::create(
        transform.m13(), CSSPrimitiveValue::UnitType::Number));
    transformValue->append(*CSSPrimitiveValue::create(
        transform.m14(), CSSPrimitiveValue::UnitType::Number));

    transformValue->append(*CSSPrimitiveValue::create(
        transform.m21(), CSSPrimitiveValue::UnitType::Number));
    transformValue->append(*CSSPrimitiveValue::create(
        transform.m22(), CSSPrimitiveValue::UnitType::Number));
    transformValue->append(*CSSPrimitiveValue::create(
        transform.m23(), CSSPrimitiveValue::UnitType::Number));
    transformValue->append(*CSSPrimitiveValue::create(
        transform.m24(), CSSPrimitiveValue::UnitType::Number));

    transformValue->append(*CSSPrimitiveValue::create(
        transform.m31(), CSSPrimitiveValue::UnitType::Number));
    transformValue->append(*CSSPrimitiveValue::create(
        transform.m32(), CSSPrimitiveValue::UnitType::Number));
    transformValue->append(*CSSPrimitiveValue::create(
        transform.m33(), CSSPrimitiveValue::UnitType::Number));
    transformValue->append(*CSSPrimitiveValue::create(
        transform.m34(), CSSPrimitiveValue::UnitType::Number));

    transformValue->append(*CSSPrimitiveValue::create(
        transform.m41(), CSSPrimitiveValue::UnitType::Number));
    transformValue->append(*CSSPrimitiveValue::create(
        transform.m42(), CSSPrimitiveValue::UnitType::Number));
    transformValue->append(*CSSPrimitiveValue::create(
        transform.m43(), CSSPrimitiveValue::UnitType::Number));
    transformValue->append(*CSSPrimitiveValue::create(
        transform.m44(), CSSPrimitiveValue::UnitType::Number));
  }

  return transformValue;
}

static CSSValue* computedTransform(const LayoutObject* layoutObject,
                                   const ComputedStyle& style) {
  if (!layoutObject || !style.hasTransform())
    return CSSIdentifierValue::create(CSSValueNone);

  IntRect box;
  if (layoutObject->isBox())
    box = pixelSnappedIntRect(toLayoutBox(layoutObject)->borderBoxRect());

  TransformationMatrix transform;
  style.applyTransform(transform, LayoutSize(box.size()),
                       ComputedStyle::ExcludeTransformOrigin,
                       ComputedStyle::ExcludeMotionPath,
                       ComputedStyle::ExcludeIndependentTransformProperties);

  // FIXME: Need to print out individual functions
  // (https://bugs.webkit.org/show_bug.cgi?id=23924)
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  list->append(*valueForMatrixTransform(transform, style));

  return list;
}

static CSSValue* createTransitionPropertyValue(
    const CSSTransitionData::TransitionProperty& property) {
  if (property.propertyType == CSSTransitionData::TransitionNone)
    return CSSIdentifierValue::create(CSSValueNone);
  if (property.propertyType == CSSTransitionData::TransitionUnknownProperty)
    return CSSCustomIdentValue::create(property.propertyString);
  ASSERT(property.propertyType == CSSTransitionData::TransitionKnownProperty);
  return CSSCustomIdentValue::create(
      getPropertyNameAtomicString(property.unresolvedProperty));
}

static CSSValue* valueForTransitionProperty(
    const CSSTransitionData* transitionData) {
  CSSValueList* list = CSSValueList::createCommaSeparated();
  if (transitionData) {
    for (size_t i = 0; i < transitionData->propertyList().size(); ++i)
      list->append(
          *createTransitionPropertyValue(transitionData->propertyList()[i]));
  } else {
    list->append(*CSSIdentifierValue::create(CSSValueAll));
  }
  return list;
}

CSSValueID valueForQuoteType(const QuoteType quoteType) {
  switch (quoteType) {
    case NO_OPEN_QUOTE:
      return CSSValueNoOpenQuote;
    case NO_CLOSE_QUOTE:
      return CSSValueNoCloseQuote;
    case CLOSE_QUOTE:
      return CSSValueCloseQuote;
    case OPEN_QUOTE:
      return CSSValueOpenQuote;
  }
  ASSERT_NOT_REACHED();
  return CSSValueInvalid;
}

static CSSValue* valueForContentData(const ComputedStyle& style) {
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  for (const ContentData* contentData = style.contentData(); contentData;
       contentData = contentData->next()) {
    if (contentData->isCounter()) {
      const CounterContent* counter =
          toCounterContentData(contentData)->counter();
      ASSERT(counter);
      CSSCustomIdentValue* identifier =
          CSSCustomIdentValue::create(counter->identifier());
      CSSStringValue* separator = CSSStringValue::create(counter->separator());
      CSSValueID listStyleIdent = CSSValueNone;
      if (counter->listStyle() != EListStyleType::NoneListStyle) {
        // TODO(sashab): Change this to use a converter instead of
        // CSSPrimitiveValueMappings.
        listStyleIdent =
            CSSIdentifierValue::create(counter->listStyle())->getValueID();
      }
      CSSIdentifierValue* listStyle =
          CSSIdentifierValue::create(listStyleIdent);
      list->append(*CSSCounterValue::create(identifier, listStyle, separator));
    } else if (contentData->isImage()) {
      const StyleImage* image = toImageContentData(contentData)->image();
      ASSERT(image);
      list->append(*image->computedCSSValue());
    } else if (contentData->isText()) {
      list->append(
          *CSSStringValue::create(toTextContentData(contentData)->text()));
    } else if (contentData->isQuote()) {
      const QuoteType quoteType = toQuoteContentData(contentData)->quote();
      list->append(*CSSIdentifierValue::create(valueForQuoteType(quoteType)));
    } else {
      ASSERT_NOT_REACHED();
    }
  }
  return list;
}

static CSSValue* valueForCounterDirectives(const ComputedStyle& style,
                                           CSSPropertyID propertyID) {
  const CounterDirectiveMap* map = style.counterDirectives();
  if (!map)
    return CSSIdentifierValue::create(CSSValueNone);

  CSSValueList* list = CSSValueList::createSpaceSeparated();
  for (const auto& item : *map) {
    bool isValidCounterValue = propertyID == CSSPropertyCounterIncrement
                                   ? item.value.isIncrement()
                                   : item.value.isReset();
    if (!isValidCounterValue)
      continue;

    list->append(*CSSCustomIdentValue::create(item.key));
    short number = propertyID == CSSPropertyCounterIncrement
                       ? item.value.incrementValue()
                       : item.value.resetValue();
    list->append(*CSSPrimitiveValue::create(
        (double)number, CSSPrimitiveValue::UnitType::Integer));
  }

  if (!list->length())
    return CSSIdentifierValue::create(CSSValueNone);

  return list;
}

static CSSValue* valueForShape(const ComputedStyle& style,
                               ShapeValue* shapeValue) {
  if (!shapeValue)
    return CSSIdentifierValue::create(CSSValueNone);
  if (shapeValue->type() == ShapeValue::Box)
    return CSSIdentifierValue::create(shapeValue->cssBox());
  if (shapeValue->type() == ShapeValue::Image) {
    if (shapeValue->image())
      return shapeValue->image()->computedCSSValue();
    return CSSIdentifierValue::create(CSSValueNone);
  }

  ASSERT(shapeValue->type() == ShapeValue::Shape);

  CSSValueList* list = CSSValueList::createSpaceSeparated();
  list->append(*valueForBasicShape(style, shapeValue->shape()));
  if (shapeValue->cssBox() != BoxMissing)
    list->append(*CSSIdentifierValue::create(shapeValue->cssBox()));
  return list;
}

static CSSValueList* valuesForSidesShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layoutObject,
    Node* styledNode,
    bool allowVisitedStyle) {
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  // Assume the properties are in the usual order top, right, bottom, left.
  const CSSValue* topValue = ComputedStyleCSSValueMapping::get(
      shorthand.properties()[0], style, layoutObject, styledNode,
      allowVisitedStyle);
  const CSSValue* rightValue = ComputedStyleCSSValueMapping::get(
      shorthand.properties()[1], style, layoutObject, styledNode,
      allowVisitedStyle);
  const CSSValue* bottomValue = ComputedStyleCSSValueMapping::get(
      shorthand.properties()[2], style, layoutObject, styledNode,
      allowVisitedStyle);
  const CSSValue* leftValue = ComputedStyleCSSValueMapping::get(
      shorthand.properties()[3], style, layoutObject, styledNode,
      allowVisitedStyle);

  // All 4 properties must be specified.
  if (!topValue || !rightValue || !bottomValue || !leftValue)
    return nullptr;

  bool showLeft = !compareCSSValuePtr(rightValue, leftValue);
  bool showBottom = !compareCSSValuePtr(topValue, bottomValue) || showLeft;
  bool showRight = !compareCSSValuePtr(topValue, rightValue) || showBottom;

  list->append(*topValue);
  if (showRight)
    list->append(*rightValue);
  if (showBottom)
    list->append(*bottomValue);
  if (showLeft)
    list->append(*leftValue);

  return list;
}

static CSSValueList* valueForBorderRadiusShorthand(const ComputedStyle& style) {
  CSSValueList* list = CSSValueList::createSlashSeparated();

  bool showHorizontalBottomLeft = style.borderTopRightRadius().width() !=
                                  style.borderBottomLeftRadius().width();
  bool showHorizontalBottomRight =
      showHorizontalBottomLeft || (style.borderBottomRightRadius().width() !=
                                   style.borderTopLeftRadius().width());
  bool showHorizontalTopRight =
      showHorizontalBottomRight || (style.borderTopRightRadius().width() !=
                                    style.borderTopLeftRadius().width());

  bool showVerticalBottomLeft = style.borderTopRightRadius().height() !=
                                style.borderBottomLeftRadius().height();
  bool showVerticalBottomRight =
      showVerticalBottomLeft || (style.borderBottomRightRadius().height() !=
                                 style.borderTopLeftRadius().height());
  bool showVerticalTopRight =
      showVerticalBottomRight || (style.borderTopRightRadius().height() !=
                                  style.borderTopLeftRadius().height());

  CSSValueList* topLeftRadius =
      valuesForBorderRadiusCorner(style.borderTopLeftRadius(), style);
  CSSValueList* topRightRadius =
      valuesForBorderRadiusCorner(style.borderTopRightRadius(), style);
  CSSValueList* bottomRightRadius =
      valuesForBorderRadiusCorner(style.borderBottomRightRadius(), style);
  CSSValueList* bottomLeftRadius =
      valuesForBorderRadiusCorner(style.borderBottomLeftRadius(), style);

  CSSValueList* horizontalRadii = CSSValueList::createSpaceSeparated();
  horizontalRadii->append(topLeftRadius->item(0));
  if (showHorizontalTopRight)
    horizontalRadii->append(topRightRadius->item(0));
  if (showHorizontalBottomRight)
    horizontalRadii->append(bottomRightRadius->item(0));
  if (showHorizontalBottomLeft)
    horizontalRadii->append(bottomLeftRadius->item(0));

  list->append(*horizontalRadii);

  CSSValueList* verticalRadii = CSSValueList::createSpaceSeparated();
  verticalRadii->append(topLeftRadius->item(1));
  if (showVerticalTopRight)
    verticalRadii->append(topRightRadius->item(1));
  if (showVerticalBottomRight)
    verticalRadii->append(bottomRightRadius->item(1));
  if (showVerticalBottomLeft)
    verticalRadii->append(bottomLeftRadius->item(1));

  if (!verticalRadii->equals(toCSSValueList(list->item(0))))
    list->append(*verticalRadii);

  return list;
}

static CSSValue* strokeDashArrayToCSSValueList(const SVGDashArray& dashes,
                                               const ComputedStyle& style) {
  if (dashes.isEmpty())
    return CSSIdentifierValue::create(CSSValueNone);

  CSSValueList* list = CSSValueList::createCommaSeparated();
  for (const Length& dashLength : dashes.vector())
    list->append(*zoomAdjustedPixelValueForLength(dashLength, style));

  return list;
}

static CSSValue* paintOrderToCSSValueList(const SVGComputedStyle& svgStyle) {
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  for (int i = 0; i < 3; i++) {
    EPaintOrderType paintOrderType = svgStyle.paintOrderType(i);
    switch (paintOrderType) {
      case PT_FILL:
      case PT_STROKE:
      case PT_MARKERS:
        list->append(*CSSIdentifierValue::create(paintOrderType));
        break;
      case PT_NONE:
      default:
        ASSERT_NOT_REACHED();
        break;
    }
  }

  return list;
}

static CSSValue* adjustSVGPaintForCurrentColor(SVGPaintType paintType,
                                               const String& url,
                                               const Color& color,
                                               const Color& currentColor) {
  if (paintType >= SVG_PAINTTYPE_URI_NONE) {
    CSSValueList* values = CSSValueList::createSpaceSeparated();
    values->append(*CSSURIValue::create(url));
    if (paintType == SVG_PAINTTYPE_URI_NONE)
      values->append(*CSSIdentifierValue::create(CSSValueNone));
    else if (paintType == SVG_PAINTTYPE_URI_CURRENTCOLOR)
      values->append(*CSSColorValue::create(currentColor.rgb()));
    else if (paintType == SVG_PAINTTYPE_URI_RGBCOLOR)
      values->append(*CSSColorValue::create(color.rgb()));
    return values;
  }
  if (paintType == SVG_PAINTTYPE_NONE)
    return CSSIdentifierValue::create(CSSValueNone);
  if (paintType == SVG_PAINTTYPE_CURRENTCOLOR)
    return CSSColorValue::create(currentColor.rgb());

  return CSSColorValue::create(color.rgb());
}

static inline String serializeAsFragmentIdentifier(
    const AtomicString& resource) {
  return "#" + resource;
}

CSSValue* ComputedStyleCSSValueMapping::valueForShadowData(
    const ShadowData& shadow,
    const ComputedStyle& style,
    bool useSpread) {
  CSSPrimitiveValue* x = zoomAdjustedPixelValue(shadow.x(), style);
  CSSPrimitiveValue* y = zoomAdjustedPixelValue(shadow.y(), style);
  CSSPrimitiveValue* blur = zoomAdjustedPixelValue(shadow.blur(), style);
  CSSPrimitiveValue* spread =
      useSpread ? zoomAdjustedPixelValue(shadow.spread(), style) : nullptr;
  CSSIdentifierValue* shadowStyle =
      shadow.style() == Normal ? nullptr
                               : CSSIdentifierValue::create(CSSValueInset);
  CSSValue* color = currentColorOrValidColor(style, shadow.color());
  return CSSShadowValue::create(x, y, blur, spread, shadowStyle, color);
}

CSSValue* ComputedStyleCSSValueMapping::valueForShadowList(
    const ShadowList* shadowList,
    const ComputedStyle& style,
    bool useSpread) {
  if (!shadowList)
    return CSSIdentifierValue::create(CSSValueNone);

  CSSValueList* list = CSSValueList::createCommaSeparated();
  size_t shadowCount = shadowList->shadows().size();
  for (size_t i = 0; i < shadowCount; ++i)
    list->append(
        *valueForShadowData(shadowList->shadows()[i], style, useSpread));
  return list;
}

CSSValue* ComputedStyleCSSValueMapping::valueForFilter(
    const ComputedStyle& style,
    const FilterOperations& filterOperations) {
  if (filterOperations.operations().isEmpty())
    return CSSIdentifierValue::create(CSSValueNone);

  CSSValueList* list = CSSValueList::createSpaceSeparated();

  CSSFunctionValue* filterValue = nullptr;

  for (const auto& operation : filterOperations.operations()) {
    FilterOperation* filterOperation = operation.get();
    switch (filterOperation->type()) {
      case FilterOperation::REFERENCE:
        filterValue = CSSFunctionValue::create(CSSValueUrl);
        filterValue->append(*CSSStringValue::create(
            toReferenceFilterOperation(filterOperation)->url()));
        break;
      case FilterOperation::GRAYSCALE:
        filterValue = CSSFunctionValue::create(CSSValueGrayscale);
        filterValue->append(*CSSPrimitiveValue::create(
            toBasicColorMatrixFilterOperation(filterOperation)->amount(),
            CSSPrimitiveValue::UnitType::Number));
        break;
      case FilterOperation::SEPIA:
        filterValue = CSSFunctionValue::create(CSSValueSepia);
        filterValue->append(*CSSPrimitiveValue::create(
            toBasicColorMatrixFilterOperation(filterOperation)->amount(),
            CSSPrimitiveValue::UnitType::Number));
        break;
      case FilterOperation::SATURATE:
        filterValue = CSSFunctionValue::create(CSSValueSaturate);
        filterValue->append(*CSSPrimitiveValue::create(
            toBasicColorMatrixFilterOperation(filterOperation)->amount(),
            CSSPrimitiveValue::UnitType::Number));
        break;
      case FilterOperation::HUE_ROTATE:
        filterValue = CSSFunctionValue::create(CSSValueHueRotate);
        filterValue->append(*CSSPrimitiveValue::create(
            toBasicColorMatrixFilterOperation(filterOperation)->amount(),
            CSSPrimitiveValue::UnitType::Degrees));
        break;
      case FilterOperation::INVERT:
        filterValue = CSSFunctionValue::create(CSSValueInvert);
        filterValue->append(*CSSPrimitiveValue::create(
            toBasicComponentTransferFilterOperation(filterOperation)->amount(),
            CSSPrimitiveValue::UnitType::Number));
        break;
      case FilterOperation::OPACITY:
        filterValue = CSSFunctionValue::create(CSSValueOpacity);
        filterValue->append(*CSSPrimitiveValue::create(
            toBasicComponentTransferFilterOperation(filterOperation)->amount(),
            CSSPrimitiveValue::UnitType::Number));
        break;
      case FilterOperation::BRIGHTNESS:
        filterValue = CSSFunctionValue::create(CSSValueBrightness);
        filterValue->append(*CSSPrimitiveValue::create(
            toBasicComponentTransferFilterOperation(filterOperation)->amount(),
            CSSPrimitiveValue::UnitType::Number));
        break;
      case FilterOperation::CONTRAST:
        filterValue = CSSFunctionValue::create(CSSValueContrast);
        filterValue->append(*CSSPrimitiveValue::create(
            toBasicComponentTransferFilterOperation(filterOperation)->amount(),
            CSSPrimitiveValue::UnitType::Number));
        break;
      case FilterOperation::BLUR:
        filterValue = CSSFunctionValue::create(CSSValueBlur);
        filterValue->append(*zoomAdjustedPixelValue(
            toBlurFilterOperation(filterOperation)->stdDeviation().value(),
            style));
        break;
      case FilterOperation::DROP_SHADOW: {
        DropShadowFilterOperation* dropShadowOperation =
            toDropShadowFilterOperation(filterOperation);
        filterValue = CSSFunctionValue::create(CSSValueDropShadow);
        // We want our computed style to look like that of a text shadow (has
        // neither spread nor inset style).
        ShadowData shadow(dropShadowOperation->location(),
                          dropShadowOperation->stdDeviation(), 0, Normal,
                          StyleColor(dropShadowOperation->getColor()));
        filterValue->append(*valueForShadowData(shadow, style, false));
        break;
      }
      default:
        ASSERT_NOT_REACHED();
        break;
    }
    list->append(*filterValue);
  }

  return list;
}

CSSValue* ComputedStyleCSSValueMapping::valueForFont(
    const ComputedStyle& style) {
  // Add a slash between size and line-height.
  CSSValueList* sizeAndLineHeight = CSSValueList::createSlashSeparated();
  sizeAndLineHeight->append(*valueForFontSize(style));
  sizeAndLineHeight->append(*valueForLineHeight(style));

  CSSValueList* list = CSSValueList::createSpaceSeparated();
  list->append(*valueForFontStyle(style));

  // Check that non-initial font-variant subproperties are not conflicting with
  // this serialization.
  CSSValue* ligaturesValue = valueForFontVariantLigatures(style);
  CSSValue* numericValue = valueForFontVariantNumeric(style);
  if (!ligaturesValue->equals(*CSSIdentifierValue::create(CSSValueNormal)) ||
      !numericValue->equals(*CSSIdentifierValue::create(CSSValueNormal)))
    return nullptr;

  CSSIdentifierValue* capsValue = valueForFontVariantCaps(style);
  if (capsValue->getValueID() != CSSValueNormal &&
      capsValue->getValueID() != CSSValueSmallCaps)
    return nullptr;
  list->append(*capsValue);

  list->append(*valueForFontWeight(style));
  list->append(*valueForFontStretch(style));
  list->append(*sizeAndLineHeight);
  list->append(*valueForFontFamily(style));

  return list;
}

static CSSValue* valueForScrollSnapDestination(const LengthPoint& destination,
                                               const ComputedStyle& style) {
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  list->append(*zoomAdjustedPixelValueForLength(destination.x(), style));
  list->append(*zoomAdjustedPixelValueForLength(destination.y(), style));
  return list;
}

static CSSValue* valueForScrollSnapPoints(const ScrollSnapPoints& points,
                                          const ComputedStyle& style) {
  if (points.hasRepeat) {
    CSSFunctionValue* repeat = CSSFunctionValue::create(CSSValueRepeat);
    repeat->append(
        *zoomAdjustedPixelValueForLength(points.repeatOffset, style));
    return repeat;
  }

  return CSSIdentifierValue::create(CSSValueNone);
}

static CSSValue* valueForScrollSnapCoordinate(
    const Vector<LengthPoint>& coordinates,
    const ComputedStyle& style) {
  if (coordinates.isEmpty())
    return CSSIdentifierValue::create(CSSValueNone);

  CSSValueList* list = CSSValueList::createCommaSeparated();

  for (auto& coordinate : coordinates) {
    auto pair = CSSValueList::createSpaceSeparated();
    pair->append(*zoomAdjustedPixelValueForLength(coordinate.x(), style));
    pair->append(*zoomAdjustedPixelValueForLength(coordinate.y(), style));
    list->append(*pair);
  }

  return list;
}

static EBreak mapToPageBreakValue(EBreak genericBreakValue) {
  switch (genericBreakValue) {
    case BreakAvoidColumn:
    case BreakColumn:
    case BreakRecto:
    case BreakVerso:
      return BreakAuto;
    case BreakPage:
      return BreakAlways;
    case BreakAvoidPage:
      return BreakAvoid;
    default:
      return genericBreakValue;
  }
}

static EBreak mapToColumnBreakValue(EBreak genericBreakValue) {
  switch (genericBreakValue) {
    case BreakAvoidPage:
    case BreakLeft:
    case BreakPage:
    case BreakRecto:
    case BreakRight:
    case BreakVerso:
      return BreakAuto;
    case BreakColumn:
      return BreakAlways;
    case BreakAvoidColumn:
      return BreakAvoid;
    default:
      return genericBreakValue;
  }
}

const CSSValue* ComputedStyleCSSValueMapping::get(
    const AtomicString customPropertyName,
    const ComputedStyle& style,
    const PropertyRegistry* registry) {
  if (registry) {
    const PropertyRegistry::Registration* registration =
        registry->registration(customPropertyName);
    if (registration) {
      const CSSValue* result = nullptr;
      if (registration->inherits()) {
        if (StyleInheritedVariables* variables = style.inheritedVariables())
          result = variables->registeredVariable(customPropertyName);
      } else {
        if (StyleNonInheritedVariables* variables =
                style.nonInheritedVariables())
          result = variables->registeredVariable(customPropertyName);
      }
      if (result)
        return result;
      return registration->initial();
    }
  }

  StyleInheritedVariables* variables = style.inheritedVariables();
  if (!variables)
    return nullptr;

  CSSVariableData* data = variables->getVariable(customPropertyName);
  if (!data)
    return nullptr;

  return CSSCustomPropertyDeclaration::create(customPropertyName, data);
}

std::unique_ptr<HashMap<AtomicString, RefPtr<CSSVariableData>>>
ComputedStyleCSSValueMapping::getVariables(const ComputedStyle& style) {
  // TODO(timloh): Also return non-inherited variables
  StyleInheritedVariables* variables = style.inheritedVariables();
  if (variables)
    return variables->getVariables();
  return nullptr;
}

const CSSValue* ComputedStyleCSSValueMapping::get(
    CSSPropertyID propertyID,
    const ComputedStyle& style,
    const LayoutObject* layoutObject,
    Node* styledNode,
    bool allowVisitedStyle) {
  const SVGComputedStyle& svgStyle = style.svgStyle();
  propertyID = CSSProperty::resolveDirectionAwareProperty(
      propertyID, style.direction(), style.getWritingMode());
  switch (propertyID) {
    case CSSPropertyInvalid:
      return nullptr;

    case CSSPropertyBackgroundColor:
      return allowVisitedStyle
                 ? CSSColorValue::create(
                       style.visitedDependentColor(CSSPropertyBackgroundColor)
                           .rgb())
                 : currentColorOrValidColor(style, style.backgroundColor());
    case CSSPropertyBackgroundImage:
    case CSSPropertyWebkitMaskImage: {
      CSSValueList* list = CSSValueList::createCommaSeparated();
      const FillLayer* currLayer = propertyID == CSSPropertyWebkitMaskImage
                                       ? &style.maskLayers()
                                       : &style.backgroundLayers();
      for (; currLayer; currLayer = currLayer->next()) {
        if (currLayer->image())
          list->append(*currLayer->image()->computedCSSValue());
        else
          list->append(*CSSIdentifierValue::create(CSSValueNone));
      }
      return list;
    }
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitMaskSize: {
      CSSValueList* list = CSSValueList::createCommaSeparated();
      const FillLayer* currLayer = propertyID == CSSPropertyWebkitMaskSize
                                       ? &style.maskLayers()
                                       : &style.backgroundLayers();
      for (; currLayer; currLayer = currLayer->next())
        list->append(*valueForFillSize(currLayer->size(), style));
      return list;
    }
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyWebkitMaskRepeat: {
      CSSValueList* list = CSSValueList::createCommaSeparated();
      const FillLayer* currLayer = propertyID == CSSPropertyWebkitMaskRepeat
                                       ? &style.maskLayers()
                                       : &style.backgroundLayers();
      for (; currLayer; currLayer = currLayer->next())
        list->append(
            *valueForFillRepeat(currLayer->repeatX(), currLayer->repeatY()));
      return list;
    }
    case CSSPropertyMaskSourceType: {
      CSSValueList* list = CSSValueList::createCommaSeparated();
      for (const FillLayer* currLayer = &style.maskLayers(); currLayer;
           currLayer = currLayer->next())
        list->append(*valueForFillSourceType(currLayer->maskSourceType()));
      return list;
    }
    case CSSPropertyWebkitMaskComposite: {
      CSSValueList* list = CSSValueList::createCommaSeparated();
      const FillLayer* currLayer = propertyID == CSSPropertyWebkitMaskComposite
                                       ? &style.maskLayers()
                                       : &style.backgroundLayers();
      for (; currLayer; currLayer = currLayer->next())
        list->append(*CSSIdentifierValue::create(currLayer->composite()));
      return list;
    }
    case CSSPropertyBackgroundAttachment: {
      CSSValueList* list = CSSValueList::createCommaSeparated();
      for (const FillLayer* currLayer = &style.backgroundLayers(); currLayer;
           currLayer = currLayer->next())
        list->append(*CSSIdentifierValue::create(currLayer->attachment()));
      return list;
    }
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyWebkitMaskOrigin: {
      bool isClip = propertyID == CSSPropertyBackgroundClip ||
                    propertyID == CSSPropertyWebkitBackgroundClip ||
                    propertyID == CSSPropertyWebkitMaskClip;
      CSSValueList* list = CSSValueList::createCommaSeparated();
      const FillLayer* currLayer = (propertyID == CSSPropertyWebkitMaskClip ||
                                    propertyID == CSSPropertyWebkitMaskOrigin)
                                       ? &style.maskLayers()
                                       : &style.backgroundLayers();
      for (; currLayer; currLayer = currLayer->next()) {
        EFillBox box = isClip ? currLayer->clip() : currLayer->origin();
        list->append(*CSSIdentifierValue::create(box));
      }
      return list;
    }
    case CSSPropertyBackgroundPosition:
    case CSSPropertyWebkitMaskPosition: {
      CSSValueList* list = CSSValueList::createCommaSeparated();
      const FillLayer* currLayer = propertyID == CSSPropertyWebkitMaskPosition
                                       ? &style.maskLayers()
                                       : &style.backgroundLayers();
      for (; currLayer; currLayer = currLayer->next())
        list->append(
            *createPositionListForLayer(propertyID, *currLayer, style));
      return list;
    }
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyWebkitMaskPositionX: {
      CSSValueList* list = CSSValueList::createCommaSeparated();
      const FillLayer* currLayer = propertyID == CSSPropertyWebkitMaskPositionX
                                       ? &style.maskLayers()
                                       : &style.backgroundLayers();
      for (; currLayer; currLayer = currLayer->next())
        list->append(
            *zoomAdjustedPixelValueForLength(currLayer->xPosition(), style));
      return list;
    }
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitMaskPositionY: {
      CSSValueList* list = CSSValueList::createCommaSeparated();
      const FillLayer* currLayer = propertyID == CSSPropertyWebkitMaskPositionY
                                       ? &style.maskLayers()
                                       : &style.backgroundLayers();
      for (; currLayer; currLayer = currLayer->next())
        list->append(
            *zoomAdjustedPixelValueForLength(currLayer->yPosition(), style));
      return list;
    }
    case CSSPropertyBorderCollapse:
      if (style.borderCollapse())
        return CSSIdentifierValue::create(CSSValueCollapse);
      return CSSIdentifierValue::create(CSSValueSeparate);
    case CSSPropertyBorderSpacing: {
      CSSValueList* list = CSSValueList::createSpaceSeparated();
      list->append(
          *zoomAdjustedPixelValue(style.horizontalBorderSpacing(), style));
      list->append(
          *zoomAdjustedPixelValue(style.verticalBorderSpacing(), style));
      return list;
    }
    case CSSPropertyWebkitBorderHorizontalSpacing:
      return zoomAdjustedPixelValue(style.horizontalBorderSpacing(), style);
    case CSSPropertyWebkitBorderVerticalSpacing:
      return zoomAdjustedPixelValue(style.verticalBorderSpacing(), style);
    case CSSPropertyBorderImageSource:
      if (style.borderImageSource())
        return style.borderImageSource()->computedCSSValue();
      return CSSIdentifierValue::create(CSSValueNone);
    case CSSPropertyBorderTopColor:
      return allowVisitedStyle
                 ? CSSColorValue::create(
                       style.visitedDependentColor(CSSPropertyBorderTopColor)
                           .rgb())
                 : currentColorOrValidColor(style, style.borderTopColor());
    case CSSPropertyBorderRightColor:
      return allowVisitedStyle
                 ? CSSColorValue::create(
                       style.visitedDependentColor(CSSPropertyBorderRightColor)
                           .rgb())
                 : currentColorOrValidColor(style, style.borderRightColor());
    case CSSPropertyBorderBottomColor:
      return allowVisitedStyle
                 ? CSSColorValue::create(
                       style.visitedDependentColor(CSSPropertyBorderBottomColor)
                           .rgb())
                 : currentColorOrValidColor(style, style.borderBottomColor());
    case CSSPropertyBorderLeftColor:
      return allowVisitedStyle
                 ? CSSColorValue::create(
                       style.visitedDependentColor(CSSPropertyBorderLeftColor)
                           .rgb())
                 : currentColorOrValidColor(style, style.borderLeftColor());
    case CSSPropertyBorderTopStyle:
      return CSSIdentifierValue::create(style.borderTopStyle());
    case CSSPropertyBorderRightStyle:
      return CSSIdentifierValue::create(style.borderRightStyle());
    case CSSPropertyBorderBottomStyle:
      return CSSIdentifierValue::create(style.borderBottomStyle());
    case CSSPropertyBorderLeftStyle:
      return CSSIdentifierValue::create(style.borderLeftStyle());
    case CSSPropertyBorderTopWidth:
      return zoomAdjustedPixelValue(style.borderTopWidth(), style);
    case CSSPropertyBorderRightWidth:
      return zoomAdjustedPixelValue(style.borderRightWidth(), style);
    case CSSPropertyBorderBottomWidth:
      return zoomAdjustedPixelValue(style.borderBottomWidth(), style);
    case CSSPropertyBorderLeftWidth:
      return zoomAdjustedPixelValue(style.borderLeftWidth(), style);
    case CSSPropertyBottom:
      return valueForPositionOffset(style, CSSPropertyBottom, layoutObject);
    case CSSPropertyWebkitBoxAlign:
      return CSSIdentifierValue::create(style.boxAlign());
    case CSSPropertyWebkitBoxDecorationBreak:
      if (style.boxDecorationBreak() == BoxDecorationBreakSlice)
        return CSSIdentifierValue::create(CSSValueSlice);
      return CSSIdentifierValue::create(CSSValueClone);
    case CSSPropertyWebkitBoxDirection:
      return CSSIdentifierValue::create(style.boxDirection());
    case CSSPropertyWebkitBoxFlex:
      return CSSPrimitiveValue::create(style.boxFlex(),
                                       CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyWebkitBoxFlexGroup:
      return CSSPrimitiveValue::create(style.boxFlexGroup(),
                                       CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyWebkitBoxLines:
      return CSSIdentifierValue::create(style.boxLines());
    case CSSPropertyWebkitBoxOrdinalGroup:
      return CSSPrimitiveValue::create(style.boxOrdinalGroup(),
                                       CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyWebkitBoxOrient:
      return CSSIdentifierValue::create(style.boxOrient());
    case CSSPropertyWebkitBoxPack:
      return CSSIdentifierValue::create(style.boxPack());
    case CSSPropertyWebkitBoxReflect:
      return valueForReflection(style.boxReflect(), style);
    case CSSPropertyBoxShadow:
      return valueForShadowList(style.boxShadow(), style, true);
    case CSSPropertyCaptionSide:
      return CSSIdentifierValue::create(style.captionSide());
    case CSSPropertyClear:
      return CSSIdentifierValue::create(style.clear());
    case CSSPropertyColor:
      return CSSColorValue::create(
          allowVisitedStyle
              ? style.visitedDependentColor(CSSPropertyColor).rgb()
              : style.color().rgb());
    case CSSPropertyWebkitPrintColorAdjust:
      return CSSIdentifierValue::create(style.getPrintColorAdjust());
    case CSSPropertyColumnCount:
      if (style.hasAutoColumnCount())
        return CSSIdentifierValue::create(CSSValueAuto);
      return CSSPrimitiveValue::create(style.columnCount(),
                                       CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyColumnFill:
      return CSSIdentifierValue::create(style.getColumnFill());
    case CSSPropertyColumnGap:
      if (style.hasNormalColumnGap())
        return CSSIdentifierValue::create(CSSValueNormal);
      return zoomAdjustedPixelValue(style.columnGap(), style);
    case CSSPropertyColumnRuleColor:
      return allowVisitedStyle
                 ? CSSColorValue::create(
                       style.visitedDependentColor(CSSPropertyOutlineColor)
                           .rgb())
                 : currentColorOrValidColor(style, style.columnRuleColor());
    case CSSPropertyColumnRuleStyle:
      return CSSIdentifierValue::create(style.columnRuleStyle());
    case CSSPropertyColumnRuleWidth:
      return zoomAdjustedPixelValue(style.columnRuleWidth(), style);
    case CSSPropertyColumnSpan:
      return CSSIdentifierValue::create(style.getColumnSpan() ? CSSValueAll
                                                              : CSSValueNone);
    case CSSPropertyWebkitColumnBreakAfter:
      return CSSIdentifierValue::create(
          mapToColumnBreakValue(style.breakAfter()));
    case CSSPropertyWebkitColumnBreakBefore:
      return CSSIdentifierValue::create(
          mapToColumnBreakValue(style.breakBefore()));
    case CSSPropertyWebkitColumnBreakInside:
      return CSSIdentifierValue::create(
          mapToColumnBreakValue(style.breakInside()));
    case CSSPropertyColumnWidth:
      if (style.hasAutoColumnWidth())
        return CSSIdentifierValue::create(CSSValueAuto);
      return zoomAdjustedPixelValue(style.columnWidth(), style);
    case CSSPropertyTabSize:
      return CSSPrimitiveValue::create(
          style.getTabSize().getPixelSize(1.0),
          style.getTabSize().isSpaces() ? CSSPrimitiveValue::UnitType::Number
                                        : CSSPrimitiveValue::UnitType::Pixels);
    case CSSPropertyTextSizeAdjust:
      if (style.getTextSizeAdjust().isAuto())
        return CSSIdentifierValue::create(CSSValueAuto);
      return CSSPrimitiveValue::create(
          style.getTextSizeAdjust().multiplier() * 100,
          CSSPrimitiveValue::UnitType::Percentage);
    case CSSPropertyCursor: {
      CSSValueList* list = nullptr;
      CursorList* cursors = style.cursors();
      if (cursors && cursors->size() > 0) {
        list = CSSValueList::createCommaSeparated();
        for (unsigned i = 0; i < cursors->size(); ++i) {
          if (StyleImage* image = cursors->at(i).image())
            list->append(*CSSCursorImageValue::create(
                image->computedCSSValue(), cursors->at(i).hotSpotSpecified(),
                cursors->at(i).hotSpot()));
        }
      }
      CSSValue* value = CSSIdentifierValue::create(style.cursor());
      if (list) {
        list->append(*value);
        return list;
      }
      return value;
    }
    case CSSPropertyDirection:
      return CSSIdentifierValue::create(style.direction());
    case CSSPropertyDisplay:
      return CSSIdentifierValue::create(style.display());
    case CSSPropertyEmptyCells:
      return CSSIdentifierValue::create(style.emptyCells());
    case CSSPropertyAlignContent:
      return valueForContentPositionAndDistributionWithOverflowAlignment(
          style.alignContent(), CSSValueStretch);
    case CSSPropertyAlignItems:
      return valueForItemPositionWithOverflowAlignment(style.alignItems());
    case CSSPropertyAlignSelf:
      return valueForItemPositionWithOverflowAlignment(style.alignSelf());
    case CSSPropertyFlex:
      return valuesForShorthandProperty(flexShorthand(), style, layoutObject,
                                        styledNode, allowVisitedStyle);
    case CSSPropertyFlexBasis:
      return zoomAdjustedPixelValueForLength(style.flexBasis(), style);
    case CSSPropertyFlexDirection:
      return CSSIdentifierValue::create(style.flexDirection());
    case CSSPropertyFlexFlow:
      return valuesForShorthandProperty(flexFlowShorthand(), style,
                                        layoutObject, styledNode,
                                        allowVisitedStyle);
    case CSSPropertyFlexGrow:
      return CSSPrimitiveValue::create(style.flexGrow(),
                                       CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyFlexShrink:
      return CSSPrimitiveValue::create(style.flexShrink(),
                                       CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyFlexWrap:
      return CSSIdentifierValue::create(style.flexWrap());
    case CSSPropertyJustifyContent:
      return valueForContentPositionAndDistributionWithOverflowAlignment(
          style.justifyContent(), CSSValueFlexStart);
    case CSSPropertyOrder:
      return CSSPrimitiveValue::create(style.order(),
                                       CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyFloat:
      if (style.display() != EDisplay::None && style.hasOutOfFlowPosition())
        return CSSIdentifierValue::create(CSSValueNone);
      return CSSIdentifierValue::create(style.floating());
    case CSSPropertyFont:
      return valueForFont(style);
    case CSSPropertyFontFamily:
      return valueForFontFamily(style);
    case CSSPropertyFontSize:
      return valueForFontSize(style);
    case CSSPropertyFontSizeAdjust:
      if (style.hasFontSizeAdjust())
        return CSSPrimitiveValue::create(style.fontSizeAdjust(),
                                         CSSPrimitiveValue::UnitType::Number);
      return CSSIdentifierValue::create(CSSValueNone);
    case CSSPropertyFontStretch:
      return valueForFontStretch(style);
    case CSSPropertyFontStyle:
      return valueForFontStyle(style);
    case CSSPropertyFontVariant:
      return valuesForFontVariantProperty(style, layoutObject, styledNode,
                                          allowVisitedStyle);
    case CSSPropertyFontWeight:
      return valueForFontWeight(style);
    case CSSPropertyFontFeatureSettings: {
      const FontFeatureSettings* featureSettings =
          style.getFontDescription().featureSettings();
      if (!featureSettings || !featureSettings->size())
        return CSSIdentifierValue::create(CSSValueNormal);
      CSSValueList* list = CSSValueList::createCommaSeparated();
      for (unsigned i = 0; i < featureSettings->size(); ++i) {
        const FontFeature& feature = featureSettings->at(i);
        CSSFontFeatureValue* featureValue =
            CSSFontFeatureValue::create(feature.tag(), feature.value());
        list->append(*featureValue);
      }
      return list;
    }
    case CSSPropertyGridAutoFlow: {
      CSSValueList* list = CSSValueList::createSpaceSeparated();
      switch (style.getGridAutoFlow()) {
        case AutoFlowRow:
        case AutoFlowRowDense:
          list->append(*CSSIdentifierValue::create(CSSValueRow));
          break;
        case AutoFlowColumn:
        case AutoFlowColumnDense:
          list->append(*CSSIdentifierValue::create(CSSValueColumn));
          break;
        default:
          ASSERT_NOT_REACHED();
      }

      switch (style.getGridAutoFlow()) {
        case AutoFlowRowDense:
        case AutoFlowColumnDense:
          list->append(*CSSIdentifierValue::create(CSSValueDense));
          break;
        default:
          // Do nothing.
          break;
      }

      return list;
    }
    // Specs mention that getComputedStyle() should return the used value of the
    // property instead of the computed one for grid-template-{rows|columns} but
    // not for the grid-auto-{rows|columns} as things like grid-auto-columns:
    // 2fr; cannot be resolved to a value in pixels as the '2fr' means very
    // different things depending on the size of the explicit grid or the number
    // of implicit tracks added to the grid. See
    // http://lists.w3.org/Archives/Public/www-style/2013Nov/0014.html
    case CSSPropertyGridAutoColumns:
      return valueForGridTrackSizeList(ForColumns, style);
    case CSSPropertyGridAutoRows:
      return valueForGridTrackSizeList(ForRows, style);

    case CSSPropertyGridTemplateColumns:
      return valueForGridTrackList(ForColumns, layoutObject, style);
    case CSSPropertyGridTemplateRows:
      return valueForGridTrackList(ForRows, layoutObject, style);

    case CSSPropertyGridColumnStart:
      return valueForGridPosition(style.gridColumnStart());
    case CSSPropertyGridColumnEnd:
      return valueForGridPosition(style.gridColumnEnd());
    case CSSPropertyGridRowStart:
      return valueForGridPosition(style.gridRowStart());
    case CSSPropertyGridRowEnd:
      return valueForGridPosition(style.gridRowEnd());
    case CSSPropertyGridColumn:
      return valuesForGridShorthand(gridColumnShorthand(), style, layoutObject,
                                    styledNode, allowVisitedStyle);
    case CSSPropertyGridRow:
      return valuesForGridShorthand(gridRowShorthand(), style, layoutObject,
                                    styledNode, allowVisitedStyle);
    case CSSPropertyGridArea:
      return valuesForGridShorthand(gridAreaShorthand(), style, layoutObject,
                                    styledNode, allowVisitedStyle);
    case CSSPropertyGridTemplate:
      return valuesForGridShorthand(gridTemplateShorthand(), style,
                                    layoutObject, styledNode,
                                    allowVisitedStyle);
    case CSSPropertyGrid:
      return valuesForGridShorthand(gridShorthand(), style, layoutObject,
                                    styledNode, allowVisitedStyle);
    case CSSPropertyGridTemplateAreas:
      if (!style.namedGridAreaRowCount()) {
        ASSERT(!style.namedGridAreaColumnCount());
        return CSSIdentifierValue::create(CSSValueNone);
      }

      return CSSGridTemplateAreasValue::create(
          style.namedGridArea(), style.namedGridAreaRowCount(),
          style.namedGridAreaColumnCount());
    case CSSPropertyGridColumnGap:
      return zoomAdjustedPixelValueForLength(style.gridColumnGap(), style);
    case CSSPropertyGridRowGap:
      return zoomAdjustedPixelValueForLength(style.gridRowGap(), style);
    case CSSPropertyGridGap:
      return valuesForShorthandProperty(gridGapShorthand(), style, layoutObject,
                                        styledNode, allowVisitedStyle);

    case CSSPropertyHeight:
      if (layoutObject) {
        // According to
        // http://www.w3.org/TR/CSS2/visudet.html#the-height-property, the
        // "height" property does not apply for non-atomic inline elements.
        if (!layoutObject->isAtomicInlineLevel() && layoutObject->isInline())
          return CSSIdentifierValue::create(CSSValueAuto);
        return zoomAdjustedPixelValue(sizingBox(layoutObject).height(), style);
      }
      return zoomAdjustedPixelValueForLength(style.height(), style);
    case CSSPropertyWebkitHighlight:
      if (style.highlight() == nullAtom)
        return CSSIdentifierValue::create(CSSValueNone);
      return CSSStringValue::create(style.highlight());
    case CSSPropertyHyphens:
      return CSSIdentifierValue::create(style.getHyphens());
    case CSSPropertyWebkitHyphenateCharacter:
      if (style.hyphenationString().isNull())
        return CSSIdentifierValue::create(CSSValueAuto);
      return CSSStringValue::create(style.hyphenationString());
    case CSSPropertyImageRendering:
      return CSSIdentifierValue::create(style.imageRendering());
    case CSSPropertyImageOrientation:
      if (style.respectImageOrientation() == RespectImageOrientation)
        return CSSIdentifierValue::create(CSSValueFromImage);
      return CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Degrees);
    case CSSPropertyIsolation:
      return CSSIdentifierValue::create(style.isolation());
    case CSSPropertyJustifyItems:
      return valueForItemPositionWithOverflowAlignment(style.justifyItems());
    case CSSPropertyJustifySelf:
      return valueForItemPositionWithOverflowAlignment(style.justifySelf());
    case CSSPropertyLeft:
      return valueForPositionOffset(style, CSSPropertyLeft, layoutObject);
    case CSSPropertyLetterSpacing:
      if (!style.letterSpacing())
        return CSSIdentifierValue::create(CSSValueNormal);
      return zoomAdjustedPixelValue(style.letterSpacing(), style);
    case CSSPropertyWebkitLineClamp:
      if (style.lineClamp().isNone())
        return CSSIdentifierValue::create(CSSValueNone);
      return CSSPrimitiveValue::create(
          style.lineClamp().value(),
          style.lineClamp().isPercentage()
              ? CSSPrimitiveValue::UnitType::Percentage
              : CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyLineHeight:
      return valueForLineHeight(style);
    case CSSPropertyListStyleImage:
      if (style.listStyleImage())
        return style.listStyleImage()->computedCSSValue();
      return CSSIdentifierValue::create(CSSValueNone);
    case CSSPropertyListStylePosition:
      return CSSIdentifierValue::create(style.listStylePosition());
    case CSSPropertyListStyleType:
      return CSSIdentifierValue::create(style.listStyleType());
    case CSSPropertyWebkitLocale:
      if (style.locale().isNull())
        return CSSIdentifierValue::create(CSSValueAuto);
      return CSSStringValue::create(style.locale());
    case CSSPropertyMarginTop: {
      Length marginTop = style.marginTop();
      if (marginTop.isFixed() || !layoutObject || !layoutObject->isBox())
        return zoomAdjustedPixelValueForLength(marginTop, style);
      return zoomAdjustedPixelValue(toLayoutBox(layoutObject)->marginTop(),
                                    style);
    }
    case CSSPropertyMarginRight: {
      Length marginRight = style.marginRight();
      if (marginRight.isFixed() || !layoutObject || !layoutObject->isBox())
        return zoomAdjustedPixelValueForLength(marginRight, style);
      float value;
      if (marginRight.isPercentOrCalc()) {
        // LayoutBox gives a marginRight() that is the distance between the
        // right-edge of the child box and the right-edge of the containing box,
        // when display == EDisplay::Block. Let's calculate the absolute value
        // of the specified margin-right % instead of relying on LayoutBox's
        // marginRight() value.
        value = minimumValueForLength(
                    marginRight, toLayoutBox(layoutObject)
                                     ->containingBlockLogicalWidthForContent())
                    .toFloat();
      } else {
        value = toLayoutBox(layoutObject)->marginRight().toFloat();
      }
      return zoomAdjustedPixelValue(value, style);
    }
    case CSSPropertyMarginBottom: {
      Length marginBottom = style.marginBottom();
      if (marginBottom.isFixed() || !layoutObject || !layoutObject->isBox())
        return zoomAdjustedPixelValueForLength(marginBottom, style);
      return zoomAdjustedPixelValue(toLayoutBox(layoutObject)->marginBottom(),
                                    style);
    }
    case CSSPropertyMarginLeft: {
      Length marginLeft = style.marginLeft();
      if (marginLeft.isFixed() || !layoutObject || !layoutObject->isBox())
        return zoomAdjustedPixelValueForLength(marginLeft, style);
      return zoomAdjustedPixelValue(toLayoutBox(layoutObject)->marginLeft(),
                                    style);
    }
    case CSSPropertyWebkitUserModify:
      return CSSIdentifierValue::create(style.userModify());
    case CSSPropertyMaxHeight: {
      const Length& maxHeight = style.maxHeight();
      if (maxHeight.isMaxSizeNone())
        return CSSIdentifierValue::create(CSSValueNone);
      return zoomAdjustedPixelValueForLength(maxHeight, style);
    }
    case CSSPropertyMaxWidth: {
      const Length& maxWidth = style.maxWidth();
      if (maxWidth.isMaxSizeNone())
        return CSSIdentifierValue::create(CSSValueNone);
      return zoomAdjustedPixelValueForLength(maxWidth, style);
    }
    case CSSPropertyMinHeight:
      if (style.minHeight().isAuto()) {
        Node* parent = styledNode->parentNode();
        if (isFlexOrGrid(parent ? parent->ensureComputedStyle() : nullptr))
          return CSSIdentifierValue::create(CSSValueAuto);
        return zoomAdjustedPixelValue(0, style);
      }
      return zoomAdjustedPixelValueForLength(style.minHeight(), style);
    case CSSPropertyMinWidth:
      if (style.minWidth().isAuto()) {
        Node* parent = styledNode->parentNode();
        if (isFlexOrGrid(parent ? parent->ensureComputedStyle() : nullptr))
          return CSSIdentifierValue::create(CSSValueAuto);
        return zoomAdjustedPixelValue(0, style);
      }
      return zoomAdjustedPixelValueForLength(style.minWidth(), style);
    case CSSPropertyObjectFit:
      return CSSIdentifierValue::create(style.getObjectFit());
    case CSSPropertyObjectPosition:
      return CSSValuePair::create(
          zoomAdjustedPixelValueForLength(style.objectPosition().x(), style),
          zoomAdjustedPixelValueForLength(style.objectPosition().y(), style),
          CSSValuePair::KeepIdenticalValues);
    case CSSPropertyOpacity:
      return CSSPrimitiveValue::create(style.opacity(),
                                       CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyOrphans:
      return CSSPrimitiveValue::create(style.orphans(),
                                       CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyOutlineColor:
      return allowVisitedStyle
                 ? CSSColorValue::create(
                       style.visitedDependentColor(CSSPropertyOutlineColor)
                           .rgb())
                 : currentColorOrValidColor(style, style.outlineColor());
    case CSSPropertyOutlineOffset:
      return zoomAdjustedPixelValue(style.outlineOffset(), style);
    case CSSPropertyOutlineStyle:
      if (style.outlineStyleIsAuto())
        return CSSIdentifierValue::create(CSSValueAuto);
      return CSSIdentifierValue::create(style.outlineStyle());
    case CSSPropertyOutlineWidth:
      return zoomAdjustedPixelValue(style.outlineWidth(), style);
    case CSSPropertyOverflow:
      return CSSIdentifierValue::create(
          max(style.overflowX(), style.overflowY()));
    case CSSPropertyOverflowAnchor:
      return CSSIdentifierValue::create(style.overflowAnchor());
    case CSSPropertyOverflowWrap:
      return CSSIdentifierValue::create(style.overflowWrap());
    case CSSPropertyOverflowX:
      return CSSIdentifierValue::create(style.overflowX());
    case CSSPropertyOverflowY:
      return CSSIdentifierValue::create(style.overflowY());
    case CSSPropertyPaddingTop: {
      Length paddingTop = style.paddingTop();
      if (paddingTop.isFixed() || !layoutObject || !layoutObject->isBox())
        return zoomAdjustedPixelValueForLength(paddingTop, style);
      return zoomAdjustedPixelValue(
          toLayoutBox(layoutObject)->computedCSSPaddingTop(), style);
    }
    case CSSPropertyPaddingRight: {
      Length paddingRight = style.paddingRight();
      if (paddingRight.isFixed() || !layoutObject || !layoutObject->isBox())
        return zoomAdjustedPixelValueForLength(paddingRight, style);
      return zoomAdjustedPixelValue(
          toLayoutBox(layoutObject)->computedCSSPaddingRight(), style);
    }
    case CSSPropertyPaddingBottom: {
      Length paddingBottom = style.paddingBottom();
      if (paddingBottom.isFixed() || !layoutObject || !layoutObject->isBox())
        return zoomAdjustedPixelValueForLength(paddingBottom, style);
      return zoomAdjustedPixelValue(
          toLayoutBox(layoutObject)->computedCSSPaddingBottom(), style);
    }
    case CSSPropertyPaddingLeft: {
      Length paddingLeft = style.paddingLeft();
      if (paddingLeft.isFixed() || !layoutObject || !layoutObject->isBox())
        return zoomAdjustedPixelValueForLength(paddingLeft, style);
      return zoomAdjustedPixelValue(
          toLayoutBox(layoutObject)->computedCSSPaddingLeft(), style);
    }
    case CSSPropertyBreakAfter:
      return CSSIdentifierValue::create(style.breakAfter());
    case CSSPropertyBreakBefore:
      return CSSIdentifierValue::create(style.breakBefore());
    case CSSPropertyBreakInside:
      return CSSIdentifierValue::create(style.breakInside());
    case CSSPropertyPageBreakAfter:
      return CSSIdentifierValue::create(
          mapToPageBreakValue(style.breakAfter()));
    case CSSPropertyPageBreakBefore:
      return CSSIdentifierValue::create(
          mapToPageBreakValue(style.breakBefore()));
    case CSSPropertyPageBreakInside:
      return CSSIdentifierValue::create(
          mapToPageBreakValue(style.breakInside()));
    case CSSPropertyPosition:
      return CSSIdentifierValue::create(style.position());
    case CSSPropertyQuotes:
      if (!style.quotes()) {
        // TODO(ramya.v): We should return the quote values that we're actually
        // using.
        return nullptr;
      }
      if (style.quotes()->size()) {
        CSSValueList* list = CSSValueList::createSpaceSeparated();
        for (int i = 0; i < style.quotes()->size(); i++) {
          list->append(
              *CSSStringValue::create(style.quotes()->getOpenQuote(i)));
          list->append(
              *CSSStringValue::create(style.quotes()->getCloseQuote(i)));
        }
        return list;
      }
      return CSSIdentifierValue::create(CSSValueNone);
    case CSSPropertyRight:
      return valueForPositionOffset(style, CSSPropertyRight, layoutObject);
    case CSSPropertyWebkitRubyPosition:
      return CSSIdentifierValue::create(style.getRubyPosition());
    case CSSPropertyScrollBehavior:
      return CSSIdentifierValue::create(style.getScrollBehavior());
    case CSSPropertyTableLayout:
      return CSSIdentifierValue::create(style.tableLayout());
    case CSSPropertyTextAlign:
      return CSSIdentifierValue::create(style.textAlign());
    case CSSPropertyTextAlignLast:
      return CSSIdentifierValue::create(style.getTextAlignLast());
    case CSSPropertyTextDecoration:
      if (RuntimeEnabledFeatures::css3TextDecorationsEnabled())
        return valuesForShorthandProperty(textDecorationShorthand(), style,
                                          layoutObject, styledNode,
                                          allowVisitedStyle);
    // Fall through.
    case CSSPropertyTextDecorationLine:
      return renderTextDecorationFlagsToCSSValue(style.getTextDecoration());
    case CSSPropertyTextDecorationSkip:
      return valueForTextDecorationSkip(style.getTextDecorationSkip());
    case CSSPropertyTextDecorationStyle:
      return valueForTextDecorationStyle(style.getTextDecorationStyle());
    case CSSPropertyTextDecorationColor:
      return currentColorOrValidColor(style, style.textDecorationColor());
    case CSSPropertyTextJustify:
      return CSSIdentifierValue::create(style.getTextJustify());
    case CSSPropertyTextUnderlinePosition:
      return CSSIdentifierValue::create(style.getTextUnderlinePosition());
    case CSSPropertyWebkitTextDecorationsInEffect:
      return renderTextDecorationFlagsToCSSValue(
          style.textDecorationsInEffect());
    case CSSPropertyWebkitTextFillColor:
      return currentColorOrValidColor(style, style.textFillColor());
    case CSSPropertyWebkitTextEmphasisColor:
      return currentColorOrValidColor(style, style.textEmphasisColor());
    case CSSPropertyWebkitTextEmphasisPosition:
      return CSSIdentifierValue::create(style.getTextEmphasisPosition());
    case CSSPropertyWebkitTextEmphasisStyle:
      switch (style.getTextEmphasisMark()) {
        case TextEmphasisMarkNone:
          return CSSIdentifierValue::create(CSSValueNone);
        case TextEmphasisMarkCustom:
          return CSSStringValue::create(style.textEmphasisCustomMark());
        case TextEmphasisMarkAuto:
          ASSERT_NOT_REACHED();
        // Fall through
        case TextEmphasisMarkDot:
        case TextEmphasisMarkCircle:
        case TextEmphasisMarkDoubleCircle:
        case TextEmphasisMarkTriangle:
        case TextEmphasisMarkSesame: {
          CSSValueList* list = CSSValueList::createSpaceSeparated();
          list->append(
              *CSSIdentifierValue::create(style.getTextEmphasisFill()));
          list->append(
              *CSSIdentifierValue::create(style.getTextEmphasisMark()));
          return list;
        }
      }
    case CSSPropertyTextIndent: {
      CSSValueList* list = CSSValueList::createSpaceSeparated();
      list->append(*zoomAdjustedPixelValueForLength(style.textIndent(), style));
      if (RuntimeEnabledFeatures::css3TextEnabled() &&
          (style.getTextIndentLine() == TextIndentEachLine ||
           style.getTextIndentType() == TextIndentHanging)) {
        if (style.getTextIndentLine() == TextIndentEachLine)
          list->append(*CSSIdentifierValue::create(CSSValueEachLine));
        if (style.getTextIndentType() == TextIndentHanging)
          list->append(*CSSIdentifierValue::create(CSSValueHanging));
      }
      return list;
    }
    case CSSPropertyTextShadow:
      return valueForShadowList(style.textShadow(), style, false);
    case CSSPropertyTextRendering:
      return CSSIdentifierValue::create(
          style.getFontDescription().textRendering());
    case CSSPropertyTextOverflow:
      if (style.getTextOverflow())
        return CSSIdentifierValue::create(CSSValueEllipsis);
      return CSSIdentifierValue::create(CSSValueClip);
    case CSSPropertyWebkitTextSecurity:
      return CSSIdentifierValue::create(style.textSecurity());
    case CSSPropertyWebkitTextStrokeColor:
      return currentColorOrValidColor(style, style.textStrokeColor());
    case CSSPropertyWebkitTextStrokeWidth:
      return zoomAdjustedPixelValue(style.textStrokeWidth(), style);
    case CSSPropertyTextTransform:
      return CSSIdentifierValue::create(style.textTransform());
    case CSSPropertyTop:
      return valueForPositionOffset(style, CSSPropertyTop, layoutObject);
    case CSSPropertyTouchAction:
      return touchActionFlagsToCSSValue(style.getTouchAction());
    case CSSPropertyUnicodeBidi:
      return CSSIdentifierValue::create(style.unicodeBidi());
    case CSSPropertyVerticalAlign:
      switch (style.verticalAlign()) {
        case VerticalAlignBaseline:
          return CSSIdentifierValue::create(CSSValueBaseline);
        case VerticalAlignMiddle:
          return CSSIdentifierValue::create(CSSValueMiddle);
        case VerticalAlignSub:
          return CSSIdentifierValue::create(CSSValueSub);
        case VerticalAlignSuper:
          return CSSIdentifierValue::create(CSSValueSuper);
        case VerticalAlignTextTop:
          return CSSIdentifierValue::create(CSSValueTextTop);
        case VerticalAlignTextBottom:
          return CSSIdentifierValue::create(CSSValueTextBottom);
        case VerticalAlignTop:
          return CSSIdentifierValue::create(CSSValueTop);
        case VerticalAlignBottom:
          return CSSIdentifierValue::create(CSSValueBottom);
        case VerticalAlignBaselineMiddle:
          return CSSIdentifierValue::create(CSSValueWebkitBaselineMiddle);
        case VerticalAlignLength:
          return zoomAdjustedPixelValueForLength(style.getVerticalAlignLength(),
                                                 style);
      }
      ASSERT_NOT_REACHED();
      return nullptr;
    case CSSPropertyVisibility:
      return CSSIdentifierValue::create(style.visibility());
    case CSSPropertyWhiteSpace:
      return CSSIdentifierValue::create(style.whiteSpace());
    case CSSPropertyWidows:
      return CSSPrimitiveValue::create(style.widows(),
                                       CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyWidth:
      if (layoutObject) {
        // According to
        // http://www.w3.org/TR/CSS2/visudet.html#the-width-property,
        // the "width" property does not apply for non-atomic inline elements.
        if (!layoutObject->isAtomicInlineLevel() && layoutObject->isInline())
          return CSSIdentifierValue::create(CSSValueAuto);
        return zoomAdjustedPixelValue(sizingBox(layoutObject).width(), style);
      }
      return zoomAdjustedPixelValueForLength(style.width(), style);
    case CSSPropertyWillChange:
      return valueForWillChange(style.willChangeProperties(),
                                style.willChangeContents(),
                                style.willChangeScrollPosition());
    case CSSPropertyWordBreak:
      return CSSIdentifierValue::create(style.wordBreak());
    case CSSPropertyWordSpacing:
      return zoomAdjustedPixelValue(style.wordSpacing(), style);
    case CSSPropertyWordWrap:
      return CSSIdentifierValue::create(style.overflowWrap());
    case CSSPropertyWebkitLineBreak:
      return CSSIdentifierValue::create(style.getLineBreak());
    case CSSPropertyResize:
      return CSSIdentifierValue::create(style.resize());
    case CSSPropertyFontKerning:
      return CSSIdentifierValue::create(
          style.getFontDescription().getKerning());
    case CSSPropertyWebkitFontSmoothing:
      return CSSIdentifierValue::create(
          style.getFontDescription().fontSmoothing());
    case CSSPropertyFontVariantLigatures:
      return valueForFontVariantLigatures(style);
    case CSSPropertyFontVariantCaps:
      return valueForFontVariantCaps(style);
    case CSSPropertyFontVariantNumeric:
      return valueForFontVariantNumeric(style);
    case CSSPropertyZIndex:
      if (style.hasAutoZIndex() || !style.isStackingContext())
        return CSSIdentifierValue::create(CSSValueAuto);
      return CSSPrimitiveValue::create(style.zIndex(),
                                       CSSPrimitiveValue::UnitType::Integer);
    case CSSPropertyZoom:
      return CSSPrimitiveValue::create(style.zoom(),
                                       CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyBoxSizing:
      if (style.boxSizing() == BoxSizingContentBox)
        return CSSIdentifierValue::create(CSSValueContentBox);
      return CSSIdentifierValue::create(CSSValueBorderBox);
    case CSSPropertyWebkitAppRegion:
      return CSSIdentifierValue::create(style.getDraggableRegionMode() ==
                                                DraggableRegionDrag
                                            ? CSSValueDrag
                                            : CSSValueNoDrag);
    case CSSPropertyAnimationDelay:
      return valueForAnimationDelay(style.animations());
    case CSSPropertyAnimationDirection: {
      CSSValueList* list = CSSValueList::createCommaSeparated();
      const CSSAnimationData* animationData = style.animations();
      if (animationData) {
        for (size_t i = 0; i < animationData->directionList().size(); ++i)
          list->append(
              *valueForAnimationDirection(animationData->directionList()[i]));
      } else {
        list->append(*CSSIdentifierValue::create(CSSValueNormal));
      }
      return list;
    }
    case CSSPropertyAnimationDuration:
      return valueForAnimationDuration(style.animations());
    case CSSPropertyAnimationFillMode: {
      CSSValueList* list = CSSValueList::createCommaSeparated();
      const CSSAnimationData* animationData = style.animations();
      if (animationData) {
        for (size_t i = 0; i < animationData->fillModeList().size(); ++i)
          list->append(
              *valueForAnimationFillMode(animationData->fillModeList()[i]));
      } else {
        list->append(*CSSIdentifierValue::create(CSSValueNone));
      }
      return list;
    }
    case CSSPropertyAnimationIterationCount: {
      CSSValueList* list = CSSValueList::createCommaSeparated();
      const CSSAnimationData* animationData = style.animations();
      if (animationData) {
        for (size_t i = 0; i < animationData->iterationCountList().size(); ++i)
          list->append(*valueForAnimationIterationCount(
              animationData->iterationCountList()[i]));
      } else {
        list->append(*CSSPrimitiveValue::create(
            CSSAnimationData::initialIterationCount(),
            CSSPrimitiveValue::UnitType::Number));
      }
      return list;
    }
    case CSSPropertyAnimationName: {
      CSSValueList* list = CSSValueList::createCommaSeparated();
      const CSSAnimationData* animationData = style.animations();
      if (animationData) {
        for (size_t i = 0; i < animationData->nameList().size(); ++i)
          list->append(
              *CSSCustomIdentValue::create(animationData->nameList()[i]));
      } else {
        list->append(*CSSIdentifierValue::create(CSSValueNone));
      }
      return list;
    }
    case CSSPropertyAnimationPlayState: {
      CSSValueList* list = CSSValueList::createCommaSeparated();
      const CSSAnimationData* animationData = style.animations();
      if (animationData) {
        for (size_t i = 0; i < animationData->playStateList().size(); ++i)
          list->append(
              *valueForAnimationPlayState(animationData->playStateList()[i]));
      } else {
        list->append(*CSSIdentifierValue::create(CSSValueRunning));
      }
      return list;
    }
    case CSSPropertyAnimationTimingFunction:
      return valueForAnimationTimingFunction(style.animations());
    case CSSPropertyAnimation: {
      const CSSAnimationData* animationData = style.animations();
      if (animationData) {
        CSSValueList* animationsList = CSSValueList::createCommaSeparated();
        for (size_t i = 0; i < animationData->nameList().size(); ++i) {
          CSSValueList* list = CSSValueList::createSpaceSeparated();
          list->append(
              *CSSCustomIdentValue::create(animationData->nameList()[i]));
          list->append(*CSSPrimitiveValue::create(
              CSSTimingData::getRepeated(animationData->durationList(), i),
              CSSPrimitiveValue::UnitType::Seconds));
          list->append(*createTimingFunctionValue(
              CSSTimingData::getRepeated(animationData->timingFunctionList(), i)
                  .get()));
          list->append(*CSSPrimitiveValue::create(
              CSSTimingData::getRepeated(animationData->delayList(), i),
              CSSPrimitiveValue::UnitType::Seconds));
          list->append(
              *valueForAnimationIterationCount(CSSTimingData::getRepeated(
                  animationData->iterationCountList(), i)));
          list->append(*valueForAnimationDirection(
              CSSTimingData::getRepeated(animationData->directionList(), i)));
          list->append(*valueForAnimationFillMode(
              CSSTimingData::getRepeated(animationData->fillModeList(), i)));
          list->append(*valueForAnimationPlayState(
              CSSTimingData::getRepeated(animationData->playStateList(), i)));
          animationsList->append(*list);
        }
        return animationsList;
      }

      CSSValueList* list = CSSValueList::createSpaceSeparated();
      // animation-name default value.
      list->append(*CSSIdentifierValue::create(CSSValueNone));
      list->append(
          *CSSPrimitiveValue::create(CSSAnimationData::initialDuration(),
                                     CSSPrimitiveValue::UnitType::Seconds));
      list->append(*createTimingFunctionValue(
          CSSAnimationData::initialTimingFunction().get()));
      list->append(
          *CSSPrimitiveValue::create(CSSAnimationData::initialDelay(),
                                     CSSPrimitiveValue::UnitType::Seconds));
      list->append(
          *CSSPrimitiveValue::create(CSSAnimationData::initialIterationCount(),
                                     CSSPrimitiveValue::UnitType::Number));
      list->append(
          *valueForAnimationDirection(CSSAnimationData::initialDirection()));
      list->append(
          *valueForAnimationFillMode(CSSAnimationData::initialFillMode()));
      // Initial animation-play-state.
      list->append(*CSSIdentifierValue::create(CSSValueRunning));
      return list;
    }
    case CSSPropertyWebkitAppearance:
      return CSSIdentifierValue::create(style.appearance());
    case CSSPropertyBackfaceVisibility:
      return CSSIdentifierValue::create(
          (style.backfaceVisibility() == BackfaceVisibilityHidden)
              ? CSSValueHidden
              : CSSValueVisible);
    case CSSPropertyWebkitBorderImage:
      return valueForNinePieceImage(style.borderImage(), style);
    case CSSPropertyBorderImageOutset:
      return valueForNinePieceImageQuad(style.borderImage().outset(), style);
    case CSSPropertyBorderImageRepeat:
      return valueForNinePieceImageRepeat(style.borderImage());
    case CSSPropertyBorderImageSlice:
      return valueForNinePieceImageSlice(style.borderImage());
    case CSSPropertyBorderImageWidth:
      return valueForNinePieceImageQuad(style.borderImage().borderSlices(),
                                        style);
    case CSSPropertyWebkitMaskBoxImage:
      return valueForNinePieceImage(style.maskBoxImage(), style);
    case CSSPropertyWebkitMaskBoxImageOutset:
      return valueForNinePieceImageQuad(style.maskBoxImage().outset(), style);
    case CSSPropertyWebkitMaskBoxImageRepeat:
      return valueForNinePieceImageRepeat(style.maskBoxImage());
    case CSSPropertyWebkitMaskBoxImageSlice:
      return valueForNinePieceImageSlice(style.maskBoxImage());
    case CSSPropertyWebkitMaskBoxImageWidth:
      return valueForNinePieceImageQuad(style.maskBoxImage().borderSlices(),
                                        style);
    case CSSPropertyWebkitMaskBoxImageSource:
      if (style.maskBoxImageSource())
        return style.maskBoxImageSource()->computedCSSValue();
      return CSSIdentifierValue::create(CSSValueNone);
    case CSSPropertyWebkitFontSizeDelta:
      // Not a real style property -- used by the editing engine -- so has no
      // computed value.
      return nullptr;
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginAfterCollapse:
      return CSSIdentifierValue::create(style.marginAfterCollapse());
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyWebkitMarginBeforeCollapse:
      return CSSIdentifierValue::create(style.marginBeforeCollapse());
    case CSSPropertyPerspective:
      if (!style.hasPerspective())
        return CSSIdentifierValue::create(CSSValueNone);
      return zoomAdjustedPixelValue(style.perspective(), style);
    case CSSPropertyPerspectiveOrigin: {
      CSSValueList* list = CSSValueList::createSpaceSeparated();
      if (layoutObject) {
        LayoutRect box;
        if (layoutObject->isBox())
          box = toLayoutBox(layoutObject)->borderBoxRect();

        list->append(*zoomAdjustedPixelValue(
            minimumValueForLength(style.perspectiveOriginX(), box.width()),
            style));
        list->append(*zoomAdjustedPixelValue(
            minimumValueForLength(style.perspectiveOriginY(), box.height()),
            style));
      } else {
        list->append(*zoomAdjustedPixelValueForLength(
            style.perspectiveOriginX(), style));
        list->append(*zoomAdjustedPixelValueForLength(
            style.perspectiveOriginY(), style));
      }
      return list;
    }
    case CSSPropertyWebkitRtlOrdering:
      return CSSIdentifierValue::create(style.rtlOrdering() ? CSSValueVisual
                                                            : CSSValueLogical);
    case CSSPropertyWebkitTapHighlightColor:
      return currentColorOrValidColor(style, style.tapHighlightColor());
    case CSSPropertyWebkitUserDrag:
      return CSSIdentifierValue::create(style.userDrag());
    case CSSPropertyUserSelect:
      return CSSIdentifierValue::create(style.userSelect());
    case CSSPropertyBorderBottomLeftRadius:
      return &valueForBorderRadiusCorner(style.borderBottomLeftRadius(), style);
    case CSSPropertyBorderBottomRightRadius:
      return &valueForBorderRadiusCorner(style.borderBottomRightRadius(),
                                         style);
    case CSSPropertyBorderTopLeftRadius:
      return &valueForBorderRadiusCorner(style.borderTopLeftRadius(), style);
    case CSSPropertyBorderTopRightRadius:
      return &valueForBorderRadiusCorner(style.borderTopRightRadius(), style);
    case CSSPropertyClip: {
      if (style.hasAutoClip())
        return CSSIdentifierValue::create(CSSValueAuto);
      CSSValue* top = zoomAdjustedPixelValueOrAuto(style.clip().top(), style);
      CSSValue* right =
          zoomAdjustedPixelValueOrAuto(style.clip().right(), style);
      CSSValue* bottom =
          zoomAdjustedPixelValueOrAuto(style.clip().bottom(), style);
      CSSValue* left = zoomAdjustedPixelValueOrAuto(style.clip().left(), style);
      return CSSQuadValue::create(top, right, bottom, left,
                                  CSSQuadValue::SerializeAsRect);
    }
    case CSSPropertySpeak:
      return CSSIdentifierValue::create(style.speak());
    case CSSPropertyTransform:
      return computedTransform(layoutObject, style);
    case CSSPropertyTransformOrigin: {
      CSSValueList* list = CSSValueList::createSpaceSeparated();
      if (layoutObject) {
        LayoutRect box;
        if (layoutObject->isBox())
          box = toLayoutBox(layoutObject)->borderBoxRect();

        list->append(*zoomAdjustedPixelValue(
            minimumValueForLength(style.transformOriginX(), box.width()),
            style));
        list->append(*zoomAdjustedPixelValue(
            minimumValueForLength(style.transformOriginY(), box.height()),
            style));
        if (style.transformOriginZ() != 0)
          list->append(
              *zoomAdjustedPixelValue(style.transformOriginZ(), style));
      } else {
        list->append(
            *zoomAdjustedPixelValueForLength(style.transformOriginX(), style));
        list->append(
            *zoomAdjustedPixelValueForLength(style.transformOriginY(), style));
        if (style.transformOriginZ() != 0)
          list->append(
              *zoomAdjustedPixelValue(style.transformOriginZ(), style));
      }
      return list;
    }
    case CSSPropertyTransformStyle:
      return CSSIdentifierValue::create(
          (style.transformStyle3D() == TransformStyle3DPreserve3D)
              ? CSSValuePreserve3d
              : CSSValueFlat);
    case CSSPropertyTransitionDelay:
      return valueForAnimationDelay(style.transitions());
    case CSSPropertyTransitionDuration:
      return valueForAnimationDuration(style.transitions());
    case CSSPropertyTransitionProperty:
      return valueForTransitionProperty(style.transitions());
    case CSSPropertyTransitionTimingFunction:
      return valueForAnimationTimingFunction(style.transitions());
    case CSSPropertyTransition: {
      const CSSTransitionData* transitionData = style.transitions();
      if (transitionData) {
        CSSValueList* transitionsList = CSSValueList::createCommaSeparated();
        for (size_t i = 0; i < transitionData->propertyList().size(); ++i) {
          CSSValueList* list = CSSValueList::createSpaceSeparated();
          list->append(*createTransitionPropertyValue(
              transitionData->propertyList()[i]));
          list->append(*CSSPrimitiveValue::create(
              CSSTimingData::getRepeated(transitionData->durationList(), i),
              CSSPrimitiveValue::UnitType::Seconds));
          list->append(*createTimingFunctionValue(
              CSSTimingData::getRepeated(transitionData->timingFunctionList(),
                                         i)
                  .get()));
          list->append(*CSSPrimitiveValue::create(
              CSSTimingData::getRepeated(transitionData->delayList(), i),
              CSSPrimitiveValue::UnitType::Seconds));
          transitionsList->append(*list);
        }
        return transitionsList;
      }

      CSSValueList* list = CSSValueList::createSpaceSeparated();
      // transition-property default value.
      list->append(*CSSIdentifierValue::create(CSSValueAll));
      list->append(
          *CSSPrimitiveValue::create(CSSTransitionData::initialDuration(),
                                     CSSPrimitiveValue::UnitType::Seconds));
      list->append(*createTimingFunctionValue(
          CSSTransitionData::initialTimingFunction().get()));
      list->append(
          *CSSPrimitiveValue::create(CSSTransitionData::initialDelay(),
                                     CSSPrimitiveValue::UnitType::Seconds));
      return list;
    }
    case CSSPropertyPointerEvents:
      return CSSIdentifierValue::create(style.pointerEvents());
    case CSSPropertyWritingMode:
    case CSSPropertyWebkitWritingMode:
      return CSSIdentifierValue::create(style.getWritingMode());
    case CSSPropertyWebkitTextCombine:
      if (style.getTextCombine() == TextCombineAll)
        return CSSIdentifierValue::create(CSSValueHorizontal);
    case CSSPropertyTextCombineUpright:
      return CSSIdentifierValue::create(style.getTextCombine());
    case CSSPropertyWebkitTextOrientation:
      if (style.getTextOrientation() == TextOrientationMixed)
        return CSSIdentifierValue::create(CSSValueVerticalRight);
    case CSSPropertyTextOrientation:
      return CSSIdentifierValue::create(style.getTextOrientation());
    case CSSPropertyContent:
      return valueForContentData(style);
    case CSSPropertyCounterIncrement:
      return valueForCounterDirectives(style, propertyID);
    case CSSPropertyCounterReset:
      return valueForCounterDirectives(style, propertyID);
    case CSSPropertyClipPath:
      if (ClipPathOperation* operation = style.clipPath()) {
        if (operation->type() == ClipPathOperation::SHAPE)
          return valueForBasicShape(
              style, toShapeClipPathOperation(operation)->basicShape());
        if (operation->type() == ClipPathOperation::REFERENCE)
          return CSSURIValue::create(
              toReferenceClipPathOperation(operation)->url());
      }
      return CSSIdentifierValue::create(CSSValueNone);
    case CSSPropertyShapeMargin:
      return CSSValue::create(style.shapeMargin(), style.effectiveZoom());
    case CSSPropertyShapeImageThreshold:
      return CSSPrimitiveValue::create(style.shapeImageThreshold(),
                                       CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyShapeOutside:
      return valueForShape(style, style.shapeOutside());
    case CSSPropertyFilter:
      return valueForFilter(style, style.filter());
    case CSSPropertyBackdropFilter:
      return valueForFilter(style, style.backdropFilter());
    case CSSPropertyMixBlendMode:
      return CSSIdentifierValue::create(style.blendMode());

    case CSSPropertyBackgroundBlendMode: {
      CSSValueList* list = CSSValueList::createCommaSeparated();
      for (const FillLayer* currLayer = &style.backgroundLayers(); currLayer;
           currLayer = currLayer->next())
        list->append(*CSSIdentifierValue::create(currLayer->blendMode()));
      return list;
    }
    case CSSPropertyBackground:
      return valuesForBackgroundShorthand(style, layoutObject, styledNode,
                                          allowVisitedStyle);
    case CSSPropertyBorder: {
      const CSSValue* value = get(CSSPropertyBorderTop, style, layoutObject,
                                  styledNode, allowVisitedStyle);
      const CSSPropertyID properties[] = {CSSPropertyBorderRight,
                                          CSSPropertyBorderBottom,
                                          CSSPropertyBorderLeft};
      for (size_t i = 0; i < WTF_ARRAY_LENGTH(properties); ++i) {
        if (!compareCSSValuePtr<CSSValue>(
                value, get(properties[i], style, layoutObject, styledNode,
                           allowVisitedStyle)))
          return nullptr;
      }
      return value;
    }
    case CSSPropertyBorderBottom:
      return valuesForShorthandProperty(borderBottomShorthand(), style,
                                        layoutObject, styledNode,
                                        allowVisitedStyle);
    case CSSPropertyBorderColor:
      return valuesForSidesShorthand(borderColorShorthand(), style,
                                     layoutObject, styledNode,
                                     allowVisitedStyle);
    case CSSPropertyBorderLeft:
      return valuesForShorthandProperty(borderLeftShorthand(), style,
                                        layoutObject, styledNode,
                                        allowVisitedStyle);
    case CSSPropertyBorderImage:
      return valueForNinePieceImage(style.borderImage(), style);
    case CSSPropertyBorderRadius:
      return valueForBorderRadiusShorthand(style);
    case CSSPropertyBorderRight:
      return valuesForShorthandProperty(borderRightShorthand(), style,
                                        layoutObject, styledNode,
                                        allowVisitedStyle);
    case CSSPropertyBorderStyle:
      return valuesForSidesShorthand(borderStyleShorthand(), style,
                                     layoutObject, styledNode,
                                     allowVisitedStyle);
    case CSSPropertyBorderTop:
      return valuesForShorthandProperty(borderTopShorthand(), style,
                                        layoutObject, styledNode,
                                        allowVisitedStyle);
    case CSSPropertyBorderWidth:
      return valuesForSidesShorthand(borderWidthShorthand(), style,
                                     layoutObject, styledNode,
                                     allowVisitedStyle);
    case CSSPropertyColumnRule:
      return valuesForShorthandProperty(columnRuleShorthand(), style,
                                        layoutObject, styledNode,
                                        allowVisitedStyle);
    case CSSPropertyColumns:
      return valuesForShorthandProperty(columnsShorthand(), style, layoutObject,
                                        styledNode, allowVisitedStyle);
    case CSSPropertyListStyle:
      return valuesForShorthandProperty(listStyleShorthand(), style,
                                        layoutObject, styledNode,
                                        allowVisitedStyle);
    case CSSPropertyMargin:
      return valuesForSidesShorthand(marginShorthand(), style, layoutObject,
                                     styledNode, allowVisitedStyle);
    case CSSPropertyOutline:
      return valuesForShorthandProperty(outlineShorthand(), style, layoutObject,
                                        styledNode, allowVisitedStyle);
    case CSSPropertyPadding:
      return valuesForSidesShorthand(paddingShorthand(), style, layoutObject,
                                     styledNode, allowVisitedStyle);
    // Individual properties not part of the spec.
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
      return nullptr;

    case CSSPropertyMotion:
      return valuesForShorthandProperty(motionShorthand(), style, layoutObject,
                                        styledNode, allowVisitedStyle);

    case CSSPropertyOffset:
      return valuesForShorthandProperty(offsetShorthand(), style, layoutObject,
                                        styledNode, allowVisitedStyle);

    case CSSPropertyOffsetAnchor:
      return valueForPosition(style.offsetAnchor(), style);

    case CSSPropertyOffsetPosition:
      return valueForPosition(style.offsetPosition(), style);

    case CSSPropertyOffsetPath:
      if (const StylePath* styleMotionPath = style.offsetPath())
        return styleMotionPath->computedCSSValue();
      return CSSIdentifierValue::create(CSSValueNone);

    case CSSPropertyOffsetDistance:
      return zoomAdjustedPixelValueForLength(style.offsetDistance(), style);

    case CSSPropertyOffsetRotate:
    case CSSPropertyOffsetRotation: {
      CSSValueList* list = CSSValueList::createSpaceSeparated();
      if (style.offsetRotation().type == OffsetRotationAuto)
        list->append(*CSSIdentifierValue::create(CSSValueAuto));
      list->append(*CSSPrimitiveValue::create(
          style.offsetRotation().angle, CSSPrimitiveValue::UnitType::Degrees));
      return list;
    }

    // Unimplemented CSS 3 properties (including CSS3 shorthand properties).
    case CSSPropertyWebkitTextEmphasis:
      return nullptr;

    // Directional properties are resolved by resolveDirectionAwareProperty()
    // before the switch.
    case CSSPropertyWebkitBorderEnd:
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderStart:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBorderAfter:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderAfterWidth:
    case CSSPropertyWebkitBorderBefore:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyWebkitMarginAfter:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitPaddingEnd:
    case CSSPropertyWebkitPaddingStart:
    case CSSPropertyWebkitPaddingAfter:
    case CSSPropertyWebkitPaddingBefore:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyWebkitLogicalHeight:
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyWebkitMinLogicalHeight:
    case CSSPropertyWebkitMaxLogicalWidth:
    case CSSPropertyWebkitMaxLogicalHeight:
      ASSERT_NOT_REACHED();
      return nullptr;

    // Unimplemented @font-face properties.
    case CSSPropertySrc:
    case CSSPropertyUnicodeRange:
      return nullptr;

    // Other unimplemented properties.
    case CSSPropertyPage:  // for @page
    case CSSPropertySize:  // for @page
      return nullptr;

    // Unimplemented -webkit- properties.
    case CSSPropertyWebkitMarginCollapse:
    case CSSPropertyWebkitMask:
    case CSSPropertyWebkitMaskRepeatX:
    case CSSPropertyWebkitMaskRepeatY:
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY:
    case CSSPropertyWebkitTextStroke:
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ:
      return nullptr;

    // @viewport rule properties.
    case CSSPropertyMaxZoom:
    case CSSPropertyMinZoom:
    case CSSPropertyOrientation:
    case CSSPropertyUserZoom:
      return nullptr;

    // SVG properties.
    case CSSPropertyClipRule:
      return CSSIdentifierValue::create(svgStyle.clipRule());
    case CSSPropertyFloodOpacity:
      return CSSPrimitiveValue::create(svgStyle.floodOpacity(),
                                       CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyStopOpacity:
      return CSSPrimitiveValue::create(svgStyle.stopOpacity(),
                                       CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyColorInterpolation:
      return CSSIdentifierValue::create(svgStyle.colorInterpolation());
    case CSSPropertyColorInterpolationFilters:
      return CSSIdentifierValue::create(svgStyle.colorInterpolationFilters());
    case CSSPropertyFillOpacity:
      return CSSPrimitiveValue::create(svgStyle.fillOpacity(),
                                       CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyFillRule:
      return CSSIdentifierValue::create(svgStyle.fillRule());
    case CSSPropertyColorRendering:
      return CSSIdentifierValue::create(svgStyle.colorRendering());
    case CSSPropertyShapeRendering:
      return CSSIdentifierValue::create(svgStyle.shapeRendering());
    case CSSPropertyStrokeLinecap:
      return CSSIdentifierValue::create(svgStyle.capStyle());
    case CSSPropertyStrokeLinejoin:
      return CSSIdentifierValue::create(svgStyle.joinStyle());
    case CSSPropertyStrokeMiterlimit:
      return CSSPrimitiveValue::create(svgStyle.strokeMiterLimit(),
                                       CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyStrokeOpacity:
      return CSSPrimitiveValue::create(svgStyle.strokeOpacity(),
                                       CSSPrimitiveValue::UnitType::Number);
    case CSSPropertyAlignmentBaseline:
      return CSSIdentifierValue::create(svgStyle.alignmentBaseline());
    case CSSPropertyDominantBaseline:
      return CSSIdentifierValue::create(svgStyle.dominantBaseline());
    case CSSPropertyTextAnchor:
      return CSSIdentifierValue::create(svgStyle.textAnchor());
    case CSSPropertyMask:
      if (!svgStyle.maskerResource().isEmpty())
        return CSSURIValue::create(
            serializeAsFragmentIdentifier(svgStyle.maskerResource()));
      return CSSIdentifierValue::create(CSSValueNone);
    case CSSPropertyFloodColor:
      return currentColorOrValidColor(style, svgStyle.floodColor());
    case CSSPropertyLightingColor:
      return currentColorOrValidColor(style, svgStyle.lightingColor());
    case CSSPropertyStopColor:
      return currentColorOrValidColor(style, svgStyle.stopColor());
    case CSSPropertyFill:
      return adjustSVGPaintForCurrentColor(
          svgStyle.fillPaintType(), svgStyle.fillPaintUri(),
          svgStyle.fillPaintColor(), style.color());
    case CSSPropertyMarkerEnd:
      if (!svgStyle.markerEndResource().isEmpty())
        return CSSURIValue::create(
            serializeAsFragmentIdentifier(svgStyle.markerEndResource()));
      return CSSIdentifierValue::create(CSSValueNone);
    case CSSPropertyMarkerMid:
      if (!svgStyle.markerMidResource().isEmpty())
        return CSSURIValue::create(
            serializeAsFragmentIdentifier(svgStyle.markerMidResource()));
      return CSSIdentifierValue::create(CSSValueNone);
    case CSSPropertyMarkerStart:
      if (!svgStyle.markerStartResource().isEmpty())
        return CSSURIValue::create(
            serializeAsFragmentIdentifier(svgStyle.markerStartResource()));
      return CSSIdentifierValue::create(CSSValueNone);
    case CSSPropertyStroke:
      return adjustSVGPaintForCurrentColor(
          svgStyle.strokePaintType(), svgStyle.strokePaintUri(),
          svgStyle.strokePaintColor(), style.color());
    case CSSPropertyStrokeDasharray:
      return strokeDashArrayToCSSValueList(*svgStyle.strokeDashArray(), style);
    case CSSPropertyStrokeDashoffset:
      return zoomAdjustedPixelValueForLength(svgStyle.strokeDashOffset(),
                                             style);
    case CSSPropertyStrokeWidth:
      return pixelValueForUnzoomedLength(svgStyle.strokeWidth(), style);
    case CSSPropertyBaselineShift: {
      switch (svgStyle.baselineShift()) {
        case BS_SUPER:
          return CSSIdentifierValue::create(CSSValueSuper);
        case BS_SUB:
          return CSSIdentifierValue::create(CSSValueSub);
        case BS_LENGTH:
          return zoomAdjustedPixelValueForLength(svgStyle.baselineShiftValue(),
                                                 style);
      }
      ASSERT_NOT_REACHED();
      return nullptr;
    }
    case CSSPropertyBufferedRendering:
      return CSSIdentifierValue::create(svgStyle.bufferedRendering());
    case CSSPropertyPaintOrder:
      return paintOrderToCSSValueList(svgStyle);
    case CSSPropertyVectorEffect:
      return CSSIdentifierValue::create(svgStyle.vectorEffect());
    case CSSPropertyMaskType:
      return CSSIdentifierValue::create(svgStyle.maskType());
    case CSSPropertyMarker:
      // the above properties are not yet implemented in the engine
      return nullptr;
    case CSSPropertyD:
      if (const StylePath* stylePath = svgStyle.d())
        return stylePath->computedCSSValue();
      return CSSIdentifierValue::create(CSSValueNone);
    case CSSPropertyCx:
      return zoomAdjustedPixelValueForLength(svgStyle.cx(), style);
    case CSSPropertyCy:
      return zoomAdjustedPixelValueForLength(svgStyle.cy(), style);
    case CSSPropertyX:
      return zoomAdjustedPixelValueForLength(svgStyle.x(), style);
    case CSSPropertyY:
      return zoomAdjustedPixelValueForLength(svgStyle.y(), style);
    case CSSPropertyR:
      return zoomAdjustedPixelValueForLength(svgStyle.r(), style);
    case CSSPropertyRx:
      return zoomAdjustedPixelValueForLength(svgStyle.rx(), style);
    case CSSPropertyRy:
      return zoomAdjustedPixelValueForLength(svgStyle.ry(), style);
    case CSSPropertyScrollSnapType:
      return CSSIdentifierValue::create(style.getScrollSnapType());
    case CSSPropertyScrollSnapPointsX:
      return valueForScrollSnapPoints(style.scrollSnapPointsX(), style);
    case CSSPropertyScrollSnapPointsY:
      return valueForScrollSnapPoints(style.scrollSnapPointsY(), style);
    case CSSPropertyScrollSnapCoordinate:
      return valueForScrollSnapCoordinate(style.scrollSnapCoordinate(), style);
    case CSSPropertyScrollSnapDestination:
      return valueForScrollSnapDestination(style.scrollSnapDestination(),
                                           style);
    case CSSPropertyTranslate: {
      if (!style.translate())
        return CSSPrimitiveValue::create(0,
                                         CSSPrimitiveValue::UnitType::Pixels);

      CSSValueList* list = CSSValueList::createSpaceSeparated();
      if (layoutObject && layoutObject->isBox()) {
        LayoutRect box = toLayoutBox(layoutObject)->borderBoxRect();
        list->append(*zoomAdjustedPixelValue(
            floatValueForLength(style.translate()->x(), box.width().toFloat()),
            style));

        if (!style.translate()->y().isZero() || style.translate()->z() != 0)
          list->append(*zoomAdjustedPixelValue(
              floatValueForLength(style.translate()->y(),
                                  box.height().toFloat()),
              style));

      } else {
        // No box to resolve the percentage values
        list->append(
            *zoomAdjustedPixelValueForLength(style.translate()->x(), style));

        if (!style.translate()->y().isZero() || style.translate()->z() != 0)
          list->append(
              *zoomAdjustedPixelValueForLength(style.translate()->y(), style));
      }

      if (style.translate()->z() != 0)
        list->append(*zoomAdjustedPixelValue(style.translate()->z(), style));

      return list;
    }
    case CSSPropertyRotate: {
      if (!style.rotate())
        return CSSPrimitiveValue::create(0,
                                         CSSPrimitiveValue::UnitType::Degrees);

      CSSValueList* list = CSSValueList::createSpaceSeparated();
      list->append(*CSSPrimitiveValue::create(
          style.rotate()->angle(), CSSPrimitiveValue::UnitType::Degrees));
      if (style.rotate()->x() != 0 || style.rotate()->y() != 0 ||
          style.rotate()->z() != 1) {
        list->append(*CSSPrimitiveValue::create(
            style.rotate()->x(), CSSPrimitiveValue::UnitType::Number));
        list->append(*CSSPrimitiveValue::create(
            style.rotate()->y(), CSSPrimitiveValue::UnitType::Number));
        list->append(*CSSPrimitiveValue::create(
            style.rotate()->z(), CSSPrimitiveValue::UnitType::Number));
      }
      return list;
    }
    case CSSPropertyScale: {
      if (!style.scale())
        return CSSPrimitiveValue::create(1,
                                         CSSPrimitiveValue::UnitType::Number);
      CSSValueList* list = CSSValueList::createSpaceSeparated();
      list->append(*CSSPrimitiveValue::create(
          style.scale()->x(), CSSPrimitiveValue::UnitType::Number));
      if (style.scale()->y() == 1 && style.scale()->z() == 1)
        return list;
      list->append(*CSSPrimitiveValue::create(
          style.scale()->y(), CSSPrimitiveValue::UnitType::Number));
      if (style.scale()->z() != 1)
        list->append(*CSSPrimitiveValue::create(
            style.scale()->z(), CSSPrimitiveValue::UnitType::Number));
      return list;
    }
    case CSSPropertyContain: {
      if (!style.contain())
        return CSSIdentifierValue::create(CSSValueNone);
      if (style.contain() == ContainsStrict)
        return CSSIdentifierValue::create(CSSValueStrict);
      if (style.contain() == ContainsContent)
        return CSSIdentifierValue::create(CSSValueContent);

      CSSValueList* list = CSSValueList::createSpaceSeparated();
      if (style.containsStyle())
        list->append(*CSSIdentifierValue::create(CSSValueStyle));
      if (style.contain() & ContainsLayout)
        list->append(*CSSIdentifierValue::create(CSSValueLayout));
      if (style.containsPaint())
        list->append(*CSSIdentifierValue::create(CSSValuePaint));
      if (style.containsSize())
        list->append(*CSSIdentifierValue::create(CSSValueSize));
      ASSERT(list->length());
      return list;
    }
    case CSSPropertySnapHeight: {
      if (!style.snapHeightUnit())
        return CSSPrimitiveValue::create(0,
                                         CSSPrimitiveValue::UnitType::Pixels);
      CSSValueList* list = CSSValueList::createSpaceSeparated();
      list->append(*CSSPrimitiveValue::create(
          style.snapHeightUnit(), CSSPrimitiveValue::UnitType::Pixels));
      if (style.snapHeightPosition())
        list->append(*CSSPrimitiveValue::create(
            style.snapHeightPosition(), CSSPrimitiveValue::UnitType::Integer));
      return list;
    }
    case CSSPropertyVariable:
      // Variables are retrieved via get(AtomicString).
      ASSERT_NOT_REACHED();
      return nullptr;
    case CSSPropertyAll:
      return nullptr;
    default:
      break;
  }
  ASSERT_NOT_REACHED();
  return nullptr;
}

}  // namespace blink

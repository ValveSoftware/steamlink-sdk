// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/LengthListPropertyFunctions.h"

#include "core/style/ComputedStyle.h"

namespace blink {

namespace {

const FillLayer* getFillLayer(CSSPropertyID property,
                              const ComputedStyle& style) {
  switch (property) {
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
      return &style.backgroundLayers();
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
      return &style.maskLayers();
    default:
      NOTREACHED();
      return nullptr;
  }
}

FillLayer* accessFillLayer(CSSPropertyID property, ComputedStyle& style) {
  switch (property) {
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
      return &style.accessBackgroundLayers();
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
      return &style.accessMaskLayers();
    default:
      NOTREACHED();
      return nullptr;
  }
}

struct FillLayerMethods {
  FillLayerMethods(CSSPropertyID property) {
    isSet = nullptr;
    getLength = nullptr;
    setLength = nullptr;
    clear = nullptr;
    switch (property) {
      case CSSPropertyBackgroundPositionX:
      case CSSPropertyWebkitMaskPositionX:
        isSet = &FillLayer::isXPositionSet;
        getLength = &FillLayer::xPosition;
        setLength = &FillLayer::setXPosition;
        clear = &FillLayer::clearXPosition;
        break;
      case CSSPropertyBackgroundPositionY:
      case CSSPropertyWebkitMaskPositionY:
        isSet = &FillLayer::isYPositionSet;
        getLength = &FillLayer::yPosition;
        setLength = &FillLayer::setYPosition;
        clear = &FillLayer::clearYPosition;
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  bool (FillLayer::*isSet)() const;
  const Length& (FillLayer::*getLength)() const;
  void (FillLayer::*setLength)(const Length&);
  void (FillLayer::*clear)();
};

}  // namespace

ValueRange LengthListPropertyFunctions::getValueRange(CSSPropertyID property) {
  switch (property) {
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyObjectPosition:
    case CSSPropertyOffsetAnchor:
    case CSSPropertyOffsetPosition:
    case CSSPropertyPerspectiveOrigin:
    case CSSPropertyTransformOrigin:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
      return ValueRangeAll;

    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyStrokeDasharray:
      return ValueRangeNonNegative;

    default:
      NOTREACHED();
      return ValueRangeAll;
  }
}

bool LengthListPropertyFunctions::getInitialLengthList(CSSPropertyID property,
                                                       Vector<Length>& result) {
  return getLengthList(property, ComputedStyle::initialStyle(), result);
}

static bool appendToVector(const LengthPoint& point, Vector<Length>& result) {
  result.append(point.x());
  result.append(point.y());
  return true;
}

static bool appendToVector(const LengthSize& size, Vector<Length>& result) {
  result.append(size.width());
  result.append(size.height());
  return true;
}

static bool appendToVector(const TransformOrigin& transformOrigin,
                           Vector<Length>& result) {
  result.append(transformOrigin.x());
  result.append(transformOrigin.y());
  result.append(Length(transformOrigin.z(), Fixed));
  return true;
}

bool LengthListPropertyFunctions::getLengthList(CSSPropertyID property,
                                                const ComputedStyle& style,
                                                Vector<Length>& result) {
  DCHECK(result.isEmpty());

  switch (property) {
    case CSSPropertyStrokeDasharray: {
      if (style.strokeDashArray())
        result.appendVector(style.strokeDashArray()->vector());
      return true;
    }

    case CSSPropertyObjectPosition:
      return appendToVector(style.objectPosition(), result);
    case CSSPropertyOffsetAnchor:
      return appendToVector(style.offsetAnchor(), result);
    case CSSPropertyOffsetPosition:
      return appendToVector(style.offsetPosition(), result);
    case CSSPropertyPerspectiveOrigin:
      return appendToVector(style.perspectiveOrigin(), result);
    case CSSPropertyBorderBottomLeftRadius:
      return appendToVector(style.borderBottomLeftRadius(), result);
    case CSSPropertyBorderBottomRightRadius:
      return appendToVector(style.borderBottomRightRadius(), result);
    case CSSPropertyBorderTopLeftRadius:
      return appendToVector(style.borderTopLeftRadius(), result);
    case CSSPropertyBorderTopRightRadius:
      return appendToVector(style.borderTopRightRadius(), result);
    case CSSPropertyTransformOrigin:
      return appendToVector(style.transformOrigin(), result);

    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY: {
      const FillLayer* fillLayer = getFillLayer(property, style);
      FillLayerMethods fillLayerMethods(property);
      while (fillLayer && (fillLayer->*fillLayerMethods.isSet)()) {
        result.append((fillLayer->*fillLayerMethods.getLength)());
        fillLayer = fillLayer->next();
      }
      return true;
    }

    default:
      NOTREACHED();
      return false;
  }
}

static LengthPoint pointFromVector(const Vector<Length>& list) {
  DCHECK_EQ(list.size(), 2U);
  return LengthPoint(list[0], list[1]);
}

static LengthSize sizeFromVector(const Vector<Length>& list) {
  DCHECK_EQ(list.size(), 2U);
  return LengthSize(list[0], list[1]);
}

static TransformOrigin transformOriginFromVector(const Vector<Length>& list) {
  DCHECK_EQ(list.size(), 3U);
  return TransformOrigin(list[0], list[1], list[2].pixels());
}

void LengthListPropertyFunctions::setLengthList(CSSPropertyID property,
                                                ComputedStyle& style,
                                                Vector<Length>&& lengthList) {
  switch (property) {
    case CSSPropertyStrokeDasharray:
      style.setStrokeDashArray(
          lengthList.isEmpty() ? nullptr : RefVector<Length>::create(
                                               std::move(lengthList)));
      return;

    case CSSPropertyObjectPosition:
      style.setObjectPosition(pointFromVector(lengthList));
      return;
    case CSSPropertyOffsetAnchor:
      style.setOffsetAnchor(pointFromVector(lengthList));
      return;
    case CSSPropertyOffsetPosition:
      style.setOffsetPosition(pointFromVector(lengthList));
      return;
    case CSSPropertyPerspectiveOrigin:
      style.setPerspectiveOrigin(pointFromVector(lengthList));
      return;

    case CSSPropertyBorderBottomLeftRadius:
      style.setBorderBottomLeftRadius(sizeFromVector(lengthList));
      return;
    case CSSPropertyBorderBottomRightRadius:
      style.setBorderBottomRightRadius(sizeFromVector(lengthList));
      return;
    case CSSPropertyBorderTopLeftRadius:
      style.setBorderTopLeftRadius(sizeFromVector(lengthList));
      return;
    case CSSPropertyBorderTopRightRadius:
      style.setBorderTopRightRadius(sizeFromVector(lengthList));
      return;

    case CSSPropertyTransformOrigin:
      style.setTransformOrigin(transformOriginFromVector(lengthList));
      return;

    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY: {
      FillLayer* fillLayer = accessFillLayer(property, style);
      FillLayer* prev = nullptr;
      FillLayerMethods fillLayerMethods(property);
      for (size_t i = 0; i < lengthList.size(); i++) {
        if (!fillLayer)
          fillLayer = prev->ensureNext();
        (fillLayer->*fillLayerMethods.setLength)(lengthList[i]);
        prev = fillLayer;
        fillLayer = fillLayer->next();
      }
      while (fillLayer) {
        (fillLayer->*fillLayerMethods.clear)();
        fillLayer = fillLayer->next();
      }
      return;
    }

    default:
      NOTREACHED();
      break;
  }
}

}  // namespace blink

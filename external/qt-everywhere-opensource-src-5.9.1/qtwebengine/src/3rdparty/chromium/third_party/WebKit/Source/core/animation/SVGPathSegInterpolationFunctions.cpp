// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGPathSegInterpolationFunctions.h"

#include <memory>

namespace blink {

std::unique_ptr<InterpolableNumber> consumeControlAxis(double value,
                                                       bool isAbsolute,
                                                       double currentValue) {
  return InterpolableNumber::create(isAbsolute ? value : currentValue + value);
}

double consumeInterpolableControlAxis(const InterpolableValue* number,
                                      bool isAbsolute,
                                      double currentValue) {
  double value = toInterpolableNumber(number)->value();
  return isAbsolute ? value : value - currentValue;
}

std::unique_ptr<InterpolableNumber>
consumeCoordinateAxis(double value, bool isAbsolute, double& currentValue) {
  if (isAbsolute)
    currentValue = value;
  else
    currentValue += value;
  return InterpolableNumber::create(currentValue);
}

double consumeInterpolableCoordinateAxis(const InterpolableValue* number,
                                         bool isAbsolute,
                                         double& currentValue) {
  double previousValue = currentValue;
  currentValue = toInterpolableNumber(number)->value();
  return isAbsolute ? currentValue : currentValue - previousValue;
}

std::unique_ptr<InterpolableValue> consumeClosePath(
    const PathSegmentData&,
    PathCoordinates& coordinates) {
  coordinates.currentX = coordinates.initialX;
  coordinates.currentY = coordinates.initialY;
  return InterpolableList::create(0);
}

PathSegmentData consumeInterpolableClosePath(const InterpolableValue&,
                                             SVGPathSegType segType,
                                             PathCoordinates& coordinates) {
  coordinates.currentX = coordinates.initialX;
  coordinates.currentY = coordinates.initialY;

  PathSegmentData segment;
  segment.command = segType;
  return segment;
}

std::unique_ptr<InterpolableValue> consumeSingleCoordinate(
    const PathSegmentData& segment,
    PathCoordinates& coordinates) {
  bool isAbsolute = isAbsolutePathSegType(segment.command);
  std::unique_ptr<InterpolableList> result = InterpolableList::create(2);
  result->set(
      0, consumeCoordinateAxis(segment.x(), isAbsolute, coordinates.currentX));
  result->set(
      1, consumeCoordinateAxis(segment.y(), isAbsolute, coordinates.currentY));

  if (toAbsolutePathSegType(segment.command) == PathSegMoveToAbs) {
    // Any upcoming 'closepath' commands bring us back to the location we have
    // just moved to.
    coordinates.initialX = coordinates.currentX;
    coordinates.initialY = coordinates.currentY;
  }

  return std::move(result);
}

PathSegmentData consumeInterpolableSingleCoordinate(
    const InterpolableValue& value,
    SVGPathSegType segType,
    PathCoordinates& coordinates) {
  const InterpolableList& list = toInterpolableList(value);
  bool isAbsolute = isAbsolutePathSegType(segType);
  PathSegmentData segment;
  segment.command = segType;
  segment.targetPoint.setX(consumeInterpolableCoordinateAxis(
      list.get(0), isAbsolute, coordinates.currentX));
  segment.targetPoint.setY(consumeInterpolableCoordinateAxis(
      list.get(1), isAbsolute, coordinates.currentY));

  if (toAbsolutePathSegType(segType) == PathSegMoveToAbs) {
    // Any upcoming 'closepath' commands bring us back to the location we have
    // just moved to.
    coordinates.initialX = coordinates.currentX;
    coordinates.initialY = coordinates.currentY;
  }

  return segment;
}

std::unique_ptr<InterpolableValue> consumeCurvetoCubic(
    const PathSegmentData& segment,
    PathCoordinates& coordinates) {
  bool isAbsolute = isAbsolutePathSegType(segment.command);
  std::unique_ptr<InterpolableList> result = InterpolableList::create(6);
  result->set(
      0, consumeControlAxis(segment.x1(), isAbsolute, coordinates.currentX));
  result->set(
      1, consumeControlAxis(segment.y1(), isAbsolute, coordinates.currentY));
  result->set(
      2, consumeControlAxis(segment.x2(), isAbsolute, coordinates.currentX));
  result->set(
      3, consumeControlAxis(segment.y2(), isAbsolute, coordinates.currentY));
  result->set(
      4, consumeCoordinateAxis(segment.x(), isAbsolute, coordinates.currentX));
  result->set(
      5, consumeCoordinateAxis(segment.y(), isAbsolute, coordinates.currentY));
  return std::move(result);
}

PathSegmentData consumeInterpolableCurvetoCubic(const InterpolableValue& value,
                                                SVGPathSegType segType,
                                                PathCoordinates& coordinates) {
  const InterpolableList& list = toInterpolableList(value);
  bool isAbsolute = isAbsolutePathSegType(segType);
  PathSegmentData segment;
  segment.command = segType;
  segment.point1.setX(consumeInterpolableControlAxis(list.get(0), isAbsolute,
                                                     coordinates.currentX));
  segment.point1.setY(consumeInterpolableControlAxis(list.get(1), isAbsolute,
                                                     coordinates.currentY));
  segment.point2.setX(consumeInterpolableControlAxis(list.get(2), isAbsolute,
                                                     coordinates.currentX));
  segment.point2.setY(consumeInterpolableControlAxis(list.get(3), isAbsolute,
                                                     coordinates.currentY));
  segment.targetPoint.setX(consumeInterpolableCoordinateAxis(
      list.get(4), isAbsolute, coordinates.currentX));
  segment.targetPoint.setY(consumeInterpolableCoordinateAxis(
      list.get(5), isAbsolute, coordinates.currentY));
  return segment;
}

std::unique_ptr<InterpolableValue> consumeCurvetoQuadratic(
    const PathSegmentData& segment,
    PathCoordinates& coordinates) {
  bool isAbsolute = isAbsolutePathSegType(segment.command);
  std::unique_ptr<InterpolableList> result = InterpolableList::create(4);
  result->set(
      0, consumeControlAxis(segment.x1(), isAbsolute, coordinates.currentX));
  result->set(
      1, consumeControlAxis(segment.y1(), isAbsolute, coordinates.currentY));
  result->set(
      2, consumeCoordinateAxis(segment.x(), isAbsolute, coordinates.currentX));
  result->set(
      3, consumeCoordinateAxis(segment.y(), isAbsolute, coordinates.currentY));
  return std::move(result);
}

PathSegmentData consumeInterpolableCurvetoQuadratic(
    const InterpolableValue& value,
    SVGPathSegType segType,
    PathCoordinates& coordinates) {
  const InterpolableList& list = toInterpolableList(value);
  bool isAbsolute = isAbsolutePathSegType(segType);
  PathSegmentData segment;
  segment.command = segType;
  segment.point1.setX(consumeInterpolableControlAxis(list.get(0), isAbsolute,
                                                     coordinates.currentX));
  segment.point1.setY(consumeInterpolableControlAxis(list.get(1), isAbsolute,
                                                     coordinates.currentY));
  segment.targetPoint.setX(consumeInterpolableCoordinateAxis(
      list.get(2), isAbsolute, coordinates.currentX));
  segment.targetPoint.setY(consumeInterpolableCoordinateAxis(
      list.get(3), isAbsolute, coordinates.currentY));
  return segment;
}

std::unique_ptr<InterpolableValue> consumeArc(const PathSegmentData& segment,
                                              PathCoordinates& coordinates) {
  bool isAbsolute = isAbsolutePathSegType(segment.command);
  std::unique_ptr<InterpolableList> result = InterpolableList::create(7);
  result->set(
      0, consumeCoordinateAxis(segment.x(), isAbsolute, coordinates.currentX));
  result->set(
      1, consumeCoordinateAxis(segment.y(), isAbsolute, coordinates.currentY));
  result->set(2, InterpolableNumber::create(segment.r1()));
  result->set(3, InterpolableNumber::create(segment.r2()));
  result->set(4, InterpolableNumber::create(segment.arcAngle()));
  // TODO(alancutter): Make these flags part of the NonInterpolableValue.
  result->set(5, InterpolableNumber::create(segment.largeArcFlag()));
  result->set(6, InterpolableNumber::create(segment.sweepFlag()));
  return std::move(result);
}

PathSegmentData consumeInterpolableArc(const InterpolableValue& value,
                                       SVGPathSegType segType,
                                       PathCoordinates& coordinates) {
  const InterpolableList& list = toInterpolableList(value);
  bool isAbsolute = isAbsolutePathSegType(segType);
  PathSegmentData segment;
  segment.command = segType;
  segment.targetPoint.setX(consumeInterpolableCoordinateAxis(
      list.get(0), isAbsolute, coordinates.currentX));
  segment.targetPoint.setY(consumeInterpolableCoordinateAxis(
      list.get(1), isAbsolute, coordinates.currentY));
  segment.arcRadii().setX(toInterpolableNumber(list.get(2))->value());
  segment.arcRadii().setY(toInterpolableNumber(list.get(3))->value());
  segment.setArcAngle(toInterpolableNumber(list.get(4))->value());
  segment.arcLarge = toInterpolableNumber(list.get(5))->value() >= 0.5;
  segment.arcSweep = toInterpolableNumber(list.get(6))->value() >= 0.5;
  return segment;
}

std::unique_ptr<InterpolableValue> consumeLinetoHorizontal(
    const PathSegmentData& segment,
    PathCoordinates& coordinates) {
  bool isAbsolute = isAbsolutePathSegType(segment.command);
  return consumeCoordinateAxis(segment.x(), isAbsolute, coordinates.currentX);
}

PathSegmentData consumeInterpolableLinetoHorizontal(
    const InterpolableValue& value,
    SVGPathSegType segType,
    PathCoordinates& coordinates) {
  bool isAbsolute = isAbsolutePathSegType(segType);
  PathSegmentData segment;
  segment.command = segType;
  segment.targetPoint.setX(consumeInterpolableCoordinateAxis(
      &value, isAbsolute, coordinates.currentX));
  return segment;
}

std::unique_ptr<InterpolableValue> consumeLinetoVertical(
    const PathSegmentData& segment,
    PathCoordinates& coordinates) {
  bool isAbsolute = isAbsolutePathSegType(segment.command);
  return consumeCoordinateAxis(segment.y(), isAbsolute, coordinates.currentY);
}

PathSegmentData consumeInterpolableLinetoVertical(
    const InterpolableValue& value,
    SVGPathSegType segType,
    PathCoordinates& coordinates) {
  bool isAbsolute = isAbsolutePathSegType(segType);
  PathSegmentData segment;
  segment.command = segType;
  segment.targetPoint.setY(consumeInterpolableCoordinateAxis(
      &value, isAbsolute, coordinates.currentY));
  return segment;
}

std::unique_ptr<InterpolableValue> consumeCurvetoCubicSmooth(
    const PathSegmentData& segment,
    PathCoordinates& coordinates) {
  bool isAbsolute = isAbsolutePathSegType(segment.command);
  std::unique_ptr<InterpolableList> result = InterpolableList::create(4);
  result->set(
      0, consumeControlAxis(segment.x2(), isAbsolute, coordinates.currentX));
  result->set(
      1, consumeControlAxis(segment.y2(), isAbsolute, coordinates.currentY));
  result->set(
      2, consumeCoordinateAxis(segment.x(), isAbsolute, coordinates.currentX));
  result->set(
      3, consumeCoordinateAxis(segment.y(), isAbsolute, coordinates.currentY));
  return std::move(result);
}

PathSegmentData consumeInterpolableCurvetoCubicSmooth(
    const InterpolableValue& value,
    SVGPathSegType segType,
    PathCoordinates& coordinates) {
  const InterpolableList& list = toInterpolableList(value);
  bool isAbsolute = isAbsolutePathSegType(segType);
  PathSegmentData segment;
  segment.command = segType;
  segment.point2.setX(consumeInterpolableControlAxis(list.get(0), isAbsolute,
                                                     coordinates.currentX));
  segment.point2.setY(consumeInterpolableControlAxis(list.get(1), isAbsolute,
                                                     coordinates.currentY));
  segment.targetPoint.setX(consumeInterpolableCoordinateAxis(
      list.get(2), isAbsolute, coordinates.currentX));
  segment.targetPoint.setY(consumeInterpolableCoordinateAxis(
      list.get(3), isAbsolute, coordinates.currentY));
  return segment;
}

std::unique_ptr<InterpolableValue>
SVGPathSegInterpolationFunctions::consumePathSeg(const PathSegmentData& segment,
                                                 PathCoordinates& coordinates) {
  switch (segment.command) {
    case PathSegClosePath:
      return consumeClosePath(segment, coordinates);

    case PathSegMoveToAbs:
    case PathSegMoveToRel:
    case PathSegLineToAbs:
    case PathSegLineToRel:
    case PathSegCurveToQuadraticSmoothAbs:
    case PathSegCurveToQuadraticSmoothRel:
      return consumeSingleCoordinate(segment, coordinates);

    case PathSegCurveToCubicAbs:
    case PathSegCurveToCubicRel:
      return consumeCurvetoCubic(segment, coordinates);

    case PathSegCurveToQuadraticAbs:
    case PathSegCurveToQuadraticRel:
      return consumeCurvetoQuadratic(segment, coordinates);

    case PathSegArcAbs:
    case PathSegArcRel:
      return consumeArc(segment, coordinates);

    case PathSegLineToHorizontalAbs:
    case PathSegLineToHorizontalRel:
      return consumeLinetoHorizontal(segment, coordinates);

    case PathSegLineToVerticalAbs:
    case PathSegLineToVerticalRel:
      return consumeLinetoVertical(segment, coordinates);

    case PathSegCurveToCubicSmoothAbs:
    case PathSegCurveToCubicSmoothRel:
      return consumeCurvetoCubicSmooth(segment, coordinates);

    case PathSegUnknown:
    default:
      NOTREACHED();
      return nullptr;
  }
}

PathSegmentData SVGPathSegInterpolationFunctions::consumeInterpolablePathSeg(
    const InterpolableValue& value,
    SVGPathSegType segType,
    PathCoordinates& coordinates) {
  switch (segType) {
    case PathSegClosePath:
      return consumeInterpolableClosePath(value, segType, coordinates);

    case PathSegMoveToAbs:
    case PathSegMoveToRel:
    case PathSegLineToAbs:
    case PathSegLineToRel:
    case PathSegCurveToQuadraticSmoothAbs:
    case PathSegCurveToQuadraticSmoothRel:
      return consumeInterpolableSingleCoordinate(value, segType, coordinates);

    case PathSegCurveToCubicAbs:
    case PathSegCurveToCubicRel:
      return consumeInterpolableCurvetoCubic(value, segType, coordinates);

    case PathSegCurveToQuadraticAbs:
    case PathSegCurveToQuadraticRel:
      return consumeInterpolableCurvetoQuadratic(value, segType, coordinates);

    case PathSegArcAbs:
    case PathSegArcRel:
      return consumeInterpolableArc(value, segType, coordinates);

    case PathSegLineToHorizontalAbs:
    case PathSegLineToHorizontalRel:
      return consumeInterpolableLinetoHorizontal(value, segType, coordinates);

    case PathSegLineToVerticalAbs:
    case PathSegLineToVerticalRel:
      return consumeInterpolableLinetoVertical(value, segType, coordinates);

    case PathSegCurveToCubicSmoothAbs:
    case PathSegCurveToCubicSmoothRel:
      return consumeInterpolableCurvetoCubicSmooth(value, segType, coordinates);

    case PathSegUnknown:
    default:
      NOTREACHED();
      return PathSegmentData();
  }
}

}  // namespace blink

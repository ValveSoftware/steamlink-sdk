/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron McCormack <cam@mcc.id.au>
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#ifndef SVGAnimationElement_h
#define SVGAnimationElement_h

#include "core/CoreExport.h"
#include "core/svg/animation/SVGSMILElement.h"
#include "ui/gfx/geometry/cubic_bezier.h"
#include "wtf/Vector.h"

namespace blink {

class ExceptionState;

enum AnimationMode {
  NoAnimation,
  FromToAnimation,
  FromByAnimation,
  ToAnimation,
  ByAnimation,
  ValuesAnimation,
  PathAnimation  // Used by AnimateMotion.
};

enum CalcMode {
  CalcModeDiscrete,
  CalcModeLinear,
  CalcModePaced,
  CalcModeSpline
};

class CORE_EXPORT SVGAnimationElement : public SVGSMILElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // SVGAnimationElement
  float getStartTime(ExceptionState&) const;
  float getCurrentTime() const;
  float getSimpleDuration(ExceptionState&) const;

  void beginElement();
  void beginElementAt(float offset);
  void endElement();
  void endElementAt(float offset);

  DEFINE_MAPPED_ATTRIBUTE_EVENT_LISTENER(begin, beginEvent);
  DEFINE_MAPPED_ATTRIBUTE_EVENT_LISTENER(end, endEvent);
  DEFINE_MAPPED_ATTRIBUTE_EVENT_LISTENER(repeat, repeatEvent);

  virtual bool isAdditive();
  bool isAccumulated() const;
  AnimationMode getAnimationMode() const { return m_animationMode; }
  CalcMode getCalcMode() const { return m_calcMode; }

  template <typename AnimatedType>
  void animateDiscreteType(float percentage,
                           const AnimatedType& fromType,
                           const AnimatedType& toType,
                           AnimatedType& animatedType) {
    if ((getAnimationMode() == FromToAnimation && percentage > 0.5) ||
        getAnimationMode() == ToAnimation || percentage == 1) {
      animatedType = AnimatedType(toType);
      return;
    }
    animatedType = AnimatedType(fromType);
  }

  void animateAdditiveNumber(float percentage,
                             unsigned repeatCount,
                             float fromNumber,
                             float toNumber,
                             float toAtEndOfDurationNumber,
                             float& animatedNumber) {
    float number;
    if (getCalcMode() == CalcModeDiscrete)
      number = percentage < 0.5 ? fromNumber : toNumber;
    else
      number = (toNumber - fromNumber) * percentage + fromNumber;

    if (isAccumulated() && repeatCount)
      number += toAtEndOfDurationNumber * repeatCount;

    if (isAdditive() && getAnimationMode() != ToAnimation)
      animatedNumber += number;
    else
      animatedNumber = number;
  }

 protected:
  SVGAnimationElement(const QualifiedName&, Document&);

  void parseAttribute(const QualifiedName&,
                      const AtomicString&,
                      const AtomicString&) override;
  void svgAttributeChanged(const QualifiedName&) override;

  String toValue() const;
  String byValue() const;
  String fromValue() const;

  // from SVGSMILElement
  void startedActiveInterval() override;
  void updateAnimation(float percent,
                       unsigned repeat,
                       SVGSMILElement* resultElement) override;

  virtual void updateAnimationMode();
  void setAnimationMode(AnimationMode animationMode) {
    m_animationMode = animationMode;
  }
  void setCalcMode(CalcMode calcMode) { m_calcMode = calcMode; }

  // Parses a list of values as specified by SVG, stripping leading
  // and trailing whitespace, and places them in result. If the
  // format of the string is not valid, parseValues empties result
  // and returns false. See
  // http://www.w3.org/TR/SVG/animate.html#ValuesAttribute .
  static bool parseValues(const String&, Vector<String>& result);

  void animationAttributeChanged() override;

 private:
  bool isValid() const final { return SVGTests::isValid(); }

  virtual bool calculateToAtEndOfDurationValue(
      const String& toAtEndOfDurationString) = 0;
  virtual bool calculateFromAndToValues(const String& fromString,
                                        const String& toString) = 0;
  virtual bool calculateFromAndByValues(const String& fromString,
                                        const String& byString) = 0;
  virtual void calculateAnimatedValue(float percent,
                                      unsigned repeatCount,
                                      SVGSMILElement* resultElement) = 0;
  virtual float calculateDistance(const String& /*fromString*/,
                                  const String& /*toString*/) {
    return -1.f;
  }

  void currentValuesForValuesAnimation(float percent,
                                       float& effectivePercent,
                                       String& from,
                                       String& to);
  void calculateKeyTimesForCalcModePaced();
  float calculatePercentFromKeyPoints(float percent) const;
  void currentValuesFromKeyPoints(float percent,
                                  float& effectivePercent,
                                  String& from,
                                  String& to) const;
  float calculatePercentForSpline(float percent, unsigned splineIndex) const;
  float calculatePercentForFromTo(float percent) const;
  unsigned calculateKeyTimesIndex(float percent) const;

  void setCalcMode(const AtomicString&);

  bool m_animationValid;

  Vector<String> m_values;
  // FIXME: We should probably use doubles for this, but there's no point
  // making such a change unless all SVG logic for sampling animations is
  // changed to use doubles.
  Vector<float> m_keyTimes;
  Vector<float> m_keyPoints;
  Vector<gfx::CubicBezier> m_keySplines;
  String m_lastValuesAnimationFrom;
  String m_lastValuesAnimationTo;
  CalcMode m_calcMode;
  AnimationMode m_animationMode;
};

}  // namespace blink

#endif  // SVGAnimationElement_h

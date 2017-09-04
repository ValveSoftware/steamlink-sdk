/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef SVGAnimateElement_h
#define SVGAnimateElement_h

#include "core/CoreExport.h"
#include "core/SVGNames.h"
#include "core/svg/SVGAnimatedTypeAnimator.h"
#include "core/svg/SVGAnimationElement.h"
#include "platform/heap/Handle.h"
#include <base/gtest_prod_util.h>

namespace blink {

// If we have 'inherit' as animation value, we need to grab the value
// during the animation since the value can be animated itself.
enum AnimatedPropertyValueType { RegularPropertyValue, InheritValue };

class CORE_EXPORT SVGAnimateElement : public SVGAnimationElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SVGAnimateElement* create(Document&);
  ~SVGAnimateElement() override;

  DECLARE_VIRTUAL_TRACE();

  bool isSVGAnimationAttributeSettingJavaScriptURL(
      const Attribute&) const override;

  AnimatedPropertyType animatedPropertyType();
  bool animatedPropertyTypeSupportsAddition();

 protected:
  SVGAnimateElement(const QualifiedName&, Document&);

  void resetAnimatedType() final;
  void clearAnimatedType() final;

  bool calculateToAtEndOfDurationValue(
      const String& toAtEndOfDurationString) final;
  bool calculateFromAndToValues(const String& fromString,
                                const String& toString) final;
  bool calculateFromAndByValues(const String& fromString,
                                const String& byString) final;
  void calculateAnimatedValue(float percentage,
                              unsigned repeatCount,
                              SVGSMILElement* resultElement) final;
  void applyResultsToTarget() final;
  float calculateDistance(const String& fromString,
                          const String& toString) final;
  bool isAdditive() final;

  void parseAttribute(const QualifiedName&,
                      const AtomicString&,
                      const AtomicString&) override;
  void svgAttributeChanged(const QualifiedName&) override;

  void setTargetElement(SVGElement*) final;
  void setAttributeName(const QualifiedName&) final;

  enum AttributeType { AttributeTypeCSS, AttributeTypeXML, AttributeTypeAuto };
  AttributeType getAttributeType() const { return m_attributeType; }

  FRIEND_TEST_ALL_PREFIXES(UnsafeSVGAttributeSanitizationTest,
                           stringsShouldNotSupportAddition);

 private:
  void resetAnimatedPropertyType();

  bool shouldApplyAnimation(const SVGElement& targetElement,
                            const QualifiedName& attributeName);

  void setAttributeType(const AtomicString&);

  void checkInvalidCSSAttributeType();
  bool hasInvalidCSSAttributeType() const {
    return m_hasInvalidCSSAttributeType;
  }
  bool hasValidTarget() final;
  bool hasValidAttributeName() const;
  virtual bool hasValidAttributeType();

  SVGPropertyBase* adjustForInheritance(SVGPropertyBase*,
                                        AnimatedPropertyValueType) const;

  Member<SVGPropertyBase> m_fromProperty;
  Member<SVGPropertyBase> m_toProperty;
  Member<SVGPropertyBase> m_toAtEndOfDurationProperty;
  Member<SVGPropertyBase> m_animatedProperty;

  SVGAnimatedTypeAnimator m_animator;

  AnimatedPropertyValueType m_fromPropertyValueType;
  AnimatedPropertyValueType m_toPropertyValueType;
  AttributeType m_attributeType;
  bool m_hasInvalidCSSAttributeType;
};

inline bool isSVGAnimateElement(const SVGElement& element) {
  return element.hasTagName(SVGNames::animateTag) ||
         element.hasTagName(SVGNames::animateTransformTag) ||
         element.hasTagName(SVGNames::setTag);
}

DEFINE_SVGELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGAnimateElement);

}  // namespace blink

#endif  // SVGAnimateElement_h

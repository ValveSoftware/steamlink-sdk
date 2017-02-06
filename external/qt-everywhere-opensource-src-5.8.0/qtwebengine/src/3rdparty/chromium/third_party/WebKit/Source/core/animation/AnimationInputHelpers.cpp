// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/AnimationInputHelpers.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/SVGNames.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/CSSToStyleMap.h"
#include "core/frame/Deprecation.h"
#include "core/svg/SVGElement.h"
#include "core/svg/animation/SVGSMILElement.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

const char kSVGPrefix[] = "svg-";
const unsigned kSVGPrefixLength = sizeof(kSVGPrefix) - 1;

static bool isSVGPrefixed(const String& property)
{
    return property.startsWith(kSVGPrefix);
}

static String removeSVGPrefix(const String& property)
{
    ASSERT(isSVGPrefixed(property));
    return property.substring(kSVGPrefixLength);
}

CSSPropertyID AnimationInputHelpers::keyframeAttributeToCSSProperty(const String& property, const Document& document)
{
    // Disallow prefixed properties.
    if (property[0] == '-' || isASCIIUpper(property[0]))
        return CSSPropertyInvalid;
    if (property == "cssFloat")
        return CSSPropertyFloat;

    StringBuilder builder;
    for (size_t i = 0; i < property.length(); ++i) {
        // Disallow hyphenated properties.
        if (property[i] == '-') {
            if (cssPropertyID(property) != CSSPropertyInvalid)
                Deprecation::countDeprecation(document, UseCounter::WebAnimationHyphenatedProperty);
            return CSSPropertyInvalid;
        }
        if (isASCIIUpper(property[i]))
            builder.append('-');
        builder.append(property[i]);
    }
    return cssPropertyID(builder.toString());
}

CSSPropertyID AnimationInputHelpers::keyframeAttributeToPresentationAttribute(const String& property, const Element& element)
{
    if (!RuntimeEnabledFeatures::webAnimationsSVGEnabled() || !element.isSVGElement() || !isSVGPrefixed(property))
        return CSSPropertyInvalid;

    String unprefixedProperty = removeSVGPrefix(property);
    if (SVGElement::isAnimatableCSSProperty(QualifiedName(nullAtom, AtomicString(unprefixedProperty), nullAtom)))
        return cssPropertyID(unprefixedProperty);

    return CSSPropertyInvalid;
}

using AttributeNameMap = HashMap<QualifiedName, const QualifiedName*>;

const AttributeNameMap& getSupportedAttributes()
{
    DEFINE_STATIC_LOCAL(AttributeNameMap, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        // Fill the set for the first use.
        // Animatable attributes from http://www.w3.org/TR/SVG/attindex.html
        const QualifiedName* attributes[] = {
            &HTMLNames::classAttr,
            &SVGNames::amplitudeAttr,
            &SVGNames::azimuthAttr,
            &SVGNames::baseFrequencyAttr,
            &SVGNames::biasAttr,
            &SVGNames::clipPathUnitsAttr,
            &SVGNames::cxAttr,
            &SVGNames::cyAttr,
            &SVGNames::dAttr,
            &SVGNames::diffuseConstantAttr,
            &SVGNames::divisorAttr,
            &SVGNames::dxAttr,
            &SVGNames::dyAttr,
            &SVGNames::edgeModeAttr,
            &SVGNames::elevationAttr,
            &SVGNames::exponentAttr,
            &SVGNames::filterUnitsAttr,
            &SVGNames::fxAttr,
            &SVGNames::fyAttr,
            &SVGNames::gradientTransformAttr,
            &SVGNames::gradientUnitsAttr,
            &SVGNames::heightAttr,
            &SVGNames::hrefAttr,
            &SVGNames::in2Attr,
            &SVGNames::inAttr,
            &SVGNames::interceptAttr,
            &SVGNames::k1Attr,
            &SVGNames::k2Attr,
            &SVGNames::k3Attr,
            &SVGNames::k4Attr,
            &SVGNames::kernelMatrixAttr,
            &SVGNames::kernelUnitLengthAttr,
            &SVGNames::lengthAdjustAttr,
            &SVGNames::limitingConeAngleAttr,
            &SVGNames::markerHeightAttr,
            &SVGNames::markerUnitsAttr,
            &SVGNames::markerWidthAttr,
            &SVGNames::maskContentUnitsAttr,
            &SVGNames::maskUnitsAttr,
            &SVGNames::methodAttr,
            &SVGNames::modeAttr,
            &SVGNames::numOctavesAttr,
            &SVGNames::offsetAttr,
            &SVGNames::operatorAttr,
            &SVGNames::orderAttr,
            &SVGNames::orientAttr,
            &SVGNames::pathLengthAttr,
            &SVGNames::patternContentUnitsAttr,
            &SVGNames::patternTransformAttr,
            &SVGNames::patternUnitsAttr,
            &SVGNames::pointsAtXAttr,
            &SVGNames::pointsAtYAttr,
            &SVGNames::pointsAtZAttr,
            &SVGNames::pointsAttr,
            &SVGNames::preserveAlphaAttr,
            &SVGNames::preserveAspectRatioAttr,
            &SVGNames::primitiveUnitsAttr,
            &SVGNames::rAttr,
            &SVGNames::radiusAttr,
            &SVGNames::refXAttr,
            &SVGNames::refYAttr,
            &SVGNames::resultAttr,
            &SVGNames::rotateAttr,
            &SVGNames::rxAttr,
            &SVGNames::ryAttr,
            &SVGNames::scaleAttr,
            &SVGNames::seedAttr,
            &SVGNames::slopeAttr,
            &SVGNames::spacingAttr,
            &SVGNames::specularConstantAttr,
            &SVGNames::specularExponentAttr,
            &SVGNames::spreadMethodAttr,
            &SVGNames::startOffsetAttr,
            &SVGNames::stdDeviationAttr,
            &SVGNames::stitchTilesAttr,
            &SVGNames::surfaceScaleAttr,
            &SVGNames::tableValuesAttr,
            &SVGNames::targetAttr,
            &SVGNames::targetXAttr,
            &SVGNames::targetYAttr,
            &SVGNames::textLengthAttr,
            &SVGNames::transformAttr,
            &SVGNames::typeAttr,
            &SVGNames::valuesAttr,
            &SVGNames::viewBoxAttr,
            &SVGNames::widthAttr,
            &SVGNames::x1Attr,
            &SVGNames::x2Attr,
            &SVGNames::xAttr,
            &SVGNames::xChannelSelectorAttr,
            &SVGNames::y1Attr,
            &SVGNames::y2Attr,
            &SVGNames::yAttr,
            &SVGNames::yChannelSelectorAttr,
            &SVGNames::zAttr,
        };
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(attributes); i++) {
            ASSERT(!SVGElement::isAnimatableCSSProperty(*attributes[i]));
            supportedAttributes.set(*attributes[i], attributes[i]);
        }
    }
    return supportedAttributes;
}

QualifiedName svgAttributeName(const String& property)
{
    ASSERT(!isSVGPrefixed(property));
    return QualifiedName(nullAtom, AtomicString(property), nullAtom);
}

const QualifiedName* AnimationInputHelpers::keyframeAttributeToSVGAttribute(const String& property, Element& element)
{
    if (!RuntimeEnabledFeatures::webAnimationsSVGEnabled() || !element.isSVGElement() || !isSVGPrefixed(property))
        return nullptr;

    SVGElement& svgElement = toSVGElement(element);
    if (isSVGSMILElement(svgElement))
        return nullptr;

    String unprefixedProperty = removeSVGPrefix(property);
    QualifiedName attributeName = svgAttributeName(unprefixedProperty);
    const AttributeNameMap& supportedAttributes = getSupportedAttributes();
    auto iter = supportedAttributes.find(attributeName);
    if (iter == supportedAttributes.end() || !svgElement.propertyFromAttribute(*iter->value))
        return nullptr;

    return iter->value;
}

PassRefPtr<TimingFunction> AnimationInputHelpers::parseTimingFunction(const String& string, Document* document, ExceptionState& exceptionState)
{
    if (string.isEmpty()) {
        exceptionState.throwTypeError("Easing may not be the empty string");
        return nullptr;
    }

    CSSValue* value = CSSParser::parseSingleValue(CSSPropertyTransitionTimingFunction, string);
    if (!value || !value->isValueList()) {
        ASSERT(!value || value->isCSSWideKeyword());
        bool throwTypeError = true;
        if (document) {
            if (string.startsWith("function")) {
                // Due to a bug in old versions of the web-animations-next
                // polyfill, in some circumstances the string passed in here
                // may be a Javascript function instead of the allowed values
                // from the spec
                // (http://w3c.github.io/web-animations/#dom-animationeffecttimingreadonly-easing)
                // This bug was fixed in
                // https://github.com/web-animations/web-animations-next/pull/423
                // and we want to track how often it is still being hit. The
                // linear case is special because 'linear' is the default value
                // for easing. See http://crbug.com/601672
                if (string == "function (a){return a}") {
                    Deprecation::countDeprecation(*document, UseCounter::WebAnimationsEasingAsFunctionLinear);
                    throwTypeError = false;
                } else {
                    UseCounter::count(*document, UseCounter::WebAnimationsEasingAsFunctionOther);
                }
            }
        }

        // TODO(suzyh): This return clause exists so that the special linear
        // function case above is exempted from causing TypeErrors. The
        // throwTypeError bool and this if-statement should be removed after the
        // M53 branch point in July 2016, so that this case will also throw
        // TypeErrors from M54 onward.
        if (!throwTypeError) {
            return Timing::defaults().timingFunction;
        }

        exceptionState.throwTypeError("'" + string + "' is not a valid value for easing");
        return nullptr;
    }
    CSSValueList* valueList = toCSSValueList(value);
    if (valueList->length() > 1) {
        exceptionState.throwTypeError("Easing may not be set to a list of values");
        return nullptr;
    }
    return CSSToStyleMap::mapAnimationTimingFunction(valueList->item(0), true);
}

} // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSInterpolationType.h"

#include "core/StylePropertyShorthand.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/resolver/CSSVariableResolver.h"
#include "core/css/resolver/StyleResolverState.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class ResolvedVariableChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<ResolvedVariableChecker> create(CSSPropertyID property, const CSSVariableReferenceValue* variableReference, const CSSValue* resolvedValue)
    {
        return wrapUnique(new ResolvedVariableChecker(property, variableReference, resolvedValue));
    }

private:
    ResolvedVariableChecker(CSSPropertyID property, const CSSVariableReferenceValue* variableReference, const CSSValue* resolvedValue)
        : m_property(property)
        , m_variableReference(variableReference)
        , m_resolvedValue(resolvedValue)
    { }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue& underlying) const final
    {
        // TODO(alancutter): Just check the variables referenced instead of doing a full CSSValue resolve.
        CSSValue* resolvedValue = CSSVariableResolver::resolveVariableReferences(environment.state().style()->variables(), m_property, *m_variableReference);
        return m_resolvedValue->equals(*resolvedValue);
    }

    CSSPropertyID m_property;
    Persistent<const CSSVariableReferenceValue> m_variableReference;
    Persistent<const CSSValue> m_resolvedValue;
};

InterpolationValue CSSInterpolationType::maybeConvertSingle(const PropertySpecificKeyframe& keyframe, const InterpolationEnvironment& environment, const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
{
    CSSValue* resolvedCSSValueOwner;
    const CSSValue* value = toCSSPropertySpecificKeyframe(keyframe).value();

    if (!value)
        return maybeConvertNeutral(underlying, conversionCheckers);

    // TODO(alancutter): Support animation of var() in shorthand properties.
    if (value->isPendingSubstitutionValue())
        return nullptr;

    if (value->isVariableReferenceValue() && !isShorthandProperty(cssProperty())) {
        resolvedCSSValueOwner = CSSVariableResolver::resolveVariableReferences(environment.state().style()->variables(), cssProperty(), toCSSVariableReferenceValue(*value));
        conversionCheckers.append(ResolvedVariableChecker::create(cssProperty(), toCSSVariableReferenceValue(value), resolvedCSSValueOwner));
        value = resolvedCSSValueOwner;
    }

    if (value->isInitialValue() || (value->isUnsetValue() && !CSSPropertyMetadata::isInheritedProperty(cssProperty())))
        return maybeConvertInitial(environment.state(), conversionCheckers);

    if (value->isInheritedValue() || (value->isUnsetValue() && CSSPropertyMetadata::isInheritedProperty(cssProperty())))
        return maybeConvertInherit(environment.state(), conversionCheckers);

    return maybeConvertValue(*value, environment.state(), conversionCheckers);
}

} // namespace blink

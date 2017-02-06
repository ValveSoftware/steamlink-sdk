/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#include "core/css/StylePropertySerializer.h"

#include "core/CSSValueKeywords.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSPendingSubstitutionValue.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/CSSValuePool.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"
#include <bitset>

namespace blink {

StylePropertySerializer::StylePropertySetForSerializer::StylePropertySetForSerializer(const StylePropertySet& properties)
    : m_propertySet(&properties)
    , m_allIndex(m_propertySet->findPropertyIndex(CSSPropertyAll))
    , m_needToExpandAll(false)
{
    if (!hasAllProperty())
        return;

    StylePropertySet::PropertyReference allProperty = m_propertySet->propertyAt(m_allIndex);
    for (unsigned i = 0; i < m_propertySet->propertyCount(); ++i) {
        StylePropertySet::PropertyReference property = m_propertySet->propertyAt(i);
        if (CSSProperty::isAffectedByAllProperty(property.id())) {
            if (allProperty.isImportant() && !property.isImportant())
                continue;
            if (static_cast<unsigned>(m_allIndex) >= i)
                continue;
            if (property.value()->equals(*allProperty.value())
                && property.isImportant() == allProperty.isImportant())
                continue;
            m_needToExpandAll = true;
        }
        if (property.id() < firstCSSProperty || property.id() > lastCSSProperty)
            continue;
        m_longhandPropertyUsed.set(property.id() - firstCSSProperty);
    }
}

DEFINE_TRACE(StylePropertySerializer::StylePropertySetForSerializer)
{
    visitor->trace(m_propertySet);
}

unsigned StylePropertySerializer::StylePropertySetForSerializer::propertyCount() const
{
    if (!hasExpandedAllProperty())
        return m_propertySet->propertyCount();
    return lastCSSProperty - firstCSSProperty + 1;
}

StylePropertySerializer::PropertyValueForSerializer StylePropertySerializer::StylePropertySetForSerializer::propertyAt(unsigned index) const
{
    if (!hasExpandedAllProperty())
        return StylePropertySerializer::PropertyValueForSerializer(m_propertySet->propertyAt(index));

    CSSPropertyID propertyID = static_cast<CSSPropertyID>(index + firstCSSProperty);
    ASSERT(firstCSSProperty <= propertyID && propertyID <= lastCSSProperty);
    if (m_longhandPropertyUsed.test(index)) {
        int index = m_propertySet->findPropertyIndex(propertyID);
        ASSERT(index != -1);
        return StylePropertySerializer::PropertyValueForSerializer(m_propertySet->propertyAt(index));
    }

    StylePropertySet::PropertyReference property = m_propertySet->propertyAt(m_allIndex);
    return StylePropertySerializer::PropertyValueForSerializer(propertyID, property.value(), property.isImportant());
}

bool StylePropertySerializer::StylePropertySetForSerializer::shouldProcessPropertyAt(unsigned index) const
{
    // StylePropertySet has all valid longhands. We should process.
    if (!hasAllProperty())
        return true;

    // If all is not expanded, we need to process "all" and properties which
    // are not overwritten by "all".
    if (!m_needToExpandAll) {
        StylePropertySet::PropertyReference property = m_propertySet->propertyAt(index);
        if (property.id() == CSSPropertyAll || !CSSProperty::isAffectedByAllProperty(property.id()))
            return true;
        if (property.id() < firstCSSProperty || property.id() > lastCSSProperty)
            return false;
        return m_longhandPropertyUsed.test(property.id() - firstCSSProperty);
    }

    CSSPropertyID propertyID = static_cast<CSSPropertyID>(index + firstCSSProperty);
    ASSERT(firstCSSProperty <= propertyID && propertyID <= lastCSSProperty);

    // Since "all" is expanded, we don't need to process "all".
    // We should not process expanded shorthands (e.g. font, background,
    // and so on) either.
    if (isShorthandProperty(propertyID) || propertyID == CSSPropertyAll)
        return false;

    // The all property is a shorthand that resets all CSS properties except
    // direction and unicode-bidi. It only accepts the CSS-wide keywords.
    // c.f. http://dev.w3.org/csswg/css-cascade/#all-shorthand
    if (!CSSProperty::isAffectedByAllProperty(propertyID))
        return m_longhandPropertyUsed.test(index);

    return true;
}

int StylePropertySerializer::StylePropertySetForSerializer::findPropertyIndex(CSSPropertyID propertyID) const
{
    if (!hasExpandedAllProperty())
        return m_propertySet->findPropertyIndex(propertyID);
    return propertyID - firstCSSProperty;
}

const CSSValue* StylePropertySerializer::StylePropertySetForSerializer::getPropertyCSSValue(CSSPropertyID propertyID) const
{
    int index = findPropertyIndex(propertyID);
    if (index == -1)
        return nullptr;
    StylePropertySerializer::PropertyValueForSerializer value = propertyAt(index);
    return value.value();
}

String StylePropertySerializer::StylePropertySetForSerializer::getPropertyValue(CSSPropertyID propertyID) const
{
    if (!hasExpandedAllProperty())
        return m_propertySet->getPropertyValue(propertyID);

    const CSSValue* value = getPropertyCSSValue(propertyID);
    if (!value)
        return String();
    return value->cssText();
}

bool StylePropertySerializer::StylePropertySetForSerializer::isPropertyImplicit(CSSPropertyID propertyID) const
{
    int index = findPropertyIndex(propertyID);
    if (index == -1)
        return false;
    StylePropertySerializer::PropertyValueForSerializer value = propertyAt(index);
    return value.isImplicit();
}

bool StylePropertySerializer::StylePropertySetForSerializer::propertyIsImportant(CSSPropertyID propertyID) const
{
    int index = findPropertyIndex(propertyID);
    if (index == -1)
        return false;
    StylePropertySerializer::PropertyValueForSerializer value = propertyAt(index);
    return value.isImportant();
}

StylePropertySerializer::StylePropertySerializer(const StylePropertySet& properties)
    : m_propertySet(properties)
{
}

String StylePropertySerializer::getCustomPropertyText(const PropertyValueForSerializer& property, bool isNotFirstDecl) const
{
    ASSERT(property.id() == CSSPropertyVariable);
    StringBuilder result;
    if (isNotFirstDecl)
        result.append(' ');
    const CSSCustomPropertyDeclaration* value = toCSSCustomPropertyDeclaration(property.value());
    result.append(value->name());
    result.append(':');
    result.append(value->customCSSText());
    if (property.isImportant())
        result.append(" !important");
    result.append(';');
    return result.toString();
}

static String getApplyAtRuleText(const CSSValue* value, bool isNotFirstDecl)
{
    StringBuilder result;
    if (isNotFirstDecl)
        result.append(' ');
    result.append("@apply ");
    result.append(toCSSCustomIdentValue(value)->value());
    result.append(';');
    return result.toString();
}

String StylePropertySerializer::getPropertyText(CSSPropertyID propertyID, const String& value, bool isImportant, bool isNotFirstDecl) const
{
    StringBuilder result;
    if (isNotFirstDecl)
        result.append(' ');
    result.append(getPropertyName(propertyID));
    result.append(": ");
    result.append(value);
    if (isImportant)
        result.append(" !important");
    result.append(';');
    return result.toString();
}

String StylePropertySerializer::asText() const
{
    StringBuilder result;

    std::bitset<numCSSProperties> longhandSerialized;
    std::bitset<numCSSProperties> shorthandAppeared;

    unsigned size = m_propertySet.propertyCount();
    unsigned numDecls = 0;
    for (unsigned n = 0; n < size; ++n) {
        if (!m_propertySet.shouldProcessPropertyAt(n))
            continue;

        StylePropertySerializer::PropertyValueForSerializer property = m_propertySet.propertyAt(n);
        CSSPropertyID propertyID = property.id();
        // Only enabled properties should be part of the style.
        ASSERT(CSSPropertyMetadata::isEnabledProperty(propertyID));
        // Shorthands with variable references are not expanded at parse time
        // and hence may still be observed during serialization.
        ASSERT(!isShorthandProperty(propertyID) || property.value()->isVariableReferenceValue());

        switch (propertyID) {
        case CSSPropertyVariable:
            result.append(getCustomPropertyText(property, numDecls++));
            continue;
        case CSSPropertyAll:
            result.append(getPropertyText(propertyID, property.value()->cssText(), property.isImportant(), numDecls++));
            continue;
        case CSSPropertyApplyAtRule:
            result.append(getApplyAtRuleText(property.value(), numDecls++));
            continue;
        default:
            break;
        }
        if (longhandSerialized.test(propertyID - firstCSSProperty))
            continue;

        Vector<StylePropertyShorthand, 4> shorthands;
        getMatchingShorthandsForLonghand(propertyID, &shorthands);
        bool serializedAsShorthand = false;
        for (const StylePropertyShorthand& shorthand : shorthands) {
            // Some aliases are implemented as a shorthand, in which case
            // we prefer to not use the shorthand.
            if (shorthand.length() == 1)
                continue;

            CSSPropertyID shorthandProperty = shorthand.id();
            int shorthandPropertyIndex = shorthandProperty - firstCSSProperty;
            // TODO(timloh): Do we actually need this check? A previous comment
            // said "old UAs can't recognize them but are important for editing"
            // but Firefox doesn't do this.
            if (shorthandProperty == CSSPropertyFont)
                continue;
            // We already tried serializing as this shorthand
            if (shorthandAppeared.test(shorthandPropertyIndex))
                continue;

            shorthandAppeared.set(shorthandPropertyIndex);
            bool serializedOtherLonghand = false;
            for (unsigned i = 0; i < shorthand.length(); i++) {
                if (longhandSerialized.test(shorthand.properties()[i] - firstCSSProperty)) {
                    serializedOtherLonghand = true;
                    break;
                }
            }
            if (serializedOtherLonghand)
                continue;

            String shorthandResult = StylePropertySerializer::getPropertyValue(shorthandProperty);
            if (shorthandResult.isEmpty())
                continue;

            result.append(getPropertyText(shorthandProperty, shorthandResult, property.isImportant(), numDecls++));
            serializedAsShorthand = true;
            for (unsigned i = 0; i < shorthand.length(); i++)
                longhandSerialized.set(shorthand.properties()[i] - firstCSSProperty);
            break;
        }

        if (serializedAsShorthand)
            continue;

        result.append(getPropertyText(propertyID, property.value()->cssText(), property.isImportant(), numDecls++));
    }

    ASSERT(!numDecls ^ !result.isEmpty());
    return result.toString();
}

// As per css-cascade, shorthands do not expand longhands to the value
// "initial", except when the shorthand is set to "initial", instead
// setting "missing" sub-properties to their initial values. This means
// that a shorthand can never represent a list of subproperties where
// some are "initial" and some are not, and so serialization should
// always fail in these cases (as per cssom). However we currently use
// "initial" instead of the initial values for certain shorthands, so
// these are special-cased here.
// TODO(timloh): Don't use "initial" in shorthands and remove this
// special-casing
static bool allowInitialInShorthand(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyBorder:
    case CSSPropertyBorderTop:
    case CSSPropertyBorderRight:
    case CSSPropertyBorderBottom:
    case CSSPropertyBorderLeft:
    case CSSPropertyOutline:
    case CSSPropertyColumnRule:
    case CSSPropertyColumns:
    case CSSPropertyFlex:
    case CSSPropertyFlexFlow:
    case CSSPropertyGridColumn:
    case CSSPropertyGridRow:
    case CSSPropertyGridArea:
    case CSSPropertyGridGap:
    case CSSPropertyMotion:
    case CSSPropertyWebkitMarginCollapse:
    case CSSPropertyListStyle:
    case CSSPropertyWebkitTextEmphasis:
    case CSSPropertyWebkitTextStroke:
        return true;
    default:
        return false;
    }
}

// TODO(timloh): This should go away eventually, see crbug.com/471917
static bool allowImplicitInitialInShorthand(CSSPropertyID propertyID)
{
    return propertyID == CSSPropertyBackground || propertyID == CSSPropertyWebkitMask;
}

String StylePropertySerializer::commonShorthandChecks(const StylePropertyShorthand& shorthand) const
{
    int longhandCount = shorthand.length();
    DCHECK_LE(longhandCount, 17);
    const CSSValue* longhands[17] = {};

    bool hasImportant = false;
    bool hasNonImportant = false;

    for (int i = 0; i < longhandCount; i++) {
        int index = m_propertySet.findPropertyIndex(shorthand.properties()[i]);
        if (index == -1)
            return emptyString();
        PropertyValueForSerializer value = m_propertySet.propertyAt(index);

        hasImportant |= value.isImportant();
        hasNonImportant |= !value.isImportant();
        longhands[i] = value.value();
    }

    if (hasImportant && hasNonImportant)
        return emptyString();

    // TODO(timloh): This should be isCSSWideKeyword()
    if (longhands[0]->isInitialValue() || longhands[0]->isInheritedValue()
        || longhands[0]->isPendingSubstitutionValue()) {
        bool success = true;
        for (int i = 1; i < longhandCount; i++) {
            if (!longhands[i]->equals(*longhands[0])) {
                // This should just return emptyString() but some shorthands currently
                // allow 'initial' for their longhands.
                success = false;
                break;
            }
        }
        if (success) {
            if (longhands[0]->isPendingSubstitutionValue())
                return toCSSPendingSubstitutionValue(longhands[0])->shorthandValue()->cssText();
            return longhands[0]->cssText();
        }
    }

    bool allowInitial = allowInitialInShorthand(shorthand.id());
    bool allowImplicitInitial = allowInitial || allowImplicitInitialInShorthand(shorthand.id());
    for (int i = 0; i < longhandCount; i++) {
        const CSSValue& value = *longhands[i];
        if (value.isImplicitInitialValue()) {
            if (allowImplicitInitial)
                continue;
            return emptyString();
        }
        if (!allowInitial && value.isInitialValue())
            return emptyString();
        // TODO(timloh): This should also check unset
        if (value.isInheritedValue() || value.isPendingSubstitutionValue())
            return emptyString();
        if (value.isVariableReferenceValue())
            return emptyString();
    }

    return String();
}

String StylePropertySerializer::getPropertyValue(CSSPropertyID propertyID) const
{
    const StylePropertyShorthand& shorthand = shorthandForProperty(propertyID);
    // TODO(timloh): This is weird, why do we call this with non-shorthands at all?
    if (!shorthand.length())
        return String();

    String result = commonShorthandChecks(shorthand);
    if (!result.isNull())
        return result;

    switch (propertyID) {
    case CSSPropertyAnimation:
        return getLayeredShorthandValue(animationShorthand());
    case CSSPropertyBorderSpacing:
        return borderSpacingValue(borderSpacingShorthand());
    case CSSPropertyBackgroundPosition:
        return getLayeredShorthandValue(backgroundPositionShorthand());
    case CSSPropertyBackgroundRepeat:
        return backgroundRepeatPropertyValue();
    case CSSPropertyBackground:
        return getLayeredShorthandValue(backgroundShorthand());
    case CSSPropertyBorder:
        return borderPropertyValue();
    case CSSPropertyBorderTop:
        return getShorthandValue(borderTopShorthand());
    case CSSPropertyBorderRight:
        return getShorthandValue(borderRightShorthand());
    case CSSPropertyBorderBottom:
        return getShorthandValue(borderBottomShorthand());
    case CSSPropertyBorderLeft:
        return getShorthandValue(borderLeftShorthand());
    case CSSPropertyOutline:
        return getShorthandValue(outlineShorthand());
    case CSSPropertyBorderColor:
        return get4Values(borderColorShorthand());
    case CSSPropertyBorderWidth:
        return get4Values(borderWidthShorthand());
    case CSSPropertyBorderStyle:
        return get4Values(borderStyleShorthand());
    case CSSPropertyColumnRule:
        return getShorthandValue(columnRuleShorthand());
    case CSSPropertyColumns:
        return getShorthandValue(columnsShorthand());
    case CSSPropertyFlex:
        return getShorthandValue(flexShorthand());
    case CSSPropertyFlexFlow:
        return getShorthandValue(flexFlowShorthand());
    case CSSPropertyGridColumn:
        return getShorthandValue(gridColumnShorthand(), " / ");
    case CSSPropertyGridRow:
        return getShorthandValue(gridRowShorthand(), " / ");
    case CSSPropertyGridArea:
        return getShorthandValue(gridAreaShorthand(), " / ");
    case CSSPropertyGridGap:
        return getShorthandValue(gridGapShorthand());
    case CSSPropertyFont:
        return fontValue();
    case CSSPropertyFontVariant:
        return fontVariantValue();
    case CSSPropertyMargin:
        return get4Values(marginShorthand());
    case CSSPropertyMotion:
        return getShorthandValue(motionShorthand());
    case CSSPropertyWebkitMarginCollapse:
        return getShorthandValue(webkitMarginCollapseShorthand());
    case CSSPropertyOverflow:
        return getCommonValue(overflowShorthand());
    case CSSPropertyPadding:
        return get4Values(paddingShorthand());
    case CSSPropertyTransition:
        return getLayeredShorthandValue(transitionShorthand());
    case CSSPropertyListStyle:
        return getShorthandValue(listStyleShorthand());
    case CSSPropertyWebkitMaskPosition:
        return getLayeredShorthandValue(webkitMaskPositionShorthand());
    case CSSPropertyWebkitMaskRepeat:
        return getLayeredShorthandValue(webkitMaskRepeatShorthand());
    case CSSPropertyWebkitMask:
        return getLayeredShorthandValue(webkitMaskShorthand());
    case CSSPropertyWebkitTextEmphasis:
        return getShorthandValue(webkitTextEmphasisShorthand());
    case CSSPropertyWebkitTextStroke:
        return getShorthandValue(webkitTextStrokeShorthand());
    case CSSPropertyMarker: {
        if (const CSSValue* value = m_propertySet.getPropertyCSSValue(CSSPropertyMarkerStart))
            return value->cssText();
        return String();
    }
    case CSSPropertyBorderRadius:
        return get4Values(borderRadiusShorthand());
    default:
        return String();
    }
}

String StylePropertySerializer::borderSpacingValue(const StylePropertyShorthand& shorthand) const
{
    const CSSValue* horizontalValue = m_propertySet.getPropertyCSSValue(shorthand.properties()[0]);
    const CSSValue* verticalValue = m_propertySet.getPropertyCSSValue(shorthand.properties()[1]);

    String horizontalValueCSSText = horizontalValue->cssText();
    String verticalValueCSSText = verticalValue->cssText();
    if (horizontalValueCSSText == verticalValueCSSText)
        return horizontalValueCSSText;
    return horizontalValueCSSText + ' ' + verticalValueCSSText;
}

void StylePropertySerializer::appendFontLonghandValueIfNotNormal(CSSPropertyID propertyID, StringBuilder& result) const
{
    int foundPropertyIndex = m_propertySet.findPropertyIndex(propertyID);
    ASSERT(foundPropertyIndex != -1);

    const CSSValue* val = m_propertySet.propertyAt(foundPropertyIndex).value();
    if (val->isPrimitiveValue() && toCSSPrimitiveValue(val)->getValueID() == CSSValueNormal)
        return;

    char prefix = '\0';
    switch (propertyID) {
    case CSSPropertyFontStyle:
        break; // No prefix.
    case CSSPropertyFontFamily:
    case CSSPropertyFontStretch:
    case CSSPropertyFontVariantCaps:
    case CSSPropertyFontVariantLigatures:
    case CSSPropertyFontVariantNumeric:
    case CSSPropertyFontWeight:
        prefix = ' ';
        break;
    case CSSPropertyLineHeight:
        prefix = '/';
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    if (prefix && !result.isEmpty())
        result.append(prefix);

    String value;
    // In the font-variant shorthand a "none" ligatures value needs to be expanded.
    if (propertyID == CSSPropertyFontVariantLigatures
        && val->isPrimitiveValue()
        && toCSSPrimitiveValue(val)->getValueID() == CSSValueNone) {
        value = "no-common-ligatures no-discretionary-ligatures no-historical-ligatures no-contextual";
    } else {
        value = m_propertySet.propertyAt(foundPropertyIndex).value()->cssText();
    }

    result.append(value);
}

String StylePropertySerializer::fontValue() const
{
    int fontSizePropertyIndex = m_propertySet.findPropertyIndex(CSSPropertyFontSize);
    int fontFamilyPropertyIndex = m_propertySet.findPropertyIndex(CSSPropertyFontFamily);
    int fontVariantCapsPropertyIndex = m_propertySet.findPropertyIndex(CSSPropertyFontVariantCaps);
    int fontVariantLigaturesPropertyIndex = m_propertySet.findPropertyIndex(CSSPropertyFontVariantLigatures);
    int fontVariantNumericPropertyIndex = m_propertySet.findPropertyIndex(CSSPropertyFontVariantNumeric);
    DCHECK_NE(fontSizePropertyIndex, -1);
    DCHECK_NE(fontFamilyPropertyIndex, -1);
    DCHECK_NE(fontVariantCapsPropertyIndex, -1);
    DCHECK_NE(fontVariantLigaturesPropertyIndex, -1);
    DCHECK_NE(fontVariantNumericPropertyIndex, -1);

    PropertyValueForSerializer fontSizeProperty = m_propertySet.propertyAt(fontSizePropertyIndex);
    PropertyValueForSerializer fontFamilyProperty = m_propertySet.propertyAt(fontFamilyPropertyIndex);
    PropertyValueForSerializer fontVariantCapsProperty = m_propertySet.propertyAt(fontVariantCapsPropertyIndex);
    PropertyValueForSerializer fontVariantLigaturesProperty = m_propertySet.propertyAt(fontVariantLigaturesPropertyIndex);
    PropertyValueForSerializer fontVariantNumericProperty = m_propertySet.propertyAt(fontVariantNumericPropertyIndex);

    // Check that non-initial font-variant subproperties are not conflicting with this serialization.
    const CSSValue* ligaturesValue = fontVariantLigaturesProperty.value();
    const CSSValue* numericValue = fontVariantNumericProperty.value();
    if ((ligaturesValue->isPrimitiveValue()
        && toCSSPrimitiveValue(ligaturesValue)->getValueID() != CSSValueNormal)
        || ligaturesValue->isValueList()
        || (numericValue->isPrimitiveValue()
        && toCSSPrimitiveValue(numericValue)->getValueID() != CSSValueNormal)
        || numericValue->isValueList())
        return emptyString();

    StringBuilder result;
    appendFontLonghandValueIfNotNormal(CSSPropertyFontStyle, result);

    const CSSValue* val = fontVariantCapsProperty.value();
    if (val->isPrimitiveValue()
        && (toCSSPrimitiveValue(val)->getValueID() != CSSValueSmallCaps
        && toCSSPrimitiveValue(val)->getValueID() != CSSValueNormal))
        return emptyString();
    appendFontLonghandValueIfNotNormal(CSSPropertyFontVariantCaps, result);

    appendFontLonghandValueIfNotNormal(CSSPropertyFontWeight, result);
    appendFontLonghandValueIfNotNormal(CSSPropertyFontStretch, result);
    if (!result.isEmpty())
        result.append(' ');
    result.append(fontSizeProperty.value()->cssText());
    appendFontLonghandValueIfNotNormal(CSSPropertyLineHeight, result);
    if (!result.isEmpty())
        result.append(' ');
    result.append(fontFamilyProperty.value()->cssText());
    return result.toString();
}

String StylePropertySerializer::fontVariantValue() const
{
    StringBuilder result;

    // TODO(drott): Decide how we want to return ligature values in shorthands, reduced to "none" or
    // spelled out, filed as W3C bug:
    // https://www.w3.org/Bugs/Public/show_bug.cgi?id=29594
    appendFontLonghandValueIfNotNormal(CSSPropertyFontVariantLigatures, result);
    appendFontLonghandValueIfNotNormal(CSSPropertyFontVariantCaps, result);
    appendFontLonghandValueIfNotNormal(CSSPropertyFontVariantNumeric, result);

    if (result.isEmpty()) {
        return "normal";
    }

    return result.toString();
}

String StylePropertySerializer::get4Values(const StylePropertyShorthand& shorthand) const
{
    // Assume the properties are in the usual order top, right, bottom, left.
    int topValueIndex = m_propertySet.findPropertyIndex(shorthand.properties()[0]);
    int rightValueIndex = m_propertySet.findPropertyIndex(shorthand.properties()[1]);
    int bottomValueIndex = m_propertySet.findPropertyIndex(shorthand.properties()[2]);
    int leftValueIndex = m_propertySet.findPropertyIndex(shorthand.properties()[3]);

    if (topValueIndex == -1 || rightValueIndex == -1 || bottomValueIndex == -1 || leftValueIndex == -1)
        return String();

    PropertyValueForSerializer top = m_propertySet.propertyAt(topValueIndex);
    PropertyValueForSerializer right = m_propertySet.propertyAt(rightValueIndex);
    PropertyValueForSerializer bottom = m_propertySet.propertyAt(bottomValueIndex);
    PropertyValueForSerializer left = m_propertySet.propertyAt(leftValueIndex);

    bool showLeft = !right.value()->equals(*left.value());
    bool showBottom = !top.value()->equals(*bottom.value()) || showLeft;
    bool showRight = !top.value()->equals(*right.value()) || showBottom;

    StringBuilder result;
    result.append(top.value()->cssText());
    if (showRight) {
        result.append(' ');
        result.append(right.value()->cssText());
    }
    if (showBottom) {
        result.append(' ');
        result.append(bottom.value()->cssText());
    }
    if (showLeft) {
        result.append(' ');
        result.append(left.value()->cssText());
    }
    return result.toString();
}

String StylePropertySerializer::getLayeredShorthandValue(const StylePropertyShorthand& shorthand) const
{
    const unsigned size = shorthand.length();

    // Begin by collecting the properties into a vector.
    HeapVector<Member<const CSSValue>> values(size);
    // If the below loop succeeds, there should always be at minimum 1 layer.
    size_t numLayers = 1U;

    for (size_t i = 0; i < size; i++) {
        values[i] = m_propertySet.getPropertyCSSValue(shorthand.properties()[i]);
        if (values[i]->isBaseValueList()) {
            const CSSValueList* valueList = toCSSValueList(values[i]);
            numLayers = std::max(numLayers, valueList->length());
        }
    }

    StringBuilder result;

    // Now stitch the properties together. Implicit initial values are flagged as such and
    // can safely be omitted.
    for (size_t layer = 0; layer < numLayers; layer++) {
        StringBuilder layerResult;
        bool useRepeatXShorthand = false;
        bool useRepeatYShorthand = false;
        bool useSingleWordShorthand = false;
        bool foundPositionXCSSProperty = false;
        bool foundPositionYCSSProperty = false;

        for (unsigned propertyIndex = 0; propertyIndex < size; propertyIndex++) {
            const CSSValue* value = nullptr;
            CSSPropertyID property = shorthand.properties()[propertyIndex];

            // Get a CSSValue for this property and layer.
            if (values[propertyIndex]->isBaseValueList()) {
                const CSSValueList* propertyValues = toCSSValueList(values[propertyIndex]);
                // There might not be an item for this layer for this property.
                if (layer < propertyValues->length())
                    value = &propertyValues->item(layer);
            } else if (layer == 0 || (layer != numLayers - 1 && property == CSSPropertyBackgroundColor)) {
                // Singletons except background color belong in the 0th layer.
                // Background color belongs in the last layer.
                value = values[propertyIndex];
            }
            // No point proceeding if there's not a value to look at.
            if (!value)
                continue;

            // Special case for background-repeat.
            if ((propertyIndex < size - 1 && m_propertySet.isPropertyImplicit(property))
                && (property == CSSPropertyBackgroundRepeatX || property == CSSPropertyWebkitMaskRepeatX)) {
                ASSERT(shorthand.properties()[propertyIndex + 1] == CSSPropertyBackgroundRepeatY
                    || shorthand.properties()[propertyIndex + 1] == CSSPropertyWebkitMaskRepeatY);
                const CSSValue& yValue = values[propertyIndex + 1]->isValueList() ?
                    toCSSValueList(values[propertyIndex + 1])->item(layer) : *values[propertyIndex + 1];


                // FIXME: At some point we need to fix this code to avoid returning an invalid shorthand,
                // since some longhand combinations are not serializable into a single shorthand.
                if (!value->isPrimitiveValue() || !yValue.isPrimitiveValue())
                    continue;

                CSSValueID xId = toCSSPrimitiveValue(value)->getValueID();
                CSSValueID yId = toCSSPrimitiveValue(yValue).getValueID();
                // Maybe advance propertyIndex to look at the next CSSValue in the list for the checks below.
                if (xId == yId) {
                    useSingleWordShorthand = true;
                    property = shorthand.properties()[++propertyIndex];
                } else if (xId == CSSValueRepeat && yId == CSSValueNoRepeat) {
                    useRepeatXShorthand = true;
                    property = shorthand.properties()[++propertyIndex];
                } else if (xId == CSSValueNoRepeat && yId == CSSValueRepeat) {
                    useRepeatYShorthand = true;
                    property = shorthand.properties()[++propertyIndex];
                }
            }

            if (!(value->isInitialValue() && toCSSInitialValue(value)->isImplicit())) {
                if (property == CSSPropertyBackgroundSize || property == CSSPropertyWebkitMaskSize) {
                    if (foundPositionYCSSProperty || foundPositionXCSSProperty)
                        layerResult.append(" / ");
                    else
                        layerResult.append(" 0% 0% / ");
                } else if (!layerResult.isEmpty()) {
                    // Do this second to avoid ending up with an extra space in the output if we hit the continue above.
                    layerResult.append(' ');
                }

                if (useRepeatXShorthand) {
                    useRepeatXShorthand = false;
                    layerResult.append(getValueName(CSSValueRepeatX));
                } else if (useRepeatYShorthand) {
                    useRepeatYShorthand = false;
                    layerResult.append(getValueName(CSSValueRepeatY));
                } else {
                    if (useSingleWordShorthand)
                        useSingleWordShorthand = false;
                    layerResult.append(value->cssText());
                }
                if (property == CSSPropertyBackgroundPositionX || property == CSSPropertyWebkitMaskPositionX)
                    foundPositionXCSSProperty = true;
                if (property == CSSPropertyBackgroundPositionY || property == CSSPropertyWebkitMaskPositionY) {
                    foundPositionYCSSProperty = true;
                    // background-position is a special case. If only the first offset is specified,
                    // the second one defaults to "center", not the same value.
                }
            }
        }
        if (!layerResult.isEmpty()) {
            if (!result.isEmpty())
                result.append(", ");
            result.append(layerResult);
        }
    }

    return result.toString();
}

String StylePropertySerializer::getShorthandValue(const StylePropertyShorthand& shorthand, String separator) const
{
    StringBuilder result;
    for (unsigned i = 0; i < shorthand.length(); ++i) {
        const CSSValue* value = m_propertySet.getPropertyCSSValue(shorthand.properties()[i]);
        String valueText = value->cssText();
        if (value->isInitialValue())
            continue;
        if (!result.isEmpty())
            result.append(separator);
        result.append(valueText);
    }
    return result.toString();
}

// only returns a non-null value if all properties have the same, non-null value
String StylePropertySerializer::getCommonValue(const StylePropertyShorthand& shorthand) const
{
    String res;
    for (unsigned i = 0; i < shorthand.length(); ++i) {
        const CSSValue* value = m_propertySet.getPropertyCSSValue(shorthand.properties()[i]);
        // FIXME: CSSInitialValue::cssText should generate the right value.
        String text = value->cssText();
        if (res.isNull())
            res = text;
        else if (res != text)
            return String();
    }
    return res;
}

String StylePropertySerializer::borderPropertyValue() const
{
    const StylePropertyShorthand properties[3] = { borderWidthShorthand(), borderStyleShorthand(), borderColorShorthand() };
    StringBuilder result;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(properties); ++i) {
        String value = getCommonValue(properties[i]);
        if (value.isNull())
            return String();
        if (value == "initial")
            continue;
        if (!result.isEmpty())
            result.append(' ');
        result.append(value);
    }
    return result.isEmpty() ? String() : result.toString();
}

static void appendBackgroundRepeatValue(StringBuilder& builder, const CSSValue& repeatXCSSValue, const CSSValue& repeatYCSSValue)
{
    // FIXME: Ensure initial values do not appear in CSS_VALUE_LISTS.
    DEFINE_STATIC_LOCAL(CSSPrimitiveValue, initialRepeatValue, (CSSPrimitiveValue::createIdentifier(CSSValueRepeat)));
    const CSSPrimitiveValue& repeatX = repeatXCSSValue.isInitialValue() ? initialRepeatValue : toCSSPrimitiveValue(repeatXCSSValue);
    const CSSPrimitiveValue& repeatY = repeatYCSSValue.isInitialValue() ? initialRepeatValue : toCSSPrimitiveValue(repeatYCSSValue);
    CSSValueID repeatXValueId = repeatX.getValueID();
    CSSValueID repeatYValueId = repeatY.getValueID();
    if (repeatXValueId == repeatYValueId) {
        builder.append(repeatX.cssText());
    } else if (repeatXValueId == CSSValueNoRepeat && repeatYValueId == CSSValueRepeat) {
        builder.append("repeat-y");
    } else if (repeatXValueId == CSSValueRepeat && repeatYValueId == CSSValueNoRepeat) {
        builder.append("repeat-x");
    } else {
        builder.append(repeatX.cssText());
        builder.append(" ");
        builder.append(repeatY.cssText());
    }
}

String StylePropertySerializer::backgroundRepeatPropertyValue() const
{
    const CSSValue& repeatX = *m_propertySet.getPropertyCSSValue(CSSPropertyBackgroundRepeatX);
    const CSSValue& repeatY = *m_propertySet.getPropertyCSSValue(CSSPropertyBackgroundRepeatY);

    const CSSValueList* repeatXList = 0;
    int repeatXLength = 1;
    if (repeatX.isValueList()) {
        repeatXList = &toCSSValueList(repeatX);
        repeatXLength = repeatXList->length();
    } else if (!repeatX.isPrimitiveValue()) {
        return String();
    }

    const CSSValueList* repeatYList = 0;
    int repeatYLength = 1;
    if (repeatY.isValueList()) {
        repeatYList = &toCSSValueList(repeatY);
        repeatYLength = repeatYList->length();
    } else if (!repeatY.isPrimitiveValue()) {
        return String();
    }

    size_t shorthandLength = lowestCommonMultiple(repeatXLength, repeatYLength);
    StringBuilder builder;
    for (size_t i = 0; i < shorthandLength; ++i) {
        if (i)
            builder.append(", ");

        const CSSValue& xValue = repeatXList ? repeatXList->item(i % repeatXList->length()) : repeatX;
        const CSSValue& yValue = repeatYList ? repeatYList->item(i % repeatYList->length()) : repeatY;
        appendBackgroundRepeatValue(builder, xValue, yValue);
    }
    return builder.toString();
}

} // namespace blink

/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2013 Apple Inc. All rights reserved.
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

#include "core/css/StylePropertySet.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/StylePropertySerializer.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParser.h"
#include "core/frame/UseCounter.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/text/StringBuilder.h"

#ifndef NDEBUG
#include "wtf/text/CString.h"
#include <stdio.h>
#endif

namespace blink {

static size_t sizeForImmutableStylePropertySetWithPropertyCount(unsigned count)
{
    return sizeof(ImmutableStylePropertySet) - sizeof(void*) + sizeof(Member<CSSValue>) * count + sizeof(StylePropertyMetadata) * count;
}

ImmutableStylePropertySet* ImmutableStylePropertySet::create(const CSSProperty* properties, unsigned count, CSSParserMode cssParserMode)
{
    ASSERT(count <= MaxArraySize);
    void* slot = ThreadHeap::allocate<StylePropertySet>(sizeForImmutableStylePropertySetWithPropertyCount(count));
    return new (slot) ImmutableStylePropertySet(properties, count, cssParserMode);
}

ImmutableStylePropertySet* StylePropertySet::immutableCopyIfNeeded() const
{
    if (!isMutable())
        return toImmutableStylePropertySet(const_cast<StylePropertySet*>(this));
    const MutableStylePropertySet* mutableThis = toMutableStylePropertySet(this);
    return ImmutableStylePropertySet::create(mutableThis->m_propertyVector.data(), mutableThis->m_propertyVector.size(), cssParserMode());
}

MutableStylePropertySet::MutableStylePropertySet(CSSParserMode cssParserMode)
    : StylePropertySet(cssParserMode)
{
}

MutableStylePropertySet::MutableStylePropertySet(const CSSProperty* properties, unsigned length)
    : StylePropertySet(HTMLStandardMode)
{
    m_propertyVector.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i)
        m_propertyVector.uncheckedAppend(properties[i]);
}

ImmutableStylePropertySet::ImmutableStylePropertySet(const CSSProperty* properties, unsigned length, CSSParserMode cssParserMode)
    : StylePropertySet(cssParserMode, length)
{
    StylePropertyMetadata* metadataArray = const_cast<StylePropertyMetadata*>(this->metadataArray());
    Member<const CSSValue>* valueArray = const_cast<Member<const CSSValue>*>(this->valueArray());
    for (unsigned i = 0; i < m_arraySize; ++i) {
        metadataArray[i] = properties[i].metadata();
        valueArray[i] = properties[i].value();
    }
}

ImmutableStylePropertySet::~ImmutableStylePropertySet()
{
}

// Convert property into an uint16_t for comparison with metadata's m_propertyID to avoid
// the compiler converting it to an int multiple times in a loop.
static uint16_t getConvertedCSSPropertyID(CSSPropertyID propertyID)
{
    return static_cast<uint16_t>(propertyID);
}

static uint16_t getConvertedCSSPropertyID(const AtomicString&)
{
    return static_cast<uint16_t>(CSSPropertyVariable);
}

static bool isPropertyMatch(const StylePropertyMetadata& metadata, const CSSValue&, uint16_t id, CSSPropertyID propertyID)
{
    ASSERT(id == propertyID);
    bool result = metadata.m_propertyID == id;
    // Only enabled properties should be part of the style.
    ASSERT(!result || CSSPropertyMetadata::isEnabledProperty(propertyID));
    return result;
}

static bool isPropertyMatch(const StylePropertyMetadata& metadata, const CSSValue& value, uint16_t id, const AtomicString& customPropertyName)
{
    ASSERT(id == CSSPropertyVariable);
    return metadata.m_propertyID == id
        && toCSSCustomPropertyDeclaration(value).name() == customPropertyName;
}

template<typename T>
int ImmutableStylePropertySet::findPropertyIndex(T property) const
{
    uint16_t id = getConvertedCSSPropertyID(property);
    for (int n = m_arraySize - 1 ; n >= 0; --n) {
        if (isPropertyMatch(metadataArray()[n], *valueArray()[n], id, property))
            return n;
    }

    return -1;
}
template CORE_EXPORT int ImmutableStylePropertySet::findPropertyIndex(CSSPropertyID) const;
template CORE_EXPORT int ImmutableStylePropertySet::findPropertyIndex(AtomicString) const;

DEFINE_TRACE_AFTER_DISPATCH(ImmutableStylePropertySet)
{
    const Member<const CSSValue>* values = valueArray();
    for (unsigned i = 0; i < m_arraySize; i++)
        visitor->trace(values[i]);
    StylePropertySet::traceAfterDispatch(visitor);
}

MutableStylePropertySet::MutableStylePropertySet(const StylePropertySet& other)
    : StylePropertySet(other.cssParserMode())
{
    if (other.isMutable()) {
        m_propertyVector = toMutableStylePropertySet(other).m_propertyVector;
    } else {
        m_propertyVector.reserveInitialCapacity(other.propertyCount());
        for (unsigned i = 0; i < other.propertyCount(); ++i)
            m_propertyVector.uncheckedAppend(other.propertyAt(i).toCSSProperty());
    }
}

static String serializeShorthand(const StylePropertySet& propertySet, CSSPropertyID propertyID)
{
    return StylePropertySerializer(propertySet).getPropertyValue(propertyID);
}

static String serializeShorthand(const StylePropertySet&, const AtomicString& customPropertyName)
{
    // Custom properties are never shorthands.
    return "";
}

template<typename T>
String StylePropertySet::getPropertyValue(T property) const
{
    CSSValue* value = getPropertyCSSValue(property);
    if (value)
        return value->cssText();
    return serializeShorthand(*this, property);
}
template CORE_EXPORT String StylePropertySet::getPropertyValue<CSSPropertyID>(CSSPropertyID) const;
template CORE_EXPORT String StylePropertySet::getPropertyValue<AtomicString>(AtomicString) const;

template<typename T>
CSSValue* StylePropertySet::getPropertyCSSValue(T property) const
{
    int foundPropertyIndex = findPropertyIndex(property);
    if (foundPropertyIndex == -1)
        return nullptr;
    return propertyAt(foundPropertyIndex).value();
}
template CORE_EXPORT CSSValue* StylePropertySet::getPropertyCSSValue<CSSPropertyID>(CSSPropertyID) const;
template CORE_EXPORT CSSValue* StylePropertySet::getPropertyCSSValue<AtomicString>(AtomicString) const;

DEFINE_TRACE(StylePropertySet)
{
    if (m_isMutable)
        toMutableStylePropertySet(this)->traceAfterDispatch(visitor);
    else
        toImmutableStylePropertySet(this)->traceAfterDispatch(visitor);
}

void StylePropertySet::finalizeGarbageCollectedObject()
{
    if (m_isMutable)
        toMutableStylePropertySet(this)->~MutableStylePropertySet();
    else
        toImmutableStylePropertySet(this)->~ImmutableStylePropertySet();
}

bool MutableStylePropertySet::removeShorthandProperty(CSSPropertyID propertyID)
{
    StylePropertyShorthand shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length())
        return false;

    return removePropertiesInSet(shorthand.properties(), shorthand.length());
}

bool MutableStylePropertySet::removePropertyAtIndex(int propertyIndex, String* returnText)
{
    if (propertyIndex == -1) {
        if (returnText)
            *returnText = "";
        return false;
    }

    if (returnText)
        *returnText = propertyAt(propertyIndex).value()->cssText();

    // A more efficient removal strategy would involve marking entries as empty
    // and sweeping them when the vector grows too big.
    m_propertyVector.remove(propertyIndex);

    return true;
}

template<typename T>
bool MutableStylePropertySet::removeProperty(T property, String* returnText)
{
    if (removeShorthandProperty(property)) {
        // FIXME: Return an equivalent shorthand when possible.
        if (returnText)
            *returnText = "";
        return true;
    }

    int foundPropertyIndex = findPropertyIndex(property);
    return removePropertyAtIndex(foundPropertyIndex, returnText);
}
template CORE_EXPORT bool MutableStylePropertySet::removeProperty(CSSPropertyID, String*);
template CORE_EXPORT bool MutableStylePropertySet::removeProperty(AtomicString, String*);

template<typename T>
bool StylePropertySet::propertyIsImportant(T property) const
{
    int foundPropertyIndex = findPropertyIndex(property);
    if (foundPropertyIndex != -1)
        return propertyAt(foundPropertyIndex).isImportant();
    return shorthandIsImportant(property);
}
template bool StylePropertySet::propertyIsImportant<CSSPropertyID>(CSSPropertyID) const;
template bool StylePropertySet::propertyIsImportant<AtomicString>(AtomicString) const;

bool StylePropertySet::shorthandIsImportant(CSSPropertyID propertyID) const
{
    StylePropertyShorthand shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length())
        return false;

    for (unsigned i = 0; i < shorthand.length(); ++i) {
        if (!propertyIsImportant(shorthand.properties()[i]))
            return false;
    }
    return true;
}

bool StylePropertySet::shorthandIsImportant(AtomicString customPropertyName) const
{
    // Custom properties are never shorthands.
    return false;
}

CSSPropertyID StylePropertySet::getPropertyShorthand(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return CSSPropertyInvalid;
    return propertyAt(foundPropertyIndex).shorthandID();
}

bool StylePropertySet::isPropertyImplicit(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return false;
    return propertyAt(foundPropertyIndex).isImplicit();
}

bool MutableStylePropertySet::setProperty(CSSPropertyID unresolvedProperty, const String& value, bool important, StyleSheetContents* contextStyleSheet)
{
    DCHECK_GE(unresolvedProperty, firstCSSProperty);

    // Setting the value to an empty string just removes the property in both IE and Gecko.
    // Setting it to null seems to produce less consistent results, but we treat it just the same.
    if (value.isEmpty())
        return removeProperty(resolveCSSPropertyID(unresolvedProperty));

    // When replacing an existing property value, this moves the property to the end of the list.
    // Firefox preserves the position, and MSIE moves the property to the beginning.
    return CSSParser::parseValue(this, unresolvedProperty, value, important, contextStyleSheet);
}

bool MutableStylePropertySet::setProperty(const AtomicString& customPropertyName, const String& value, bool important, StyleSheetContents* contextStyleSheet)
{
    if (value.isEmpty())
        return removeProperty(customPropertyName);
    return CSSParser::parseValueForCustomProperty(this, customPropertyName, value, important, contextStyleSheet);
}

void MutableStylePropertySet::setProperty(CSSPropertyID propertyID, CSSValue* value, bool important)
{
    StylePropertyShorthand shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length()) {
        setProperty(CSSProperty(propertyID, *value, important));
        return;
    }

    removePropertiesInSet(shorthand.properties(), shorthand.length());

    for (unsigned i = 0; i < shorthand.length(); ++i)
        m_propertyVector.append(CSSProperty(shorthand.properties()[i], *value, important));
}

bool MutableStylePropertySet::setProperty(const CSSProperty& property, CSSProperty* slot)
{
    if (!removeShorthandProperty(property.id())) {
        const AtomicString& name = (property.id() == CSSPropertyVariable) ?
            toCSSCustomPropertyDeclaration(property.value())->name() : nullAtom;
        CSSProperty* toReplace = slot ? slot : findCSSPropertyWithID(property.id(), name);
        if (toReplace && *toReplace == property)
            return false;
        if (toReplace) {
            *toReplace = property;
            return true;
        }
    }
    m_propertyVector.append(property);
    return true;
}

bool MutableStylePropertySet::setProperty(CSSPropertyID propertyID, CSSValueID identifier, bool important)
{
    setProperty(CSSProperty(propertyID, *CSSPrimitiveValue::createIdentifier(identifier), important));
    return true;
}

void MutableStylePropertySet::parseDeclarationList(const String& styleDeclaration, StyleSheetContents* contextStyleSheet)
{
    m_propertyVector.clear();

    CSSParserContext context(cssParserMode(), UseCounter::getFrom(contextStyleSheet));
    if (contextStyleSheet) {
        context = contextStyleSheet->parserContext();
        context.setMode(cssParserMode());
    }

    CSSParser::parseDeclarationList(context, this, styleDeclaration);
}

bool MutableStylePropertySet::addParsedProperties(const HeapVector<CSSProperty, 256>& properties)
{
    bool changed = false;
    m_propertyVector.reserveCapacity(m_propertyVector.size() + properties.size());
    for (unsigned i = 0; i < properties.size(); ++i)
        changed |= setProperty(properties[i]);
    return changed;
}

bool MutableStylePropertySet::addRespectingCascade(const CSSProperty& property)
{
    // Only add properties that have no !important counterpart present
    if (!propertyIsImportant(property.id()) || property.isImportant())
        return setProperty(property);
    return false;
}

String StylePropertySet::asText() const
{
    return StylePropertySerializer(*this).asText();
}

void MutableStylePropertySet::mergeAndOverrideOnConflict(const StylePropertySet* other)
{
    unsigned size = other->propertyCount();
    for (unsigned n = 0; n < size; ++n) {
        PropertyReference toMerge = other->propertyAt(n);
        // TODO(leviw): This probably doesn't work correctly with Custom Properties
        CSSProperty* old = findCSSPropertyWithID(toMerge.id());
        if (old)
            setProperty(toMerge.toCSSProperty(), old);
        else
            m_propertyVector.append(toMerge.toCSSProperty());
    }
}

bool StylePropertySet::hasFailedOrCanceledSubresources() const
{
    unsigned size = propertyCount();
    for (unsigned i = 0; i < size; ++i) {
        if (propertyAt(i).value()->hasFailedOrCanceledSubresources())
            return true;
    }
    return false;
}

void MutableStylePropertySet::clear()
{
    m_propertyVector.clear();
}

inline bool containsId(const CSSPropertyID* set, unsigned length, CSSPropertyID id)
{
    for (unsigned i = 0; i < length; ++i) {
        if (set[i] == id)
            return true;
    }
    return false;
}

bool MutableStylePropertySet::removePropertiesInSet(const CSSPropertyID* set, unsigned length)
{
    if (m_propertyVector.isEmpty())
        return false;

    CSSProperty* properties = m_propertyVector.data();
    unsigned oldSize = m_propertyVector.size();
    unsigned newIndex = 0;
    for (unsigned oldIndex = 0; oldIndex < oldSize; ++oldIndex) {
        const CSSProperty& property = properties[oldIndex];
        if (containsId(set, length, property.id()))
            continue;
        // Modify m_propertyVector in-place since this method is performance-sensitive.
        properties[newIndex++] = properties[oldIndex];
    }
    if (newIndex != oldSize) {
        m_propertyVector.shrink(newIndex);
        return true;
    }
    return false;
}

CSSProperty* MutableStylePropertySet::findCSSPropertyWithID(CSSPropertyID propertyID, const AtomicString& customPropertyName)
{
    int foundPropertyIndex = -1;
    if (propertyID == CSSPropertyVariable && !customPropertyName.isNull()) {
        // TODO(shanestephens): fix call sites so we always have a customPropertyName
        // here.
        foundPropertyIndex = findPropertyIndex(customPropertyName);
    } else {
        ASSERT(customPropertyName.isNull());
        foundPropertyIndex = findPropertyIndex(propertyID);
    }
    if (foundPropertyIndex == -1)
        return nullptr;
    return &m_propertyVector.at(foundPropertyIndex);
}

bool StylePropertySet::propertyMatches(CSSPropertyID propertyID, const CSSValue* propertyValue) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return false;
    return propertyAt(foundPropertyIndex).value()->equals(*propertyValue);
}

void MutableStylePropertySet::removeEquivalentProperties(const StylePropertySet* style)
{
    Vector<CSSPropertyID> propertiesToRemove;
    unsigned size = m_propertyVector.size();
    for (unsigned i = 0; i < size; ++i) {
        PropertyReference property = propertyAt(i);
        if (style->propertyMatches(property.id(), property.value()))
            propertiesToRemove.append(property.id());
    }
    // FIXME: This should use mass removal.
    for (unsigned i = 0; i < propertiesToRemove.size(); ++i)
        removeProperty(propertiesToRemove[i]);
}

void MutableStylePropertySet::removeEquivalentProperties(const CSSStyleDeclaration* style)
{
    Vector<CSSPropertyID> propertiesToRemove;
    unsigned size = m_propertyVector.size();
    for (unsigned i = 0; i < size; ++i) {
        PropertyReference property = propertyAt(i);
        if (style->cssPropertyMatches(property.id(), property.value()))
            propertiesToRemove.append(property.id());
    }
    // FIXME: This should use mass removal.
    for (unsigned i = 0; i < propertiesToRemove.size(); ++i)
        removeProperty(propertiesToRemove[i]);
}

MutableStylePropertySet* StylePropertySet::mutableCopy() const
{
    return new MutableStylePropertySet(*this);
}

MutableStylePropertySet* StylePropertySet::copyPropertiesInSet(const Vector<CSSPropertyID>& properties) const
{
    HeapVector<CSSProperty, 256> list;
    list.reserveInitialCapacity(properties.size());
    for (unsigned i = 0; i < properties.size(); ++i) {
        CSSValue* value = getPropertyCSSValue(properties[i]);
        if (value)
            list.append(CSSProperty(properties[i], *value, false));
    }
    return MutableStylePropertySet::create(list.data(), list.size());
}

CSSStyleDeclaration* MutableStylePropertySet::ensureCSSStyleDeclaration()
{
    // FIXME: get rid of this weirdness of a CSSStyleDeclaration inside of a
    // style property set.
    if (m_cssomWrapper) {
        ASSERT(!static_cast<CSSStyleDeclaration*>(m_cssomWrapper.get())->parentRule());
        ASSERT(!m_cssomWrapper->parentElement());
        return m_cssomWrapper.get();
    }
    m_cssomWrapper = new PropertySetCSSStyleDeclaration(*this);
    return m_cssomWrapper.get();
}

template<typename T>
int MutableStylePropertySet::findPropertyIndex(T property) const
{
    const CSSProperty* begin = m_propertyVector.data();
    const CSSProperty* end = begin + m_propertyVector.size();

    uint16_t id = getConvertedCSSPropertyID(property);

    const CSSProperty* it = std::find_if(begin, end, [property, id](const CSSProperty& cssProperty) -> bool {
        return isPropertyMatch(cssProperty.metadata(), *cssProperty.value(), id, property);
    });

    return (it == end) ? -1 : it - begin;
}
template CORE_EXPORT int MutableStylePropertySet::findPropertyIndex(CSSPropertyID) const;
template CORE_EXPORT int MutableStylePropertySet::findPropertyIndex(AtomicString) const;

DEFINE_TRACE_AFTER_DISPATCH(MutableStylePropertySet)
{
    visitor->trace(m_cssomWrapper);
    visitor->trace(m_propertyVector);
    StylePropertySet::traceAfterDispatch(visitor);
}

unsigned StylePropertySet::averageSizeInBytes()
{
    // Please update this if the storage scheme changes so that this longer reflects the actual size.
    return sizeForImmutableStylePropertySetWithPropertyCount(4);
}

// See the function above if you need to update this.
struct SameSizeAsStylePropertySet : public GarbageCollectedFinalized<SameSizeAsStylePropertySet> {
    unsigned bitfield;
};
static_assert(sizeof(StylePropertySet) == sizeof(SameSizeAsStylePropertySet), "StylePropertySet should stay small");

#ifndef NDEBUG
void StylePropertySet::showStyle()
{
    fprintf(stderr, "%s\n", asText().ascii().data());
}
#endif

MutableStylePropertySet* MutableStylePropertySet::create(CSSParserMode cssParserMode)
{
    return new MutableStylePropertySet(cssParserMode);
}

MutableStylePropertySet* MutableStylePropertySet::create(const CSSProperty* properties, unsigned count)
{
    return new MutableStylePropertySet(properties, count);
}

} // namespace blink

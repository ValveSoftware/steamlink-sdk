/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#ifndef StylePropertySet_h
#define StylePropertySet_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSProperty.h"
#include "core/css/PropertySetCSSStyleDeclaration.h"
#include "core/css/parser/CSSParserMode.h"
#include "wtf/ListHashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <algorithm>

namespace blink {

class CSSStyleDeclaration;
class ImmutableStylePropertySet;
class MutableStylePropertySet;
class StyleSheetContents;

class CORE_EXPORT StylePropertySet : public GarbageCollectedFinalized<StylePropertySet> {
    WTF_MAKE_NONCOPYABLE(StylePropertySet);
    friend class PropertyReference;
public:

    void finalizeGarbageCollectedObject();

    class PropertyReference {
        STACK_ALLOCATED();
    public:
        PropertyReference(const StylePropertySet& propertySet, unsigned index)
            : m_propertySet(&propertySet)
            , m_index(index)
        {
        }

        CSSPropertyID id() const { return static_cast<CSSPropertyID>(propertyMetadata().m_propertyID); }
        CSSPropertyID shorthandID() const { return propertyMetadata().shorthandID(); }

        bool isImportant() const { return propertyMetadata().m_important; }
        bool isInherited() const { return propertyMetadata().m_inherited; }
        bool isImplicit() const { return propertyMetadata().m_implicit; }

        const CSSValue* value() const { return propertyValue(); }
        // FIXME: We should try to remove this mutable overload.
        CSSValue* value() { return const_cast<CSSValue*>(propertyValue()); }

        // FIXME: Remove this.
        CSSProperty toCSSProperty() const { return CSSProperty(propertyMetadata(), *propertyValue()); }

        const StylePropertyMetadata& propertyMetadata() const;

    private:
        const CSSValue* propertyValue() const;

        Member<const StylePropertySet> m_propertySet;
        unsigned m_index;
    };

    unsigned propertyCount() const;
    bool isEmpty() const;
    PropertyReference propertyAt(unsigned index) const { return PropertyReference(*this, index); }

    template<typename T> // CSSPropertyID or AtomicString
    int findPropertyIndex(T property) const;

    bool hasProperty(CSSPropertyID property) const { return findPropertyIndex(property) != -1; }

    template<typename T> // CSSPropertyID or AtomicString
    CSSValue* getPropertyCSSValue(T property) const;

    template<typename T> // CSSPropertyID or AtomicString
    String getPropertyValue(T property) const;

    template<typename T> // CSSPropertyID or AtomicString
    bool propertyIsImportant(T property) const;

    bool shorthandIsImportant(CSSPropertyID) const;
    bool shorthandIsImportant(AtomicString customPropertyName) const;

    CSSPropertyID getPropertyShorthand(CSSPropertyID) const;
    bool isPropertyImplicit(CSSPropertyID) const;

    CSSParserMode cssParserMode() const { return static_cast<CSSParserMode>(m_cssParserMode); }

    MutableStylePropertySet* mutableCopy() const;
    ImmutableStylePropertySet* immutableCopyIfNeeded() const;

    MutableStylePropertySet* copyPropertiesInSet(const Vector<CSSPropertyID>&) const;

    String asText() const;

    bool isMutable() const { return m_isMutable; }

    bool hasFailedOrCanceledSubresources() const;

    static unsigned averageSizeInBytes();

#ifndef NDEBUG
    void showStyle();
#endif

    bool propertyMatches(CSSPropertyID, const CSSValue*) const;

    DECLARE_TRACE();
    DEFINE_INLINE_TRACE_AFTER_DISPATCH() { }

protected:

    enum { MaxArraySize = (1 << 28) - 1 };

    StylePropertySet(CSSParserMode cssParserMode)
        : m_cssParserMode(cssParserMode)
        , m_isMutable(true)
        , m_arraySize(0)
    { }

    StylePropertySet(CSSParserMode cssParserMode, unsigned immutableArraySize)
        : m_cssParserMode(cssParserMode)
        , m_isMutable(false)
        , m_arraySize(std::min(immutableArraySize, unsigned(MaxArraySize)))
    { }

    unsigned m_cssParserMode : 3;
    mutable unsigned m_isMutable : 1;
    unsigned m_arraySize : 28;

    friend class PropertySetCSSStyleDeclaration;
};

class CORE_EXPORT ImmutableStylePropertySet : public StylePropertySet {
public:
    ~ImmutableStylePropertySet();
    static ImmutableStylePropertySet* create(const CSSProperty* properties, unsigned count, CSSParserMode);

    unsigned propertyCount() const { return m_arraySize; }

    const Member<const CSSValue>* valueArray() const;
    const StylePropertyMetadata* metadataArray() const;

    template<typename T> // CSSPropertyID or AtomicString
    int findPropertyIndex(T property) const;

    DECLARE_TRACE_AFTER_DISPATCH();

    void* operator new(std::size_t, void* location)
    {
        return location;
    }

    void* m_storage;

private:
    ImmutableStylePropertySet(const CSSProperty*, unsigned count, CSSParserMode);
};

inline const Member<const CSSValue>* ImmutableStylePropertySet::valueArray() const
{
    return reinterpret_cast<const Member<const CSSValue>*>(const_cast<const void**>(&(this->m_storage)));
}

inline const StylePropertyMetadata* ImmutableStylePropertySet::metadataArray() const
{
    return reinterpret_cast<const StylePropertyMetadata*>(&reinterpret_cast<const char*>(&(this->m_storage))[m_arraySize * sizeof(Member<CSSValue>)]);
}

DEFINE_TYPE_CASTS(ImmutableStylePropertySet, StylePropertySet, set, !set->isMutable(), !set.isMutable());

class CORE_EXPORT MutableStylePropertySet : public StylePropertySet {
public:
    ~MutableStylePropertySet() { }
    static MutableStylePropertySet* create(CSSParserMode);
    static MutableStylePropertySet* create(const CSSProperty* properties, unsigned count);

    unsigned propertyCount() const { return m_propertyVector.size(); }

    // Returns whether this style set was changed.
    bool addParsedProperties(const HeapVector<CSSProperty, 256>&);
    bool addRespectingCascade(const CSSProperty&);

    // These expand shorthand properties into multiple properties.
    bool setProperty(CSSPropertyID unresolvedProperty, const String& value, bool important = false, StyleSheetContents* contextStyleSheet = 0);
    bool setProperty(const AtomicString& customPropertyName, const String& value, bool important = false, StyleSheetContents* contextStyleSheet = 0);
    void setProperty(CSSPropertyID, CSSValue*, bool important = false);

    // These do not. FIXME: This is too messy, we can do better.
    bool setProperty(CSSPropertyID, CSSValueID identifier, bool important = false);
    bool setProperty(const CSSProperty&, CSSProperty* slot = 0);

    template<typename T> // CSSPropertyID or AtomicString
    bool removeProperty(T property, String* returnText = 0);
    bool removePropertiesInSet(const CSSPropertyID* set, unsigned length);
    void removeEquivalentProperties(const StylePropertySet*);
    void removeEquivalentProperties(const CSSStyleDeclaration*);

    void mergeAndOverrideOnConflict(const StylePropertySet*);

    void clear();
    void parseDeclarationList(const String& styleDeclaration, StyleSheetContents* contextStyleSheet);

    CSSStyleDeclaration* ensureCSSStyleDeclaration();

    template<typename T> // CSSPropertyID or AtomicString
    int findPropertyIndex(T property) const;

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    explicit MutableStylePropertySet(CSSParserMode);
    explicit MutableStylePropertySet(const StylePropertySet&);
    MutableStylePropertySet(const CSSProperty* properties, unsigned count);

    bool removePropertyAtIndex(int, String* returnText);

    bool removeShorthandProperty(CSSPropertyID);
    bool removeShorthandProperty(const AtomicString& customPropertyName) { return false; }
    CSSProperty* findCSSPropertyWithID(CSSPropertyID, const AtomicString& customPropertyName = nullAtom);
    Member<PropertySetCSSStyleDeclaration> m_cssomWrapper;

    friend class StylePropertySet;

    HeapVector<CSSProperty, 4> m_propertyVector;
};

DEFINE_TYPE_CASTS(MutableStylePropertySet, StylePropertySet, set, set->isMutable(), set.isMutable());

inline MutableStylePropertySet* toMutableStylePropertySet(const Persistent<StylePropertySet>& set)
{
    return toMutableStylePropertySet(set.get());
}

inline MutableStylePropertySet* toMutableStylePropertySet(const Member<StylePropertySet>& set)
{
    return toMutableStylePropertySet(set.get());
}

inline const StylePropertyMetadata& StylePropertySet::PropertyReference::propertyMetadata() const
{
    if (m_propertySet->isMutable())
        return toMutableStylePropertySet(*m_propertySet).m_propertyVector.at(m_index).metadata();
    return toImmutableStylePropertySet(*m_propertySet).metadataArray()[m_index];
}

inline const CSSValue* StylePropertySet::PropertyReference::propertyValue() const
{
    if (m_propertySet->isMutable())
        return toMutableStylePropertySet(*m_propertySet).m_propertyVector.at(m_index).value();
    return toImmutableStylePropertySet(*m_propertySet).valueArray()[m_index];
}

inline unsigned StylePropertySet::propertyCount() const
{
    if (m_isMutable)
        return toMutableStylePropertySet(this)->m_propertyVector.size();
    return m_arraySize;
}

inline bool StylePropertySet::isEmpty() const
{
    return !propertyCount();
}

template<typename T>
inline int StylePropertySet::findPropertyIndex(T property) const
{
    if (m_isMutable)
        return toMutableStylePropertySet(this)->findPropertyIndex(property);
    return toImmutableStylePropertySet(this)->findPropertyIndex(property);
}

} // namespace blink

#endif // StylePropertySet_h

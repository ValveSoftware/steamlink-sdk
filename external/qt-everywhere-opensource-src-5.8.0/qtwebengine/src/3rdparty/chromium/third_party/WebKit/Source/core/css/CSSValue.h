/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef CSSValue_h
#define CSSValue_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/HashMap.h"
#include "wtf/ListHashSet.h"
#include "wtf/RefPtr.h"

namespace blink {

class CORE_EXPORT CSSValue : public GarbageCollectedFinalized<CSSValue> {
public:
    // Override operator new to allocate CSSValue subtype objects onto
    // a dedicated heap.
    GC_PLUGIN_IGNORE("crbug.com/443854")
    void* operator new(size_t size)
    {
        return allocateObject(size, false);
    }
    static void* allocateObject(size_t size, bool isEager)
    {
        ThreadState* state = ThreadStateFor<ThreadingTrait<CSSValue>::Affinity>::state();
        const char typeName[] = "blink::CSSValue";
        return ThreadHeap::allocateOnArenaIndex(state, size, isEager ? BlinkGC::EagerSweepArenaIndex : BlinkGC::CSSValueArenaIndex, GCInfoTrait<CSSValue>::index(), typeName);
    }

    String cssText() const;

    bool isPrimitiveValue() const { return m_classType == PrimitiveClass; }
    bool isValuePair() const { return m_classType == ValuePairClass; }
    bool isValueList() const { return m_classType >= ValueListClass; }

    bool isBaseValueList() const { return m_classType == ValueListClass; }

    bool isBasicShapeValue() const { return m_classType >= BasicShapeCircleClass && m_classType <= BasicShapeInsetClass; }
    bool isBasicShapeCircleValue() const { return m_classType == BasicShapeCircleClass; }
    bool isBasicShapeEllipseValue() const { return m_classType == BasicShapeEllipseClass; }
    bool isBasicShapePolygonValue() const { return m_classType == BasicShapePolygonClass; }
    bool isBasicShapeInsetValue() const { return m_classType == BasicShapeInsetClass; }

    bool isBorderImageSliceValue() const { return m_classType == BorderImageSliceClass; }
    bool isColorValue() const { return m_classType == ColorClass; }
    bool isCounterValue() const { return m_classType == CounterClass; }
    bool isCursorImageValue() const { return m_classType == CursorImageClass; }
    bool isCrossfadeValue() const { return m_classType == CrossfadeClass; }
    bool isPaintValue() const { return m_classType == PaintClass; }
    bool isFontFeatureValue() const { return m_classType == FontFeatureClass; }
    bool isFontFamilyValue() const { return m_classType == FontFamilyClass; }
    bool isFontFaceSrcValue() const { return m_classType == FontFaceSrcClass; }
    bool isFunctionValue() const { return m_classType == FunctionClass; }
    bool isCustomIdentValue() const { return m_classType == CustomIdentClass; }
    bool isImageGeneratorValue() const { return m_classType >= CrossfadeClass && m_classType <= RadialGradientClass; }
    bool isGradientValue() const { return m_classType >= LinearGradientClass && m_classType <= RadialGradientClass; }
    bool isImageSetValue() const { return m_classType == ImageSetClass; }
    bool isImageValue() const { return m_classType == ImageClass; }
    bool isImplicitInitialValue() const;
    bool isInheritedValue() const { return m_classType == InheritedClass; }
    bool isInitialValue() const { return m_classType == InitialClass; }
    bool isUnsetValue() const { return m_classType == UnsetClass; }
    bool isCSSWideKeyword() const { return m_classType >= InheritedClass && m_classType <= UnsetClass; }
    bool isLinearGradientValue() const { return m_classType == LinearGradientClass; }
    bool isPathValue() const { return m_classType == PathClass; }
    bool isQuadValue() const { return m_classType == QuadClass; }
    bool isRadialGradientValue() const { return m_classType == RadialGradientClass; }
    bool isReflectValue() const { return m_classType == ReflectClass; }
    bool isShadowValue() const { return m_classType == ShadowClass; }
    bool isStringValue() const { return m_classType == StringClass; }
    bool isURIValue() const { return m_classType == URIClass; }
    bool isCubicBezierTimingFunctionValue() const { return m_classType == CubicBezierTimingFunctionClass; }
    bool isStepsTimingFunctionValue() const { return m_classType == StepsTimingFunctionClass; }
    bool isGridTemplateAreasValue() const { return m_classType == GridTemplateAreasClass; }
    bool isSVGDocumentValue() const { return m_classType == CSSSVGDocumentClass; }
    bool isContentDistributionValue() const { return m_classType == CSSContentDistributionClass; }
    bool isUnicodeRangeValue() const { return m_classType == UnicodeRangeClass; }
    bool isGridLineNamesValue() const { return m_classType == GridLineNamesClass; }
    bool isCustomPropertyDeclaration() const { return m_classType == CustomPropertyDeclarationClass; }
    bool isVariableReferenceValue() const { return m_classType == VariableReferenceClass; }
    bool isGridAutoRepeatValue() const { return m_classType == GridAutoRepeatClass; }
    bool isPendingSubstitutionValue() const { return m_classType == PendingSubstitutionValueClass; }

    bool hasFailedOrCanceledSubresources() const;

    bool equals(const CSSValue&) const;

    void finalizeGarbageCollectedObject();
    DEFINE_INLINE_TRACE_AFTER_DISPATCH() { }
    DECLARE_TRACE();

    // ~CSSValue should be public, because non-public ~CSSValue causes C2248
    // error: 'blink::CSSValue::~CSSValue' : cannot access protected member
    // declared in class 'blink::CSSValue' when compiling
    // 'source\wtf\refcounted.h' by using msvc.
    ~CSSValue() { }

protected:
    static const size_t ClassTypeBits = 6;
    enum ClassType {
        PrimitiveClass,
        ColorClass,
        CounterClass,
        QuadClass,
        CustomIdentClass,
        StringClass,
        URIClass,
        ValuePairClass,

        // Basic shape classes.
        // TODO(sashab): Represent these as a single subclass, BasicShapeClass.
        BasicShapeCircleClass,
        BasicShapeEllipseClass,
        BasicShapePolygonClass,
        BasicShapeInsetClass,

        // Image classes.
        ImageClass,
        CursorImageClass,

        // Image generator classes.
        CrossfadeClass,
        PaintClass,
        LinearGradientClass,
        RadialGradientClass,

        // Timing function classes.
        CubicBezierTimingFunctionClass,
        StepsTimingFunctionClass,

        // Other class types.
        BorderImageSliceClass,
        FontFeatureClass,
        FontFaceSrcClass,
        FontFamilyClass,

        InheritedClass,
        InitialClass,
        UnsetClass,

        ReflectClass,
        ShadowClass,
        UnicodeRangeClass,
        GridTemplateAreasClass,
        PathClass,
        VariableReferenceClass,
        CustomPropertyDeclarationClass,
        PendingSubstitutionValueClass,

        // SVG classes.
        CSSSVGDocumentClass,

        CSSContentDistributionClass,

        // List class types must appear after ValueListClass.
        ValueListClass,
        FunctionClass,
        ImageSetClass,
        GridLineNamesClass,
        GridAutoRepeatClass,
        // Do not append non-list class types here.
    };

    static const size_t ValueListSeparatorBits = 2;
    enum ValueListSeparator {
        SpaceSeparator,
        CommaSeparator,
        SlashSeparator
    };

    ClassType getClassType() const { return static_cast<ClassType>(m_classType); }

    explicit CSSValue(ClassType classType)
        : m_primitiveUnitType(0)
        , m_hasCachedCSSText(false)
        , m_valueListSeparator(SpaceSeparator)
        , m_classType(classType)
    {
    }

    // NOTE: This class is non-virtual for memory and performance reasons.
    // Don't go making it virtual again unless you know exactly what you're doing!

private:
    void destroy();

protected:
    // The bits in this section are only used by specific subclasses but kept here
    // to maximize struct packing.

    // CSSPrimitiveValue bits:
    unsigned m_primitiveUnitType : 7; // CSSPrimitiveValue::UnitType
    mutable unsigned m_hasCachedCSSText : 1;

    unsigned m_valueListSeparator : ValueListSeparatorBits;

private:
    unsigned m_classType : ClassTypeBits; // ClassType
};

template<typename CSSValueType, size_t inlineCapacity>
inline bool compareCSSValueVector(const HeapVector<Member<CSSValueType>, inlineCapacity>& firstVector, const HeapVector<Member<CSSValueType>, inlineCapacity>& secondVector)
{
    size_t size = firstVector.size();
    if (size != secondVector.size())
        return false;

    for (size_t i = 0; i < size; i++) {
        const Member<CSSValueType>& firstPtr = firstVector[i];
        const Member<CSSValueType>& secondPtr = secondVector[i];
        if (firstPtr == secondPtr || (firstPtr && secondPtr && firstPtr->equals(*secondPtr)))
            continue;
        return false;
    }
    return true;
}

template<typename CSSValueType>
inline bool compareCSSValuePtr(const RefPtr<CSSValueType>& first, const RefPtr<CSSValueType>& second)
{
    if (first == second)
        return true;
    if (!first || !second)
        return false;
    return first->equals(*second);
}

template<typename CSSValueType>
inline bool compareCSSValuePtr(const CSSValueType* first, const CSSValueType* second)
{
    if (first == second)
        return true;
    if (!first || !second)
        return false;
    return first->equals(*second);
}

template<typename CSSValueType>
inline bool compareCSSValuePtr(const Member<CSSValueType>& first, const Member<CSSValueType>& second)
{
    if (first == second)
        return true;
    if (!first || !second)
        return false;
    return first->equals(*second);
}

#define DEFINE_CSS_VALUE_TYPE_CASTS(thisType, predicate) \
    DEFINE_TYPE_CASTS(thisType, CSSValue, value, value->predicate, value.predicate)

} // namespace blink

#endif // CSSValue_h

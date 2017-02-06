/*
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/css/CSSValue.h"

#include "core/css/CSSBasicShapeValues.h"
#include "core/css/CSSBorderImageSliceValue.h"
#include "core/css/CSSColorValue.h"
#include "core/css/CSSContentDistributionValue.h"
#include "core/css/CSSCounterValue.h"
#include "core/css/CSSCrossfadeValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSFontFamilyValue.h"
#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSGridAutoRepeatValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSGridTemplateAreasValue.h"
#include "core/css/CSSImageSetValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSInheritedValue.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/CSSPaintValue.h"
#include "core/css/CSSPathValue.h"
#include "core/css/CSSPendingSubstitutionValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSSVGDocumentValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSStringValue.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/CSSURIValue.h"
#include "core/css/CSSUnicodeRangeValue.h"
#include "core/css/CSSUnsetValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"
#include "core/css/CSSVariableReferenceValue.h"

namespace blink {

struct SameSizeAsCSSValue : public GarbageCollectedFinalized<SameSizeAsCSSValue> {
    uint32_t bitfields;
};

static_assert(sizeof(CSSValue) <= sizeof(SameSizeAsCSSValue), "CSSValue should stay small");

bool CSSValue::isImplicitInitialValue() const
{
    return m_classType == InitialClass && toCSSInitialValue(this)->isImplicit();
}

bool CSSValue::hasFailedOrCanceledSubresources() const
{
    if (isValueList())
        return toCSSValueList(this)->hasFailedOrCanceledSubresources();
    if (getClassType() == FontFaceSrcClass)
        return toCSSFontFaceSrcValue(this)->hasFailedOrCanceledSubresources();
    if (getClassType() == ImageClass)
        return toCSSImageValue(this)->hasFailedOrCanceledSubresources();
    if (getClassType() == CrossfadeClass)
        return toCSSCrossfadeValue(this)->hasFailedOrCanceledSubresources();
    if (getClassType() == ImageSetClass)
        return toCSSImageSetValue(this)->hasFailedOrCanceledSubresources();

    return false;
}

template<class ChildClassType>
inline static bool compareCSSValues(const CSSValue& first, const CSSValue& second)
{
    return static_cast<const ChildClassType&>(first).equals(static_cast<const ChildClassType&>(second));
}

bool CSSValue::equals(const CSSValue& other) const
{
    if (m_classType == other.m_classType) {
        switch (getClassType()) {
        case BasicShapeCircleClass:
            return compareCSSValues<CSSBasicShapeCircleValue>(*this, other);
        case BasicShapeEllipseClass:
            return compareCSSValues<CSSBasicShapeEllipseValue>(*this, other);
        case BasicShapePolygonClass:
            return compareCSSValues<CSSBasicShapePolygonValue>(*this, other);
        case BasicShapeInsetClass:
            return compareCSSValues<CSSBasicShapeInsetValue>(*this, other);
        case BorderImageSliceClass:
            return compareCSSValues<CSSBorderImageSliceValue>(*this, other);
        case ColorClass:
            return compareCSSValues<CSSColorValue>(*this, other);
        case CounterClass:
            return compareCSSValues<CSSCounterValue>(*this, other);
        case CursorImageClass:
            return compareCSSValues<CSSCursorImageValue>(*this, other);
        case FontFaceSrcClass:
            return compareCSSValues<CSSFontFaceSrcValue>(*this, other);
        case FontFamilyClass:
            return compareCSSValues<CSSFontFamilyValue>(*this, other);
        case FontFeatureClass:
            return compareCSSValues<CSSFontFeatureValue>(*this, other);
        case FunctionClass:
            return compareCSSValues<CSSFunctionValue>(*this, other);
        case LinearGradientClass:
            return compareCSSValues<CSSLinearGradientValue>(*this, other);
        case RadialGradientClass:
            return compareCSSValues<CSSRadialGradientValue>(*this, other);
        case CrossfadeClass:
            return compareCSSValues<CSSCrossfadeValue>(*this, other);
        case PaintClass:
            return compareCSSValues<CSSPaintValue>(*this, other);
        case CustomIdentClass:
            return compareCSSValues<CSSCustomIdentValue>(*this, other);
        case ImageClass:
            return compareCSSValues<CSSImageValue>(*this, other);
        case InheritedClass:
            return compareCSSValues<CSSInheritedValue>(*this, other);
        case InitialClass:
            return compareCSSValues<CSSInitialValue>(*this, other);
        case UnsetClass:
            return compareCSSValues<CSSUnsetValue>(*this, other);
        case GridAutoRepeatClass:
            return compareCSSValues<CSSGridAutoRepeatValue>(*this, other);
        case GridLineNamesClass:
            return compareCSSValues<CSSGridLineNamesValue>(*this, other);
        case GridTemplateAreasClass:
            return compareCSSValues<CSSGridTemplateAreasValue>(*this, other);
        case PathClass:
            return compareCSSValues<CSSPathValue>(*this, other);
        case PrimitiveClass:
            return compareCSSValues<CSSPrimitiveValue>(*this, other);
        case QuadClass:
            return compareCSSValues<CSSQuadValue>(*this, other);
        case ReflectClass:
            return compareCSSValues<CSSReflectValue>(*this, other);
        case ShadowClass:
            return compareCSSValues<CSSShadowValue>(*this, other);
        case StringClass:
            return compareCSSValues<CSSStringValue>(*this, other);
        case CubicBezierTimingFunctionClass:
            return compareCSSValues<CSSCubicBezierTimingFunctionValue>(*this, other);
        case StepsTimingFunctionClass:
            return compareCSSValues<CSSStepsTimingFunctionValue>(*this, other);
        case UnicodeRangeClass:
            return compareCSSValues<CSSUnicodeRangeValue>(*this, other);
        case URIClass:
            return compareCSSValues<CSSURIValue>(*this, other);
        case ValueListClass:
            return compareCSSValues<CSSValueList>(*this, other);
        case ValuePairClass:
            return compareCSSValues<CSSValuePair>(*this, other);
        case ImageSetClass:
            return compareCSSValues<CSSImageSetValue>(*this, other);
        case CSSSVGDocumentClass:
            return compareCSSValues<CSSSVGDocumentValue>(*this, other);
        case CSSContentDistributionClass:
            return compareCSSValues<CSSContentDistributionValue>(*this, other);
        case CustomPropertyDeclarationClass:
            return compareCSSValues<CSSCustomPropertyDeclaration>(*this, other);
        case VariableReferenceClass:
            return compareCSSValues<CSSVariableReferenceValue>(*this, other);
        case PendingSubstitutionValueClass:
            return compareCSSValues<CSSPendingSubstitutionValue>(*this, other);
        }
        ASSERT_NOT_REACHED();
        return false;
    }
    return false;
}

String CSSValue::cssText() const
{
    switch (getClassType()) {
    case BasicShapeCircleClass:
        return toCSSBasicShapeCircleValue(this)->customCSSText();
    case BasicShapeEllipseClass:
        return toCSSBasicShapeEllipseValue(this)->customCSSText();
    case BasicShapePolygonClass:
        return toCSSBasicShapePolygonValue(this)->customCSSText();
    case BasicShapeInsetClass:
        return toCSSBasicShapeInsetValue(this)->customCSSText();
    case BorderImageSliceClass:
        return toCSSBorderImageSliceValue(this)->customCSSText();
    case ColorClass:
        return toCSSColorValue(this)->customCSSText();
    case CounterClass:
        return toCSSCounterValue(this)->customCSSText();
    case CursorImageClass:
        return toCSSCursorImageValue(this)->customCSSText();
    case FontFaceSrcClass:
        return toCSSFontFaceSrcValue(this)->customCSSText();
    case FontFamilyClass:
        return toCSSFontFamilyValue(this)->customCSSText();
    case FontFeatureClass:
        return toCSSFontFeatureValue(this)->customCSSText();
    case FunctionClass:
        return toCSSFunctionValue(this)->customCSSText();
    case LinearGradientClass:
        return toCSSLinearGradientValue(this)->customCSSText();
    case RadialGradientClass:
        return toCSSRadialGradientValue(this)->customCSSText();
    case CrossfadeClass:
        return toCSSCrossfadeValue(this)->customCSSText();
    case PaintClass:
        return toCSSPaintValue(this)->customCSSText();
    case CustomIdentClass:
        return toCSSCustomIdentValue(this)->customCSSText();
    case ImageClass:
        return toCSSImageValue(this)->customCSSText();
    case InheritedClass:
        return toCSSInheritedValue(this)->customCSSText();
    case UnsetClass:
        return toCSSUnsetValue(this)->customCSSText();
    case InitialClass:
        return toCSSInitialValue(this)->customCSSText();
    case GridAutoRepeatClass:
        return toCSSGridAutoRepeatValue(this)->customCSSText();
    case GridLineNamesClass:
        return toCSSGridLineNamesValue(this)->customCSSText();
    case GridTemplateAreasClass:
        return toCSSGridTemplateAreasValue(this)->customCSSText();
    case PathClass:
        return toCSSPathValue(this)->customCSSText();
    case PrimitiveClass:
        return toCSSPrimitiveValue(this)->customCSSText();
    case QuadClass:
        return toCSSQuadValue(this)->customCSSText();
    case ReflectClass:
        return toCSSReflectValue(this)->customCSSText();
    case ShadowClass:
        return toCSSShadowValue(this)->customCSSText();
    case StringClass:
        return toCSSStringValue(this)->customCSSText();
    case CubicBezierTimingFunctionClass:
        return toCSSCubicBezierTimingFunctionValue(this)->customCSSText();
    case StepsTimingFunctionClass:
        return toCSSStepsTimingFunctionValue(this)->customCSSText();
    case UnicodeRangeClass:
        return toCSSUnicodeRangeValue(this)->customCSSText();
    case URIClass:
        return toCSSURIValue(this)->customCSSText();
    case ValuePairClass:
        return toCSSValuePair(this)->customCSSText();
    case ValueListClass:
        return toCSSValueList(this)->customCSSText();
    case ImageSetClass:
        return toCSSImageSetValue(this)->customCSSText();
    case CSSSVGDocumentClass:
        return toCSSSVGDocumentValue(this)->customCSSText();
    case CSSContentDistributionClass:
        return toCSSContentDistributionValue(this)->customCSSText();
    case VariableReferenceClass:
        return toCSSVariableReferenceValue(this)->customCSSText();
    case CustomPropertyDeclarationClass:
        return toCSSCustomPropertyDeclaration(this)->customCSSText();
    case PendingSubstitutionValueClass:
        return toCSSPendingSubstitutionValue(this)->customCSSText();
    }
    ASSERT_NOT_REACHED();
    return String();
}

void CSSValue::destroy()
{
    switch (getClassType()) {
    case BasicShapeCircleClass:
        delete toCSSBasicShapeCircleValue(this);
        return;
    case BasicShapeEllipseClass:
        delete toCSSBasicShapeEllipseValue(this);
        return;
    case BasicShapePolygonClass:
        delete toCSSBasicShapePolygonValue(this);
        return;
    case BasicShapeInsetClass:
        delete toCSSBasicShapeInsetValue(this);
        return;
    case BorderImageSliceClass:
        delete toCSSBorderImageSliceValue(this);
        return;
    case ColorClass:
        delete toCSSColorValue(this);
        return;
    case CounterClass:
        delete toCSSCounterValue(this);
        return;
    case CursorImageClass:
        delete toCSSCursorImageValue(this);
        return;
    case FontFaceSrcClass:
        delete toCSSFontFaceSrcValue(this);
        return;
    case FontFamilyClass:
        delete toCSSFontFamilyValue(this);
        return;
    case FontFeatureClass:
        delete toCSSFontFeatureValue(this);
        return;
    case FunctionClass:
        delete toCSSFunctionValue(this);
        return;
    case LinearGradientClass:
        delete toCSSLinearGradientValue(this);
        return;
    case RadialGradientClass:
        delete toCSSRadialGradientValue(this);
        return;
    case CrossfadeClass:
        delete toCSSCrossfadeValue(this);
        return;
    case PaintClass:
        delete toCSSPaintValue(this);
        return;
    case CustomIdentClass:
        delete toCSSCustomIdentValue(this);
        return;
    case ImageClass:
        delete toCSSImageValue(this);
        return;
    case InheritedClass:
        delete toCSSInheritedValue(this);
        return;
    case InitialClass:
        delete toCSSInitialValue(this);
        return;
    case UnsetClass:
        delete toCSSUnsetValue(this);
        return;
    case GridAutoRepeatClass:
        delete toCSSGridAutoRepeatValue(this);
        return;
    case GridLineNamesClass:
        delete toCSSGridLineNamesValue(this);
        return;
    case GridTemplateAreasClass:
        delete toCSSGridTemplateAreasValue(this);
        return;
    case PathClass:
        delete toCSSPathValue(this);
        return;
    case PrimitiveClass:
        delete toCSSPrimitiveValue(this);
        return;
    case QuadClass:
        delete toCSSQuadValue(this);
        return;
    case ReflectClass:
        delete toCSSReflectValue(this);
        return;
    case ShadowClass:
        delete toCSSShadowValue(this);
        return;
    case StringClass:
        delete toCSSStringValue(this);
        return;
    case CubicBezierTimingFunctionClass:
        delete toCSSCubicBezierTimingFunctionValue(this);
        return;
    case StepsTimingFunctionClass:
        delete toCSSStepsTimingFunctionValue(this);
        return;
    case UnicodeRangeClass:
        delete toCSSUnicodeRangeValue(this);
        return;
    case URIClass:
        delete toCSSURIValue(this);
        return;
    case ValuePairClass:
        delete toCSSValuePair(this);
        return;
    case ValueListClass:
        delete toCSSValueList(this);
        return;
    case ImageSetClass:
        delete toCSSImageSetValue(this);
        return;
    case CSSSVGDocumentClass:
        delete toCSSSVGDocumentValue(this);
        return;
    case CSSContentDistributionClass:
        delete toCSSContentDistributionValue(this);
        return;
    case VariableReferenceClass:
        delete toCSSVariableReferenceValue(this);
        return;
    case CustomPropertyDeclarationClass:
        delete toCSSCustomPropertyDeclaration(this);
        return;
    case PendingSubstitutionValueClass:
        delete toCSSPendingSubstitutionValue(this);
        return;
    }
    ASSERT_NOT_REACHED();
}

void CSSValue::finalizeGarbageCollectedObject()
{
    switch (getClassType()) {
    case BasicShapeCircleClass:
        toCSSBasicShapeCircleValue(this)->~CSSBasicShapeCircleValue();
        return;
    case BasicShapeEllipseClass:
        toCSSBasicShapeEllipseValue(this)->~CSSBasicShapeEllipseValue();
        return;
    case BasicShapePolygonClass:
        toCSSBasicShapePolygonValue(this)->~CSSBasicShapePolygonValue();
        return;
    case BasicShapeInsetClass:
        toCSSBasicShapeInsetValue(this)->~CSSBasicShapeInsetValue();
        return;
    case BorderImageSliceClass:
        toCSSBorderImageSliceValue(this)->~CSSBorderImageSliceValue();
        return;
    case ColorClass:
        toCSSColorValue(this)->~CSSColorValue();
        return;
    case CounterClass:
        toCSSCounterValue(this)->~CSSCounterValue();
        return;
    case CursorImageClass:
        toCSSCursorImageValue(this)->~CSSCursorImageValue();
        return;
    case FontFaceSrcClass:
        toCSSFontFaceSrcValue(this)->~CSSFontFaceSrcValue();
        return;
    case FontFamilyClass:
        toCSSFontFamilyValue(this)->~CSSFontFamilyValue();
        return;
    case FontFeatureClass:
        toCSSFontFeatureValue(this)->~CSSFontFeatureValue();
        return;
    case FunctionClass:
        toCSSFunctionValue(this)->~CSSFunctionValue();
        return;
    case LinearGradientClass:
        toCSSLinearGradientValue(this)->~CSSLinearGradientValue();
        return;
    case RadialGradientClass:
        toCSSRadialGradientValue(this)->~CSSRadialGradientValue();
        return;
    case CrossfadeClass:
        toCSSCrossfadeValue(this)->~CSSCrossfadeValue();
        return;
    case PaintClass:
        toCSSPaintValue(this)->~CSSPaintValue();
        return;
    case CustomIdentClass:
        toCSSCustomIdentValue(this)->~CSSCustomIdentValue();
        return;
    case ImageClass:
        toCSSImageValue(this)->~CSSImageValue();
        return;
    case InheritedClass:
        toCSSInheritedValue(this)->~CSSInheritedValue();
        return;
    case InitialClass:
        toCSSInitialValue(this)->~CSSInitialValue();
        return;
    case UnsetClass:
        toCSSUnsetValue(this)->~CSSUnsetValue();
        return;
    case GridAutoRepeatClass:
        toCSSGridAutoRepeatValue(this)->~CSSGridAutoRepeatValue();
        return;
    case GridLineNamesClass:
        toCSSGridLineNamesValue(this)->~CSSGridLineNamesValue();
        return;
    case GridTemplateAreasClass:
        toCSSGridTemplateAreasValue(this)->~CSSGridTemplateAreasValue();
        return;
    case PathClass:
        toCSSPathValue(this)->~CSSPathValue();
        return;
    case PrimitiveClass:
        toCSSPrimitiveValue(this)->~CSSPrimitiveValue();
        return;
    case QuadClass:
        toCSSQuadValue(this)->~CSSQuadValue();
        return;
    case ReflectClass:
        toCSSReflectValue(this)->~CSSReflectValue();
        return;
    case ShadowClass:
        toCSSShadowValue(this)->~CSSShadowValue();
        return;
    case StringClass:
        toCSSStringValue(this)->~CSSStringValue();
        return;
    case CubicBezierTimingFunctionClass:
        toCSSCubicBezierTimingFunctionValue(this)->~CSSCubicBezierTimingFunctionValue();
        return;
    case StepsTimingFunctionClass:
        toCSSStepsTimingFunctionValue(this)->~CSSStepsTimingFunctionValue();
        return;
    case UnicodeRangeClass:
        toCSSUnicodeRangeValue(this)->~CSSUnicodeRangeValue();
        return;
    case URIClass:
        toCSSURIValue(this)->~CSSURIValue();
        return;
    case ValueListClass:
        toCSSValueList(this)->~CSSValueList();
        return;
    case ValuePairClass:
        toCSSValuePair(this)->~CSSValuePair();
        return;
    case ImageSetClass:
        toCSSImageSetValue(this)->~CSSImageSetValue();
        return;
    case CSSSVGDocumentClass:
        toCSSSVGDocumentValue(this)->~CSSSVGDocumentValue();
        return;
    case CSSContentDistributionClass:
        toCSSContentDistributionValue(this)->~CSSContentDistributionValue();
        return;
    case VariableReferenceClass:
        toCSSVariableReferenceValue(this)->~CSSVariableReferenceValue();
        return;
    case CustomPropertyDeclarationClass:
        toCSSCustomPropertyDeclaration(this)->~CSSCustomPropertyDeclaration();
        return;
    case PendingSubstitutionValueClass:
        toCSSPendingSubstitutionValue(this)->~CSSPendingSubstitutionValue();
        return;
    }
    ASSERT_NOT_REACHED();
}

DEFINE_TRACE(CSSValue)
{
    switch (getClassType()) {
    case BasicShapeCircleClass:
        toCSSBasicShapeCircleValue(this)->traceAfterDispatch(visitor);
        return;
    case BasicShapeEllipseClass:
        toCSSBasicShapeEllipseValue(this)->traceAfterDispatch(visitor);
        return;
    case BasicShapePolygonClass:
        toCSSBasicShapePolygonValue(this)->traceAfterDispatch(visitor);
        return;
    case BasicShapeInsetClass:
        toCSSBasicShapeInsetValue(this)->traceAfterDispatch(visitor);
        return;
    case BorderImageSliceClass:
        toCSSBorderImageSliceValue(this)->traceAfterDispatch(visitor);
        return;
    case ColorClass:
        toCSSColorValue(this)->traceAfterDispatch(visitor);
        return;
    case CounterClass:
        toCSSCounterValue(this)->traceAfterDispatch(visitor);
        return;
    case CursorImageClass:
        toCSSCursorImageValue(this)->traceAfterDispatch(visitor);
        return;
    case FontFaceSrcClass:
        toCSSFontFaceSrcValue(this)->traceAfterDispatch(visitor);
        return;
    case FontFamilyClass:
        toCSSFontFamilyValue(this)->traceAfterDispatch(visitor);
        return;
    case FontFeatureClass:
        toCSSFontFeatureValue(this)->traceAfterDispatch(visitor);
        return;
    case FunctionClass:
        toCSSFunctionValue(this)->traceAfterDispatch(visitor);
        return;
    case LinearGradientClass:
        toCSSLinearGradientValue(this)->traceAfterDispatch(visitor);
        return;
    case RadialGradientClass:
        toCSSRadialGradientValue(this)->traceAfterDispatch(visitor);
        return;
    case CrossfadeClass:
        toCSSCrossfadeValue(this)->traceAfterDispatch(visitor);
        return;
    case PaintClass:
        toCSSPaintValue(this)->traceAfterDispatch(visitor);
        return;
    case CustomIdentClass:
        toCSSCustomIdentValue(this)->traceAfterDispatch(visitor);
        return;
    case ImageClass:
        toCSSImageValue(this)->traceAfterDispatch(visitor);
        return;
    case InheritedClass:
        toCSSInheritedValue(this)->traceAfterDispatch(visitor);
        return;
    case InitialClass:
        toCSSInitialValue(this)->traceAfterDispatch(visitor);
        return;
    case UnsetClass:
        toCSSUnsetValue(this)->traceAfterDispatch(visitor);
        return;
    case GridAutoRepeatClass:
        toCSSGridAutoRepeatValue(this)->traceAfterDispatch(visitor);
        return;
    case GridLineNamesClass:
        toCSSGridLineNamesValue(this)->traceAfterDispatch(visitor);
        return;
    case GridTemplateAreasClass:
        toCSSGridTemplateAreasValue(this)->traceAfterDispatch(visitor);
        return;
    case PathClass:
        toCSSPathValue(this)->traceAfterDispatch(visitor);
        return;
    case PrimitiveClass:
        toCSSPrimitiveValue(this)->traceAfterDispatch(visitor);
        return;
    case QuadClass:
        toCSSQuadValue(this)->traceAfterDispatch(visitor);
        return;
    case ReflectClass:
        toCSSReflectValue(this)->traceAfterDispatch(visitor);
        return;
    case ShadowClass:
        toCSSShadowValue(this)->traceAfterDispatch(visitor);
        return;
    case StringClass:
        toCSSStringValue(this)->traceAfterDispatch(visitor);
        return;
    case CubicBezierTimingFunctionClass:
        toCSSCubicBezierTimingFunctionValue(this)->traceAfterDispatch(visitor);
        return;
    case StepsTimingFunctionClass:
        toCSSStepsTimingFunctionValue(this)->traceAfterDispatch(visitor);
        return;
    case UnicodeRangeClass:
        toCSSUnicodeRangeValue(this)->traceAfterDispatch(visitor);
        return;
    case URIClass:
        toCSSURIValue(this)->traceAfterDispatch(visitor);
        return;
    case ValueListClass:
        toCSSValueList(this)->traceAfterDispatch(visitor);
        return;
    case ValuePairClass:
        toCSSValuePair(this)->traceAfterDispatch(visitor);
        return;
    case ImageSetClass:
        toCSSImageSetValue(this)->traceAfterDispatch(visitor);
        return;
    case CSSSVGDocumentClass:
        toCSSSVGDocumentValue(this)->traceAfterDispatch(visitor);
        return;
    case CSSContentDistributionClass:
        toCSSContentDistributionValue(this)->traceAfterDispatch(visitor);
        return;
    case VariableReferenceClass:
        toCSSVariableReferenceValue(this)->traceAfterDispatch(visitor);
        return;
    case CustomPropertyDeclarationClass:
        toCSSCustomPropertyDeclaration(this)->traceAfterDispatch(visitor);
        return;
    case PendingSubstitutionValueClass:
        toCSSPendingSubstitutionValue(this)->traceAfterDispatch(visitor);
        return;
    }
    ASSERT_NOT_REACHED();
}

} // namespace blink

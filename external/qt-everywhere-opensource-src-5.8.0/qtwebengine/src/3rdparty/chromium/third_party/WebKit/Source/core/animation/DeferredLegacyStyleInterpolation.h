// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeferredLegacyStyleInterpolation_h
#define DeferredLegacyStyleInterpolation_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"

namespace blink {

class CSSBasicShapeCircleValue;
class CSSBasicShapeEllipseValue;
class CSSBasicShapePolygonValue;
class CSSBasicShapeInsetValue;
class CSSImageValue;
class CSSPrimitiveValue;
class CSSQuadValue;
class CSSShadowValue;
class CSSSVGDocumentValue;
class CSSValue;
class CSSValueList;
class CSSValuePair;

// TODO(alancutter): Remove this once compositor keyframes are no longer snapshot during EffectInput::convert().
class CORE_EXPORT DeferredLegacyStyleInterpolation {
public:
    STATIC_ONLY(DeferredLegacyStyleInterpolation);
    static bool interpolationRequiresStyleResolve(const CSSValue&);
    static bool interpolationRequiresStyleResolve(const CSSPrimitiveValue&);
    static bool interpolationRequiresStyleResolve(const CSSImageValue&);
    static bool interpolationRequiresStyleResolve(const CSSShadowValue&);
    static bool interpolationRequiresStyleResolve(const CSSSVGDocumentValue&);
    static bool interpolationRequiresStyleResolve(const CSSValueList&);
    static bool interpolationRequiresStyleResolve(const CSSValuePair&);
    static bool interpolationRequiresStyleResolve(const CSSBasicShapeCircleValue&);
    static bool interpolationRequiresStyleResolve(const CSSBasicShapeEllipseValue&);
    static bool interpolationRequiresStyleResolve(const CSSBasicShapePolygonValue&);
    static bool interpolationRequiresStyleResolve(const CSSBasicShapeInsetValue&);
    static bool interpolationRequiresStyleResolve(const CSSQuadValue&);
};

} // namespace blink

#endif

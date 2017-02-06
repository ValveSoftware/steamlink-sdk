// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTransformComponent_h
#define CSSTransformComponent_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/css/CSSFunctionValue.h"

namespace blink {

class CSSMatrixTransformComponent;

class CORE_EXPORT CSSTransformComponent : public GarbageCollectedFinalized<CSSTransformComponent>, public ScriptWrappable {
    WTF_MAKE_NONCOPYABLE(CSSTransformComponent);
    DEFINE_WRAPPERTYPEINFO();
public:
    enum TransformComponentType {
        MatrixType, PerspectiveType, RotationType, ScaleType, SkewType, TranslationType,
        Matrix3DType, Rotation3DType, Scale3DType, Translation3DType
    };

    static CSSTransformComponent* fromCSSValue(const CSSValue&);

    static bool is2DComponentType(TransformComponentType transformType)
    {
        return transformType != Matrix3DType
            && transformType != PerspectiveType
            && transformType != Rotation3DType
            && transformType != Scale3DType
            && transformType != Translation3DType;
    }

    virtual ~CSSTransformComponent() { }

    virtual TransformComponentType type() const = 0;

    bool is2D() const { return is2DComponentType(type()); }

    String cssText() const
    {
        return toCSSValue()->cssText();
    }

    virtual CSSFunctionValue* toCSSValue() const = 0;
    virtual CSSMatrixTransformComponent* asMatrix() const = 0;

    DEFINE_INLINE_VIRTUAL_TRACE() { }

protected:
    CSSTransformComponent() = default;
};

} // namespace blink

#endif

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BasicShapePropertyFunctions_h
#define BasicShapePropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/style/BasicShapes.h"
#include "core/style/ComputedStyle.h"
#include "wtf/Allocator.h"

namespace blink {

class ComputedStyle;

class BasicShapePropertyFunctions {
    STATIC_ONLY(BasicShapePropertyFunctions);
public:
    static const BasicShape* getInitialBasicShape(CSSPropertyID property)
    {
        return getBasicShape(property, ComputedStyle::initialStyle());
    }

    static const BasicShape* getBasicShape(CSSPropertyID property, const ComputedStyle& style)
    {
        switch (property) {
        case CSSPropertyShapeOutside:
            if (!style.shapeOutside())
                return nullptr;
            if (style.shapeOutside()->type() != ShapeValue::Shape)
                return nullptr;
            if (style.shapeOutside()->cssBox() != BoxMissing)
                return nullptr;
            return style.shapeOutside()->shape();
        case CSSPropertyWebkitClipPath:
            if (!style.clipPath())
                return nullptr;
            if (style.clipPath()->type() != ClipPathOperation::SHAPE)
                return nullptr;
            return toShapeClipPathOperation(style.clipPath())->basicShape();
        default:
            NOTREACHED();
            return nullptr;
        }
    }

    static void setBasicShape(CSSPropertyID property, ComputedStyle& style, PassRefPtr<BasicShape> shape)
    {
        switch (property) {
        case CSSPropertyShapeOutside:
            style.setShapeOutside(ShapeValue::createShapeValue(shape, BoxMissing));
            break;
        case CSSPropertyWebkitClipPath:
            style.setClipPath(ShapeClipPathOperation::create(shape));
            break;
        default:
            NOTREACHED();
        }
    }
};

} // namespace blink

#endif // BasicShapePropertyFunctions_h

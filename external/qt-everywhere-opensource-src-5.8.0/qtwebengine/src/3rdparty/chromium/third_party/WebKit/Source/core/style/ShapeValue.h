/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef ShapeValue_h
#define ShapeValue_h

#include "core/fetch/ImageResource.h"
#include "core/style/BasicShapes.h"
#include "core/style/DataEquivalency.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/style/StyleImage.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class ShapeValue final : public GarbageCollectedFinalized<ShapeValue> {
public:
    enum ShapeValueType {
        // The Auto value is defined by a null ShapeValue*
        Shape,
        Box,
        Image
    };

    static ShapeValue* createShapeValue(PassRefPtr<BasicShape> shape, CSSBoxType cssBox)
    {
        return new ShapeValue(shape, cssBox);
    }

    static ShapeValue* createBoxShapeValue(CSSBoxType cssBox)
    {
        return new ShapeValue(cssBox);
    }

    static ShapeValue* createImageValue(StyleImage* image)
    {
        return new ShapeValue(image);
    }

    ShapeValueType type() const { return m_type; }
    BasicShape* shape() const { return m_shape.get(); }

    StyleImage* image() const { return m_image.get(); }
    bool isImageValid() const
    {
        if (!image())
            return false;
        if (image()->isImageResource() || image()->isImageResourceSet())
            return image()->cachedImage() && image()->cachedImage()->hasImage();
        return image()->isGeneratedImage();
    }
    void setImage(StyleImage* image)
    {
        ASSERT(type() == Image);
        if (m_image != image)
            m_image = image;
    }
    CSSBoxType cssBox() const { return m_cssBox; }

    bool operator==(const ShapeValue& other) const;

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_image);
    }

private:
    ShapeValue(PassRefPtr<BasicShape> shape, CSSBoxType cssBox)
        : m_type(Shape)
        , m_shape(shape)
        , m_cssBox(cssBox)
    {
    }
    ShapeValue(ShapeValueType type)
        : m_type(type)
        , m_cssBox(BoxMissing)
    {
    }
    ShapeValue(StyleImage* image)
        : m_type(Image)
        , m_image(image)
        , m_cssBox(ContentBox)
    {
    }
    ShapeValue(CSSBoxType cssBox)
        : m_type(Box)
        , m_cssBox(cssBox)
    {
    }


    ShapeValueType m_type;
    RefPtr<BasicShape> m_shape;
    Member<StyleImage> m_image;
    CSSBoxType m_cssBox;
};

inline bool ShapeValue::operator==(const ShapeValue& other) const
{
    if (type() != other.type())
        return false;

    switch (type()) {
    case Shape:
        return dataEquivalent(shape(), other.shape()) && cssBox() == other.cssBox();
    case Box:
        return cssBox() == other.cssBox();
    case Image:
        return dataEquivalent(image(), other.image());
    }

    ASSERT_NOT_REACHED();
    return false;
}

} // namespace blink

#endif

/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AnimatableSVGPaint_h
#define AnimatableSVGPaint_h

#include "core/animation/animatable/AnimatableColor.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "core/style/SVGComputedStyleDefs.h"

namespace blink {

class AnimatableSVGPaint final : public AnimatableValue {
public:
    ~AnimatableSVGPaint() override { }
    static PassRefPtr<AnimatableSVGPaint> create(
        SVGPaintType type, SVGPaintType visitedLinkType,
        const Color& color, const Color& visitedLinkColor,
        const String& uri, const String& visitedLinkURI)
    {
        return create(type, visitedLinkType, AnimatableColor::create(color, visitedLinkColor), uri, visitedLinkURI);
    }
    static PassRefPtr<AnimatableSVGPaint> create(
        SVGPaintType type, SVGPaintType visitedLinkType,
        PassRefPtr<AnimatableColor> color,
        const String& uri, const String& visitedLinkURI)
    {
        return adoptRef(new AnimatableSVGPaint(type, visitedLinkType, color, uri, visitedLinkURI));
    }
    SVGPaintType paintType() const { return m_type; }
    SVGPaintType visitedLinkPaintType() const { return m_visitedLinkType; }
    Color getColor() const { return m_color->getColor(); }
    Color visitedLinkColor() const { return m_color->visitedLinkColor(); }
    const String& uri() const { return m_uri; }
    const String& visitedLinkURI() const { return m_visitedLinkURI; }

protected:
    PassRefPtr<AnimatableValue> interpolateTo(const AnimatableValue*, double fraction) const override;
    bool usesDefaultInterpolationWith(const AnimatableValue*) const override;

private:
    AnimatableSVGPaint(SVGPaintType type, SVGPaintType visitedLinkType, PassRefPtr<AnimatableColor> color, const String& uri, const String& visitedLinkURI)
        : m_type(type)
        , m_visitedLinkType(visitedLinkType)
        , m_color(color)
        , m_uri(uri)
        , m_visitedLinkURI(visitedLinkURI)
    {
    }
    AnimatableType type() const override { return TypeSVGPaint; }
    bool equalTo(const AnimatableValue*) const override;

    SVGPaintType m_type;
    SVGPaintType m_visitedLinkType;
    // AnimatableColor includes a visited link color.
    RefPtr<AnimatableColor> m_color;
    String m_uri;
    String m_visitedLinkURI;
};

DEFINE_ANIMATABLE_VALUE_TYPE_CASTS(AnimatableSVGPaint, isSVGPaint());

} // namespace blink

#endif // AnimatableSVGPaint_h

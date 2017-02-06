/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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
 */

#include "modules/canvas2d/CanvasStyle.h"

#include "core/CSSPropertyNames.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParser.h"
#include "core/html/HTMLCanvasElement.h"
#include "modules/canvas2d/CanvasGradient.h"
#include "modules/canvas2d/CanvasPattern.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkShader.h"
#include "wtf/PassRefPtr.h"

namespace blink {

enum ColorParseResult { ParsedRGBA, ParsedCurrentColor, ParsedSystemColor, ParseFailed };

static ColorParseResult parseColor(Color& parsedColor, const String& colorString)
{
    if (equalIgnoringCase(colorString, "currentcolor"))
        return ParsedCurrentColor;
    const bool useStrictParsing = true;
    if (CSSParser::parseColor(parsedColor, colorString, useStrictParsing))
        return ParsedRGBA;
    if (CSSParser::parseSystemColor(parsedColor, colorString))
        return ParsedSystemColor;
    return ParseFailed;
}

static Color currentColor(HTMLCanvasElement* canvas)
{
    if (!canvas || !canvas->inShadowIncludingDocument() || !canvas->inlineStyle())
        return Color::black;
    Color color = Color::black;
    CSSParser::parseColor(color, canvas->inlineStyle()->getPropertyValue(CSSPropertyColor));
    return color;
}

bool parseColorOrCurrentColor(Color& parsedColor, const String& colorString, HTMLCanvasElement* canvas)
{
    ColorParseResult parseResult = parseColor(parsedColor, colorString);
    switch (parseResult) {
    case ParsedRGBA:
    case ParsedSystemColor:
        return true;
    case ParsedCurrentColor:
        parsedColor = canvas ? currentColor(canvas) : Color::black;
        return true;
    case ParseFailed:
        return false;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

CanvasStyle::CanvasStyle(RGBA32 rgba)
    : m_type(ColorRGBA)
    , m_rgba(rgba)
{
}

CanvasStyle::CanvasStyle(CanvasGradient* gradient)
    : m_type(Gradient)
    , m_gradient(gradient)
{
}

CanvasStyle::CanvasStyle(CanvasPattern* pattern)
    : m_type(ImagePattern)
    , m_pattern(pattern)
{
}

CanvasStyle* CanvasStyle::createFromGradient(CanvasGradient* gradient)
{
    ASSERT(gradient);
    return new CanvasStyle(gradient);
}

CanvasStyle* CanvasStyle::createFromPattern(CanvasPattern* pattern)
{
    ASSERT(pattern);
    return new CanvasStyle(pattern);
}

void CanvasStyle::applyToPaint(SkPaint& paint) const
{
    switch (m_type) {
    case ColorRGBA:
        paint.setShader(nullptr);
        break;
    case Gradient:
        getCanvasGradient()->getGradient()->applyToPaint(paint, SkMatrix::I());
        break;
    case ImagePattern:
        getCanvasPattern()->getPattern()->applyToPaint(paint, affineTransformToSkMatrix(getCanvasPattern()->getTransform()));
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

RGBA32 CanvasStyle::paintColor() const
{
    if (m_type == ColorRGBA)
        return m_rgba;
    ASSERT(m_type == Gradient || m_type == ImagePattern);
    return Color::black;
}

DEFINE_TRACE(CanvasStyle)
{
    visitor->trace(m_gradient);
    visitor->trace(m_pattern);
}

} // namespace blink

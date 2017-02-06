// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebFont.h"

#include "platform/fonts/Font.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/SkPictureBuilder.h"
#include "platform/text/TextRun.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebFontDescription.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebTextRun.h"

namespace blink {

WebFont* WebFont::create(const WebFontDescription& description)
{
    return new WebFont(description);
}

class WebFont::Impl final {
public:
    explicit Impl(const WebFontDescription& description)
        : m_font(description)
    {
        m_font.update(nullptr);
    }

    const Font& getFont() const { return m_font; }

private:
    Font m_font;
};

WebFont::WebFont(const WebFontDescription& description)
    : m_private(new Impl(description))
{
}

WebFont::~WebFont()
{
    m_private.reset(0);
}

WebFontDescription WebFont::getFontDescription() const
{
    return WebFontDescription(m_private->getFont().getFontDescription());
}

int WebFont::ascent() const
{
    return m_private->getFont().getFontMetrics().ascent();
}

int WebFont::descent() const
{
    return m_private->getFont().getFontMetrics().descent();
}

int WebFont::height() const
{
    return m_private->getFont().getFontMetrics().height();
}

int WebFont::lineSpacing() const
{
    return m_private->getFont().getFontMetrics().lineSpacing();
}

float WebFont::xHeight() const
{
    return m_private->getFont().getFontMetrics().xHeight();
}

void WebFont::drawText(WebCanvas* canvas, const WebTextRun& run,
    const WebFloatPoint& leftBaseline, WebColor color, const WebRect& clip) const
{
    FontCachePurgePreventer fontCachePurgePreventer;
    FloatRect textClipRect(clip);
    TextRun textRun(run);
    TextRunPaintInfo runInfo(textRun);
    runInfo.bounds = textClipRect;

    IntRect intRect(clip);
    SkPictureBuilder pictureBuilder(intRect);
    GraphicsContext& context = pictureBuilder.context();

    {
        DrawingRecorder drawingRecorder(context, pictureBuilder, DisplayItem::WebFont, intRect);
        context.save();
        context.setFillColor(color);
        context.clip(textClipRect);
        context.drawText(m_private->getFont(), runInfo, leftBaseline);
        context.restore();
    }

    pictureBuilder.endRecording()->playback(canvas);
}

int WebFont::calculateWidth(const WebTextRun& run) const
{
    return m_private->getFont().width(run, 0);
}

int WebFont::offsetForPosition(const WebTextRun& run, float position) const
{
    return m_private->getFont().offsetForPosition(run, position, true);
}

WebFloatRect WebFont::selectionRectForText(const WebTextRun& run, const WebFloatPoint& leftBaseline, int height, int from, int to) const
{
    return m_private->getFont().selectionRectForText(run, leftBaseline, height, from, to);
}

} // namespace blink

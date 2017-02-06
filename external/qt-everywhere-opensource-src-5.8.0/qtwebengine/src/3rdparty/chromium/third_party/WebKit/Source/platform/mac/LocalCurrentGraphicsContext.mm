/*
 * Copyright (C) 2006 Apple Computer, Inc.
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

#include "platform/mac/LocalCurrentGraphicsContext.h"

#include <AppKit/NSGraphicsContext.h>
#include "platform/graphics/GraphicsContext.h"
#include "platform/mac/ThemeMac.h"
#include "platform_canvas.h"

namespace blink {

LocalCurrentGraphicsContext::LocalCurrentGraphicsContext(GraphicsContext& graphicsContext, const IntRect& dirtyRect)
    : LocalCurrentGraphicsContext(graphicsContext.canvas(), graphicsContext.deviceScaleFactor(), dirtyRect)
{
}

static IntRect clampRect(int size, const IntRect& rect)
{
    IntRect clampedRect(rect);
    clampedRect.setSize(IntSize(
        std::min(size, clampedRect.width()),
        std::min(size, clampedRect.height())));
    return clampedRect;
}

static const int kMaxDirtyRectPixelSize = 10000;

LocalCurrentGraphicsContext::LocalCurrentGraphicsContext(SkCanvas* canvas, float deviceScaleFactor, const IntRect& dirtyRect)
    : m_didSetGraphicsContext(false)
    , m_inflatedDirtyRect(ThemeMac::inflateRectForAA(dirtyRect))
    , m_skiaBitLocker(canvas,
                      m_inflatedDirtyRect,
                      deviceScaleFactor)
{
    m_savedCanvas = canvas;
    canvas->save();

    // Constrain the maximum size of what we paint to something reasonable. This accordingly
    // means we will not paint the entirety of truly huge native form elements, which is
    // deemed an acceptable tradeoff for this simple approach to manage such an edge case.
    if (dirtyRect.width() > kMaxDirtyRectPixelSize || dirtyRect.height() > kMaxDirtyRectPixelSize)
        canvas->clipRect(clampRect(kMaxDirtyRectPixelSize, dirtyRect), SkRegion::kIntersect_Op);

    CGContextRef cgContext = this->cgContext();
    if (cgContext == [[NSGraphicsContext currentContext] graphicsPort]) {
        m_savedNSGraphicsContext = 0;
        return;
    }

    m_savedNSGraphicsContext = [[NSGraphicsContext currentContext] retain];
    NSGraphicsContext* newContext = [NSGraphicsContext graphicsContextWithGraphicsPort:cgContext flipped:YES];
    [NSGraphicsContext setCurrentContext:newContext];
    m_didSetGraphicsContext = true;
}

LocalCurrentGraphicsContext::~LocalCurrentGraphicsContext()
{
    if (m_didSetGraphicsContext) {
        [NSGraphicsContext setCurrentContext:m_savedNSGraphicsContext];
        [m_savedNSGraphicsContext release];
    }

    m_savedCanvas->restore();
}

CGContextRef LocalCurrentGraphicsContext::cgContext()
{
    // This synchronizes the CGContext to reflect the current SkCanvas state.
    // The implementation may not return the same CGContext each time.
    CGContextRef cgContext = m_skiaBitLocker.cgContext();

    return cgContext;
}

}

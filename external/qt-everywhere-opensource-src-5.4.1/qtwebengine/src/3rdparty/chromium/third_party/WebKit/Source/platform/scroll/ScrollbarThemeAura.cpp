/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT{
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,{
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/scroll/ScrollbarThemeAura.h"

#include "platform/LayoutTestSupport.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/scroll/ScrollbarThemeClient.h"
#include "platform/scroll/ScrollbarThemeOverlay.h"
#include "public/platform/Platform.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebThemeEngine.h"

namespace WebCore {

static bool useMockTheme()
{
    return isRunningLayoutTest();
}

ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    if (RuntimeEnabledFeatures::overlayScrollbarsEnabled()) {
        DEFINE_STATIC_LOCAL(ScrollbarThemeOverlay, theme, (10, 0, ScrollbarThemeOverlay::AllowHitTest));
        return &theme;
    }

    DEFINE_STATIC_LOCAL(ScrollbarThemeAura, theme, ());
    return &theme;
}

int ScrollbarThemeAura::scrollbarThickness(ScrollbarControlSize controlSize)
{
    // Horiz and Vert scrollbars are the same thickness.
    // In unit tests we don't have the mock theme engine (because of layering violations), so we hard code the size (see bug 327470).
    if (useMockTheme())
        return 15;
    IntSize scrollbarSize = blink::Platform::current()->themeEngine()->getSize(blink::WebThemeEngine::PartScrollbarVerticalTrack);
    return scrollbarSize.width();
}

void ScrollbarThemeAura::paintTrackPiece(GraphicsContext* gc, ScrollbarThemeClient* scrollbar, const IntRect& rect, ScrollbarPart partType)
{
    if (gc->paintingDisabled())
        return;
    blink::WebThemeEngine::State state = scrollbar->hoveredPart() == partType ? blink::WebThemeEngine::StateHover : blink::WebThemeEngine::StateNormal;

    if (useMockTheme() && !scrollbar->enabled())
        state = blink::WebThemeEngine::StateDisabled;

    IntRect alignRect = trackRect(scrollbar, false);
    blink::WebThemeEngine::ExtraParams extraParams;
    blink::WebCanvas* canvas = gc->canvas();
    extraParams.scrollbarTrack.isBack = (partType == BackTrackPart);
    extraParams.scrollbarTrack.trackX = alignRect.x();
    extraParams.scrollbarTrack.trackY = alignRect.y();
    extraParams.scrollbarTrack.trackWidth = alignRect.width();
    extraParams.scrollbarTrack.trackHeight = alignRect.height();
    blink::Platform::current()->themeEngine()->paint(canvas, scrollbar->orientation() == HorizontalScrollbar ? blink::WebThemeEngine::PartScrollbarHorizontalTrack : blink::WebThemeEngine::PartScrollbarVerticalTrack, state, blink::WebRect(rect), &extraParams);
}

void ScrollbarThemeAura::paintButton(GraphicsContext* gc, ScrollbarThemeClient* scrollbar, const IntRect& rect, ScrollbarPart part)
{
    if (gc->paintingDisabled())
        return;
    blink::WebThemeEngine::Part paintPart;
    blink::WebThemeEngine::State state = blink::WebThemeEngine::StateNormal;
    blink::WebCanvas* canvas = gc->canvas();
    bool checkMin = false;
    bool checkMax = false;

    if (scrollbar->orientation() == HorizontalScrollbar) {
        if (part == BackButtonStartPart) {
            paintPart = blink::WebThemeEngine::PartScrollbarLeftArrow;
            checkMin = true;
        } else if (useMockTheme() && part != ForwardButtonEndPart) {
            return;
        } else {
            paintPart = blink::WebThemeEngine::PartScrollbarRightArrow;
            checkMax = true;
        }
    } else {
        if (part == BackButtonStartPart) {
            paintPart = blink::WebThemeEngine::PartScrollbarUpArrow;
            checkMin = true;
        } else if (useMockTheme() && part != ForwardButtonEndPart) {
            return;
        } else {
            paintPart = blink::WebThemeEngine::PartScrollbarDownArrow;
            checkMax = true;
        }
    }
    if (useMockTheme() && !scrollbar->enabled()) {
        state = blink::WebThemeEngine::StateDisabled;
    } else if (!useMockTheme() && ((checkMin && (scrollbar->currentPos() <= 0))
        || (checkMax && scrollbar->currentPos() >= scrollbar->maximum()))) {
        state = blink::WebThemeEngine::StateDisabled;
    } else {
        if (part == scrollbar->pressedPart())
            state = blink::WebThemeEngine::StatePressed;
        else if (part == scrollbar->hoveredPart())
            state = blink::WebThemeEngine::StateHover;
    }
    blink::Platform::current()->themeEngine()->paint(canvas, paintPart, state, blink::WebRect(rect), 0);
}

void ScrollbarThemeAura::paintThumb(GraphicsContext* gc, ScrollbarThemeClient* scrollbar, const IntRect& rect)
{
    if (gc->paintingDisabled())
        return;
    blink::WebThemeEngine::State state;
    blink::WebCanvas* canvas = gc->canvas();
    if (scrollbar->pressedPart() == ThumbPart)
        state = blink::WebThemeEngine::StatePressed;
    else if (scrollbar->hoveredPart() == ThumbPart)
        state = blink::WebThemeEngine::StateHover;
    else
        state = blink::WebThemeEngine::StateNormal;
    blink::Platform::current()->themeEngine()->paint(canvas, scrollbar->orientation() == HorizontalScrollbar ? blink::WebThemeEngine::PartScrollbarHorizontalThumb : blink::WebThemeEngine::PartScrollbarVerticalThumb, state, blink::WebRect(rect), 0);
}

IntSize ScrollbarThemeAura::buttonSize(ScrollbarThemeClient* scrollbar)
{
    if (scrollbar->orientation() == VerticalScrollbar) {
        IntSize size = blink::Platform::current()->themeEngine()->getSize(blink::WebThemeEngine::PartScrollbarUpArrow);
        return IntSize(size.width(), scrollbar->height() < 2 * size.height() ? scrollbar->height() / 2 : size.height());
    }

    // HorizontalScrollbar
    IntSize size = blink::Platform::current()->themeEngine()->getSize(blink::WebThemeEngine::PartScrollbarLeftArrow);
    return IntSize(scrollbar->width() < 2 * size.width() ? scrollbar->width() / 2 : size.width(), size.height());
}

int ScrollbarThemeAura::minimumThumbLength(ScrollbarThemeClient* scrollbar)
{
    if (scrollbar->orientation() == VerticalScrollbar) {
        IntSize size = blink::Platform::current()->themeEngine()->getSize(blink::WebThemeEngine::PartScrollbarVerticalThumb);
        return size.height();
    }

    IntSize size = blink::Platform::current()->themeEngine()->getSize(blink::WebThemeEngine::PartScrollbarHorizontalThumb);
    return size.width();
}

} // namespace WebCore

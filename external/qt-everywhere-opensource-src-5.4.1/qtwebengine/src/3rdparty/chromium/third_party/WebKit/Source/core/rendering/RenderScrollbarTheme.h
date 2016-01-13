/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RenderScrollbarTheme_h
#define RenderScrollbarTheme_h

#include "platform/scroll/ScrollbarTheme.h"

namespace WebCore {

class PlatformMouseEvent;

class RenderScrollbarTheme FINAL : public ScrollbarTheme {
public:
    virtual ~RenderScrollbarTheme() { }

    virtual int scrollbarThickness(ScrollbarControlSize controlSize) OVERRIDE { return ScrollbarTheme::theme()->scrollbarThickness(controlSize); }

    virtual ScrollbarButtonsPlacement buttonsPlacement() const OVERRIDE { return ScrollbarTheme::theme()->buttonsPlacement(); }

    virtual void paintScrollCorner(GraphicsContext*, const IntRect& cornerRect) OVERRIDE;

    virtual bool shouldCenterOnThumb(ScrollbarThemeClient* scrollbar, const PlatformMouseEvent& event) OVERRIDE { return ScrollbarTheme::theme()->shouldCenterOnThumb(scrollbar, event); }
    virtual bool shouldSnapBackToDragOrigin(ScrollbarThemeClient* scrollbar, const PlatformMouseEvent& event) OVERRIDE { return ScrollbarTheme::theme()->shouldSnapBackToDragOrigin(scrollbar, event); }

    virtual double initialAutoscrollTimerDelay() OVERRIDE { return ScrollbarTheme::theme()->initialAutoscrollTimerDelay(); }
    virtual double autoscrollTimerDelay() OVERRIDE { return ScrollbarTheme::theme()->autoscrollTimerDelay(); }

    virtual void registerScrollbar(ScrollbarThemeClient* scrollbar) OVERRIDE { return ScrollbarTheme::theme()->registerScrollbar(scrollbar); }
    virtual void unregisterScrollbar(ScrollbarThemeClient* scrollbar) OVERRIDE { return ScrollbarTheme::theme()->unregisterScrollbar(scrollbar); }

    virtual int minimumThumbLength(ScrollbarThemeClient*) OVERRIDE;

    void buttonSizesAlongTrackAxis(ScrollbarThemeClient*, int& beforeSize, int& afterSize);

    static RenderScrollbarTheme* renderScrollbarTheme();

protected:
    virtual bool hasButtons(ScrollbarThemeClient*) OVERRIDE;
    virtual bool hasThumb(ScrollbarThemeClient*) OVERRIDE;

    virtual IntRect backButtonRect(ScrollbarThemeClient*, ScrollbarPart, bool painting = false) OVERRIDE;
    virtual IntRect forwardButtonRect(ScrollbarThemeClient*, ScrollbarPart, bool painting = false) OVERRIDE;
    virtual IntRect trackRect(ScrollbarThemeClient*, bool painting = false) OVERRIDE;

    virtual void paintScrollbarBackground(GraphicsContext*, ScrollbarThemeClient*) OVERRIDE;
    virtual void paintTrackBackground(GraphicsContext*, ScrollbarThemeClient*, const IntRect&) OVERRIDE;
    virtual void paintTrackPiece(GraphicsContext*, ScrollbarThemeClient*, const IntRect&, ScrollbarPart) OVERRIDE;
    virtual void paintButton(GraphicsContext*, ScrollbarThemeClient*, const IntRect&, ScrollbarPart) OVERRIDE;
    virtual void paintThumb(GraphicsContext*, ScrollbarThemeClient*, const IntRect&) OVERRIDE;
    virtual void paintTickmarks(GraphicsContext*, ScrollbarThemeClient*, const IntRect&) OVERRIDE;

    virtual IntRect constrainTrackRectToTrackPieces(ScrollbarThemeClient*, const IntRect&) OVERRIDE;
};

} // namespace WebCore

#endif // RenderScrollbarTheme_h

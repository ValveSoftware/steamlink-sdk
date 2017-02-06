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

#ifndef ScrollbarThemeMac_h
#define ScrollbarThemeMac_h

#include "platform/mac/NSScrollerImpDetails.h"
#include "platform/scroll/ScrollbarTheme.h"

typedef id ScrollbarPainter;

class SkCanvas;

namespace blink {

class Pattern;

class PLATFORM_EXPORT ScrollbarThemeMac : public ScrollbarTheme {
public:
    ~ScrollbarThemeMac() override;

    void registerScrollbar(ScrollbarThemeClient&) override;
    void unregisterScrollbar(ScrollbarThemeClient&) override;
    void preferencesChanged(float initialButtonDelay, float autoscrollButtonDelay, NSScrollerStyle preferredScrollerStyle, bool redraw, WebScrollbarButtonsPlacement);

    bool supportsControlTints() const override { return true; }

    double initialAutoscrollTimerDelay() override;
    double autoscrollTimerDelay() override;

    void paintTickmarks(GraphicsContext&, const Scrollbar&, const IntRect&) override;

    bool shouldRepaintAllPartsOnInvalidation() const override { return false; }
    ScrollbarPart invalidateOnThumbPositionChange(
        const ScrollbarThemeClient&, float oldPosition, float newPosition) const override;
    void updateEnabledState(const ScrollbarThemeClient&) override;
    int scrollbarThickness(ScrollbarControlSize = RegularScrollbar) override;
    bool usesOverlayScrollbars() const override;
    void updateScrollbarOverlayStyle(const ScrollbarThemeClient&) override;
    WebScrollbarButtonsPlacement buttonsPlacement() const override;

    void setNewPainterForScrollbar(ScrollbarThemeClient&, ScrollbarPainter);
    ScrollbarPainter painterForScrollbar(const ScrollbarThemeClient&) const;

    void paintTrackBackground(GraphicsContext&, const Scrollbar&, const IntRect&) override;
    void paintThumb(GraphicsContext&, const Scrollbar&, const IntRect&) override;

    float thumbOpacity(const ScrollbarThemeClient&) const override;

    static NSScrollerStyle recommendedScrollerStyle();

protected:
    int maxOverlapBetweenPages() override { return 40; }

    bool shouldDragDocumentInsteadOfThumb(const ScrollbarThemeClient&, const PlatformMouseEvent&) override;
    int scrollbarPartToHIPressedState(ScrollbarPart);

    virtual void updateButtonPlacement(WebScrollbarButtonsPlacement) {}

    void paintGivenTickmarks(SkCanvas*, const Scrollbar&, const IntRect&, const Vector<IntRect>&);

    IntRect trackRect(const ScrollbarThemeClient&, bool painting = false) override;
    IntRect backButtonRect(const ScrollbarThemeClient&, ScrollbarPart, bool painting = false) override;
    IntRect forwardButtonRect(const ScrollbarThemeClient&, ScrollbarPart, bool painting = false) override;

    bool hasButtons(const ScrollbarThemeClient&) override { return false; }
    bool hasThumb(const ScrollbarThemeClient&) override;

    int minimumThumbLength(const ScrollbarThemeClient&) override;

    RefPtr<Pattern> m_overhangPattern;
};

}

#endif // ScrollbarThemeMac_h

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

#ifndef ScrollbarThemeMacCommon_h
#define ScrollbarThemeMacCommon_h

#include "platform/mac/NSScrollerImpDetails.h"
#include "platform/scroll/ScrollbarTheme.h"

namespace WebCore {

class Pattern;

class PLATFORM_EXPORT ScrollbarThemeMacCommon : public ScrollbarTheme {
public:
    virtual ~ScrollbarThemeMacCommon();

    virtual void registerScrollbar(ScrollbarThemeClient*) OVERRIDE;
    virtual void unregisterScrollbar(ScrollbarThemeClient*) OVERRIDE;
    void preferencesChanged(float initialButtonDelay, float autoscrollButtonDelay, NSScrollerStyle preferredScrollerStyle, bool redraw);

    virtual bool supportsControlTints() const OVERRIDE { return true; }

    virtual double initialAutoscrollTimerDelay() OVERRIDE;
    virtual double autoscrollTimerDelay() OVERRIDE;

    virtual void paintOverhangBackground(GraphicsContext*, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect) OVERRIDE;
    virtual void paintOverhangShadows(GraphicsContext*, const IntSize& scrollOffset, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect) OVERRIDE;
    virtual void paintTickmarks(GraphicsContext*, ScrollbarThemeClient*, const IntRect&) OVERRIDE;

    static NSScrollerStyle recommendedScrollerStyle();

    static bool isOverlayAPIAvailable();

protected:
    virtual int maxOverlapBetweenPages() OVERRIDE { return 40; }

    virtual bool shouldDragDocumentInsteadOfThumb(ScrollbarThemeClient*, const PlatformMouseEvent&) OVERRIDE;
    int scrollbarPartToHIPressedState(ScrollbarPart);

    virtual void updateButtonPlacement() { }

    void paintGivenTickmarks(GraphicsContext*, ScrollbarThemeClient*, const IntRect&, const Vector<IntRect>&);

    RefPtr<Pattern> m_overhangPattern;
};

}

#endif // ScrollbarThemeMacCommon_h

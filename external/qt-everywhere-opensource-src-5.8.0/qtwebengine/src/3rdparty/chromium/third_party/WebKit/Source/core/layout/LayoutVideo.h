/*
 * Copyright (C) 2007, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef LayoutVideo_h
#define LayoutVideo_h

#include "core/layout/LayoutMedia.h"

namespace blink {

class HTMLVideoElement;

class LayoutVideo final : public LayoutMedia {
public:
    LayoutVideo(HTMLVideoElement*);
    ~LayoutVideo() override;

    IntRect videoBox() const;

    static LayoutSize defaultSize();

    bool supportsAcceleratedRendering() const;

    bool shouldDisplayVideo() const;
    HTMLVideoElement* videoElement() const;

    const char* name() const override { return "LayoutVideo"; }

private:
    void updateFromElement() override;

    void intrinsicSizeChanged() override;
    LayoutSize calculateIntrinsicSize();
    void updateIntrinsicSize();

    void imageChanged(WrappedImagePtr, const IntRect*) override;

    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectVideo || LayoutMedia::isOfType(type); }

    void paintReplaced(const PaintInfo&, const LayoutPoint&) const override;

    void layout() override;

    LayoutUnit computeReplacedLogicalWidth(ShouldComputePreferred = ComputeActual) const override;
    LayoutUnit computeReplacedLogicalHeight(LayoutUnit estimatedUsedWidth = LayoutUnit()) const override;
    LayoutUnit minimumReplacedHeight() const override;

    LayoutUnit offsetLeft(const Element*) const override;
    LayoutUnit offsetTop(const Element*) const override;
    LayoutUnit offsetWidth() const override;
    LayoutUnit offsetHeight() const override;

    CompositingReasons additionalCompositingReasons() const override;

    void updatePlayer();

    LayoutSize m_cachedImageSize;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutVideo, isVideo());

} // namespace blink

#endif // LayoutVideo_h

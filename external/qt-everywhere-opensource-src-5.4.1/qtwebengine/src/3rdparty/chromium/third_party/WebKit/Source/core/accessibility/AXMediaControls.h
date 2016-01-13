/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef AXMediaControls_h
#define AXMediaControls_h

#include "core/accessibility/AXSlider.h"
#include "core/html/shadow/MediaControlElements.h"

namespace WebCore {

class AccessibilityMediaControl : public AXRenderObject {

public:
    static PassRefPtr<AXObject> create(RenderObject*);
    virtual ~AccessibilityMediaControl() { }

    virtual AccessibilityRole roleValue() const OVERRIDE;

    virtual String title() const OVERRIDE FINAL;
    virtual String accessibilityDescription() const OVERRIDE;
    virtual String helpText() const OVERRIDE;

protected:
    explicit AccessibilityMediaControl(RenderObject*);
    MediaControlElementType controlType() const;
    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;
};


class AccessibilityMediaTimeline FINAL : public AXSlider {

public:
    static PassRefPtr<AXObject> create(RenderObject*);
    virtual ~AccessibilityMediaTimeline() { }

    virtual String helpText() const OVERRIDE;
    virtual String valueDescription() const OVERRIDE;
    const AtomicString& getAttribute(const QualifiedName& attribute) const;

private:
    explicit AccessibilityMediaTimeline(RenderObject*);
};


class AXMediaControlsContainer FINAL : public AccessibilityMediaControl {

public:
    static PassRefPtr<AXObject> create(RenderObject*);
    virtual ~AXMediaControlsContainer() { }

    virtual AccessibilityRole roleValue() const OVERRIDE { return ToolbarRole; }

    virtual String helpText() const OVERRIDE;
    virtual String accessibilityDescription() const OVERRIDE;

private:
    explicit AXMediaControlsContainer(RenderObject*);
    bool controllingVideoElement() const;
    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;
};


class AccessibilityMediaTimeDisplay FINAL : public AccessibilityMediaControl {

public:
    static PassRefPtr<AXObject> create(RenderObject*);
    virtual ~AccessibilityMediaTimeDisplay() { }

    virtual AccessibilityRole roleValue() const OVERRIDE { return StaticTextRole; }

    virtual String stringValue() const OVERRIDE;
    virtual String accessibilityDescription() const OVERRIDE;

private:
    explicit AccessibilityMediaTimeDisplay(RenderObject*);
    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;
};


} // namespace WebCore

#endif // AXMediaControls_h

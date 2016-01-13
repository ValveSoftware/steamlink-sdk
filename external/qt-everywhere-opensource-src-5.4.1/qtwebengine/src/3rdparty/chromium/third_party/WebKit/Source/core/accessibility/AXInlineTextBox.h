/*
 * Copyright (C) 2013, Google Inc. All rights reserved.
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

#ifndef AXInlineTextBox_h
#define AXInlineTextBox_h

#include "core/accessibility/AXObject.h"
#include "core/rendering/AbstractInlineTextBox.h"

namespace WebCore {

class AXInlineTextBox FINAL : public AXObject {

private:
    AXInlineTextBox(PassRefPtr<AbstractInlineTextBox>);

public:
    static PassRefPtr<AXInlineTextBox> create(PassRefPtr<AbstractInlineTextBox>);
    virtual ~AXInlineTextBox();

    virtual void init() OVERRIDE;
    virtual void detach() OVERRIDE;

    void setInlineTextBox(AbstractInlineTextBox* inlineTextBox) { m_inlineTextBox = inlineTextBox; }

    virtual AccessibilityRole roleValue() const OVERRIDE { return InlineTextBoxRole; }
    virtual String stringValue() const OVERRIDE;
    virtual void textCharacterOffsets(Vector<int>&) const OVERRIDE;
    virtual void wordBoundaries(Vector<PlainTextRange>& words) const OVERRIDE;
    virtual LayoutRect elementRect() const OVERRIDE;
    virtual AXObject* parentObject() const OVERRIDE;
    virtual AccessibilityTextDirection textDirection() const OVERRIDE;

private:
    RefPtr<AbstractInlineTextBox> m_inlineTextBox;
    AXObjectCache* m_axObjectCache;

    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;
};

} // namespace WebCore

#endif // AXInlineTextBox_h

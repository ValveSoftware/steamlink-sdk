/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef SelectionModifier_h
#define SelectionModifier_h

#include "base/macros.h"
#include "core/editing/FrameSelection.h"
#include "platform/LayoutUnit.h"
#include "wtf/Allocator.h"

namespace blink {

class SelectionModifier {
    STACK_ALLOCATED();
public:
    using EAlteration = FrameSelection::EAlteration;
    using VerticalDirection = FrameSelection::VerticalDirection;

    // |frame| is used for providing settings.
    SelectionModifier(const LocalFrame& /* frame */, const VisibleSelection&, LayoutUnit);
    SelectionModifier(const LocalFrame&, const VisibleSelection&);

    LayoutUnit xPosForVerticalArrowNavigation() const { return m_xPosForVerticalArrowNavigation; }
    const VisibleSelection& selection() const { return m_selection; }

    bool modify(EAlteration, SelectionDirection, TextGranularity);
    bool modifyWithPageGranularity(EAlteration, unsigned verticalDistance, VerticalDirection);

    DECLARE_VIRTUAL_TRACE();

private:
    // TODO(yosin): We should move |EPositionType| to "SelectionModifier.cpp",
    // it is only used for implementing |modify()|.
    // TODO(yosin) We should use capitalized name for |EPositionType|.
    enum EPositionType { START, END, BASE, EXTENT }; // NOLINT

    LocalFrame* frame() const { return m_frame; }

    TextDirection directionOfEnclosingBlock() const;
    TextDirection directionOfSelection() const;
    VisiblePosition positionForPlatform(bool isGetStart) const;
    VisiblePosition startForPlatform() const;
    VisiblePosition endForPlatform() const;
    LayoutUnit lineDirectionPointForBlockDirectionNavigation(EPositionType);
    VisiblePosition modifyExtendingRight(TextGranularity);
    VisiblePosition modifyExtendingForward(TextGranularity);
    VisiblePosition modifyMovingRight(TextGranularity);
    VisiblePosition modifyMovingForward(TextGranularity);
    VisiblePosition modifyExtendingLeft(TextGranularity);
    VisiblePosition modifyExtendingBackward(TextGranularity);
    VisiblePosition modifyMovingLeft(TextGranularity);
    VisiblePosition modifyMovingBackward(TextGranularity);
    VisiblePosition nextWordPositionForPlatform(const VisiblePosition&);
    void willBeModified(EAlteration, SelectionDirection);

    Member<LocalFrame> m_frame;
    VisibleSelection m_selection;
    LayoutUnit m_xPosForVerticalArrowNavigation;

    DISALLOW_COPY_AND_ASSIGN(SelectionModifier);
};

LayoutUnit NoXPosForVerticalArrowNavigation();

} // namespace blink

#endif // SelectionModifier_h

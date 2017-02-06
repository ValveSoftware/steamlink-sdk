/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef FrameCaret_h
#define FrameCaret_h

#include "core/editing/CaretBase.h"
#include "platform/geometry/IntRect.h"

namespace blink {

class FrameCaret final : public CaretBase {
public:
    FrameCaret(LocalFrame*);
    ~FrameCaret() override;

    void setCaretPosition(const PositionWithAffinity&);
    void clear();
    bool isActive() const { return m_caretPosition.isNotNull(); }

    void updateAppearance();

    // Used to suspend caret blinking while the mouse is down.
    void setCaretBlinkingSuspended(bool suspended) { m_isCaretBlinkingSuspended = suspended; }
    bool isCaretBlinkingSuspended() const { return m_isCaretBlinkingSuspended; }
    void stopCaretBlinkTimer();
    void startBlinkCaret();

    void setCaretVisibility(CaretVisibility) override;
    bool isCaretBoundsDirty() const { return m_caretRectDirty; }
    void setCaretRectNeedsUpdate();
    void invalidateCaretRect();
    IntRect absoluteCaretBounds();

    bool shouldShowBlockCursor() const { return m_shouldShowBlockCursor; }
    void setShouldShowBlockCursor(bool);

    void paintCaret(GraphicsContext&, const LayoutPoint&);

    void dataWillChange(const CharacterData&);
    void nodeWillBeRemoved(Node&);

    void documentDetached();

    // For unittests
    bool shouldPaintCaretForTesting() const { return m_shouldPaintCaret; }
    bool isPreviousCaretDirtyForTesting() const { return m_previousCaretNode; }

    DECLARE_VIRTUAL_TRACE();

private:
    bool shouldBlinkCaret() const;
    void caretBlinkTimerFired(Timer<FrameCaret>*);
    bool caretPositionIsValidForDocument(const Document&) const;

    PositionWithAffinity m_caretPosition;
    const Member<LocalFrame> m_frame;
    // The last node which painted the caret. Retained for clearing the old
    // caret when it moves.
    Member<Node> m_previousCaretNode;
    LayoutRect m_previousCaretRect;
    CaretVisibility m_previousCaretVisibility;
    Timer<FrameCaret> m_caretBlinkTimer;
    bool m_caretRectDirty : 1;
    bool m_shouldPaintCaret : 1;
    bool m_isCaretBlinkingSuspended : 1;
    bool m_shouldShowBlockCursor : 1;
};

} // namespace blink

#endif // FrameCaret_h

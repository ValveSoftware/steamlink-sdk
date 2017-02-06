/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef InputMethodController_h
#define InputMethodController_h

#include "core/CoreExport.h"
#include "core/dom/Range.h"
#include "core/editing/CompositionUnderline.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/PlainTextRange.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class Editor;
class LocalFrame;
class Range;
class Text;

class CORE_EXPORT InputMethodController final : public GarbageCollected<InputMethodController> {
    WTF_MAKE_NONCOPYABLE(InputMethodController);
public:
    enum ConfirmCompositionBehavior {
        DoNotKeepSelection,
        KeepSelection,
    };

    static InputMethodController* create(LocalFrame&);
    DECLARE_TRACE();

    // international text input composition
    bool hasComposition() const;
    void setComposition(const String&, const Vector<CompositionUnderline>&, int selectionStart, int selectionEnd);
    void setCompositionFromExistingText(const Vector<CompositionUnderline>&, unsigned compositionStart, unsigned compositionEnd);
    // Inserts the text that is being composed as a regular text and returns true
    // if composition exists.
    bool confirmComposition();
    // Inserts the given text string in the place of the existing composition
    // and returns true.
    bool confirmComposition(const String& text, ConfirmCompositionBehavior confirmBehavior = KeepSelection);
    // Inserts the text that is being composed or specified non-empty text and
    // returns true.
    bool confirmCompositionOrInsertText(const String& text, ConfirmCompositionBehavior);
    // Deletes the existing composition text.
    void cancelComposition();
    void cancelCompositionIfSelectionIsInvalid();
    EphemeralRange compositionEphemeralRange() const;
    Range* compositionRange() const;

    void clear();
    void documentDetached();

    PlainTextRange getSelectionOffsets() const;
    // Returns true if setting selection to specified offsets, otherwise false.
    bool setEditableSelectionOffsets(const PlainTextRange&);
    void extendSelectionAndDelete(int before, int after);

private:
    class SelectionOffsetsScope {
        WTF_MAKE_NONCOPYABLE(SelectionOffsetsScope);
        STACK_ALLOCATED();
    public:
        explicit SelectionOffsetsScope(InputMethodController*);
        ~SelectionOffsetsScope();
    private:
        Member<InputMethodController> m_inputMethodController;
        const PlainTextRange m_offsets;
    };
    friend class SelectionOffsetsScope;

    Member<LocalFrame> m_frame;
    Member<Range> m_compositionRange;
    bool m_isDirty;
    bool m_hasComposition;

    explicit InputMethodController(LocalFrame&);

    Editor& editor() const;
    LocalFrame& frame() const
    {
        DCHECK(m_frame);
        return *m_frame;
    }

    String composingText() const;
    void selectComposition() const;
    bool setSelectionOffsets(const PlainTextRange&);
};

} // namespace blink

#endif // InputMethodController_h

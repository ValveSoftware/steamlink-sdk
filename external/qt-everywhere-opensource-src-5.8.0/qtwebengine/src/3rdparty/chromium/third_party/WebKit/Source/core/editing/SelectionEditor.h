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

#ifndef SelectionEditor_h
#define SelectionEditor_h

#include "core/editing/FrameSelection.h"
#include "core/events/EventDispatchResult.h"

namespace blink {

// TODO(yosin): We will rename |SelectionEditor| to appropriate name since
// it is no longer have a changing selection functionality, it was moved to
// |SelectionModifier| class.
class SelectionEditor final : public GarbageCollectedFinalized<SelectionEditor> {
    WTF_MAKE_NONCOPYABLE(SelectionEditor);
public:
    static SelectionEditor* create(FrameSelection& frameSelection)
    {
        return new SelectionEditor(frameSelection);
    }
    virtual ~SelectionEditor();
    void dispose();

    bool hasEditableStyle() const { return m_selection.hasEditableStyle(); }
    bool isContentEditable() const { return m_selection.isContentEditable(); }
    bool isContentRichlyEditable() const { return m_selection.isContentRichlyEditable(); }

    bool setSelectedRange(const EphemeralRange&, TextAffinity, SelectionDirectionalMode, FrameSelection::SetSelectionOptions);

    template <typename Strategy>
    const VisibleSelectionTemplate<Strategy>& visibleSelection() const;
    void setVisibleSelection(const VisibleSelection&, FrameSelection::SetSelectionOptions);
    void setVisibleSelection(const VisibleSelectionInFlatTree&, FrameSelection::SetSelectionOptions);

    void setWithoutValidation(const Position& base, const Position& extent);

    void documentAttached(Document*);
    void documentDetached(const Document&);

    // If this FrameSelection has a logical range which is still valid, this
    // function return its clone. Otherwise, the return value from underlying
    // |VisibleSelection|'s |firstRange()| is returned.
    Range* firstRange() const;

    // There functions are exposed for |FrameSelection|.
    void resetLogicalRange();
    void setLogicalRange(Range*);

    // Updates |m_selection| and |m_selectionInFlatTree| with up-to-date
    // layout if needed.
    void updateIfNeeded();

    DECLARE_VIRTUAL_TRACE();

private:
    explicit SelectionEditor(FrameSelection&);

    const Document& document() const;
    LocalFrame* frame() const;

    void clearVisibleSelection();
    bool shouldAlwaysUseDirectionalSelection() const;

    Member<Document> m_document;
    Member<FrameSelection> m_frameSelection;

    VisibleSelection m_selection;
    VisibleSelectionInFlatTree m_selectionInFlatTree;
    bool m_observingVisibleSelection;

    // The range specified by the user, which may not be visually canonicalized
    // (hence "logical"). This will be invalidated if the underlying
    // |VisibleSelection| changes. If that happens, this variable will
    // become |nullptr|, in which case logical positions == visible positions.
    Member<Range> m_logicalRange;
};

} // namespace blink

#endif // SelectionEditor_h

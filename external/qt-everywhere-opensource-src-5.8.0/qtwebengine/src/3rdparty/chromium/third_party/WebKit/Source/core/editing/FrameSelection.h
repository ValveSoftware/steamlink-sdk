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

#ifndef FrameSelection_h
#define FrameSelection_h

#include "core/CoreExport.h"
#include "core/dom/Range.h"
#include "core/editing/EditingStyle.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/iterators/TextIteratorFlags.h"
#include "core/layout/ScrollAlignment.h"
#include "platform/Timer.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include <memory>

namespace blink {

class CaretBase;
class CharacterData;
class CullRect;
class LayoutBlock;
class LocalFrame;
class FrameCaret;
class GranularityStrategy;
class GraphicsContext;
class HTMLFormElement;
class SelectionEditor;
class PendingSelection;
class Text;

enum class CursorAlignOnScroll { IfNeeded, Always };

enum EUserTriggered { NotUserTriggered = 0, UserTriggered = 1 };

enum RevealExtentOption {
    RevealExtent,
    DoNotRevealExtent
};

enum class SelectionDirectionalMode { NonDirectional, Directional };

enum class CaretVisibility;

class CORE_EXPORT FrameSelection final : public GarbageCollectedFinalized<FrameSelection> {
    WTF_MAKE_NONCOPYABLE(FrameSelection);
public:
    static FrameSelection* create(LocalFrame* frame)
    {
        return new FrameSelection(frame);
    }
    ~FrameSelection();

    enum EAlteration { AlterationMove, AlterationExtend };
    enum SetSelectionOption {
        // 1 << 0 is reserved for EUserTriggered
        CloseTyping = 1 << 1,
        ClearTypingStyle = 1 << 2,
        DoNotSetFocus = 1 << 3,
        DoNotUpdateAppearance = 1 << 4,
        DoNotClearStrategy = 1 << 5,
        DoNotAdjustInFlatTree = 1 << 6,
    };
    typedef unsigned SetSelectionOptions; // Union of values in SetSelectionOption and EUserTriggered
    static inline EUserTriggered selectionOptionsToUserTriggered(SetSelectionOptions options)
    {
        return static_cast<EUserTriggered>(options & UserTriggered);
    }

    bool isAvailable() const { return m_document; }
    // You should not call |document()| when |!isAvailable()|.
    const Document& document() const;
    Document& document();
    LocalFrame* frame() const { return m_frame; }
    Element* rootEditableElement() const { return selection().rootEditableElement(); }
    Element* rootEditableElementOrDocumentElement() const;
    ContainerNode* rootEditableElementOrTreeScopeRootNode() const;

    bool hasEditableStyle() const { return selection().hasEditableStyle(); }
    bool isContentEditable() const { return selection().isContentEditable(); }
    bool isContentRichlyEditable() const { return selection().isContentRichlyEditable(); }

    void moveTo(const VisiblePosition&, EUserTriggered = NotUserTriggered, CursorAlignOnScroll = CursorAlignOnScroll::IfNeeded);
    void moveTo(const VisiblePosition&, const VisiblePosition&, EUserTriggered = NotUserTriggered);
    void moveTo(const Position&, TextAffinity);

    template <typename Strategy>
    const VisibleSelectionTemplate<Strategy>& visibleSelection() const;

    const VisibleSelection& selection() const;
    void setSelection(const VisibleSelection&, SetSelectionOptions = CloseTyping | ClearTypingStyle, CursorAlignOnScroll = CursorAlignOnScroll::IfNeeded, TextGranularity = CharacterGranularity);
    void setSelection(const VisibleSelectionInFlatTree&, SetSelectionOptions = CloseTyping | ClearTypingStyle, CursorAlignOnScroll = CursorAlignOnScroll::IfNeeded, TextGranularity = CharacterGranularity);
    // TODO(yosin) We should get rid of two parameters version of
    // |setSelection()| to avoid conflict of four parameters version.
    void setSelection(const VisibleSelection& selection, TextGranularity granularity) { setSelection(selection, CloseTyping | ClearTypingStyle, CursorAlignOnScroll::IfNeeded, granularity); }
    // TODO(yosin) We should get rid of |Range| version of |setSelectedRagne()|
    // for Oilpan.
    bool setSelectedRange(Range*, TextAffinity, SelectionDirectionalMode = SelectionDirectionalMode::NonDirectional, SetSelectionOptions = CloseTyping | ClearTypingStyle);
    bool setSelectedRange(const EphemeralRange&, TextAffinity, SelectionDirectionalMode = SelectionDirectionalMode::NonDirectional, FrameSelection::SetSelectionOptions = CloseTyping | ClearTypingStyle);
    void selectAll();
    void clear();

    // Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
    void selectFrameElementInParentIfFullySelected();

    bool contains(const LayoutPoint&);

    SelectionType getSelectionType() const { return selection().getSelectionType(); }

    TextAffinity affinity() const { return selection().affinity(); }

    bool modify(EAlteration, SelectionDirection, TextGranularity, EUserTriggered = NotUserTriggered);
    enum VerticalDirection { DirectionUp, DirectionDown };
    bool modify(EAlteration, unsigned verticalDistance, VerticalDirection, EUserTriggered = NotUserTriggered, CursorAlignOnScroll = CursorAlignOnScroll::IfNeeded);

    // Moves the selection extent based on the selection granularity strategy.
    // This function does not allow the selection to collapse. If the new
    // extent is resolved to the same position as the current base, this
    // function will do nothing.
    void moveRangeSelectionExtent(const IntPoint&);
    void moveRangeSelection(const VisiblePosition& base, const VisiblePosition& extent, TextGranularity);

    TextGranularity granularity() const { return m_granularity; }

    Position base() const { return selection().base(); }
    Position extent() const { return selection().extent(); }
    Position start() const { return selection().start(); }
    Position end() const { return selection().end(); }

    // Return the layoutObject that is responsible for painting the caret (in the selection start node)
    LayoutBlock* caretLayoutObject() const;

    // Bounds of (possibly transformed) caret in absolute coords
    IntRect absoluteCaretBounds();

    void didChangeFocus();

    bool isNone() const { return selection().isNone(); }
    bool isCaret() const { return selection().isCaret(); }
    bool isRange() const { return selection().isRange(); }
    bool isInPasswordField() const;
    bool isDirectional() const { return selection().isDirectional(); }

    // If this FrameSelection has a logical range which is still valid, this function return its clone. Otherwise,
    // the return value from underlying VisibleSelection's firstRange() is returned.
    Range* firstRange() const;

    void documentAttached(Document*);
    void documentDetached(const Document&);
    void nodeWillBeRemoved(Node&);
    void dataWillChange(const CharacterData& node);
    void didUpdateCharacterData(CharacterData*, unsigned offset, unsigned oldLength, unsigned newLength);
    void didMergeTextNodes(const Text& oldNode, unsigned offset);
    void didSplitTextNode(const Text& oldNode);

    bool isAppearanceDirty() const;
    void commitAppearanceIfNeeded(LayoutView&);
    void updateAppearance();
    void setCaretVisible(bool caretIsVisible);
    bool isCaretBoundsDirty() const;
    void setCaretRectNeedsUpdate();
    void scheduleVisualUpdate() const;
    void invalidateCaretRect();
    void paintCaret(GraphicsContext&, const LayoutPoint&);

    // Used to suspend caret blinking while the mouse is down.
    void setCaretBlinkingSuspended(bool);
    bool isCaretBlinkingSuspended() const;

    // Focus
    void setFocused(bool);
    bool isFocused() const { return m_focused; }
    bool isFocusedAndActive() const;
    void pageActivationChanged();

    void updateSecureKeyboardEntryIfActive();

    // Returns true if a word is selected.
    bool selectWordAroundPosition(const VisiblePosition&);

#ifndef NDEBUG
    void formatForDebugger(char* buffer, unsigned length) const;
    void showTreeForThis() const;
#endif

    enum EndPointsAdjustmentMode { AdjustEndpointsAtBidiBoundary, DoNotAdjsutEndpoints };
    void setNonDirectionalSelectionIfNeeded(const VisibleSelectionInFlatTree&, TextGranularity, EndPointsAdjustmentMode = DoNotAdjsutEndpoints);
    void setFocusedNodeIfNeeded();
    void notifyLayoutObjectOfSelectionChange(EUserTriggered);

    EditingStyle* typingStyle() const;
    void setTypingStyle(EditingStyle*);
    void clearTypingStyle();

    String selectedHTMLForClipboard() const;
    String selectedText(TextIteratorBehavior = TextIteratorDefaultBehavior) const;
    String selectedTextForClipboard() const;

    // The bounds are clipped to the viewport as this is what callers expect.
    LayoutRect bounds() const;
    LayoutRect unclippedBounds() const;

    HTMLFormElement* currentForm() const;

    // TODO(tkent): This function has a bug that scrolling doesn't work well in
    // a case of RangeSelection. crbug.com/443061
    void revealSelection(const ScrollAlignment& = ScrollAlignment::alignCenterIfNeeded, RevealExtentOption = DoNotRevealExtent);
    void setSelectionFromNone();

    bool shouldShowBlockCursor() const;
    void setShouldShowBlockCursor(bool);

    // TODO(yosin): We should check DOM tree version and style version in
    // |FrameSelection::selection()| to make sure we use updated selection,
    // rather than having |updateIfNeeded()|. Once, we update all layout tests
    // to use updated selection, we should make |updateIfNeeded()| private.
    void updateIfNeeded();

    DECLARE_VIRTUAL_TRACE();

private:
    friend class FrameSelectionTest;

    explicit FrameSelection(LocalFrame*);

    // Note: We have |selectionInFlatTree()| for unit tests, we should
    // use |visibleSelection<EditingInFlatTreeStrategy>()|.
    const VisibleSelectionInFlatTree& selectionInFlatTree() const;

    template <typename Strategy>
    VisiblePositionTemplate<Strategy> originalBase() const;
    void setOriginalBase(const VisiblePosition&);
    void setOriginalBase(const VisiblePositionInFlatTree&);

    template <typename Strategy>
    void setNonDirectionalSelectionIfNeededAlgorithm(const VisibleSelectionTemplate<Strategy>&, TextGranularity, EndPointsAdjustmentMode);

    template <typename Strategy>
    void setSelectionAlgorithm(const VisibleSelectionTemplate<Strategy>&, SetSelectionOptions, CursorAlignOnScroll, TextGranularity);

    void respondToNodeModification(Node&, bool baseRemoved, bool extentRemoved, bool startRemoved, bool endRemoved);

    void notifyAccessibilityForSelectionChange();
    void notifyCompositorForSelectionChange();
    void notifyEventHandlerForSelectionChange();

    void focusedOrActiveStateChanged();

    void setUseSecureKeyboardEntry(bool);

    void updateSelectionIfNeeded(const Position& base, const Position& extent, const Position& start, const Position& end);

    template <typename Strategy>
    VisibleSelectionTemplate<Strategy> validateSelection(const VisibleSelectionTemplate<Strategy>&);

    GranularityStrategy* granularityStrategy();

    // For unittests
    bool shouldPaintCaretForTesting() const;
    bool isPreviousCaretDirtyForTesting() const;

    Member<Document> m_document;
    Member<LocalFrame> m_frame;
    const Member<PendingSelection> m_pendingSelection;
    const Member<SelectionEditor> m_selectionEditor;

    // Used to store base before the adjustment at bidi boundary
    VisiblePosition m_originalBase;
    VisiblePositionInFlatTree m_originalBaseInFlatTree;
    TextGranularity m_granularity;
    LayoutUnit m_xPosForVerticalArrowNavigation;

    Member<EditingStyle> m_typingStyle;

    bool m_focused : 1;

    // Controls text granularity used to adjust the selection's extent in moveRangeSelectionExtent.
    std::unique_ptr<GranularityStrategy> m_granularityStrategy;

    const Member<FrameCaret> m_frameCaret;
};

inline EditingStyle* FrameSelection::typingStyle() const
{
    return m_typingStyle.get();
}

inline void FrameSelection::clearTypingStyle()
{
    m_typingStyle.clear();
}

inline void FrameSelection::setTypingStyle(EditingStyle* style)
{
    m_typingStyle = style;
}
} // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const blink::FrameSelection&);
void showTree(const blink::FrameSelection*);
#endif

#endif // FrameSelection_h

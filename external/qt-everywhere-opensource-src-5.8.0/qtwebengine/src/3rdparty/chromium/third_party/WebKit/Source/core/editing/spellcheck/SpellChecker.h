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

#ifndef SpellChecker_h
#define SpellChecker_h

#include "core/CoreExport.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/markers/DocumentMarker.h"
#include "platform/heap/Handle.h"
#include "platform/text/TextChecking.h"

namespace blink {

class LocalFrame;
class SpellCheckerClient;
class SpellCheckRequest;
class SpellCheckRequester;
class TextCheckerClient;
class TextCheckingParagraph;
struct TextCheckingResult;

class CORE_EXPORT SpellChecker final : public GarbageCollected<SpellChecker> {
    WTF_MAKE_NONCOPYABLE(SpellChecker);
public:
    static SpellChecker* create(LocalFrame&);

    DECLARE_TRACE();

    SpellCheckerClient& spellCheckerClient() const;
    TextCheckerClient& textChecker() const;

    bool isContinuousSpellCheckingEnabled() const;
    void toggleContinuousSpellChecking();
    void ignoreSpelling();
    bool isSpellCheckingEnabledInFocusedNode() const;
    bool isSpellCheckingEnabledFor(Node*) const;
    static bool isSpellCheckingEnabledFor(const VisibleSelection&);
    void markMisspellingsAfterLineBreak(const VisibleSelection& wordSelection);
    void markMisspellingsAfterTypingToWord(const VisiblePosition &wordStart, const VisibleSelection& selectionAfterTyping);
    bool markMisspellings(const VisibleSelection&);
    void markBadGrammar(const VisibleSelection&);
    void markMisspellingsAndBadGrammar(const VisibleSelection& spellingSelection, bool markGrammar, const VisibleSelection& grammarSelection);
    void markAndReplaceFor(SpellCheckRequest*, const Vector<TextCheckingResult>&);
    void markAllMisspellingsAndBadGrammarInRanges(TextCheckingTypeMask, const EphemeralRange& spellingRange, const EphemeralRange& grammarRange);
    void advanceToNextMisspelling(bool startBeforeSelection = false);
    void showSpellingGuessPanel();
    void didBeginEditing(Element*);
    void clearMisspellingsAndBadGrammar(const VisibleSelection&);
    void markMisspellingsAndBadGrammar(const VisibleSelection&);
    void respondToChangedSelection(const VisibleSelection& oldSelection, FrameSelection::SetSelectionOptions);
    void replaceMisspelledRange(const String&);
    void removeSpellingMarkers();
    void removeSpellingMarkersUnderWords(const Vector<String>& words);
    void spellCheckAfterBlur();
    void spellCheckOldSelection(const VisibleSelection& oldSelection, const VisibleSelection& newAdjacentWords);

    void didEndEditingOnTextField(Element*);
    bool selectionStartHasMarkerFor(DocumentMarker::MarkerType, int from, int length) const;
    bool selectionStartHasSpellingMarkerFor(int from, int length) const;
    void updateMarkersForWordsAffectedByEditing(bool onlyHandleWordsContainingSelection);
    void cancelCheck();
    void chunkAndMarkAllMisspellingsAndBadGrammar(Node*, const EphemeralRange&);
    void requestTextChecking(const Element&);

    // Exposed for testing only
    SpellCheckRequester& spellCheckRequester() const { return *m_spellCheckRequester; }

    // The leak detector will report leaks should queued requests be posted
    // while it GCs repeatedly, as the requests keep their associated element
    // alive.
    //
    // Hence allow the leak detector to effectively stop the spell checker to
    // ensure leak reporting stability.
    void prepareForLeakDetection();

private:
    explicit SpellChecker(LocalFrame&);

    LocalFrame& frame() const
    {
        DCHECK(m_frame);
        return *m_frame;
    }

    bool markMisspellingsOrBadGrammar(const VisibleSelection&, bool checkSpelling);
    TextCheckingTypeMask resolveTextCheckingTypeMask(TextCheckingTypeMask);

    void removeMarkers(const VisibleSelection&, DocumentMarker::MarkerTypes);
    bool unifiedTextCheckerEnabled() const;

    void chunkAndMarkAllMisspellingsAndBadGrammar(TextCheckingTypeMask textCheckingOptions, const TextCheckingParagraph& fullParagraphToCheck);

    Member<LocalFrame> m_frame;
    const Member<SpellCheckRequester> m_spellCheckRequester;
};

} // namespace blink

#endif // SpellChecker_h

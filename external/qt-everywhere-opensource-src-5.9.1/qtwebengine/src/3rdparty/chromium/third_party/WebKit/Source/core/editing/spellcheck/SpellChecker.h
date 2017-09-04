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

class CompositeEditCommand;
class LocalFrame;
class ReplaceSelectionCommand;
class SpellCheckerClient;
class SpellCheckRequest;
class SpellCheckRequester;
class TextCheckerClient;
class TextCheckingParagraph;
struct TextCheckingResult;
class TypingCommand;

class CORE_EXPORT SpellChecker final : public GarbageCollected<SpellChecker> {
  WTF_MAKE_NONCOPYABLE(SpellChecker);

 public:
  static SpellChecker* create(LocalFrame&);

  DECLARE_TRACE();

  SpellCheckerClient& spellCheckerClient() const;
  TextCheckerClient& textChecker() const;

  bool isSpellCheckingEnabled() const;
  void toggleSpellCheckingEnabled();
  void ignoreSpelling();
  bool isSpellCheckingEnabledInFocusedNode() const;
  void markMisspellingsAfterApplyingCommand(const CompositeEditCommand&);
  void markAndReplaceFor(SpellCheckRequest*, const Vector<TextCheckingResult>&);
  void advanceToNextMisspelling(bool startBeforeSelection = false);
  void showSpellingGuessPanel();
  void didBeginEditing(Element*);
  void clearMisspellingsForMovingParagraphs(const VisibleSelection&);
  void markMisspellingsForMovingParagraphs(const VisibleSelection&);
  void respondToChangedSelection(const Position& oldSelectionStart,
                                 FrameSelection::SetSelectionOptions);
  void replaceMisspelledRange(const String&);
  void removeSpellingMarkers();
  void removeSpellingMarkersUnderWords(const Vector<String>& words);
  void spellCheckAfterBlur();

  void didEndEditingOnTextField(Element*);
  bool selectionStartHasMarkerFor(DocumentMarker::MarkerType,
                                  int from,
                                  int length) const;
  bool selectionStartHasSpellingMarkerFor(int from, int length) const;
  void updateMarkersForWordsAffectedByEditing(
      bool onlyHandleWordsContainingSelection);
  void cancelCheck();
  void requestTextChecking(const Element&);

  // Exposed for testing only
  SpellCheckRequester& spellCheckRequester() const {
    return *m_spellCheckRequester;
  }

  // The leak detector will report leaks should queued requests be posted
  // while it GCs repeatedly, as the requests keep their associated element
  // alive.
  //
  // Hence allow the leak detector to effectively stop the spell checker to
  // ensure leak reporting stability.
  void prepareForLeakDetection();

 private:
  explicit SpellChecker(LocalFrame&);

  LocalFrame& frame() const {
    DCHECK(m_frame);
    return *m_frame;
  }

  // Helper functions for advanceToNextMisspelling()
  Vector<TextCheckingResult> findMisspellings(const String&);
  std::pair<String, int> findFirstMisspelling(const Position&, const Position&);

  void markMisspellingsAfterLineBreak(const VisibleSelection& wordSelection);
  void markMisspellingsAfterTypingToWord(const VisiblePosition& wordStart);
  void markMisspellingsAfterTypingCommand(const TypingCommand&);
  void markMisspellingsAfterReplaceSelectionCommand(
      const ReplaceSelectionCommand&);

  void removeMarkers(const VisibleSelection&, DocumentMarker::MarkerTypes);

  void markMisspellingsInternal(const VisibleSelection&);
  void chunkAndMarkAllMisspellings(
      const TextCheckingParagraph& fullParagraphToCheck);
  void spellCheckOldSelection(const Position& oldSelectionStart,
                              const VisibleSelection& newAdjacentWords);

  Member<LocalFrame> m_frame;
  const Member<SpellCheckRequester> m_spellCheckRequester;
};

}  // namespace blink

#endif  // SpellChecker_h

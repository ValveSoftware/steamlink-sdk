/*
 * Copyright (C) 2006, 2007 Apple, Inc.  All rights reserved.
 * Copyright (C) 2012 Google, Inc.  All rights reserved.
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

#include "web/SpellCheckerClientImpl.h"

#include "core/dom/Element.h"
#include "core/editing/Editor.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "public/web/WebSpellCheckClient.h"
#include "public/web/WebTextCheckingResult.h"
#include "web/WebTextCheckingCompletionImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

SpellCheckerClientImpl::SpellCheckerClientImpl(WebViewImpl* webview)
    : m_webView(webview), m_spellCheckThisFieldStatus(SpellCheckAutomatic) {}

SpellCheckerClientImpl::~SpellCheckerClientImpl() {}

bool SpellCheckerClientImpl::shouldSpellcheckByDefault() {
  // Spellcheck should be enabled for all editable areas (such as textareas,
  // contentEditable regions, designMode docs and inputs).
  if (!m_webView->focusedCoreFrame()->isLocalFrame())
    return false;
  const LocalFrame* frame = toLocalFrame(m_webView->focusedCoreFrame());
  if (!frame)
    return false;
  if (frame->spellChecker().isSpellCheckingEnabledInFocusedNode())
    return true;
  const Document* document = frame->document();
  if (!document)
    return false;
  const Element* element = document->focusedElement();
  // If |element| is null, we default to allowing spellchecking. This is done
  // in order to mitigate the issue when the user clicks outside the textbox,
  // as a result of which |element| becomes null, resulting in all the spell
  // check markers being deleted. Also, the LocalFrame will decide not to do
  // spellchecking if the user can't edit - so returning true here will not
  // cause any problems to the LocalFrame's behavior.
  if (!element)
    return true;
  const LayoutObject* layoutObject = element->layoutObject();
  if (!layoutObject)
    return false;

  return true;
}

bool SpellCheckerClientImpl::isSpellCheckingEnabled() {
  if (m_spellCheckThisFieldStatus == SpellCheckForcedOff)
    return false;
  if (m_spellCheckThisFieldStatus == SpellCheckForcedOn)
    return true;
  return shouldSpellcheckByDefault();
}

void SpellCheckerClientImpl::toggleSpellCheckingEnabled() {
  if (isSpellCheckingEnabled()) {
    m_spellCheckThisFieldStatus = SpellCheckForcedOff;
    if (Page* page = m_webView->page()) {
      for (Frame* frame = page->mainFrame(); frame;
           frame = frame->tree().traverseNext()) {
        if (!frame->isLocalFrame())
          continue;
        toLocalFrame(frame)->document()->markers().removeMarkers(
            DocumentMarker::MisspellingMarkers());
      }
    }
  } else {
    m_spellCheckThisFieldStatus = SpellCheckForcedOn;
    if (m_webView->focusedCoreFrame()->isLocalFrame()) {
      if (LocalFrame* frame = toLocalFrame(m_webView->focusedCoreFrame())) {
        VisibleSelection frameSelection = frame->selection().selection();
        // If a selection is in an editable element spell check its content.
        if (Element* rootEditableElement =
                frameSelection.rootEditableElement()) {
          frame->spellChecker().didBeginEditing(rootEditableElement);
        }
      }
    }
  }
}

void SpellCheckerClientImpl::checkSpellingOfString(const String& text,
                                                   int* misspellingLocation,
                                                   int* misspellingLength) {
  // SpellCheckWord will write (0, 0) into the output vars, which is what our
  // caller expects if the word is spelled correctly.
  int spellLocation = -1;
  int spellLength = 0;

  // Check to see if the provided text is spelled correctly.
  if (m_webView->spellCheckClient()) {
    m_webView->spellCheckClient()->spellCheck(text, spellLocation, spellLength,
                                              nullptr);
  } else {
    spellLocation = 0;
    spellLength = 0;
  }

  // Note: the Mac code checks if the pointers are null before writing to them,
  // so we do too.
  if (misspellingLocation)
    *misspellingLocation = spellLocation;
  if (misspellingLength)
    *misspellingLength = spellLength;
}

void SpellCheckerClientImpl::requestCheckingOfString(
    TextCheckingRequest* request) {
  if (!m_webView->spellCheckClient())
    return;
  const String& text = request->data().text();
  const Vector<uint32_t>& markers = request->data().markers();
  const Vector<unsigned>& markerOffsets = request->data().offsets();
  m_webView->spellCheckClient()->requestCheckingOfText(
      text, markers, markerOffsets, new WebTextCheckingCompletionImpl(request));
}

void SpellCheckerClientImpl::cancelAllPendingRequests() {
  if (!m_webView->spellCheckClient())
    return;
  m_webView->spellCheckClient()->cancelAllPendingRequests();
}

void SpellCheckerClientImpl::updateSpellingUIWithMisspelledWord(
    const String& misspelledWord) {
  if (m_webView->spellCheckClient())
    m_webView->spellCheckClient()->updateSpellingUIWithMisspelledWord(
        WebString(misspelledWord));
}

void SpellCheckerClientImpl::showSpellingUI(bool show) {
  if (m_webView->spellCheckClient())
    m_webView->spellCheckClient()->showSpellingUI(show);
}

bool SpellCheckerClientImpl::spellingUIIsShowing() {
  if (m_webView->spellCheckClient())
    return m_webView->spellCheckClient()->isShowingSpellingUI();
  return false;
}

}  // namespace blink

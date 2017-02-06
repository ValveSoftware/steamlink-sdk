// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/spellchecker/spellcheck_provider.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/spellcheck_marker.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/common/spellcheck_result.h"
#include "chrome/renderer/spellchecker/spellcheck.h"
#include "chrome/renderer/spellchecker/spellcheck_language.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebTextCheckingCompletion.h"
#include "third_party/WebKit/public/web/WebTextCheckingResult.h"
#include "third_party/WebKit/public/web/WebTextDecorationType.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebElement;
using blink::WebFrame;
using blink::WebString;
using blink::WebTextCheckingCompletion;
using blink::WebTextCheckingResult;
using blink::WebTextDecorationType;
using blink::WebVector;

static_assert(int(blink::WebTextDecorationTypeSpelling) ==
              int(SpellCheckResult::SPELLING), "mismatching enums");
static_assert(int(blink::WebTextDecorationTypeGrammar) ==
              int(SpellCheckResult::GRAMMAR), "mismatching enums");
static_assert(int(blink::WebTextDecorationTypeInvisibleSpellcheck) ==
              int(SpellCheckResult::INVISIBLE), "mismatching enums");

SpellCheckProvider::SpellCheckProvider(
    content::RenderView* render_view,
    SpellCheck* spellcheck)
    : content::RenderViewObserver(render_view),
      content::RenderViewObserverTracker<SpellCheckProvider>(render_view),
      spelling_panel_visible_(false),
      spellcheck_(spellcheck) {
  DCHECK(spellcheck_);
  if (render_view) {  // NULL in unit tests.
    render_view->GetWebView()->setSpellCheckClient(this);
    EnableSpellcheck(spellcheck_->IsSpellcheckEnabled());
  }
}

SpellCheckProvider::~SpellCheckProvider() {
}

void SpellCheckProvider::RequestTextChecking(
    const base::string16& text,
    WebTextCheckingCompletion* completion,
    const std::vector<SpellCheckMarker>& markers) {
  // Ignore invalid requests.
  if (text.empty() || !HasWordCharacters(text, 0)) {
    completion->didCancelCheckingText();
    return;
  }

  // Try to satisfy check from cache.
  if (SatisfyRequestFromCache(text, completion))
    return;

  // Send this text to a browser. A browser checks the user profile and send
  // this text to the Spelling service only if a user enables this feature.
  last_request_.clear();
  last_results_.assign(blink::WebVector<blink::WebTextCheckingResult>());

#if defined(USE_BROWSER_SPELLCHECKER)
  // Text check (unified request for grammar and spell check) is only
  // available for browser process, so we ask the system spellchecker
  // over IPC or return an empty result if the checker is not
  // available.
  Send(new SpellCheckHostMsg_RequestTextCheck(
      routing_id(),
      text_check_completions_.Add(completion),
      text,
      markers));
#else
  Send(new SpellCheckHostMsg_CallSpellingService(
      routing_id(),
      text_check_completions_.Add(completion),
      base::string16(text),
      markers));
#endif  // !USE_BROWSER_SPELLCHECKER
}

bool SpellCheckProvider::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SpellCheckProvider, message)
#if !defined(USE_BROWSER_SPELLCHECKER)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_RespondSpellingService,
                        OnRespondSpellingService)
#endif
#if defined(USE_BROWSER_SPELLCHECKER)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_AdvanceToNextMisspelling,
                        OnAdvanceToNextMisspelling)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_RespondTextCheck, OnRespondTextCheck)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_ToggleSpellPanel, OnToggleSpellPanel)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SpellCheckProvider::FocusedNodeChanged(const blink::WebNode& unused) {
#if defined(USE_BROWSER_SPELLCHECKER)
  WebFrame* frame = render_view()->GetWebView()->focusedFrame();
  WebElement element = frame->document().isNull() ? WebElement() :
      frame->document().focusedElement();
  bool enabled = !element.isNull() && element.isEditable();
  bool checked = enabled && frame->isContinuousSpellCheckingEnabled();

  Send(new SpellCheckHostMsg_ToggleSpellCheck(routing_id(), enabled, checked));
#endif  // USE_BROWSER_SPELLCHECKER
}

void SpellCheckProvider::spellCheck(
    const WebString& text,
    int& offset,
    int& length,
    WebVector<WebString>* optional_suggestions) {
  base::string16 word(text);
  std::vector<base::string16> suggestions;
  const int kWordStart = 0;
  spellcheck_->SpellCheckWord(
      word.c_str(), kWordStart, word.size(), routing_id(),
      &offset, &length, optional_suggestions ? & suggestions : NULL);
  if (optional_suggestions) {
    *optional_suggestions = suggestions;
    UMA_HISTOGRAM_COUNTS("SpellCheck.api.check.suggestions", word.size());
  } else {
    UMA_HISTOGRAM_COUNTS("SpellCheck.api.check", word.size());
    // If optional_suggestions is not requested, the API is called
    // for marking.  So we use this for counting markable words.
    Send(new SpellCheckHostMsg_NotifyChecked(routing_id(), word, 0 < length));
  }
}

void SpellCheckProvider::checkTextOfParagraph(
    const blink::WebString& text,
    blink::WebTextCheckingTypeMask mask,
    blink::WebVector<blink::WebTextCheckingResult>* results) {
  if (!results)
    return;

  if (!(mask & blink::WebTextCheckingTypeSpelling))
    return;

  // TODO(groby): As far as I can tell, this method is never invoked.
  // UMA results seem to support that. Investigate, clean up if true.
  NOTREACHED();
  spellcheck_->SpellCheckParagraph(text, results);
  UMA_HISTOGRAM_COUNTS("SpellCheck.api.paragraph", text.length());
}

void SpellCheckProvider::requestCheckingOfText(
    const WebString& text,
    const WebVector<uint32_t>& markers,
    const WebVector<unsigned>& marker_offsets,
    WebTextCheckingCompletion* completion) {
  std::vector<SpellCheckMarker> spellcheck_markers;
  for (size_t i = 0; i < markers.size(); ++i) {
    spellcheck_markers.push_back(
        SpellCheckMarker(markers[i], marker_offsets[i]));
  }
  RequestTextChecking(text, completion, spellcheck_markers);
  UMA_HISTOGRAM_COUNTS("SpellCheck.api.async", text.length());
}

void SpellCheckProvider::showSpellingUI(bool show) {
#if defined(USE_BROWSER_SPELLCHECKER)
  UMA_HISTOGRAM_BOOLEAN("SpellCheck.api.showUI", show);
  Send(new SpellCheckHostMsg_ShowSpellingPanel(routing_id(), show));
#endif
}

bool SpellCheckProvider::isShowingSpellingUI() {
  return spelling_panel_visible_;
}

void SpellCheckProvider::updateSpellingUIWithMisspelledWord(
    const WebString& word) {
#if defined(USE_BROWSER_SPELLCHECKER)
  Send(new SpellCheckHostMsg_UpdateSpellingPanelWithMisspelledWord(routing_id(),
                                                                   word));
#endif
}

#if !defined(USE_BROWSER_SPELLCHECKER)
void SpellCheckProvider::OnRespondSpellingService(
    int identifier,
    bool succeeded,
    const base::string16& line,
    const std::vector<SpellCheckResult>& results) {
  WebTextCheckingCompletion* completion =
      text_check_completions_.Lookup(identifier);
  if (!completion)
    return;
  text_check_completions_.Remove(identifier);

  // If |succeeded| is false, we use local spellcheck as a fallback.
  if (!succeeded) {
    spellcheck_->RequestTextChecking(line, completion);
    return;
  }

  // Double-check the returned spellchecking results with our spellchecker to
  // visualize the differences between ours and the on-line spellchecker.
  blink::WebVector<blink::WebTextCheckingResult> textcheck_results;
  spellcheck_->CreateTextCheckingResults(SpellCheck::USE_NATIVE_CHECKER,
                                         0,
                                         line,
                                         results,
                                         &textcheck_results);
  completion->didFinishCheckingText(textcheck_results);

  // Cache the request and the converted results.
  last_request_ = line;
  last_results_.swap(textcheck_results);
}
#endif

bool SpellCheckProvider::HasWordCharacters(
    const base::string16& text,
    int index) const {
  const base::char16* data = text.data();
  int length = text.length();
  while (index < length) {
    uint32_t code = 0;
    U16_NEXT(data, index, length, code);
    UErrorCode error = U_ZERO_ERROR;
    if (uscript_getScript(code, &error) != USCRIPT_COMMON)
      return true;
  }
  return false;
}

#if defined(USE_BROWSER_SPELLCHECKER)
void SpellCheckProvider::OnAdvanceToNextMisspelling() {
  if (!render_view()->GetWebView())
    return;
  render_view()->GetWebView()->focusedFrame()->executeCommand(
      WebString::fromUTF8("AdvanceToNextMisspelling"));
}

void SpellCheckProvider::OnRespondTextCheck(
    int identifier,
    const base::string16& line,
    const std::vector<SpellCheckResult>& results) {
  // TODO(groby): Unify with SpellCheckProvider::OnRespondSpellingService
  DCHECK(spellcheck_);
  WebTextCheckingCompletion* completion =
      text_check_completions_.Lookup(identifier);
  if (!completion)
    return;
  text_check_completions_.Remove(identifier);
  blink::WebVector<blink::WebTextCheckingResult> textcheck_results;
  spellcheck_->CreateTextCheckingResults(SpellCheck::DO_NOT_MODIFY,
                                         0,
                                         line,
                                         results,
                                         &textcheck_results);
  completion->didFinishCheckingText(textcheck_results);

  // TODO(groby): Add request caching once OSX reports back original request.
  // (cf. SpellCheckProvider::OnRespondSpellingService)
  // Cache the request and the converted results.
}

void SpellCheckProvider::OnToggleSpellPanel(bool is_currently_visible) {
  if (!render_view()->GetWebView())
    return;
  // We need to tell the webView whether the spelling panel is visible or not so
  // that it won't need to make ipc calls later.
  spelling_panel_visible_ = is_currently_visible;
  render_view()->GetWebView()->focusedFrame()->executeCommand(
      WebString::fromUTF8("ToggleSpellPanel"));
}
#endif

void SpellCheckProvider::EnableSpellcheck(bool enable) {
  if (!render_view()->GetWebView())
    return;

  WebFrame* frame = render_view()->GetWebView()->focusedFrame();
  frame->enableContinuousSpellChecking(enable);
  if (!enable)
    frame->removeSpellingMarkers();
}

bool SpellCheckProvider::SatisfyRequestFromCache(
    const base::string16& text,
    WebTextCheckingCompletion* completion) {
  size_t last_length = last_request_.length();

  // Send back the |last_results_| if the |last_request_| is a substring of
  // |text| and |text| does not have more words to check. Provider cannot cancel
  // the spellcheck request here, because WebKit might have discarded the
  // previous spellcheck results and erased the spelling markers in response to
  // the user editing the text.
  base::string16 request(text);
  size_t text_length = request.length();
  if (text_length >= last_length &&
      !request.compare(0, last_length, last_request_)) {
    if (text_length == last_length || !HasWordCharacters(text, last_length)) {
      completion->didFinishCheckingText(last_results_);
      return true;
    }
    int code = 0;
    int length = static_cast<int>(text_length);
    U16_PREV(text.data(), 0, length, code);
    UErrorCode error = U_ZERO_ERROR;
    if (uscript_getScript(code, &error) != USCRIPT_COMMON) {
      completion->didCancelCheckingText();
      return true;
    }
  }
  // Create a subset of the cached results and return it if the given text is a
  // substring of the cached text.
  if (text_length < last_length &&
      !last_request_.compare(0, text_length, request)) {
    size_t result_size = 0;
    for (size_t i = 0; i < last_results_.size(); ++i) {
      size_t start = last_results_[i].location;
      size_t end = start + last_results_[i].length;
      if (start <= text_length && end <= text_length)
        ++result_size;
    }
    if (result_size > 0) {
      blink::WebVector<blink::WebTextCheckingResult> results(result_size);
      for (size_t i = 0; i < result_size; ++i) {
        results[i].decoration = last_results_[i].decoration;
        results[i].location = last_results_[i].location;
        results[i].length = last_results_[i].length;
        results[i].replacement = last_results_[i].replacement;
      }
      completion->didFinishCheckingText(results);
      return true;
    }
  }

  return false;
}

void SpellCheckProvider::OnDestruct() {
  delete this;
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/spellchecker/spellcheck.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#ifndef TOOLKIT_QT
#include "chrome/common/channel_info.h"
#endif
#include "chrome/common/chrome_switches.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/common/spellcheck_result.h"
#include "chrome/renderer/spellchecker/spellcheck_language.h"
#include "chrome/renderer/spellchecker/spellcheck_provider.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebTextCheckingCompletion.h"
#include "third_party/WebKit/public/web/WebTextCheckingResult.h"
#include "third_party/WebKit/public/web/WebTextDecorationType.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebVector;
using blink::WebString;
using blink::WebTextCheckingResult;
using blink::WebTextDecorationType;

namespace {
const int kNoOffset = 0;
const int kNoTag = 0;

class UpdateSpellcheckEnabled : public content::RenderViewVisitor {
 public:
  explicit UpdateSpellcheckEnabled(bool enabled) : enabled_(enabled) {}
  bool Visit(content::RenderView* render_view) override;

 private:
  bool enabled_;  // New spellcheck-enabled state.
  DISALLOW_COPY_AND_ASSIGN(UpdateSpellcheckEnabled);
};

bool UpdateSpellcheckEnabled::Visit(content::RenderView* render_view) {
  SpellCheckProvider* provider = SpellCheckProvider::Get(render_view);
  DCHECK(provider);
  provider->EnableSpellcheck(enabled_);
  return true;
}

class DocumentMarkersCollector : public content::RenderViewVisitor {
 public:
  DocumentMarkersCollector() {}
  ~DocumentMarkersCollector() override {}
  const std::vector<uint32_t>& markers() const { return markers_; }
  bool Visit(content::RenderView* render_view) override;

 private:
  std::vector<uint32_t> markers_;
  DISALLOW_COPY_AND_ASSIGN(DocumentMarkersCollector);
};

bool DocumentMarkersCollector::Visit(content::RenderView* render_view) {
  if (!render_view || !render_view->GetWebView())
    return true;
  WebVector<uint32_t> markers;
  render_view->GetWebView()->spellingMarkers(&markers);
  for (size_t i = 0; i < markers.size(); ++i)
    markers_.push_back(markers[i]);
  // Visit all render views.
  return true;
}

class DocumentMarkersRemover : public content::RenderViewVisitor {
 public:
  explicit DocumentMarkersRemover(const std::set<std::string>& words);
  ~DocumentMarkersRemover() override {}
  bool Visit(content::RenderView* render_view) override;

 private:
  WebVector<WebString> words_;
  DISALLOW_COPY_AND_ASSIGN(DocumentMarkersRemover);
};

DocumentMarkersRemover::DocumentMarkersRemover(
    const std::set<std::string>& words)
    : words_(words.size()) {
  std::transform(words.begin(), words.end(), words_.begin(),
                 [](const std::string& w) { return WebString::fromUTF8(w); });
}

bool DocumentMarkersRemover::Visit(content::RenderView* render_view) {
  if (render_view && render_view->GetWebView())
    render_view->GetWebView()->removeSpellingMarkersUnderWords(words_);
  return true;
}

bool IsApostrophe(base::char16 c) {
  const base::char16 kApostrophe = 0x27;
  const base::char16 kRightSingleQuotationMark = 0x2019;
  return c == kApostrophe || c == kRightSingleQuotationMark;
}

// Makes sure that the apostrophes in the |spelling_suggestion| are the same
// type as in the |misspelled_word| and in the same order. Ignore differences in
// the number of apostrophes.
void PreserveOriginalApostropheTypes(const base::string16& misspelled_word,
                                     base::string16* spelling_suggestion) {
  auto it = spelling_suggestion->begin();
  for (const base::char16& c : misspelled_word) {
    if (IsApostrophe(c)) {
      it = std::find_if(it, spelling_suggestion->end(), IsApostrophe);
      if (it == spelling_suggestion->end())
        return;

      *it++ = c;
    }
  }
}

}  // namespace

class SpellCheck::SpellcheckRequest {
 public:
  SpellcheckRequest(const base::string16& text,
                    blink::WebTextCheckingCompletion* completion)
      : text_(text), completion_(completion) {
    DCHECK(completion);
  }
  ~SpellcheckRequest() {}

  base::string16 text() { return text_; }
  blink::WebTextCheckingCompletion* completion() { return completion_; }

 private:
  base::string16 text_;  // Text to be checked in this task.

  // The interface to send the misspelled ranges to WebKit.
  blink::WebTextCheckingCompletion* completion_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckRequest);
};


// Initializes SpellCheck object.
// spellcheck_enabled_ currently MUST be set to true, due to peculiarities of
// the initialization sequence.
// Since it defaults to true, newly created SpellCheckProviders will enable
// spellchecking. After the first word is typed, the provider requests a check,
// which in turn triggers the delayed initialization sequence in SpellCheck.
// This does send a message to the browser side, which triggers the creation
// of the SpellcheckService. That does create the observer for the preference
// responsible for enabling/disabling checking, which allows subsequent changes
// to that preference to be sent to all SpellCheckProviders.
// Setting |spellcheck_enabled_| to false by default prevents that mechanism,
// and as such the SpellCheckProviders will never be notified of different
// values.
// TODO(groby): Simplify this.
SpellCheck::SpellCheck() : spellcheck_enabled_(true) {}

SpellCheck::~SpellCheck() {
}

void SpellCheck::FillSuggestions(
    const std::vector<std::vector<base::string16>>& suggestions_list,
    std::vector<base::string16>* optional_suggestions) {
  DCHECK(optional_suggestions);
  size_t num_languages = suggestions_list.size();

  // Compute maximum number of suggestions in a single language.
  size_t max_suggestions = 0;
  for (const auto& suggestions : suggestions_list)
    max_suggestions = std::max(max_suggestions, suggestions.size());

  for (size_t count = 0; count < (max_suggestions * num_languages); ++count) {
    size_t language = count % num_languages;
    size_t index = count / num_languages;

    if (suggestions_list[language].size() <= index)
      continue;

    const base::string16& suggestion = suggestions_list[language][index];
    // Only add the suggestion if it's unique.
    if (!ContainsValue(*optional_suggestions, suggestion)) {
        optional_suggestions->push_back(suggestion);
    }
    if (optional_suggestions->size() >=
        chrome::spellcheck_common::kMaxSuggestions) {
      break;
    }
  }
}

bool SpellCheck::OnControlMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SpellCheck, message)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_Init, OnInit)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_CustomDictionaryChanged,
                        OnCustomDictionaryChanged)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_EnableSpellCheck, OnEnableSpellCheck)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_RequestDocumentMarkers,
                        OnRequestDocumentMarkers)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void SpellCheck::OnInit(
    const std::vector<SpellCheckBDictLanguage>& bdict_languages,
    const std::set<std::string>& custom_words) {
  languages_.clear();
  for (const auto& bdict_language : bdict_languages) {
    AddSpellcheckLanguage(
        IPC::PlatformFileForTransitToFile(bdict_language.file),
        bdict_language.language);
  }

  custom_dictionary_.Init(custom_words);
#if !defined(USE_BROWSER_SPELLCHECKER)
  PostDelayedSpellCheckTask(pending_request_param_.release());
#endif
}

void SpellCheck::OnCustomDictionaryChanged(
    const std::set<std::string>& words_added,
    const std::set<std::string>& words_removed) {
  custom_dictionary_.OnCustomDictionaryChanged(words_added, words_removed);
  if (words_added.empty())
    return;
  DocumentMarkersRemover markersRemover(words_added);
  content::RenderView::ForEach(&markersRemover);
}

void SpellCheck::OnEnableSpellCheck(bool enable) {
  spellcheck_enabled_ = enable;
  UpdateSpellcheckEnabled updater(enable);
  content::RenderView::ForEach(&updater);
}

void SpellCheck::OnRequestDocumentMarkers() {
  DocumentMarkersCollector collector;
  content::RenderView::ForEach(&collector);
  content::RenderThread::Get()->Send(
      new SpellCheckHostMsg_RespondDocumentMarkers(collector.markers()));
}

// TODO(groby): Make sure we always have a spelling engine, even before
// AddSpellcheckLanguage() is called.
void SpellCheck::AddSpellcheckLanguage(base::File file,
                                       const std::string& language) {
  languages_.push_back(new SpellcheckLanguage());
  languages_.back()->Init(std::move(file), language);
}

bool SpellCheck::SpellCheckWord(
    const base::char16* text_begin,
    int position_in_text,
    int text_length,
    int tag,
    int* misspelling_start,
    int* misspelling_len,
    std::vector<base::string16>* optional_suggestions) {
  DCHECK(text_length >= position_in_text);
  DCHECK(misspelling_start && misspelling_len) << "Out vars must be given.";

  // Do nothing if we need to delay initialization. (Rather than blocking,
  // report the word as correctly spelled.)
  if (InitializeIfNeeded())
    return true;

  // These are for holding misspelling or skippable word positions and lengths
  // between calls to SpellcheckLanguage::SpellCheckWord.
  int possible_misspelling_start;
  int possible_misspelling_len;
  // The longest sequence of text that all languages agree is skippable.
  int agreed_skippable_len;
  // A vector of vectors containing spelling suggestions from different
  // languages.
  std::vector<std::vector<base::string16>> suggestions_list;
  // A vector to hold a language's misspelling suggestions between spellcheck
  // calls.
  std::vector<base::string16> language_suggestions;

  // This loop only advances if all languages agree that a sequence of text is
  // skippable.
  for (; position_in_text <= text_length;
       position_in_text += agreed_skippable_len) {
    // Reseting |agreed_skippable_len| to the worst-case length each time
    // prevents some unnecessary iterations.
    agreed_skippable_len = text_length;
    *misspelling_start = 0;
    *misspelling_len = 0;
    suggestions_list.clear();

    for (ScopedVector<SpellcheckLanguage>::iterator language =
             languages_.begin();
         language != languages_.end();) {
      language_suggestions.clear();
      SpellcheckLanguage::SpellcheckWordResult result =
          (*language)->SpellCheckWord(
              text_begin, position_in_text, text_length, tag,
              &possible_misspelling_start, &possible_misspelling_len,
              optional_suggestions ? &language_suggestions : nullptr);

      switch (result) {
        case SpellcheckLanguage::SpellcheckWordResult::IS_CORRECT:
          *misspelling_start = 0;
          *misspelling_len = 0;
          return true;
        case SpellcheckLanguage::SpellcheckWordResult::IS_SKIPPABLE:
          agreed_skippable_len =
              std::min(agreed_skippable_len, possible_misspelling_len);
          // If true, this means the spellchecker moved past a word that was
          // previously determined to be misspelled or skippable, which means
          // another spellcheck language marked it as correct.
          if (position_in_text != possible_misspelling_start) {
            *misspelling_len = 0;
            position_in_text = possible_misspelling_start;
            suggestions_list.clear();
            language = languages_.begin();
          } else {
            language++;
          }
          break;
        case SpellcheckLanguage::SpellcheckWordResult::IS_MISSPELLED:
          *misspelling_start = possible_misspelling_start;
          *misspelling_len = possible_misspelling_len;
          // If true, this means the spellchecker moved past a word that was
          // previously determined to be misspelled or skippable, which means
          // another spellcheck language marked it as correct.
          if (position_in_text != *misspelling_start) {
            suggestions_list.clear();
            language = languages_.begin();
            position_in_text = *misspelling_start;
          } else {
            suggestions_list.push_back(language_suggestions);
            language++;
          }
          break;
      }
    }

    // If |*misspelling_len| is non-zero, that means at least one language
    // marked a word misspelled and no other language considered it correct.
    if (*misspelling_len != 0) {
      if (optional_suggestions)
        FillSuggestions(suggestions_list, optional_suggestions);
      return false;
    }
  }

  NOTREACHED();
  return true;
}

bool SpellCheck::SpellCheckParagraph(
    const base::string16& text,
    WebVector<WebTextCheckingResult>* results) {
#if !defined(USE_BROWSER_SPELLCHECKER)
  // Mac and Android have their own spell checkers,so this method won't be used
  DCHECK(results);
  std::vector<WebTextCheckingResult> textcheck_results;
  size_t length = text.length();
  size_t position_in_text = 0;

  // Spellcheck::SpellCheckWord() automatically breaks text into words and
  // checks the spellings of the extracted words. This function sets the
  // position and length of the first misspelled word and returns false when
  // the text includes misspelled words. Therefore, we just repeat calling the
  // function until it returns true to check the whole text.
  int misspelling_start = 0;
  int misspelling_length = 0;
  while (position_in_text <= length) {
    if (SpellCheckWord(text.c_str(),
                       position_in_text,
                       length,
                       kNoTag,
                       &misspelling_start,
                       &misspelling_length,
                       NULL)) {
      results->assign(textcheck_results);
      return true;
    }

    if (!custom_dictionary_.SpellCheckWord(
            text, misspelling_start, misspelling_length)) {
      base::string16 replacement;
      textcheck_results.push_back(WebTextCheckingResult(
          blink::WebTextDecorationTypeSpelling,
          misspelling_start,
          misspelling_length,
          replacement));
    }
    position_in_text = misspelling_start + misspelling_length;
  }
  results->assign(textcheck_results);
  return false;
#else
  // This function is only invoked for spell checker functionality that runs
  // on the render thread. OSX and Android builds don't have that.
  NOTREACHED();
  return true;
#endif
}

// OSX and Android use their own spell checkers
#if !defined(USE_BROWSER_SPELLCHECKER)
void SpellCheck::RequestTextChecking(
    const base::string16& text,
    blink::WebTextCheckingCompletion* completion) {
  // Clean up the previous request before starting a new request.
  if (pending_request_param_.get())
    pending_request_param_->completion()->didCancelCheckingText();

  pending_request_param_.reset(new SpellcheckRequest(
      text, completion));
  // We will check this text after we finish loading the hunspell dictionary.
  if (InitializeIfNeeded())
    return;

  PostDelayedSpellCheckTask(pending_request_param_.release());
}
#endif

bool SpellCheck::InitializeIfNeeded() {
  if (languages_.empty())
    return true;

  bool initialize_if_needed = false;
  for (SpellcheckLanguage* language : languages_)
    initialize_if_needed |= language->InitializeIfNeeded();

  return initialize_if_needed;
}

// OSX and Android don't have |pending_request_param_|
#if !defined(USE_BROWSER_SPELLCHECKER)
void SpellCheck::PostDelayedSpellCheckTask(SpellcheckRequest* request) {
  if (!request)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&SpellCheck::PerformSpellCheck, AsWeakPtr(),
                            base::Owned(request)));
}
#endif

// Mac and Android use their platform engines instead.
#if !defined(USE_BROWSER_SPELLCHECKER)
void SpellCheck::PerformSpellCheck(SpellcheckRequest* param) {
  DCHECK(param);

  if (languages_.empty() ||
      std::find_if(languages_.begin(), languages_.end(),
                   [](SpellcheckLanguage* language) {
                     return !language->IsEnabled();
                   }) != languages_.end()) {
    param->completion()->didCancelCheckingText();
  } else {
    WebVector<blink::WebTextCheckingResult> results;
    SpellCheckParagraph(param->text(), &results);
    param->completion()->didFinishCheckingText(results);
  }
}
#endif

void SpellCheck::CreateTextCheckingResults(
    ResultFilter filter,
    int line_offset,
    const base::string16& line_text,
    const std::vector<SpellCheckResult>& spellcheck_results,
    WebVector<WebTextCheckingResult>* textcheck_results) {
  DCHECK(!line_text.empty());

  std::vector<WebTextCheckingResult> results;
  for (const SpellCheckResult& spellcheck_result : spellcheck_results) {
    DCHECK_LE(static_cast<size_t>(spellcheck_result.location),
              line_text.length());
    DCHECK_LE(static_cast<size_t>(spellcheck_result.location +
                                  spellcheck_result.length),
              line_text.length());

    const base::string16& misspelled_word =
        line_text.substr(spellcheck_result.location, spellcheck_result.length);
    base::string16 replacement = spellcheck_result.replacement;
    SpellCheckResult::Decoration decoration = spellcheck_result.decoration;

    // Ignore words in custom dictionary.
    if (custom_dictionary_.SpellCheckWord(misspelled_word, 0,
                                          misspelled_word.length())) {
      continue;
    }

    // Use the same types of appostrophes as in the mispelled word.
    PreserveOriginalApostropheTypes(misspelled_word, &replacement);

    // Ignore misspellings due the typographical apostrophe.
    if (misspelled_word == replacement)
      continue;

    if (filter == USE_NATIVE_CHECKER) {
      // Double-check misspelled words with out spellchecker and attach grammar
      // markers to them if our spellchecker tells us they are correct words,
      // i.e. they are probably contextually-misspelled words.
      int unused_misspelling_start = 0;
      int unused_misspelling_length = 0;
      if (decoration == SpellCheckResult::SPELLING &&
          SpellCheckWord(misspelled_word.c_str(), kNoOffset,
                         misspelled_word.length(), kNoTag,
                         &unused_misspelling_start, &unused_misspelling_length,
                         nullptr)) {
        decoration = SpellCheckResult::GRAMMAR;
      }
    }

    results.push_back(WebTextCheckingResult(
        static_cast<WebTextDecorationType>(decoration),
        line_offset + spellcheck_result.location, spellcheck_result.length,
        replacement, spellcheck_result.hash));
  }

  textcheck_results->assign(results);
}

bool SpellCheck::IsSpellcheckEnabled() {
#if defined(OS_ANDROID)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kEnableAndroidSpellChecker)) {
    return false;
  }
#endif
  return spellcheck_enabled_;
}

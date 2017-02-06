// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/spellchecker/platform_spelling_engine.h"

#include "chrome/common/spellcheck_messages.h"
#include "content/public/renderer/render_thread.h"

using content::RenderThread;

SpellingEngine* CreateNativeSpellingEngine() {
  return new PlatformSpellingEngine();
}

void PlatformSpellingEngine::Init(base::File bdict_file) {
  DCHECK(!bdict_file.IsValid());
}

bool PlatformSpellingEngine::InitializeIfNeeded() {
  return false;
}

bool PlatformSpellingEngine::IsEnabled() {
  return true;
}

// Synchronously query against the platform's spellchecker.
// TODO(groby): We might want async support here, too. Ideally,
// all engines share a similar path for async requests.
bool PlatformSpellingEngine::CheckSpelling(const base::string16& word_to_check,
                                           int tag) {
  bool word_correct = false;
  RenderThread::Get()->Send(new SpellCheckHostMsg_CheckSpelling(
      word_to_check, tag, &word_correct));
  return word_correct;
}

// Synchronously query against the platform's spellchecker.
// TODO(groby): We might want async support here, too. Ideally,
// all engines share a similar path for async requests.
void PlatformSpellingEngine::FillSuggestionList(
    const base::string16& wrong_word,
    std::vector<base::string16>* optional_suggestions) {
    RenderThread::Get()->Send(new SpellCheckHostMsg_FillSuggestionList(
        wrong_word, optional_suggestions));
}

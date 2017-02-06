// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SPELLCHECK_COMMON_H_
#define CHROME_COMMON_SPELLCHECK_COMMON_H_

#include <stddef.h>

#include <string>
#include <vector>

namespace base {
class FilePath;
}

namespace chrome {
namespace spellcheck_common {

// The number of hours that a session of feedback for spelling service lasts.
// After this number of hours passes, all feedback.
static const int kSessionHours = 6;

// The number of context words to keep on either side of a misspelling for
// spelling service feedback.
static const int kContextWordCount = 2;

// The number of seconds between sending feedback to spelling service.
static const int kFeedbackIntervalSeconds = 1800;  // 30 minutes

// Max number of dictionary suggestions.
static const int kMaxSuggestions = 5;

// Maximum number of words in the custom spellcheck dictionary that can be
// synced.
static const size_t MAX_SYNCABLE_DICTIONARY_WORDS = 1300;

// Maximum number of bytes in a word that can be added to the custom spellcheck
// dictionary.
static const size_t MAX_CUSTOM_DICTIONARY_WORD_BYTES = 99;

base::FilePath GetVersionedFileName(const std::string& input_language,
                                    const base::FilePath& dict_dir);

// Returns the spellcheck language that should be used for |language|. For
// example, converts "hu-HU" into "hu", because we have only one variant of
// Hungarian. Converts "en-US" into "en-US", because we have several variants of
// English dictionaries.
//
// Returns an empty string if no spellcheck language found. For example, there's
// no single dictionary for English, so this function returns an empty string
// for "en".
std::string GetCorrespondingSpellCheckLanguage(const std::string& language);

// Get SpellChecker supported languages.
void SpellCheckLanguages(std::vector<std::string>* languages);

// Gets the ISO codes for the language and country of this |locale|. The
// |locale| is an ISO locale ID that may not include a country ID, e.g., "fr" or
// "de". This method converts the UI locale to a full locale ID and converts the
// full locale ID to an ISO language code and an ISO3 country code.
void GetISOLanguageCountryCodeFromLocale(const std::string& locale,
                                         std::string* language_code,
                                         std::string* country_code);

}  // namespace spellcheck_common
}  // namespace chrome

#endif  // CHROME_COMMON_SPELLCHECK_COMMON_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/spellcheck_common.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/common/unicode/urename.h"
#include "third_party/icu/source/common/unicode/utypes.h"

namespace chrome {
namespace spellcheck_common {

struct LanguageRegion {
  const char* language;  // The language.
  const char* language_region;  // language & region, used by dictionaries.
};

struct LanguageVersion {
  const char* language;  // The language input.
  const char* version;   // The corresponding version.
};

static const LanguageRegion g_supported_spellchecker_languages[] = {
  // Several languages are not to be included in the spellchecker list:
  // th-TH, vi-VI.
  {"af", "af-ZA"},
  {"bg", "bg-BG"},
  {"ca", "ca-ES"},
  {"cs", "cs-CZ"},
  {"da", "da-DK"},
  {"de", "de-DE"},
  {"el", "el-GR"},
  {"en-AU", "en-GB"},
  {"en-CA", "en-CA"},
  {"en-GB", "en-GB"},
  {"en-US", "en-US"},
  {"es", "es-ES"},
  {"es-419", "es-ES"},
  {"es-AR", "es-ES"},
  {"es-ES", "es-ES"},
  {"es-MX", "es-ES"},
  {"es-US", "es-ES"},
  {"et", "et-EE"},
  {"fa", "fa-IR"},
  {"fo", "fo-FO"},
  {"fr", "fr-FR"},
  {"he", "he-IL"},
  {"hi", "hi-IN"},
  {"hr", "hr-HR"},
  {"hu", "hu-HU"},
  {"id", "id-ID"},
  {"it", "it-IT"},
  {"ko", "ko"},
  {"lt", "lt-LT"},
  {"lv", "lv-LV"},
  {"nb", "nb-NO"},
  {"nl", "nl-NL"},
  {"pl", "pl-PL"},
  {"pt-BR", "pt-BR"},
  {"pt-PT", "pt-PT"},
  {"ro", "ro-RO"},
  {"ru", "ru-RU"},
  {"sh", "sh"},
  {"sk", "sk-SK"},
  {"sl", "sl-SI"},
  {"sq", "sq"},
  {"sr", "sr"},
  {"sv", "sv-SE"},
  {"ta", "ta-IN"},
  {"tg", "tg-TG"},
  {"tr", "tr-TR"},
  {"uk", "uk-UA"},
  {"vi", "vi-VN"},
};

bool IsValidRegion(const std::string& region) {
  for (size_t i = 0; i < arraysize(g_supported_spellchecker_languages);
       ++i) {
    if (g_supported_spellchecker_languages[i].language_region == region)
      return true;
  }
  return false;
}

// This function returns the language-region version of language name.
// e.g. returns hi-IN for hi.
std::string GetSpellCheckLanguageRegion(const std::string& input_language) {
  for (size_t i = 0; i < arraysize(g_supported_spellchecker_languages);
       ++i) {
    if (g_supported_spellchecker_languages[i].language == input_language) {
      return std::string(
          g_supported_spellchecker_languages[i].language_region);
    }
  }

  return input_language;
}

base::FilePath GetVersionedFileName(const std::string& input_language,
                                    const base::FilePath& dict_dir) {
#if !defined(TOOLKIT_QT)
  // The default dictionary version is 3-0. This version indicates that the bdic
  // file contains a checksum.
  static const char kDefaultVersionString[] = "-3-0";

  // Add non-default version strings here. Use the same version for all the
  // dictionaries that you add at the same time. Increment the major version
  // number if you're updating either dic or aff files. Increment the minor
  // version number if you're updating only dic_delta files.
  static LanguageVersion special_version_string[] = {
    {"tr-TR", "-4-0"},  // Jan 9, 2013: Add "FLAG num" to aff to avoid heapcheck
                        // crash.
    {"tg-TG", "-5-0"},  // Mar 4, 2014: Add Tajik dictionary.

    // April 2016: Local fixes
    {"en-CA", "-7-1"},
    {"en-GB", "-7-1"},
    {"en-US", "-7-1"},

    // March 2016: Initial check-in of Persian
    {"fa-IR", "-7-0"},
  };

  // Generate the bdict file name using default version string or special
  // version string, depending on the language.
  std::string language = GetSpellCheckLanguageRegion(input_language);
  std::string versioned_bdict_file_name(language + kDefaultVersionString +
                                        ".bdic");
  for (size_t i = 0; i < arraysize(special_version_string); ++i) {
    if (language == special_version_string[i].language) {
      versioned_bdict_file_name =
          language + special_version_string[i].version + ".bdic";
      break;
    }
  }
#else
  std::string versioned_bdict_file_name(input_language + ".bdic");
#endif
  return dict_dir.AppendASCII(versioned_bdict_file_name);
}

std::string GetCorrespondingSpellCheckLanguage(const std::string& language) {
  std::string best_match;
  // Look for exact match in the Spell Check language list.
  for (size_t i = 0; i < arraysize(g_supported_spellchecker_languages);
       ++i) {
    // First look for exact match in the language region of the list.
    std::string spellcheck_language(
        g_supported_spellchecker_languages[i].language);
    if (spellcheck_language == language)
      return language;

    // Next, look for exact match in the language_region part of the list.
    std::string spellcheck_language_region(
        g_supported_spellchecker_languages[i].language_region);
    if (spellcheck_language_region == language) {
      if (best_match.empty())
        best_match = g_supported_spellchecker_languages[i].language;
    }
  }

  // No match found - return best match, if any.
  return best_match;
}

void SpellCheckLanguages(std::vector<std::string>* languages) {
  for (size_t i = 0; i < arraysize(g_supported_spellchecker_languages);
       ++i) {
    languages->push_back(g_supported_spellchecker_languages[i].language);
  }
}

void GetISOLanguageCountryCodeFromLocale(const std::string& locale,
                                         std::string* language_code,
                                         std::string* country_code) {
  DCHECK(language_code);
  DCHECK(country_code);
  char language[ULOC_LANG_CAPACITY] = ULOC_ENGLISH;
  const char* country = "USA";
  if (!locale.empty()) {
    UErrorCode error = U_ZERO_ERROR;
    char id[ULOC_LANG_CAPACITY + ULOC_SCRIPT_CAPACITY + ULOC_COUNTRY_CAPACITY];
    uloc_addLikelySubtags(locale.c_str(), id, arraysize(id), &error);
    error = U_ZERO_ERROR;
    uloc_getLanguage(id, language, arraysize(language), &error);
    country = uloc_getISO3Country(id);
  }
  *language_code = std::string(language);
  *country_code = std::string(country);
}

}  // namespace spellcheck_common
}  // namespace chrome

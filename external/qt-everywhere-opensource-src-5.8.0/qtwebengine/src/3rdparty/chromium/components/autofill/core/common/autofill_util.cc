// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_util.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_switches.h"

namespace autofill {

namespace {

const char kSplitCharacters[] = " .,-_@";

template <typename Char>
struct Compare : base::CaseInsensitiveCompareASCII<Char> {
  explicit Compare(bool case_sensitive) : case_sensitive_(case_sensitive) {}
  bool operator()(Char x, Char y) const {
    return case_sensitive_ ? (x == y)
                           : base::CaseInsensitiveCompareASCII<Char>::
                             operator()(x, y);
  }

 private:
  bool case_sensitive_;
};

}  // namespace

bool IsFeatureSubstringMatchEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableSuggestionsWithSubstringMatch);
}

bool IsKeyboardAccessoryEnabled() {
#if defined(OS_ANDROID)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableAccessorySuggestionView) ||
         (base::FieldTrialList::FindFullName("AutofillKeyboardAccessory") ==
              "Enabled" &&
          !base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableAccessorySuggestionView));
#else  // !defined(OS_ANDROID)
  return false;
#endif
}

bool FieldIsSuggestionSubstringStartingOnTokenBoundary(
    const base::string16& suggestion,
    const base::string16& field_contents,
    bool case_sensitive) {
  if (!IsFeatureSubstringMatchEnabled()) {
    return base::StartsWith(suggestion, field_contents,
                            case_sensitive
                                ? base::CompareCase::SENSITIVE
                                : base::CompareCase::INSENSITIVE_ASCII);
  }

  return suggestion.length() >= field_contents.length() &&
         GetTextSelectionStart(suggestion, field_contents, case_sensitive) !=
             base::string16::npos;
}

size_t GetTextSelectionStart(const base::string16& suggestion,
                             const base::string16& field_contents,
                             bool case_sensitive) {
  const base::string16 kSplitChars = base::ASCIIToUTF16(kSplitCharacters);

  // Loop until we find either the |field_contents| is a prefix of |suggestion|
  // or character right before the match is one of the splitting characters.
  for (base::string16::const_iterator it = suggestion.begin();
       (it = std::search(
            it, suggestion.end(), field_contents.begin(), field_contents.end(),
            Compare<base::string16::value_type>(case_sensitive))) !=
           suggestion.end();
       ++it) {
    if (it == suggestion.begin() ||
        kSplitChars.find(*(it - 1)) != std::string::npos) {
      // Returns the character position right after the |field_contents| within
      // |suggestion| text as a caret position for text selection.
      return it - suggestion.begin() + field_contents.size();
    }
  }

  // Unable to find the |field_contents| in |suggestion| text.
  return base::string16::npos;
}

bool IsDesktopPlatform() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  return false;
#else
  return true;
#endif
}

}  // namespace autofill

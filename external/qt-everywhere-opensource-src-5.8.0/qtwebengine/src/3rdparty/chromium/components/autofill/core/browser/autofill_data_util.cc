// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_data_util.h"

#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {
namespace data_util {

namespace {
const char* const name_prefixes[] = {
    "1lt",     "1st", "2lt", "2nd",    "3rd",  "admiral", "capt",
    "captain", "col", "cpt", "dr",     "gen",  "general", "lcdr",
    "lt",      "ltc", "ltg", "ltjg",   "maj",  "major",   "mg",
    "mr",      "mrs", "ms",  "pastor", "prof", "rep",     "reverend",
    "rev",     "sen", "st"};

const char* const name_suffixes[] = {"b.a", "ba", "d.d.s", "dds",  "i",   "ii",
                                     "iii", "iv", "ix",    "jr",   "m.a", "m.d",
                                     "ma",  "md", "ms",    "ph.d", "phd", "sr",
                                     "v",   "vi", "vii",   "viii", "x"};

const char* const family_name_prefixes[] = {"d'",  "de",  "del", "der", "di",
                                            "la",  "le",  "mc",  "san", "st",
                                            "ter", "van", "von"};

// Returns true if |set| contains |element|, modulo a final period.
bool ContainsString(const char* const set[],
                    size_t set_size,
                    const base::string16& element) {
  if (!base::IsStringASCII(element))
    return false;

  base::string16 trimmed_element;
  base::TrimString(element, base::ASCIIToUTF16("."), &trimmed_element);

  for (size_t i = 0; i < set_size; ++i) {
    if (base::LowerCaseEqualsASCII(trimmed_element, set[i]))
      return true;
  }

  return false;
}

// Removes common name prefixes from |name_tokens|.
void StripPrefixes(std::vector<base::string16>* name_tokens) {
  std::vector<base::string16>::iterator iter = name_tokens->begin();
  while (iter != name_tokens->end()) {
    if (!ContainsString(name_prefixes, arraysize(name_prefixes), *iter))
      break;
    ++iter;
  }

  std::vector<base::string16> copy_vector;
  copy_vector.assign(iter, name_tokens->end());
  *name_tokens = copy_vector;
}

// Removes common name suffixes from |name_tokens|.
void StripSuffixes(std::vector<base::string16>* name_tokens) {
  while (!name_tokens->empty()) {
    if (!ContainsString(name_suffixes, arraysize(name_suffixes),
                        name_tokens->back())) {
      break;
    }
    name_tokens->pop_back();
  }
}

}  // namespace

NameParts SplitName(const base::string16& name) {
  std::vector<base::string16> name_tokens =
      base::SplitString(name, base::ASCIIToUTF16(" ,"), base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  StripPrefixes(&name_tokens);

  // Don't assume "Ma" is a suffix in John Ma.
  if (name_tokens.size() > 2)
    StripSuffixes(&name_tokens);

  NameParts parts;

  if (name_tokens.empty()) {
    // Bad things have happened; just assume the whole thing is a given name.
    parts.given = name;
    return parts;
  }

  // Only one token, assume given name.
  if (name_tokens.size() == 1) {
    parts.given = name_tokens[0];
    return parts;
  }

  // 2 or more tokens. Grab the family, which is the last word plus any
  // recognizable family prefixes.
  std::vector<base::string16> reverse_family_tokens;
  reverse_family_tokens.push_back(name_tokens.back());
  name_tokens.pop_back();
  while (name_tokens.size() >= 1 &&
         ContainsString(family_name_prefixes, arraysize(family_name_prefixes),
                        name_tokens.back())) {
    reverse_family_tokens.push_back(name_tokens.back());
    name_tokens.pop_back();
  }

  std::vector<base::string16> family_tokens(reverse_family_tokens.rbegin(),
                                            reverse_family_tokens.rend());
  parts.family = base::JoinString(family_tokens, base::ASCIIToUTF16(" "));

  // Take the last remaining token as the middle name (if there are at least 2
  // tokens).
  if (name_tokens.size() >= 2) {
    parts.middle = name_tokens.back();
    name_tokens.pop_back();
  }

  // Remainder is given name.
  parts.given = base::JoinString(name_tokens, base::ASCIIToUTF16(" "));

  return parts;
}

bool ProfileMatchesFullName(const base::string16 full_name,
                            const autofill::AutofillProfile& profile) {
  const base::string16 kSpace = base::ASCIIToUTF16(" ");
  const base::string16 kPeriodSpace = base::ASCIIToUTF16(". ");

  // First Last
  base::string16 candidate = profile.GetRawInfo(autofill::NAME_FIRST) + kSpace +
                             profile.GetRawInfo(autofill::NAME_LAST);
  if (!full_name.compare(candidate)) {
    return true;
  }

  // First Middle Last
  candidate = profile.GetRawInfo(autofill::NAME_FIRST) + kSpace +
              profile.GetRawInfo(autofill::NAME_MIDDLE) + kSpace +
              profile.GetRawInfo(autofill::NAME_LAST);
  if (!full_name.compare(candidate)) {
    return true;
  }

  // First M Last
  candidate = profile.GetRawInfo(autofill::NAME_FIRST) + kSpace +
              profile.GetRawInfo(autofill::NAME_MIDDLE_INITIAL) + kSpace +
              profile.GetRawInfo(autofill::NAME_LAST);
  if (!full_name.compare(candidate)) {
    return true;
  }

  // First M. Last
  candidate = profile.GetRawInfo(autofill::NAME_FIRST) + kSpace +
              profile.GetRawInfo(autofill::NAME_MIDDLE_INITIAL) + kPeriodSpace +
              profile.GetRawInfo(autofill::NAME_LAST);
  if (!full_name.compare(candidate)) {
    return true;
  }

  return false;
}

}  // namespace data_util
}  // namespace autofill

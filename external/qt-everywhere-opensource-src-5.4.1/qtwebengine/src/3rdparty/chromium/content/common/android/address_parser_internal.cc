// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/address_parser_internal.h"

#include <bitset>

#include "base/logging.h"
#include "base/strings/string_util.h"

namespace {

// Number of digits for a valid zip code.
const size_t kZipDigits = 5;

// Number of digits for a valid zip code in the Zip Plus 4 format.
const size_t kZipPlus4Digits = 9;

// Maximum number of digits of a house number, including possible hyphens.
const size_t kMaxHouseDigits = 5;

base::char16 SafePreviousChar(const base::string16::const_iterator& it,
    const base::string16::const_iterator& begin) {
  if (it == begin)
    return ' ';
  return *(it - 1);
}

base::char16 SafeNextChar(const base::string16::const_iterator& it,
    const base::string16::const_iterator& end) {
  if (it == end)
    return ' ';
  return *(it + 1);
}

bool WordLowerCaseEqualsASCII(base::string16::const_iterator word_begin,
    base::string16::const_iterator word_end, const char* ascii_to_match) {
  for (base::string16::const_iterator it = word_begin; it != word_end;
      ++it, ++ascii_to_match) {
    if (!*ascii_to_match || base::ToLowerASCII(*it) != *ascii_to_match)
      return false;
  }
  return *ascii_to_match == 0 || *ascii_to_match == ' ';
}

bool LowerCaseEqualsASCIIWithPlural(base::string16::const_iterator word_begin,
    base::string16::const_iterator word_end, const char* ascii_to_match,
    bool allow_plural) {
  for (base::string16::const_iterator it = word_begin; it != word_end;
      ++it, ++ascii_to_match) {
    if (!*ascii_to_match && allow_plural && *it == 's' && it + 1 == word_end)
      return true;

    if (!*ascii_to_match || base::ToLowerASCII(*it) != *ascii_to_match)
      return false;
  }
  return *ascii_to_match == 0;
}

}  // anonymous namespace

namespace content {

namespace address_parser {

namespace internal {

Word::Word(const base::string16::const_iterator& begin,
           const base::string16::const_iterator& end)
    : begin(begin),
      end(end) {
  DCHECK(begin <= end);
}

bool HouseNumberParser::IsPreDelimiter(base::char16 character) {
  return character == ':' || IsPostDelimiter(character);
}

bool HouseNumberParser::IsPostDelimiter(base::char16 character) {
  return IsWhitespace(character) || strchr(",\"'", character);
}

void HouseNumberParser::RestartOnNextDelimiter() {
  ResetState();
  for (; it_ != end_ && !IsPreDelimiter(*it_); ++it_) {}
}

void HouseNumberParser::AcceptChars(size_t num_chars) {
  size_t offset = std::min(static_cast<size_t>(std::distance(it_, end_)),
                           num_chars);
  it_ += offset;
  result_chars_ += offset;
}

void HouseNumberParser::SkipChars(size_t num_chars) {
  it_ += std::min(static_cast<size_t>(std::distance(it_, end_)), num_chars);
}

void HouseNumberParser::ResetState() {
  num_digits_ = 0;
  result_chars_ = 0;
}

bool HouseNumberParser::CheckFinished(Word* word) const {
  // There should always be a number after a hyphen.
  if (result_chars_ == 0 || SafePreviousChar(it_, begin_) == '-')
    return false;

  if (word) {
    word->begin = it_ - result_chars_;
    word->end = it_;
  }
  return true;
}

bool HouseNumberParser::Parse(
    const base::string16::const_iterator& begin,
    const base::string16::const_iterator& end, Word* word) {
  it_ = begin_ = begin;
  end_ = end;
  ResetState();

  // Iterations only used as a fail-safe against any buggy infinite loops.
  size_t iterations = 0;
  size_t max_iterations = end - begin + 1;
  for (; it_ != end_ && iterations < max_iterations; ++iterations) {

    // Word finished case.
    if (IsPostDelimiter(*it_)) {
      if (CheckFinished(word))
        return true;
      else if (result_chars_)
        ResetState();

      SkipChars(1);
      continue;
    }

    // More digits. There should be no more after a letter was found.
    if (IsAsciiDigit(*it_)) {
      if (num_digits_ >= kMaxHouseDigits) {
        RestartOnNextDelimiter();
      } else {
        AcceptChars(1);
        ++num_digits_;
      }
      continue;
    }

    if (IsAsciiAlpha(*it_)) {
      // Handle special case 'one'.
      if (result_chars_ == 0) {
        if (it_ + 3 <= end_ && LowerCaseEqualsASCII(it_, it_ + 3, "one"))
          AcceptChars(3);
        else
          RestartOnNextDelimiter();
        continue;
      }

      // There should be more than 1 character because of result_chars.
      DCHECK_GT(result_chars_, 0U);
      DCHECK(it_ != begin_);
      base::char16 previous = SafePreviousChar(it_, begin_);
      if (IsAsciiDigit(previous)) {
        // Check cases like '12A'.
        base::char16 next = SafeNextChar(it_, end_);
        if (IsPostDelimiter(next)) {
          AcceptChars(1);
          continue;
        }

        // Handle cases like 12a, 1st, 2nd, 3rd, 7th.
        if (IsAsciiAlpha(next)) {
          base::char16 last_digit = previous;
          base::char16 first_letter = base::ToLowerASCII(*it_);
          base::char16 second_letter = base::ToLowerASCII(next);
          bool is_teen = SafePreviousChar(it_ - 1, begin_) == '1' &&
              num_digits_ == 2;

          switch (last_digit - '0') {
          case 1:
            if ((first_letter == 's' && second_letter == 't') ||
                (first_letter == 't' && second_letter == 'h' && is_teen)) {
              AcceptChars(2);
              continue;
            }
            break;

          case 2:
            if ((first_letter == 'n' && second_letter == 'd') ||
                (first_letter == 't' && second_letter == 'h' && is_teen)) {
              AcceptChars(2);
              continue;
            }
            break;

          case 3:
            if ((first_letter == 'r' && second_letter == 'd') ||
                (first_letter == 't' && second_letter == 'h' && is_teen)) {
              AcceptChars(2);
              continue;
            }
            break;

          case 0:
            // Explicitly exclude '0th'.
            if (num_digits_ == 1)
              break;

          case 4:
          case 5:
          case 6:
          case 7:
          case 8:
          case 9:
            if (first_letter == 't' && second_letter == 'h') {
              AcceptChars(2);
              continue;
            }
            break;

          default:
            NOTREACHED();
          }
        }
      }

      RestartOnNextDelimiter();
      continue;
    }

    if (*it_ == '-' && num_digits_ > 0) {
      AcceptChars(1);
      ++num_digits_;
      continue;
    }

    RestartOnNextDelimiter();
    SkipChars(1);
  }

  if (iterations >= max_iterations)
    return false;

  return CheckFinished(word);
}

bool FindStateStartingInWord(WordList* words,
                             size_t state_first_word,
                             size_t* state_last_word,
                             String16Tokenizer* tokenizer,
                             size_t* state_index) {

  // Bitmasks containing the allowed suffixes for 2-letter state codes.
  static const int state_two_letter_suffix[23] = {
    0x02060c00,  // A followed by: [KLRSZ].
    0x00000000,  // B.
    0x00084001,  // C followed by: [AOT].
    0x00000014,  // D followed by: [CE].
    0x00000000,  // E.
    0x00001800,  // F followed by: [LM].
    0x00100001,  // G followed by: [AU].
    0x00000100,  // H followed by: [I].
    0x00002809,  // I followed by: [ADLN].
    0x00000000,  // J.
    0x01040000,  // K followed by: [SY].
    0x00000001,  // L followed by: [A].
    0x000ce199,  // M followed by: [ADEHINOPST].
    0x0120129c,  // N followed by: [CDEHJMVY].
    0x00020480,  // O followed by: [HKR].
    0x00420001,  // P followed by: [ARW].
    0x00000000,  // Q.
    0x00000100,  // R followed by: [I].
    0x0000000c,  // S followed by: [CD].
    0x00802000,  // T followed by: [NX].
    0x00080000,  // U followed by: [T].
    0x00080101,  // V followed by: [AIT].
    0x01200101   // W followed by: [AIVY].
  };

  // Accumulative number of states for the 2-letter code indexed by the first.
  static const int state_two_letter_accumulative[24] = {
     0,  5,  5,  8, 10, 10, 12, 14,
    15, 19, 19, 21, 22, 32, 40, 43,
    46, 46, 47, 49, 51, 52, 55, 59
  };

  // State names sorted alphabetically with their lengths.
  // There can be more than one possible name for a same state if desired.
  static const struct StateNameInfo {
    const char* string;
    char first_word_length;
    char length;
    char state_index; // Relative to two-character code alphabetical order.
  } state_names[59] = {
    { "alabama", 7, 7, 1 }, { "alaska", 6, 6, 0 },
    { "american samoa", 8, 14, 3 }, { "arizona", 7, 7, 4 },
    { "arkansas", 8, 8, 2 },
    { "california", 10, 10, 5 }, { "colorado", 8, 8, 6 },
    { "connecticut", 11, 11, 7 }, { "delaware", 8, 8, 9 },
    { "district of columbia", 8, 20, 8 },
    { "federated states of micronesia", 9, 30, 11 }, { "florida", 7, 7, 10 },
    { "guam", 4, 4, 13 }, { "georgia", 7, 7, 12 },
    { "hawaii", 6, 6, 14 },
    { "idaho", 5, 5, 16 }, { "illinois", 8, 8, 17 }, { "indiana", 7, 7, 18 },
    { "iowa", 4, 4, 15 },
    { "kansas", 6, 6, 19 }, { "kentucky", 8, 8, 20 },
    { "louisiana", 9, 9, 21 },
    { "maine", 5, 5, 24 }, { "marshall islands", 8, 16, 25 },
    { "maryland", 8, 8, 23 }, { "massachusetts", 13, 13, 22 },
    { "michigan", 8, 8, 26 }, { "minnesota", 9, 9, 27 },
    { "mississippi", 11, 11, 30 }, { "missouri", 8, 8, 28 },
    { "montana", 7, 7, 31 },
    { "nebraska", 8, 8, 34 }, { "nevada", 6, 6, 38 },
    { "new hampshire", 3, 13, 35 }, { "new jersey", 3, 10, 36 },
    { "new mexico", 3, 10, 37 }, { "new york", 3, 8, 39 },
    { "north carolina", 5, 14, 32 }, { "north dakota", 5, 12, 33 },
    { "northern mariana islands", 8, 24, 29 },
    { "ohio", 4, 4, 40 }, { "oklahoma", 8, 8, 41 }, { "oregon", 6, 6, 42 },
    { "palau", 5, 5, 45 }, { "pennsylvania", 12, 12, 43 },
    { "puerto rico", 6, 11, 44 },
    { "rhode island", 5, 5, 46 },
    { "south carolina", 5, 14, 47 }, { "south dakota", 5, 12, 48 },
    { "tennessee", 9, 9, 49 }, { "texas", 5, 5, 50 },
    { "utah", 4, 4, 51 },
    { "vermont", 7, 7, 54 }, { "virgin islands", 6, 14, 53 },
    { "virginia", 8, 8, 52 },
    { "washington", 10, 10, 55 }, { "west virginia", 4, 13, 57 },
    { "wisconsin", 9, 9, 56 }, { "wyoming", 7, 7, 58 }
  };

  // Accumulative number of states for sorted names indexed by the first letter.
  // Required a different one since there are codes that don't share their
  // first letter with the name of their state (MP = Northern Mariana Islands).
  static const int state_names_accumulative[24] = {
     0,  5,  5,  8, 10, 10, 12, 14,
    15, 19, 19, 21, 22, 31, 40, 43,
    46, 46, 47, 49, 51, 52, 55, 59
  };

  DCHECK_EQ(state_names_accumulative[arraysize(state_names_accumulative) - 1],
      static_cast<int>(ARRAYSIZE_UNSAFE(state_names)));

  const Word& first_word = words->at(state_first_word);
  int length = first_word.end - first_word.begin;
  if (length < 2 || !IsAsciiAlpha(*first_word.begin))
    return false;

  // No state names start with x, y, z.
  base::char16 first_letter = base::ToLowerASCII(*first_word.begin);
  if (first_letter > 'w')
    return false;

  DCHECK(first_letter >= 'a');
  int first_index = first_letter - 'a';

  // Look for two-letter state names.
  if (length == 2 && IsAsciiAlpha(*(first_word.begin + 1))) {
    base::char16 second_letter = base::ToLowerASCII(*(first_word.begin + 1));
    DCHECK(second_letter >= 'a');

    int second_index = second_letter - 'a';
    if (!(state_two_letter_suffix[first_index] & (1 << second_index)))
      return false;

    std::bitset<32> previous_suffixes = state_two_letter_suffix[first_index] &
        ((1 << second_index) - 1);
    *state_last_word = state_first_word;
    *state_index = state_two_letter_accumulative[first_index] +
        previous_suffixes.count();
    return true;
  }

  // Look for full state names by their first letter. Discard by length.
  for (int state = state_names_accumulative[first_index];
      state < state_names_accumulative[first_index + 1]; ++state) {
    if (state_names[state].first_word_length != length)
      continue;

    bool state_match = false;
    size_t state_word = state_first_word;
    for (int pos = 0; true; ) {
      if (!WordLowerCaseEqualsASCII(words->at(state_word).begin,
          words->at(state_word).end, &state_names[state].string[pos]))
        break;

      pos += words->at(state_word).end - words->at(state_word).begin + 1;
      if (pos >= state_names[state].length) {
        state_match = true;
        break;
      }

      // Ran out of words, extract more from the tokenizer.
      if (++state_word == words->size()) {
        do {
          if (!tokenizer->GetNext())
            break;
        } while (tokenizer->token_is_delim());
        words->push_back(
            Word(tokenizer->token_begin(), tokenizer->token_end()));
      }
    }

    if (state_match) {
      *state_last_word = state_word;
      *state_index = state_names[state].state_index;
      return true;
    }
  }

  return false;
}

bool IsZipValid(const Word& word, size_t state_index) {
  size_t length = word.end - word.begin;
  if (length != kZipDigits && length != kZipPlus4Digits + 1)
    return false;

  for (base::string16::const_iterator it = word.begin; it != word.end; ++it) {
    size_t pos = it - word.begin;
    if (IsAsciiDigit(*it) || (*it == '-' && pos == kZipDigits))
      continue;
    return false;
  }
  return IsZipValidForState(word, state_index);
}

bool IsZipValidForState(const Word& word, size_t state_index) {
  // List of valid zip code ranges.
  static const struct {
    signed char low;
    signed char high;
    signed char exception1;
    signed char exception2;
  } zip_range[] = {
    { 99, 99, -1, -1 }, // AK Alaska.
    { 35, 36, -1, -1 }, // AL Alabama.
    { 71, 72, -1, -1 }, // AR Arkansas.
    { 96, 96, -1, -1 }, // AS American Samoa.
    { 85, 86, -1, -1 }, // AZ Arizona.
    { 90, 96, -1, -1 }, // CA California.
    { 80, 81, -1, -1 }, // CO Colorado.
    {  6,  6, -1, -1 }, // CT Connecticut.
    { 20, 20, -1, -1 }, // DC District of Columbia.
    { 19, 19, -1, -1 }, // DE Delaware.
    { 32, 34, -1, -1 }, // FL Florida.
    { 96, 96, -1, -1 }, // FM Federated States of Micronesia.
    { 30, 31, -1, -1 }, // GA Georgia.
    { 96, 96, -1, -1 }, // GU Guam.
    { 96, 96, -1, -1 }, // HI Hawaii.
    { 50, 52, -1, -1 }, // IA Iowa.
    { 83, 83, -1, -1 }, // ID Idaho.
    { 60, 62, -1, -1 }, // IL Illinois.
    { 46, 47, -1, -1 }, // IN Indiana.
    { 66, 67, 73, -1 }, // KS Kansas.
    { 40, 42, -1, -1 }, // KY Kentucky.
    { 70, 71, -1, -1 }, // LA Louisiana.
    {  1,  2, -1, -1 }, // MA Massachusetts.
    { 20, 21, -1, -1 }, // MD Maryland.
    {  3,  4, -1, -1 }, // ME Maine.
    { 96, 96, -1, -1 }, // MH Marshall Islands.
    { 48, 49, -1, -1 }, // MI Michigan.
    { 55, 56, -1, -1 }, // MN Minnesota.
    { 63, 65, -1, -1 }, // MO Missouri.
    { 96, 96, -1, -1 }, // MP Northern Mariana Islands.
    { 38, 39, -1, -1 }, // MS Mississippi.
    { 55, 56, -1, -1 }, // MT Montana.
    { 27, 28, -1, -1 }, // NC North Carolina.
    { 58, 58, -1, -1 }, // ND North Dakota.
    { 68, 69, -1, -1 }, // NE Nebraska.
    {  3,  4, -1, -1 }, // NH New Hampshire.
    {  7,  8, -1, -1 }, // NJ New Jersey.
    { 87, 88, 86, -1 }, // NM New Mexico.
    { 88, 89, 96, -1 }, // NV Nevada.
    { 10, 14,  0,  6 }, // NY New York.
    { 43, 45, -1, -1 }, // OH Ohio.
    { 73, 74, -1, -1 }, // OK Oklahoma.
    { 97, 97, -1, -1 }, // OR Oregon.
    { 15, 19, -1, -1 }, // PA Pennsylvania.
    {  6,  6,  0,  9 }, // PR Puerto Rico.
    { 96, 96, -1, -1 }, // PW Palau.
    {  2,  2, -1, -1 }, // RI Rhode Island.
    { 29, 29, -1, -1 }, // SC South Carolina.
    { 57, 57, -1, -1 }, // SD South Dakota.
    { 37, 38, -1, -1 }, // TN Tennessee.
    { 75, 79, 87, 88 }, // TX Texas.
    { 84, 84, -1, -1 }, // UT Utah.
    { 22, 24, 20, -1 }, // VA Virginia.
    {  6,  9, -1, -1 }, // VI Virgin Islands.
    {  5,  5, -1, -1 }, // VT Vermont.
    { 98, 99, -1, -1 }, // WA Washington.
    { 53, 54, -1, -1 }, // WI Wisconsin.
    { 24, 26, -1, -1 }, // WV West Virginia.
    { 82, 83, -1, -1 }  // WY Wyoming.
  };

  // Zip numeric value for the first two characters.
  DCHECK(word.begin != word.end);
  DCHECK(IsAsciiDigit(*word.begin));
  DCHECK(IsAsciiDigit(*(word.begin + 1)));
  int zip_prefix = (*word.begin - '0') * 10 + (*(word.begin + 1) - '0');

  if ((zip_prefix >= zip_range[state_index].low &&
       zip_prefix <= zip_range[state_index].high) ||
      zip_prefix == zip_range[state_index].exception1 ||
      zip_prefix == zip_range[state_index].exception2) {
    return true;
  }
  return false;
}

bool IsValidLocationName(const Word& word) {
  // Supported location names sorted alphabetically and grouped by first letter.
  static const struct LocationNameInfo {
    const char* string;
    char length;
    bool allow_plural;
  } location_names[157] = {
    { "alley", 5, false }, { "annex", 5, false }, { "arcade", 6, false },
    { "ave", 3, false }, { "ave.", 4, false }, { "avenue", 6, false },
    { "alameda", 7, false },
    { "bayou", 5, false }, { "beach", 5, false }, { "bend", 4, false },
    { "bluff", 5, true }, { "bottom", 6, false }, { "boulevard", 9, false },
    { "branch", 6, false }, { "bridge", 6, false }, { "brook", 5, true },
    { "burg", 4, true }, { "bypass", 6, false }, { "broadway", 8, false },
    { "camino", 6, false }, { "camp", 4, false }, { "canyon", 6, false },
    { "cape", 4, false }, { "causeway", 8, false }, { "center", 6, true },
    { "circle", 6, true }, { "cliff", 5, true }, { "club", 4, false },
    { "common", 6, false }, { "corner", 6, true }, { "course", 6, false },
    { "court", 5, true }, { "cove", 4, true }, { "creek", 5, false },
    { "crescent", 8, false }, { "crest", 5, false }, { "crossing", 8, false },
    { "crossroad", 9, false }, { "curve", 5, false }, { "circulo", 7, false },
    { "dale", 4, false }, { "dam", 3, false }, { "divide", 6, false },
    { "drive", 5, true },
    { "estate", 6, true }, { "expressway", 10, false },
    { "extension", 9, true },
    { "fall", 4, true }, { "ferry", 5, false }, { "field", 5, true },
    { "flat", 4, true }, { "ford", 4, true }, { "forest", 6, false },
    { "forge", 5, true }, { "fork", 4, true }, { "fort", 4, false },
    { "freeway", 7, false },
    { "garden", 6, true }, { "gateway", 7, false }, { "glen", 4, true },
    { "green", 5, true }, { "grove", 5, true },
    { "harbor", 6, true }, { "haven", 5, false }, { "heights", 7, false },
    { "highway", 7, false }, { "hill", 4, true }, { "hollow", 6, false },
    { "inlet", 5, false }, { "island", 6, true }, { "isle", 4, false },
    { "junction", 8, true },
    { "key", 3, true }, { "knoll", 5, true },
    { "lake", 4, true }, { "land", 4, false }, { "landing", 7, false },
    { "lane", 4, false }, { "light", 5, true }, { "loaf", 4, false },
    { "lock", 4, true }, { "lodge", 5, false }, { "loop", 4, false },
    { "mall", 4, false }, { "manor", 5, true }, { "meadow", 6, true },
    { "mews", 4, false }, { "mill", 4, true }, { "mission", 7, false },
    { "motorway", 8, false }, { "mount", 5, false }, { "mountain", 8, true },
    { "neck", 4, false },
    { "orchard", 7, false }, { "oval", 4, false }, { "overpass", 8, false },
    { "park", 4, true }, { "parkway", 7, true }, { "pass", 4, false },
    { "passage", 7, false }, { "path", 4, false }, { "pike", 4, false },
    { "pine", 4, true }, { "plain", 5, true }, { "plaza", 5, false },
    { "point", 5, true }, { "port", 4, true }, { "prairie", 7, false },
    { "privada", 7, false },
    { "radial", 6, false }, { "ramp", 4, false }, { "ranch", 5, false },
    { "rapid", 5, true }, { "rest", 4, false }, { "ridge", 5, true },
    { "river", 5, false }, { "road", 4, true }, { "route", 5, false },
    { "row", 3, false }, { "rue", 3, false }, { "run", 3, false },
    { "shoal", 5, true }, { "shore", 5, true }, { "skyway", 6, false },
    { "spring", 6, true }, { "spur", 4, true }, { "square", 6, true },
    { "station", 7, false }, { "stravenue", 9, false }, { "stream", 6, false },
    { "st", 2, false }, { "st.", 3, false }, { "street", 6, true },
    { "summit", 6, false }, { "speedway", 8, false },
    { "terrace", 7, false }, { "throughway", 10, false }, { "trace", 5, false },
    { "track", 5, false }, { "trafficway", 10, false }, { "trail", 5, false },
    { "tunnel", 6, false }, { "turnpike", 8, false },
    { "underpass", 9, false }, { "union", 5, true },
    { "valley", 6, true }, { "viaduct", 7, false }, { "view", 4, true },
    { "village", 7, true }, { "ville", 5, false }, { "vista", 5, false },
    { "walk", 4, true }, { "wall", 4, false }, { "way", 3, true },
    { "well", 4, true },
    { "xing", 4, false }, { "xrd", 3, false }
  };

  // Accumulative number of location names for each starting letter.
  static const int location_names_accumulative[25] = {
      0,   7,  19,  40,  44,
     47,  57,  62,  68,  71,
     72,  74,  83,  92,  93,
     96, 109, 109, 121, 135,
    143, 145, 151, 155, 157
  };

  DCHECK_EQ(
      location_names_accumulative[arraysize(location_names_accumulative) - 1],
      static_cast<int>(ARRAYSIZE_UNSAFE(location_names)));

  if (!IsAsciiAlpha(*word.begin))
    return false;

  // No location names start with y, z.
  base::char16 first_letter = base::ToLowerASCII(*word.begin);
  if (first_letter > 'x')
    return false;

  DCHECK(first_letter >= 'a');
  int index = first_letter - 'a';
  int length = std::distance(word.begin, word.end);
  for (int i = location_names_accumulative[index];
      i < location_names_accumulative[index + 1]; ++i) {
    if (location_names[i].length != length &&
        (location_names[i].allow_plural &&
         location_names[i].length + 1 != length)) {
      continue;
    }

    if (LowerCaseEqualsASCIIWithPlural(word.begin, word.end,
                                       location_names[i].string,
                                       location_names[i].allow_plural)) {
      return true;
    }
  }

  return false;
}

} // namespace internal

} // namespace address_parser

}  // namespace content

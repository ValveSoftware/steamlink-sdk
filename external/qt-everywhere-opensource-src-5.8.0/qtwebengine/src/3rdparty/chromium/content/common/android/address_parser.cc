// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/address_parser.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/common/android/address_parser_internal.h"

namespace {

// Minimum number of words in an address after the house number
// before a state is expected to be found.
// A value too high can miss short addresses.
const size_t kMinAddressWords = 3;

// Maximum number of words allowed in an address between the house number
// and the state, both not included.
const size_t kMaxAddressWords = 12;

// Maximum number of lines allowed in an address between the house number
// and the state, both not included.
const size_t kMaxAddressLines = 5;

// Maximum length allowed for any address word between the house number
// and the state, both not included.
const size_t kMaxAddressNameWordLength = 25;

// Maximum number of words after the house number in which the location name
// should be found.
const size_t kMaxLocationNameDistance = 4;

// Additional characters used as new line delimiters.
const base::char16 kNewlineDelimiters[] = {
  '\n',
  ',',
  '*',
  0x2022,  // Unicode bullet
  0,
};

}  // anonymous namespace

namespace content {

namespace address_parser {

using namespace internal;

bool FindAddress(const base::string16& text, base::string16* address) {
  size_t start, end;
  if (FindAddress(text.begin(), text.end(), &start, &end)) {
    size_t len = end >= start ? end - start : 0;
    address->assign(text.substr(start, len));
    return true;
  }
  return false;
}

bool FindAddress(const base::string16::const_iterator& begin,
                 const base::string16::const_iterator& end,
                 size_t* start_pos,
                 size_t* end_pos) {
  HouseNumberParser house_number_parser;

  // Keep going through the input string until a potential house number is
  // detected. Start tokenizing the following words to find a valid
  // street name within a word range. Then, find a state name followed
  // by a valid zip code for that state. Also keep a look for any other
  // possible house numbers to continue from in case of no match and for
  // state names not followed by a zip code (e.g. New York, NY 10000).
  const base::string16 newline_delimiters = kNewlineDelimiters;
  const base::string16 delimiters = base::kWhitespaceUTF16 + newline_delimiters;
  for (base::string16::const_iterator it = begin; it != end; ) {
    Word house_number;
    if (!house_number_parser.Parse(it, end, &house_number))
      return false;

    String16Tokenizer tokenizer(house_number.end, end, delimiters);
    tokenizer.set_options(String16Tokenizer::RETURN_DELIMS);

    WordList words;
    words.push_back(house_number);

    bool found_location_name = false;
    bool continue_on_house_number = true;
    bool consecutive_house_numbers = true;
    size_t next_house_number_word = 0;
    size_t num_lines = 1;

    // Don't include the house number in the word count.
    size_t next_word = 1;
    for (; next_word <= kMaxAddressWords + 1; ++next_word) {

      // Extract a new word from the tokenizer.
      if (next_word == words.size()) {
        do {
          if (!tokenizer.GetNext())
            return false;

          // Check the number of address lines.
          if (tokenizer.token_is_delim() && newline_delimiters.find(
              *tokenizer.token_begin()) != base::string16::npos) {
            ++num_lines;
          }
        } while (tokenizer.token_is_delim());

        if (num_lines > kMaxAddressLines)
          break;

        words.push_back(Word(tokenizer.token_begin(), tokenizer.token_end()));
      }

      // Check the word length. If too long, don't try to continue from
      // the next house number as no address can hold this word.
      const Word& current_word = words[next_word];
      DCHECK_GT(std::distance(current_word.begin, current_word.end), 0);
      size_t current_word_length = std::distance(
          current_word.begin, current_word.end);
      if (current_word_length > kMaxAddressNameWordLength) {
        continue_on_house_number = false;
        break;
      }

      // Check if the new word is a valid house number.
      if (house_number_parser.Parse(current_word.begin, current_word.end,
          NULL)) {
        // Increase the number of consecutive house numbers since the beginning.
        if (consecutive_house_numbers) {
          // Check if there is a new line between consecutive house numbers.
          // This avoids false positives of the form "Cafe 21\n 750 Fifth Ave.."
          if (num_lines > 1) {
            next_house_number_word = next_word;
            break;
          }
        }

        // Keep the next candidate to resume parsing from in case of failure.
        if (next_house_number_word == 0) {
          next_house_number_word = next_word;
          continue;
        }
      } else {
        consecutive_house_numbers = false;
      }

      // Look for location names in the words after the house number.
      // A range limitation is introduced to avoid matching
      // anything that starts with a number before a legitimate address.
      if (next_word <= kMaxLocationNameDistance &&
          IsValidLocationName(current_word)) {
        found_location_name = true;
        continue;
      }

      // Don't count the house number.
      if (next_word > kMinAddressWords) {
        // Looking for the state is likely to add new words to the list while
        // checking for multi-word state names.
        size_t state_first_word = next_word;
        size_t state_last_word, state_index;
        if (FindStateStartingInWord(&words, state_first_word, &state_last_word,
                                    &tokenizer, &state_index)) {

          // A location name should have been found at this point.
          if (!found_location_name)
            break;

          // Explicitly exclude "et al", as "al" is a valid state code.
          if (current_word_length == 2 && words.size() > 2) {
            const Word& previous_word = words[state_first_word - 1];
            if (previous_word.end - previous_word.begin == 2 &&
                base::LowerCaseEqualsASCII(
                    base::StringPiece16(previous_word.begin, previous_word.end),
                    "et") &&
                base::LowerCaseEqualsASCII(
                     base::StringPiece16(current_word.begin, current_word.end),
                     "al"))
              break;
          }

          // Extract one more word from the tokenizer if not already available.
          size_t zip_word = state_last_word + 1;
          if (zip_word == words.size()) {
            do {
              if (!tokenizer.GetNext()) {
                // The address ends with a state name without a zip code. This
                // is legal according to WebView#findAddress public
                // documentation.
                *start_pos = words[0].begin - begin;
                *end_pos = words[state_last_word].end - begin;
                return true;
              }
            } while (tokenizer.token_is_delim());
            words.push_back(Word(tokenizer.token_begin(),
                            tokenizer.token_end()));
          }

          // Check the parsing validity and state range of the zip code.
          next_word = state_last_word;
          if (!IsZipValid(words[zip_word], state_index))
            continue;

          *start_pos = words[0].begin - begin;
          *end_pos = words[zip_word].end - begin;
          return true;
        }
      }
    }

    // Avoid skipping too many words because of a non-address number
    // at the beginning of the contents to parse.
    if (continue_on_house_number && next_house_number_word > 0) {
      it = words[next_house_number_word].begin;
    } else {
      DCHECK(!words.empty());
      next_word = std::min(next_word, words.size() - 1);
      it = words[next_word].end;
    }
  }

  return false;
}

}  // namespace address_parser

}  // namespace content

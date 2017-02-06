// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An object to store user feedback to a single spellcheck suggestion.
//
// Stores the spellcheck suggestion, its uint32_t hash identifier, and user's
// feedback. The feedback is indirect, in the sense that we record user's
// |action| instead of asking them how they feel about a spellcheck suggestion.
// The object can serialize itself.

#ifndef CHROME_BROWSER_SPELLCHECKER_MISSPELLING_H_
#define CHROME_BROWSER_SPELLCHECKER_MISSPELLING_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/spellchecker/spellcheck_action.h"

// Stores user feedback to a spellcheck suggestion. Sample usage:
//    Misspelling misspelling.
//    misspelling.context = base::ASCIIToUTF16("Helllo world");
//    misspelling.location = 0;
//    misspelling.length = 6;
//    misspelling.suggestions =
//        std::vector<base::string16>(1, base::ASCIIToUTF16("Hello"));
//    misspelling.hash = GenerateRandomHash();
//    misspelling.action.set_type(SpellcheckAction::TYPE_SELECT);
//    misspelling.action.set_index(0);
//    Process(SerializeMisspelling(misspelling));
struct Misspelling {
  Misspelling();
  Misspelling(const base::string16& context,
              size_t location,
              size_t length,
              const std::vector<base::string16>& suggestions,
              uint32_t hash);
  Misspelling(const Misspelling& other);
  ~Misspelling();

  // A several-word text snippet that immediately surrounds the misspelling.
  base::string16 context;

  // The number of characters between the beginning of |context| and the first
  // misspelled character.
  size_t location;

  // The number of characters in the misspelling.
  size_t length;

  // Spelling suggestions.
  std::vector<base::string16> suggestions;

  // The hash that identifies the misspelling.
  uint32_t hash;

  // User action.
  SpellcheckAction action;

  // The time when the user applied the action.
  base::Time timestamp;
};

// Serializes the data in this object into a dictionary value.
std::unique_ptr<base::DictionaryValue> SerializeMisspelling(
    const Misspelling& misspelling);

// Returns the substring of |context| that begins at |location| and contains
// |length| characters.
base::string16 GetMisspelledString(const Misspelling& misspelling);

// Returns the approximate size of the misspelling when serialized.
size_t ApproximateSerializedSize(const Misspelling& misspelling);

#endif  // CHROME_BROWSER_SPELLCHECKER_MISSPELLING_H_

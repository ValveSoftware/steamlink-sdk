// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_ACTION_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_ACTION_H_

#include "base/strings/string16.h"

namespace base {
class DictionaryValue;
}

// User's action on a misspelled word.
class SpellcheckAction {
 public:
  // Type of spellcheck action.
  enum SpellcheckActionType {
    // User added the word to the dictionary and cannot take more actions on
    // this misspelling.
    TYPE_ADD_TO_DICT,
    // User took a look at the suggestions in the context menu, but did not
    // select any suggestions. The user cannot take any more actions on the
    // misspelling, because it has been deleted from the web page.
    TYPE_IGNORE,
    // The misspelling is in user's custom spellcheck dictionary. The user will
    // not see spellcheck suggestions for this misspelling.
    TYPE_IN_DICTIONARY,
    // The user manually corrected the word to |value|. The user cannot take
    // more actions on this misspelling.
    TYPE_MANUALLY_CORRECTED,
    // The user has taken no action on the misspelling and will not take any
    // more actions, because the misspelled text has been removed from the web
    // page.
    TYPE_NO_ACTION,
    // The user has taken no action on the misspelled yet, but might take an
    // action in the future, because the misspelling is still on the web page.
    TYPE_PENDING,
    // User took a look at the suggestions in the context menu, but did not
    // select any suggestions. The user still can take further actions on the
    // misspelling.
    TYPE_PENDING_IGNORE,
    // The user has selected the suggestion at |index| and cannot take more
    // actions on this misspelling.
    TYPE_SELECT,
  };

  SpellcheckAction();
  SpellcheckAction(SpellcheckActionType type, int index, base::string16 value);
  ~SpellcheckAction();

  // Returns true if the action is final and should be sent to the feedback
  // server. Otherwise returns false.
  bool IsFinal() const;

  // Makes this action final and ready to be sent to the feedback server. The
  // method is idempotent. Finalizing an action that is already final does
  // nothing.
  void Finalize();

  // Serializes the data in this object into a dictionary value. The caller owns
  // the result.
  base::DictionaryValue* Serialize() const;

  void set_type(SpellcheckActionType type) { type_ = type; }
  void set_index(int index) { index_ = index; }
  void set_value(const base::string16& value) { value_ = value; }

  SpellcheckActionType type() const { return type_; }

 private:
  // User action.
  SpellcheckActionType type_;

  // The index for the user action, if applicable.
  int index_;

  // The value for the user action, if applicable.
  base::string16 value_;
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_ACTION_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SCANNER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SCANNER_H_

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"

namespace autofill {

class AutofillField;

// A helper class for parsing a stream of |AutofillField|'s with lookahead.
class AutofillScanner {
 public:
  explicit AutofillScanner(const std::vector<AutofillField*>& fields);
  ~AutofillScanner();

  // Advances the cursor by one step, if possible.
  void Advance();

  // Returns the current field in the stream, or |NULL| if there are no more
  // fields in the stream.
  AutofillField* Cursor() const;

  // Returns |true| if the cursor has reached the end of the stream.
  bool IsEnd() const;

  // Restores the most recently saved cursor. See also |SaveCursor()|.
  void Rewind();

  // Repositions the cursor to the specified |index|. See also |SaveCursor()|.
  void RewindTo(size_t index);

  // Saves and returns the current cursor position. See also |Rewind()| and
  // |RewindTo()|.
  size_t SaveCursor();

 private:
  // Indicates the current position in the stream, represented as a vector.
  std::vector<AutofillField*>::const_iterator cursor_;

  // The most recently saved cursor.
  std::vector<AutofillField*>::const_iterator saved_cursor_;

  // The beginning pointer for the stream.
  const std::vector<AutofillField*>::const_iterator begin_;

  // The past-the-end pointer for the stream.
  const std::vector<AutofillField*>::const_iterator end_;

  DISALLOW_COPY_AND_ASSIGN(AutofillScanner);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SCANNER_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DETAIL_INPUT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DETAIL_INPUT_H_

#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// This struct describes a single input control for the imperative autocomplete
// dialog.
struct DetailInput {
  enum Length {
    SHORT,      // Shares a line with other short inputs, like display: inline.
    SHORT_EOL,  // Like SHORT but starts a new line directly afterward. Used to
                // separate groups of short inputs into different lines.
    LONG,       // Will be given its own full line, like display: block.
    NONE,       // Input will not be shown.
  };

  // Returns whether this input can spread across multiple lines.
  bool IsMultiline() const;

  // Used to determine which inputs share lines when laying out.
  Length length;

  ServerFieldType type;

  // Text shown when the input is at its default state (e.g. empty).
  base::string16 placeholder_text;

  // A number between 0 and 1.0 that describes how much of the horizontal space
  // in the row should be allotted to this input. 0 is equivalent to 1.
  float expand_weight;

  // When non-empty, indicates the starting value for this input. This will be
  // used when the user is editing existing data.
  base::string16 initial_value;
};

typedef std::vector<DetailInput> DetailInputs;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DETAIL_INPUT_H_

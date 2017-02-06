// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_UTIL_H_

#include <stddef.h>

#include "base/strings/string16.h"

namespace autofill {

// Returns true when command line switch |kEnableSuggestionsWithSubstringMatch|
// is on.
bool IsFeatureSubstringMatchEnabled();

// Returns true when keyboard accessory is enabled.
bool IsKeyboardAccessoryEnabled();

// A token is a sequences of contiguous characters separated by any of the
// characters that are part of delimiter set {' ', '.', ',', '-', '_', '@'}.

// Returns true if the |field_contents| is a substring of the |suggestion|
// starting at token boundaries. |field_contents| can span multiple |suggestion|
// tokens.
bool FieldIsSuggestionSubstringStartingOnTokenBoundary(
    const base::string16& suggestion,
    const base::string16& field_contents,
    bool case_sensitive);

// Finds the first occurrence of a searched substring |field_contents| within
// the string |suggestion| starting at token boundaries and returns the index to
// the end of the located substring, or base::string16::npos if the substring is
// not found. "preview-on-hover" feature is one such use case where the
// substring |field_contents| may not be found within the string |suggestion|.
size_t GetTextSelectionStart(const base::string16& suggestion,
                             const base::string16& field_contents,
                             bool case_sensitive);

// Returns true if running on a desktop platform. Any platform that is not
// Android or iOS is considered desktop.
bool IsDesktopPlatform();

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_UTIL_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for dealing with pluralization of
// localized content.

#ifndef UI_BASE_L10N_L10N_UTIL_PLURALS_H_
#define UI_BASE_L10N_L10N_UTIL_PLURALS_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "third_party/icu/source/i18n/unicode/plurfmt.h"
#include "third_party/icu/source/i18n/unicode/plurrule.h"

namespace l10n_util {

// Returns a PluralRules for the current locale.
scoped_ptr<icu::PluralRules> BuildPluralRules();

// Returns a PluralFormat from |message_ids|.  |message_ids| must be size 6 and
// in order: default, singular, zero, two, few, many.
scoped_ptr<icu::PluralFormat> BuildPluralFormat(
    const std::vector<int>& message_ids);

}  // namespace l10n_util

#endif  // UI_BASE_L10N_L10N_UTIL_PLURALS_H_

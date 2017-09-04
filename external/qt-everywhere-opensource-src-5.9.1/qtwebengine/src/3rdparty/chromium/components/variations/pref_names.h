// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_PREF_NAMES_H_
#define COMPONENTS_VARIATIONS_PREF_NAMES_H_

namespace variations {
namespace prefs {

// Alphabetical list of preference names specific to the variations component.
// Keep alphabetized, and document each in the .cc file.

extern const char kVariationsCompressedSeed[];
extern const char kVariationsLastFetchTime[];
extern const char kVariationsPermanentConsistencyCountry[];
extern const char kVariationsPermutedEntropyCache[];
extern const char kVariationsCountry[];
extern const char kVariationsRestrictParameter[];
extern const char kVariationsSeed[];
extern const char kVariationsSeedDate[];
extern const char kVariationsSeedSignature[];

}  // namespace prefs
}  // namespace metrics

#endif  // COMPONENTS_VARIATIONS_PREF_NAMES_H_

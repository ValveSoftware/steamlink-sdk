// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebDistillability_h
#define WebDistillability_h

namespace blink {

struct WebDistillabilityFeatures {
  bool isMobileFriendly;
  // The rest of the fields are only valid when isMobileFriendly==false.
  bool openGraph;
  unsigned elementCount;
  unsigned anchorCount;
  unsigned formCount;
  unsigned textInputCount;
  unsigned passwordInputCount;
  unsigned pCount;
  unsigned preCount;
  // The following scores are derived from the triggering logic in Readability
  // from Mozilla.
  // https://github.com/mozilla/readability/blob/85101066386a0872526a6c4ae164c18fcd6cc1db/Readability.js#L1704
  double mozScore;
  double mozScoreAllSqrt;
  double mozScoreAllLinear;
};

}  // namespace blink

#endif  // WebDistillability_h

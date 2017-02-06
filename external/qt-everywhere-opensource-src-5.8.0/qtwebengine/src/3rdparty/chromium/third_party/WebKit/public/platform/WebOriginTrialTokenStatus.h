// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebOriginTrialTokenStatus_h
#define WebOriginTrialTokenStatus_h

namespace blink {

// The enum entries below are written to histograms and thus cannot be deleted
// or reordered.
// New entries must be added immediately before the end.
enum class WebOriginTrialTokenStatus {
    Success = 0,
    NotSupported = 1,
    Insecure = 2,
    NoTokens = 3,
    Expired = 4,
    WrongFeature = 5,
    WrongOrigin = 6,
    InvalidSignature = 7,
    Malformed = 8,
    WrongVersion = 9,
    FeatureDisabled = 10,
    Last = FeatureDisabled
};

} // namespace blink

#endif // WebOriginTrialTokenStatus_h

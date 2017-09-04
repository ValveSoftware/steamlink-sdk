// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/origin_trials/web_trial_token_validator_impl.h"

#include "content/common/origin_trials/trial_token_validator.h"
#include "third_party/WebKit/public/platform/WebOriginTrialTokenStatus.h"

namespace content {

WebTrialTokenValidatorImpl::WebTrialTokenValidatorImpl() {}
WebTrialTokenValidatorImpl::~WebTrialTokenValidatorImpl() {}

blink::WebOriginTrialTokenStatus WebTrialTokenValidatorImpl::validateToken(
    const blink::WebString& token,
    const blink::WebSecurityOrigin& origin,
    blink::WebString* feature_name) {
  std::string feature;
  blink::WebOriginTrialTokenStatus status =
      TrialTokenValidator::ValidateToken(token.utf8(), origin, &feature);
  if (status == blink::WebOriginTrialTokenStatus::Success)
    *feature_name = blink::WebString::fromUTF8(feature);
  return status;
}

}  // namespace content

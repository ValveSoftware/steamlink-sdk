// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ORIGIN_TRIALS_WEB_TRIAL_TOKEN_VALIDATOR_IMPL_H_
#define CONTENT_RENDERER_ORIGIN_TRIALS_WEB_TRIAL_TOKEN_VALIDATOR_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebTrialTokenValidator.h"

namespace content {

// The TrialTokenValidator is called by the Origin Trials Framework code in
// Blink to validate tokens to enable experimental features.
//
// This class is thread-safe.
class CONTENT_EXPORT WebTrialTokenValidatorImpl
    : public NON_EXPORTED_BASE(blink::WebTrialTokenValidator) {
 public:
  WebTrialTokenValidatorImpl();
  ~WebTrialTokenValidatorImpl() override;

  // blink::WebTrialTokenValidator implementation
  blink::WebOriginTrialTokenStatus validateToken(
      const blink::WebString& token,
      const blink::WebSecurityOrigin& origin,
      blink::WebString* feature_name) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebTrialTokenValidatorImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ORIGIN_TRIALS_WEB_TRIAL_TOKEN_VALIDATOR_IMPL_H_

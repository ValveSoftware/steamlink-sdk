// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer.h"

namespace content {

// static.
Referrer Referrer::SanitizeForRequest(const GURL& request,
                                      const Referrer& referrer) {
  Referrer sanitized_referrer(referrer.url.GetAsReferrer(), referrer.policy);
  if (sanitized_referrer.policy == blink::WebReferrerPolicyDefault) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kReducedReferrerGranularity)) {
      sanitized_referrer.policy =
          blink::WebReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin;
    } else {
      sanitized_referrer.policy =
          blink::WebReferrerPolicyNoReferrerWhenDowngrade;
    }
  }

  if (!request.SchemeIsHTTPOrHTTPS() ||
      !sanitized_referrer.url.SchemeIsValidForReferrer()) {
    sanitized_referrer.url = GURL();
    return sanitized_referrer;
  }

  bool is_downgrade = sanitized_referrer.url.SchemeIsCryptographic() &&
                      !request.SchemeIsCryptographic();

  if (sanitized_referrer.policy < 0 ||
      sanitized_referrer.policy > blink::WebReferrerPolicyLast) {
    NOTREACHED();
    sanitized_referrer.policy = blink::WebReferrerPolicyNever;
  }

  switch (sanitized_referrer.policy) {
    case blink::WebReferrerPolicyDefault:
      NOTREACHED();
      break;
    case blink::WebReferrerPolicyNoReferrerWhenDowngrade:
      if (is_downgrade)
        sanitized_referrer.url = GURL();
      break;
    case blink::WebReferrerPolicyAlways:
      break;
    case blink::WebReferrerPolicyNever:
      sanitized_referrer.url = GURL();
      break;
    case blink::WebReferrerPolicyOrigin:
      sanitized_referrer.url = sanitized_referrer.url.GetOrigin();
      break;
    case blink::WebReferrerPolicyOriginWhenCrossOrigin:
      if (request.GetOrigin() != sanitized_referrer.url.GetOrigin())
        sanitized_referrer.url = sanitized_referrer.url.GetOrigin();
      break;
    case blink::WebReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin:
      if (is_downgrade) {
        sanitized_referrer.url = GURL();
      } else if (request.GetOrigin() != sanitized_referrer.url.GetOrigin()) {
        sanitized_referrer.url = sanitized_referrer.url.GetOrigin();
      }
      break;
  }
  return sanitized_referrer;
}

}  // namespace content

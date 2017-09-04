// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReferrerPolicyEnumTraits_h
#define ReferrerPolicyEnumTraits_h

#include "base/logging.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "third_party/WebKit/public/platform/referrer.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<::blink::mojom::ReferrerPolicy, ::blink::WebReferrerPolicy> {
  static ::blink::mojom::ReferrerPolicy ToMojom(
      ::blink::WebReferrerPolicy policy) {
    switch (policy) {
      case ::blink::WebReferrerPolicyAlways:
        return ::blink::mojom::ReferrerPolicy::ALWAYS;
      case ::blink::WebReferrerPolicyDefault:
        return ::blink::mojom::ReferrerPolicy::DEFAULT;
      case ::blink::WebReferrerPolicyNoReferrerWhenDowngrade:
        return ::blink::mojom::ReferrerPolicy::NO_REFERRER_WHEN_DOWNGRADE;
      case ::blink::WebReferrerPolicyNever:
        return ::blink::mojom::ReferrerPolicy::NEVER;
      case ::blink::WebReferrerPolicyOrigin:
        return ::blink::mojom::ReferrerPolicy::ORIGIN;
      case ::blink::WebReferrerPolicyOriginWhenCrossOrigin:
        return ::blink::mojom::ReferrerPolicy::ORIGIN_WHEN_CROSS_ORIGIN;
      case ::blink::
          WebReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin:
        return ::blink::mojom::ReferrerPolicy::
            NO_REFERRER_WHEN_DOWNGRADE_ORIGIN_WHEN_CROSS_ORIGIN;
      default:
        NOTREACHED();
        return ::blink::mojom::ReferrerPolicy::DEFAULT;
    }
  }

  static bool FromMojom(::blink::mojom::ReferrerPolicy policy,
                        ::blink::WebReferrerPolicy* out) {
    switch (policy) {
      case ::blink::mojom::ReferrerPolicy::ALWAYS:
        *out = ::blink::WebReferrerPolicyAlways;
        return true;
      case ::blink::mojom::ReferrerPolicy::DEFAULT:
        *out = ::blink::WebReferrerPolicyDefault;
        return true;
      case ::blink::mojom::ReferrerPolicy::NO_REFERRER_WHEN_DOWNGRADE:
        *out = ::blink::WebReferrerPolicyNoReferrerWhenDowngrade;
        return true;
      case ::blink::mojom::ReferrerPolicy::NEVER:
        *out = ::blink::WebReferrerPolicyNever;
        return true;
      case ::blink::mojom::ReferrerPolicy::ORIGIN:
        *out = ::blink::WebReferrerPolicyOrigin;
        return true;
      case ::blink::mojom::ReferrerPolicy::ORIGIN_WHEN_CROSS_ORIGIN:
        *out = ::blink::WebReferrerPolicyOriginWhenCrossOrigin;
        return true;
      case ::blink::mojom::ReferrerPolicy::
          NO_REFERRER_WHEN_DOWNGRADE_ORIGIN_WHEN_CROSS_ORIGIN:
        *out = ::blink::
            WebReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin;
        return true;
      default:
        NOTREACHED();
        return false;
    }
  }
};

}  // namespace mojo

#endif

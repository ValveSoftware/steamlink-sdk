// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/security_style_explanations.h"

namespace content {

SecurityStyleExplanations::SecurityStyleExplanations()
    : ran_mixed_content(false),
      displayed_mixed_content(false),
      ran_content_with_cert_errors(false),
      displayed_content_with_cert_errors(false),
      ran_insecure_content_style(blink::WebSecurityStyleUnknown),
      displayed_insecure_content_style(blink::WebSecurityStyleUnknown),
      scheme_is_cryptographic(false),
      pkp_bypassed(false) {}

SecurityStyleExplanations::~SecurityStyleExplanations() {
}

}  // namespace content

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/security_style_explanations.h"

namespace content {

SecurityStyleExplanations::SecurityStyleExplanations()
    : ran_insecure_content(false),
      displayed_insecure_content(false),
      ran_insecure_content_style(SECURITY_STYLE_UNKNOWN),
      displayed_insecure_content_style(SECURITY_STYLE_UNKNOWN),
      scheme_is_cryptographic(false),
      pkp_bypassed(false) {}

SecurityStyleExplanations::~SecurityStyleExplanations() {
}

}  // namespace content

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/test_password_autofill_agent.h"

namespace autofill {

TestPasswordAutofillAgent::TestPasswordAutofillAgent(
    content::RenderFrame* render_frame)
    : PasswordAutofillAgent(render_frame) {
}

TestPasswordAutofillAgent::~TestPasswordAutofillAgent() {}

bool TestPasswordAutofillAgent::OriginCanAccessPasswordManager(
    const blink::WebSecurityOrigin& origin) {
  return true;
}

}  // namespace autofill

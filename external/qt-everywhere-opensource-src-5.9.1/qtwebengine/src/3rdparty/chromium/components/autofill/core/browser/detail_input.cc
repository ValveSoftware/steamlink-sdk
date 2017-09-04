// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/detail_input.h"

namespace autofill {

// Street address is multi-line, except in countries where it shares a line
// with other inputs (such as Coite d'Ivoire).
bool DetailInput::IsMultiline() const {
  return (type == ADDRESS_HOME_STREET_ADDRESS ||
          type == ADDRESS_BILLING_STREET_ADDRESS) &&
         length == DetailInput::LONG;
}

}  // namespace autofill

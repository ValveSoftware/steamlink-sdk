// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_METRO_DRIVER_IME_INPUT_SCOPE_H_
#define WIN8_METRO_DRIVER_IME_INPUT_SCOPE_H_

#include <InputScope.h>
#include <vector>

#include "base/win/scoped_comptr.h"

namespace metro_driver {

// Returns an instance of ITfInputScope object, which represents an array of
// InputScope enumeration passed as |input_scopes|.
base::win::ScopedComPtr<ITfInputScope>
CreteInputScope(const std::vector<InputScope>& input_scopes);

}  // namespace metro_driver

#endif  // WIN8_METRO_DRIVER_IME_TEXT_STORE_H_

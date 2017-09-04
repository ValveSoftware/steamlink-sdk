// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_RESULT_H_
#define BLIMP_HELIUM_RESULT_H_

#include "blimp/helium/blimp_helium_export.h"

namespace blimp {
namespace helium {

// Defines the canonical list of Helium result codes.
// Result::OK is the only non-error code.
// All other codes are considered errors and are prefixed by ERR_.
// See error_list.h for the unprefixed list of error codes.
//
// (Approach is inspired by net/base/net_errors.h)
enum Result {
  SUCCESS,
#define HELIUM_ERROR(label, value) ERR_##label = value,
#include "blimp/helium/errors.h"
#undef HELIUM_ERROR
};

// Gets a human-readable string representation of |result|.
BLIMP_HELIUM_EXPORT const char* ResultToString(Result result);

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_RESULT_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/result.h"

#include "base/logging.h"

namespace blimp {
namespace helium {

const char* ResultToString(Result result) {
  switch (result) {
    case Result::SUCCESS:
      return "SUCCESS";
      break;
#define HELIUM_ERROR(label, value) \
  case Result::ERR_##label:        \
    return "ERR_" #label;
      break;
#include "blimp/helium/errors.h"
#undef HELIUM_ERROR
  }

  NOTREACHED();
  return "";
}

}  // namespace helium
}  // namespace blimp

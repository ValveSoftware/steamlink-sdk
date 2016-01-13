// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_RESULT_CODES_H_
#define CONTENT_PUBLIC_COMMON_RESULT_CODES_H_

namespace content {

enum ResultCode {

#define RESULT_CODE(label, value) RESULT_CODE_ ## label = value,
#include "content/public/common/result_codes_list.h"
#undef RESULT_CODE

  // Last return code (keep this last).
  RESULT_CODE_LAST_CODE
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RESULT_CODES_H_

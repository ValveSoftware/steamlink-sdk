// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_COMMON_H_
#define CONTENT_RENDERER_PEPPER_COMMON_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_var.h"

namespace content {

inline PP_Bool BoolToPPBool(bool value) { return value ? PP_TRUE : PP_FALSE; }

inline bool PPBoolToBool(PP_Bool value) { return (PP_TRUE == value); }

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_COMMON_H_

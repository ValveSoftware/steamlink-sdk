// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_PEPPER_CONTAINER_APP_THUNK_H_
#define MOJO_EXAMPLES_PEPPER_CONTAINER_APP_THUNK_H_

#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_opengles2.h"

namespace mojo {
namespace examples {

const PPB_Core_1_0* GetPPB_Core_1_0_Thunk();
const PPB_OpenGLES2* GetPPB_OpenGLES2_Thunk();

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_PEPPER_CONTAINER_APP_THUNK_H_

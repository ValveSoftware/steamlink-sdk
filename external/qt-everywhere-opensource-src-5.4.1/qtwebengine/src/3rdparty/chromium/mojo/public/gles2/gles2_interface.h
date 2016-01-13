// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_GLES2_GLES2_INTERFACE_H_
#define MOJO_PUBLIC_GLES2_GLES2_INTERFACE_H_

#include <GLES2/gl2.h>

namespace mojo {

class GLES2Interface {
 public:
  virtual ~GLES2Interface() {}
#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS) \
  virtual ReturnType Function PARAMETERS = 0;
#include "mojo/public/c/gles2/gles2_call_visitor_autogen.h"
#undef VISIT_GL_CALL
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_GLES2_GLES2_INTERFACE_H_

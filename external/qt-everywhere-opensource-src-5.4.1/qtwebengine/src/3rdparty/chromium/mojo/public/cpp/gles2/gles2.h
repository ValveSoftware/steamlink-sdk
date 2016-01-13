// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_GLES2_GLES2_H_
#define MOJO_PUBLIC_CPP_GLES2_GLES2_H_

#include "mojo/public/c/gles2/gles2.h"
#include "mojo/public/cpp/environment/environment.h"

namespace mojo {

class GLES2Initializer {
 public:
  explicit GLES2Initializer(const MojoAsyncWaiter* async_waiter =
                                Environment::GetDefaultAsyncWaiter()) {
    MojoGLES2Initialize(async_waiter);
  }
  ~GLES2Initializer() { MojoGLES2Terminate(); }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_GLES2_GLES2_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_context_stub.h"

#include "ui/gl/gl_gl_api_implementation.h"

namespace gl {

GLContextStub::GLContextStub() : GLContextReal(nullptr) {}
GLContextStub::GLContextStub(GLShareGroup* share_group)
    : GLContextReal(share_group) {}

bool GLContextStub::Initialize(GLSurface* compatible_surface,
                               const GLContextAttribs& attribs) {
  return true;
}

bool GLContextStub::MakeCurrent(GLSurface* surface) {
  SetGLToStubGLApi();
  SetCurrent(surface);
  InitializeDynamicBindings();
  return true;
}

void GLContextStub::ReleaseCurrent(GLSurface* surface) {
  SetCurrent(nullptr);
}

bool GLContextStub::IsCurrent(GLSurface* surface) {
  return GetRealCurrent() == this;
}

void* GLContextStub::GetHandle() {
  return nullptr;
}

void GLContextStub::OnSetSwapInterval(int interval) {
}

std::string GLContextStub::GetGLRenderer() {
  return std::string("CHROMIUM");
}

GLContextStub::~GLContextStub() {}

}  // namespace gl

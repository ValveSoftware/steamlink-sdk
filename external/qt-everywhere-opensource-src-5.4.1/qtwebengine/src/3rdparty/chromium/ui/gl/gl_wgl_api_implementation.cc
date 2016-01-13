// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_wgl_api_implementation.h"
#include "ui/gl/gl_implementation.h"

namespace gfx {

RealWGLApi* g_real_wgl;

void InitializeStaticGLBindingsWGL() {
  g_driver_wgl.InitializeStaticBindings();
  if (!g_real_wgl) {
    g_real_wgl = new RealWGLApi();
  }
  g_real_wgl->Initialize(&g_driver_wgl);
  g_current_wgl_context = g_real_wgl;
}

void InitializeDynamicGLBindingsWGL(GLContext* context) {
  g_driver_wgl.InitializeDynamicBindings(context);
}

void InitializeDebugGLBindingsWGL() {
  g_driver_wgl.InitializeDebugBindings();
}

void ClearGLBindingsWGL() {
  if (g_real_wgl) {
    delete g_real_wgl;
    g_real_wgl = NULL;
  }
  g_current_wgl_context = NULL;
  g_driver_wgl.ClearBindings();
}

WGLApi::WGLApi() {
}

WGLApi::~WGLApi() {
}

WGLApiBase::WGLApiBase()
    : driver_(NULL) {
}

WGLApiBase::~WGLApiBase() {
}

void WGLApiBase::InitializeBase(DriverWGL* driver) {
  driver_ = driver;
}

RealWGLApi::RealWGLApi() {
}

RealWGLApi::~RealWGLApi() {
}

void RealWGLApi::Initialize(DriverWGL* driver) {
  InitializeBase(driver);
}

TraceWGLApi::~TraceWGLApi() {
}

bool GetGLWindowSystemBindingInfoWGL(GLWindowSystemBindingInfo* info) {
  const char* extensions = wglGetExtensionsStringEXT();
  *info = GLWindowSystemBindingInfo();
  if (extensions)
    info->extensions = extensions;
  return true;
}

}  // namespace gfx



// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread_local.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_version_info.h"

namespace gfx {

namespace {
base::LazyInstance<base::ThreadLocalPointer<GLContext> >::Leaky
    current_context_ = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::ThreadLocalPointer<GLContext> >::Leaky
    current_real_context_ = LAZY_INSTANCE_INITIALIZER;
}  // namespace

GLContext::ScopedReleaseCurrent::ScopedReleaseCurrent() : canceled_(false) {}

GLContext::ScopedReleaseCurrent::~ScopedReleaseCurrent() {
  if (!canceled_ && GetCurrent()) {
    GetCurrent()->ReleaseCurrent(NULL);
  }
}

void GLContext::ScopedReleaseCurrent::Cancel() {
  canceled_ = true;
}

GLContext::FlushEvent::FlushEvent() {
}

GLContext::FlushEvent::~FlushEvent() {
}

void GLContext::FlushEvent::Signal() {
  flag_.Set();
}

bool GLContext::FlushEvent::IsSignaled() {
  return flag_.IsSet();
}

GLContext::GLContext(GLShareGroup* share_group) : share_group_(share_group) {
  if (!share_group_.get())
    share_group_ = new GLShareGroup;

  share_group_->AddContext(this);
}

GLContext::~GLContext() {
  share_group_->RemoveContext(this);
  if (GetCurrent() == this) {
    SetCurrent(NULL);
  }
}

scoped_refptr<GLContext::FlushEvent> GLContext::SignalFlush() {
  DCHECK(IsCurrent(NULL));
  scoped_refptr<FlushEvent> flush_event = new FlushEvent();
  flush_events_.push_back(flush_event);
  return flush_event;
}

bool GLContext::GetTotalGpuMemory(size_t* bytes) {
  DCHECK(bytes);
  *bytes = 0;
  return false;
}

void GLContext::SetSafeToForceGpuSwitch() {
}

void GLContext::SetUnbindFboOnMakeCurrent() {
  NOTIMPLEMENTED();
}

std::string GLContext::GetExtensions() {
  DCHECK(IsCurrent(NULL));
  const char* ext = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
  return std::string(ext ? ext : "");
}

std::string GLContext::GetGLVersion() {
  DCHECK(IsCurrent(NULL));
  const char *version =
      reinterpret_cast<const char*>(glGetString(GL_VERSION));
  return std::string(version ? version : "");
}

std::string GLContext::GetGLRenderer() {
  DCHECK(IsCurrent(NULL));
  const char *renderer =
      reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  return std::string(renderer ? renderer : "");
}

bool GLContext::HasExtension(const char* name) {
  std::string extensions = GetExtensions();
  extensions += " ";

  std::string delimited_name(name);
  delimited_name += " ";

  return extensions.find(delimited_name) != std::string::npos;
}

const GLVersionInfo* GLContext::GetVersionInfo() {
  if(!version_info_) {
    std::string version = GetGLVersion();
    std::string renderer = GetGLRenderer();
    version_info_ = scoped_ptr<GLVersionInfo>(
        new GLVersionInfo(version.c_str(), renderer.c_str()));
  }
  return version_info_.get();
}

GLShareGroup* GLContext::share_group() {
  return share_group_.get();
}

bool GLContext::LosesAllContextsOnContextLost() {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
      return false;
    case kGLImplementationEGLGLES2:
      return true;
    case kGLImplementationOSMesaGL:
    case kGLImplementationAppleGL:
      return false;
    case kGLImplementationMockGL:
      return false;
    default:
      NOTREACHED();
      return true;
  }
}

GLContext* GLContext::GetCurrent() {
  return current_context_.Pointer()->Get();
}

GLContext* GLContext::GetRealCurrent() {
  return current_real_context_.Pointer()->Get();
}

void GLContext::SetCurrent(GLSurface* surface) {
  current_context_.Pointer()->Set(surface ? this : NULL);
  GLSurface::SetCurrent(surface);
  // Leave the real GL api current so that unit tests work correctly.
  // TODO(sievers): Remove this, but needs all gpu_unittest classes
  // to create and make current a context.
  if (!surface && GetGLImplementation() != kGLImplementationMockGL) {
    SetGLApiToNoContext();
  }
}

GLStateRestorer* GLContext::GetGLStateRestorer() {
  return state_restorer_.get();
}

void GLContext::SetGLStateRestorer(GLStateRestorer* state_restorer) {
  state_restorer_ = make_scoped_ptr(state_restorer);
}

bool GLContext::WasAllocatedUsingRobustnessExtension() {
  return false;
}

bool GLContext::InitializeDynamicBindings() {
  DCHECK(IsCurrent(NULL));
  static bool initialized = false;
  if (initialized)
    return initialized;
  initialized = InitializeDynamicGLBindings(GetGLImplementation(), this);
  if (!initialized)
    LOG(ERROR) << "Could not initialize dynamic bindings.";
  return initialized;
}

void GLContext::SetupForVirtualization() {
  if (!virtual_gl_api_) {
    virtual_gl_api_.reset(new VirtualGLApi());
    virtual_gl_api_->Initialize(&g_driver_gl, this);
  }
}

bool GLContext::MakeVirtuallyCurrent(
    GLContext* virtual_context, GLSurface* surface) {
  DCHECK(virtual_gl_api_);
  return virtual_gl_api_->MakeCurrent(virtual_context, surface);
}

void GLContext::OnReleaseVirtuallyCurrent(GLContext* virtual_context) {
  if (virtual_gl_api_)
    virtual_gl_api_->OnReleaseVirtuallyCurrent(virtual_context);
}

void GLContext::SetRealGLApi() {
  SetGLToRealGLApi();
}

void GLContext::OnFlush() {
  for (size_t n = 0; n < flush_events_.size(); n++)
    flush_events_[n]->Signal();
  flush_events_.clear();
}

GLContextReal::GLContextReal(GLShareGroup* share_group)
    : GLContext(share_group) {}

GLContextReal::~GLContextReal() {}

void GLContextReal::SetCurrent(GLSurface* surface) {
  GLContext::SetCurrent(surface);
  current_real_context_.Pointer()->Set(surface ? this : NULL);
}

}  // namespace gfx

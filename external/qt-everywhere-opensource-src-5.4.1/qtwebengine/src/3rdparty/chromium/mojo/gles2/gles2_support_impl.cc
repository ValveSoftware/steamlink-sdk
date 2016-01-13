// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/gles2/gles2_support_impl.h"

#include "base/lazy_instance.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "mojo/gles2/gles2_context.h"
#include "mojo/public/gles2/gles2_interface.h"
#include "mojo/public/gles2/gles2_private.h"

namespace mojo {
namespace gles2 {

namespace {

class GLES2ImplForCommandBuffer : public GLES2Interface {
 public:
  GLES2ImplForCommandBuffer() : gpu_interface_(NULL) {}

  void set_gpu_interface(gpu::gles2::GLES2Interface* gpu_interface) {
    gpu_interface_ = gpu_interface;
  }
  gpu::gles2::GLES2Interface* gpu_interface() const { return gpu_interface_; }

#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS) \
  virtual ReturnType Function PARAMETERS OVERRIDE {                \
    return gpu_interface_->Function ARGUMENTS;                     \
  }
#include "mojo/public/c/gles2/gles2_call_visitor_autogen.h"
#undef VISIT_GL_CALL

 private:
  gpu::gles2::GLES2Interface* gpu_interface_;
  DISALLOW_COPY_AND_ASSIGN(GLES2ImplForCommandBuffer);
};

base::LazyInstance<GLES2ImplForCommandBuffer> g_gles2_interface =
    LAZY_INSTANCE_INITIALIZER;

}  // anonymous namespace

GLES2SupportImpl::GLES2SupportImpl() : async_waiter_(NULL) {}
GLES2SupportImpl::~GLES2SupportImpl() {}

// static
void GLES2SupportImpl::Init() { GLES2Support::Init(new GLES2SupportImpl()); }

void GLES2SupportImpl::Initialize(const MojoAsyncWaiter* async_waiter) {
  DCHECK(!async_waiter_);
  DCHECK(async_waiter);
  async_waiter_ = async_waiter;
}

void GLES2SupportImpl::Terminate() {
  DCHECK(async_waiter_);
  async_waiter_ = NULL;
}

MojoGLES2Context GLES2SupportImpl::CreateContext(
    MessagePipeHandle handle,
    MojoGLES2ContextLost lost_callback,
    MojoGLES2DrawAnimationFrame animation_callback,
    void* closure) {
  ScopedMessagePipeHandle scoped_handle(handle);
  scoped_ptr<GLES2Context> client(new GLES2Context(async_waiter_,
                                                   scoped_handle.Pass(),
                                                   lost_callback,
                                                   animation_callback,
                                                   closure));
  if (!client->Initialize())
    client.reset();
  return client.release();
}

void GLES2SupportImpl::DestroyContext(MojoGLES2Context context) {
  delete static_cast<GLES2Context*>(context);
}

void GLES2SupportImpl::MakeCurrent(MojoGLES2Context context) {
  gpu::gles2::GLES2Interface* interface = NULL;
  if (context) {
    GLES2Context* client = static_cast<GLES2Context*>(context);
    interface = client->interface();
    DCHECK(interface);
  }
  g_gles2_interface.Get().set_gpu_interface(interface);
}

void GLES2SupportImpl::SwapBuffers() {
  g_gles2_interface.Get().gpu_interface()->SwapBuffers();
}

void GLES2SupportImpl::RequestAnimationFrames(MojoGLES2Context context) {
  static_cast<GLES2Context*>(context)->RequestAnimationFrames();
}

void GLES2SupportImpl::CancelAnimationFrames(MojoGLES2Context context) {
  static_cast<GLES2Context*>(context)->CancelAnimationFrames();
}

void* GLES2SupportImpl::GetGLES2Interface(MojoGLES2Context context) {
  return static_cast<GLES2Context*>(context)->interface();
}

void* GLES2SupportImpl::GetContextSupport(MojoGLES2Context context) {
  return static_cast<GLES2Context*>(context)->context_support();
}

GLES2Interface* GLES2SupportImpl::GetGLES2InterfaceForCurrentContext() {
  return &g_gles2_interface.Get();
}

}  // namespace gles2
}  // namespace mojo

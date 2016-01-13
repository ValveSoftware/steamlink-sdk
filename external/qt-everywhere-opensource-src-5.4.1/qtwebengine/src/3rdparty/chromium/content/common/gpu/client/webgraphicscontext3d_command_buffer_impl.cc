// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"

#include "third_party/khronos/GLES2/gl2.h"
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include "third_party/khronos/GLES2/gl2ext.h"

#include <algorithm>
#include <map>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/gles2_trace_implementation.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace content {

namespace {

static base::LazyInstance<base::Lock>::Leaky
    g_default_share_groups_lock = LAZY_INSTANCE_INITIALIZER;

typedef std::map<GpuChannelHost*,
    scoped_refptr<WebGraphicsContext3DCommandBufferImpl::ShareGroup> >
    ShareGroupMap;
static base::LazyInstance<ShareGroupMap> g_default_share_groups =
    LAZY_INSTANCE_INITIALIZER;

scoped_refptr<WebGraphicsContext3DCommandBufferImpl::ShareGroup>
    GetDefaultShareGroupForHost(GpuChannelHost* host) {
  base::AutoLock lock(g_default_share_groups_lock.Get());

  ShareGroupMap& share_groups = g_default_share_groups.Get();
  ShareGroupMap::iterator it = share_groups.find(host);
  if (it == share_groups.end()) {
    scoped_refptr<WebGraphicsContext3DCommandBufferImpl::ShareGroup> group =
        new WebGraphicsContext3DCommandBufferImpl::ShareGroup();
    share_groups[host] = group;
    return group;
  }
  return it->second;
}

// Singleton used to initialize and terminate the gles2 library.
class GLES2Initializer {
 public:
  GLES2Initializer() {
    gles2::Initialize();
  }

  ~GLES2Initializer() {
    gles2::Terminate();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GLES2Initializer);
};

////////////////////////////////////////////////////////////////////////////////

base::LazyInstance<GLES2Initializer> g_gles2_initializer =
    LAZY_INSTANCE_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////

} // namespace anonymous

WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits::SharedMemoryLimits()
    : command_buffer_size(kDefaultCommandBufferSize),
      start_transfer_buffer_size(kDefaultStartTransferBufferSize),
      min_transfer_buffer_size(kDefaultMinTransferBufferSize),
      max_transfer_buffer_size(kDefaultMaxTransferBufferSize),
      mapped_memory_reclaim_limit(gpu::gles2::GLES2Implementation::kNoLimit) {}

WebGraphicsContext3DCommandBufferImpl::ShareGroup::ShareGroup() {
}

WebGraphicsContext3DCommandBufferImpl::ShareGroup::~ShareGroup() {
  DCHECK(contexts_.empty());
}

WebGraphicsContext3DCommandBufferImpl::WebGraphicsContext3DCommandBufferImpl(
    int surface_id,
    const GURL& active_url,
    GpuChannelHost* host,
    const Attributes& attributes,
    bool lose_context_when_out_of_memory,
    const SharedMemoryLimits& limits,
    WebGraphicsContext3DCommandBufferImpl* share_context)
    : lose_context_when_out_of_memory_(lose_context_when_out_of_memory),
      attributes_(attributes),
      visible_(false),
      host_(host),
      surface_id_(surface_id),
      active_url_(active_url),
      gpu_preference_(attributes.preferDiscreteGPU ? gfx::PreferDiscreteGpu
                                                   : gfx::PreferIntegratedGpu),
      weak_ptr_factory_(this),
      mem_limits_(limits) {
  if (share_context) {
    DCHECK(!attributes_.shareResources);
    share_group_ = share_context->share_group_;
  } else {
    share_group_ = attributes_.shareResources
        ? GetDefaultShareGroupForHost(host)
        : scoped_refptr<WebGraphicsContext3DCommandBufferImpl::ShareGroup>(
            new ShareGroup());
  }
}

WebGraphicsContext3DCommandBufferImpl::
    ~WebGraphicsContext3DCommandBufferImpl() {
  if (real_gl_) {
    real_gl_->SetErrorMessageCallback(NULL);
  }

  Destroy();
}

bool WebGraphicsContext3DCommandBufferImpl::MaybeInitializeGL() {
  if (initialized_)
    return true;

  if (initialize_failed_)
    return false;

  TRACE_EVENT0("gpu", "WebGfxCtx3DCmdBfrImpl::MaybeInitializeGL");

  if (!CreateContext(surface_id_ != 0)) {
    Destroy();
    initialize_failed_ = true;
    return false;
  }

  // TODO(twiz):  This code is too fragile in that it assumes that only WebGL
  // contexts will request noExtensions.
  if (gl_ && attributes_.noExtensions)
    gl_->EnableFeatureCHROMIUM("webgl_enable_glsl_webgl_validation");

  command_buffer_->SetChannelErrorCallback(
      base::Bind(&WebGraphicsContext3DCommandBufferImpl::OnGpuChannelLost,
                 weak_ptr_factory_.GetWeakPtr()));

  command_buffer_->SetOnConsoleMessageCallback(
      base::Bind(&WebGraphicsContext3DCommandBufferImpl::OnErrorMessage,
                 weak_ptr_factory_.GetWeakPtr()));

  real_gl_->SetErrorMessageCallback(getErrorMessageCallback());

  visible_ = true;
  initialized_ = true;
  return true;
}

bool WebGraphicsContext3DCommandBufferImpl::InitializeCommandBuffer(
    bool onscreen, WebGraphicsContext3DCommandBufferImpl* share_context) {
  if (!host_.get())
    return false;

  CommandBufferProxyImpl* share_group_command_buffer = NULL;

  if (share_context) {
    share_group_command_buffer = share_context->command_buffer_.get();
  }

  std::vector<int32> attribs;
  attribs.push_back(ALPHA_SIZE);
  attribs.push_back(attributes_.alpha ? 8 : 0);
  attribs.push_back(DEPTH_SIZE);
  attribs.push_back(attributes_.depth ? 24 : 0);
  attribs.push_back(STENCIL_SIZE);
  attribs.push_back(attributes_.stencil ? 8 : 0);
  attribs.push_back(SAMPLES);
  attribs.push_back(attributes_.antialias ? 4 : 0);
  attribs.push_back(SAMPLE_BUFFERS);
  attribs.push_back(attributes_.antialias ? 1 : 0);
  attribs.push_back(FAIL_IF_MAJOR_PERF_CAVEAT);
  attribs.push_back(attributes_.failIfMajorPerformanceCaveat ? 1 : 0);
  attribs.push_back(LOSE_CONTEXT_WHEN_OUT_OF_MEMORY);
  attribs.push_back(lose_context_when_out_of_memory_ ? 1 : 0);
  attribs.push_back(BIND_GENERATES_RESOURCES);
  attribs.push_back(0);
  attribs.push_back(NONE);

  // Create a proxy to a command buffer in the GPU process.
  if (onscreen) {
    command_buffer_.reset(host_->CreateViewCommandBuffer(
        surface_id_,
        share_group_command_buffer,
        attribs,
        active_url_,
        gpu_preference_));
  } else {
    command_buffer_.reset(host_->CreateOffscreenCommandBuffer(
        gfx::Size(1, 1),
        share_group_command_buffer,
        attribs,
        active_url_,
        gpu_preference_));
  }

  if (!command_buffer_) {
    DLOG(ERROR) << "GpuChannelHost failed to create command buffer.";
    return false;
  }

  DVLOG_IF(1, gpu::error::IsError(command_buffer_->GetLastError()))
      << "Context dead on arrival. Last error: "
      << command_buffer_->GetLastError();
  // Initialize the command buffer.
  bool result = command_buffer_->Initialize();
  LOG_IF(ERROR, !result) << "CommandBufferProxy::Initialize failed.";
  return result;
}

bool WebGraphicsContext3DCommandBufferImpl::CreateContext(bool onscreen) {
  TRACE_EVENT0("gpu", "WebGfxCtx3DCmdBfrImpl::CreateContext");
  // Ensure the gles2 library is initialized first in a thread safe way.
  g_gles2_initializer.Get();

  scoped_refptr<gpu::gles2::ShareGroup> gles2_share_group;

  scoped_ptr<base::AutoLock> share_group_lock;
  bool add_to_share_group = false;
  if (!command_buffer_) {
    WebGraphicsContext3DCommandBufferImpl* share_context = NULL;

    share_group_lock.reset(new base::AutoLock(share_group_->lock()));
    share_context = share_group_->GetAnyContextLocked();

    if (!InitializeCommandBuffer(onscreen, share_context)) {
      LOG(ERROR) << "Failed to initialize command buffer.";
      return false;
    }

    if (share_context)
      gles2_share_group = share_context->GetImplementation()->share_group();

    add_to_share_group = true;
  }

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_.reset(new gpu::gles2::GLES2CmdHelper(command_buffer_.get()));
  if (!gles2_helper_->Initialize(mem_limits_.command_buffer_size)) {
    LOG(ERROR) << "Failed to initialize GLES2CmdHelper.";
    return false;
  }

  if (attributes_.noAutomaticFlushes)
    gles2_helper_->SetAutomaticFlushes(false);
  // Create a transfer buffer used to copy resources between the renderer
  // process and the GPU process.
  transfer_buffer_ .reset(new gpu::TransferBuffer(gles2_helper_.get()));

  DCHECK(host_.get());

  // Create the object exposing the OpenGL API.
  bool bind_generates_resources = false;
  real_gl_.reset(
      new gpu::gles2::GLES2Implementation(gles2_helper_.get(),
                                          gles2_share_group,
                                          transfer_buffer_.get(),
                                          bind_generates_resources,
                                          lose_context_when_out_of_memory_,
                                          command_buffer_.get()));
  setGLInterface(real_gl_.get());

  if (!real_gl_->Initialize(
      mem_limits_.start_transfer_buffer_size,
      mem_limits_.min_transfer_buffer_size,
      mem_limits_.max_transfer_buffer_size,
      mem_limits_.mapped_memory_reclaim_limit)) {
    LOG(ERROR) << "Failed to initialize GLES2Implementation.";
    return false;
  }

  if (add_to_share_group)
    share_group_->AddContextLocked(this);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableGpuClientTracing)) {
    trace_gl_.reset(new gpu::gles2::GLES2TraceImplementation(GetGLInterface()));
    setGLInterface(trace_gl_.get());
  }
  return true;
}

bool WebGraphicsContext3DCommandBufferImpl::makeContextCurrent() {
  if (!MaybeInitializeGL()) {
    DLOG(ERROR) << "Failed to initialize context.";
    return false;
  }
  gles2::SetGLContext(GetGLInterface());
  if (gpu::error::IsError(command_buffer_->GetLastError())) {
    LOG(ERROR) << "Context dead on arrival. Last error: "
               << command_buffer_->GetLastError();
    return false;
  }

  return true;
}

void WebGraphicsContext3DCommandBufferImpl::Destroy() {
  share_group_->RemoveContext(this);

  gpu::gles2::GLES2Interface* gl = GetGLInterface();
  if (gl) {
    // First flush the context to ensure that any pending frees of resources
    // are completed. Otherwise, if this context is part of a share group,
    // those resources might leak. Also, any remaining side effects of commands
    // issued on this context might not be visible to other contexts in the
    // share group.
    gl->Flush();
    setGLInterface(NULL);
  }

  trace_gl_.reset();
  real_gl_.reset();
  transfer_buffer_.reset();
  gles2_helper_.reset();
  real_gl_.reset();

  if (command_buffer_) {
    if (host_.get())
      host_->DestroyCommandBuffer(command_buffer_.release());
    command_buffer_.reset();
  }

  host_ = NULL;
}

gpu::ContextSupport*
WebGraphicsContext3DCommandBufferImpl::GetContextSupport() {
  return real_gl_.get();
}

bool WebGraphicsContext3DCommandBufferImpl::isContextLost() {
  return initialize_failed_ ||
      (command_buffer_ && IsCommandBufferContextLost()) ||
      context_lost_reason_ != GL_NO_ERROR;
}

WGC3Denum WebGraphicsContext3DCommandBufferImpl::getGraphicsResetStatusARB() {
  if (IsCommandBufferContextLost() &&
      context_lost_reason_ == GL_NO_ERROR) {
    return GL_UNKNOWN_CONTEXT_RESET_ARB;
  }

  return context_lost_reason_;
}

bool WebGraphicsContext3DCommandBufferImpl::IsCommandBufferContextLost() {
  // If the channel shut down unexpectedly, let that supersede the
  // command buffer's state.
  if (host_.get() && host_->IsLost())
    return true;
  gpu::CommandBuffer::State state = command_buffer_->GetLastState();
  return state.error == gpu::error::kLostContext;
}

// static
WebGraphicsContext3DCommandBufferImpl*
WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
    GpuChannelHost* host,
    const WebGraphicsContext3D::Attributes& attributes,
    bool lose_context_when_out_of_memory,
    const GURL& active_url,
    const SharedMemoryLimits& limits,
    WebGraphicsContext3DCommandBufferImpl* share_context) {
  if (!host)
    return NULL;

  if (share_context && share_context->IsCommandBufferContextLost())
    return NULL;

  return new WebGraphicsContext3DCommandBufferImpl(
      0,
      active_url,
      host,
      attributes,
      lose_context_when_out_of_memory,
      limits,
      share_context);
}

namespace {

WGC3Denum convertReason(gpu::error::ContextLostReason reason) {
  switch (reason) {
  case gpu::error::kGuilty:
    return GL_GUILTY_CONTEXT_RESET_ARB;
  case gpu::error::kInnocent:
    return GL_INNOCENT_CONTEXT_RESET_ARB;
  case gpu::error::kUnknown:
    return GL_UNKNOWN_CONTEXT_RESET_ARB;
  }

  NOTREACHED();
  return GL_UNKNOWN_CONTEXT_RESET_ARB;
}

}  // anonymous namespace

void WebGraphicsContext3DCommandBufferImpl::OnGpuChannelLost() {
  context_lost_reason_ = convertReason(
      command_buffer_->GetLastState().context_lost_reason);
  if (context_lost_callback_) {
    context_lost_callback_->onContextLost();
  }

  share_group_->RemoveAllContexts();

  DCHECK(host_.get());
  {
    base::AutoLock lock(g_default_share_groups_lock.Get());
    g_default_share_groups.Get().erase(host_.get());
  }
}

}  // namespace content

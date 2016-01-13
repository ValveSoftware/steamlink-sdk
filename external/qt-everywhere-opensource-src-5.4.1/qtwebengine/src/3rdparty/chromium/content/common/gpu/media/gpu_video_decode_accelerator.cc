// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/gpu_video_decode_accelerator.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"

#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/message_filter.h"
#include "media/base/limits.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "content/common/gpu/media/dxva_video_decode_accelerator.h"
#elif defined(OS_MACOSX)
#include "content/common/gpu/media/vt_video_decode_accelerator.h"
#elif defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL) && defined(USE_X11)
#include "content/common/gpu/media/v4l2_video_decode_accelerator.h"
#include "content/common/gpu/media/v4l2_video_device.h"
#elif defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY) && defined(USE_X11)
#include "content/common/gpu/media/vaapi_video_decode_accelerator.h"
#include "ui/gl/gl_context_glx.h"
#include "ui/gl/gl_implementation.h"
#elif defined(USE_OZONE)
#include "media/ozone/media_ozone_platform.h"
#elif defined(OS_ANDROID)
#include "content/common/gpu/media/android_video_decode_accelerator.h"
#endif

#include "ui/gfx/size.h"

namespace content {

static bool MakeDecoderContextCurrent(
    const base::WeakPtr<GpuCommandBufferStub> stub) {
  if (!stub) {
    DLOG(ERROR) << "Stub is gone; won't MakeCurrent().";
    return false;
  }

  if (!stub->decoder()->MakeCurrent()) {
    DLOG(ERROR) << "Failed to MakeCurrent()";
    return false;
  }

  return true;
}

// DebugAutoLock works like AutoLock but only acquires the lock when
// DCHECK is on.
#if DCHECK_IS_ON
typedef base::AutoLock DebugAutoLock;
#else
class DebugAutoLock {
 public:
  explicit DebugAutoLock(base::Lock&) {}
};
#endif

class GpuVideoDecodeAccelerator::MessageFilter : public IPC::MessageFilter {
 public:
  MessageFilter(GpuVideoDecodeAccelerator* owner, int32 host_route_id)
      : owner_(owner), host_route_id_(host_route_id) {}

  virtual void OnChannelError() OVERRIDE { sender_ = NULL; }

  virtual void OnChannelClosing() OVERRIDE { sender_ = NULL; }

  virtual void OnFilterAdded(IPC::Sender* sender) OVERRIDE {
    sender_ = sender;
  }

  virtual void OnFilterRemoved() OVERRIDE {
    // This will delete |owner_| and |this|.
    owner_->OnFilterRemoved();
  }

  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE {
    if (msg.routing_id() != host_route_id_)
      return false;

    IPC_BEGIN_MESSAGE_MAP(MessageFilter, msg)
      IPC_MESSAGE_FORWARD(AcceleratedVideoDecoderMsg_Decode, owner_,
                          GpuVideoDecodeAccelerator::OnDecode)
      IPC_MESSAGE_UNHANDLED(return false;)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  bool SendOnIOThread(IPC::Message* message) {
    DCHECK(!message->is_sync());
    if (!sender_) {
      delete message;
      return false;
    }
    return sender_->Send(message);
  }

 protected:
  virtual ~MessageFilter() {}

 private:
  GpuVideoDecodeAccelerator* owner_;
  int32 host_route_id_;
  // The sender to which this filter was added.
  IPC::Sender* sender_;
};

GpuVideoDecodeAccelerator::GpuVideoDecodeAccelerator(
    int32 host_route_id,
    GpuCommandBufferStub* stub,
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop)
    : host_route_id_(host_route_id),
      stub_(stub),
      texture_target_(0),
      filter_removed_(true, false),
      io_message_loop_(io_message_loop),
      weak_factory_for_io_(this) {
  DCHECK(stub_);
  stub_->AddDestructionObserver(this);
  child_message_loop_ = base::MessageLoopProxy::current();
  make_context_current_ =
      base::Bind(&MakeDecoderContextCurrent, stub_->AsWeakPtr());
}

GpuVideoDecodeAccelerator::~GpuVideoDecodeAccelerator() {
  // This class can only be self-deleted from OnWillDestroyStub(), which means
  // the VDA has already been destroyed in there.
  DCHECK(!video_decode_accelerator_);
}

bool GpuVideoDecodeAccelerator::OnMessageReceived(const IPC::Message& msg) {
  if (!video_decode_accelerator_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuVideoDecodeAccelerator, msg)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Decode, OnDecode)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_AssignPictureBuffers,
                        OnAssignPictureBuffers)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_ReusePictureBuffer,
                        OnReusePictureBuffer)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Flush, OnFlush)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Reset, OnReset)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Destroy, OnDestroy)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GpuVideoDecodeAccelerator::ProvidePictureBuffers(
    uint32 requested_num_of_buffers,
    const gfx::Size& dimensions,
    uint32 texture_target) {
  if (dimensions.width() > media::limits::kMaxDimension ||
      dimensions.height() > media::limits::kMaxDimension ||
      dimensions.GetArea() > media::limits::kMaxCanvas) {
    NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  if (!Send(new AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers(
           host_route_id_,
           requested_num_of_buffers,
           dimensions,
           texture_target))) {
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers) "
                << "failed";
  }
  texture_dimensions_ = dimensions;
  texture_target_ = texture_target;
}

void GpuVideoDecodeAccelerator::DismissPictureBuffer(
    int32 picture_buffer_id) {
  // Notify client that picture buffer is now unused.
  if (!Send(new AcceleratedVideoDecoderHostMsg_DismissPictureBuffer(
          host_route_id_, picture_buffer_id))) {
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_DismissPictureBuffer) "
                << "failed";
  }
  DebugAutoLock auto_lock(debug_uncleared_textures_lock_);
  uncleared_textures_.erase(picture_buffer_id);
}

void GpuVideoDecodeAccelerator::PictureReady(
    const media::Picture& picture) {
  // VDA may call PictureReady on IO thread. SetTextureCleared should run on
  // the child thread. VDA is responsible to call PictureReady on the child
  // thread when a picture buffer is delivered the first time.
  if (child_message_loop_->BelongsToCurrentThread()) {
    SetTextureCleared(picture);
  } else {
    DCHECK(io_message_loop_->BelongsToCurrentThread());
    DebugAutoLock auto_lock(debug_uncleared_textures_lock_);
    DCHECK_EQ(0u, uncleared_textures_.count(picture.picture_buffer_id()));
  }

  if (!Send(new AcceleratedVideoDecoderHostMsg_PictureReady(
          host_route_id_,
          picture.picture_buffer_id(),
          picture.bitstream_buffer_id()))) {
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_PictureReady) failed";
  }
}

void GpuVideoDecodeAccelerator::NotifyError(
    media::VideoDecodeAccelerator::Error error) {
  if (!Send(new AcceleratedVideoDecoderHostMsg_ErrorNotification(
          host_route_id_, error))) {
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_ErrorNotification) "
                << "failed";
  }
}

void GpuVideoDecodeAccelerator::Initialize(
    const media::VideoCodecProfile profile,
    IPC::Message* init_done_msg) {
  DCHECK(!video_decode_accelerator_.get());

  if (!stub_->channel()->AddRoute(host_route_id_, this)) {
    DLOG(ERROR) << "GpuVideoDecodeAccelerator::Initialize(): "
                   "failed to add route";
    SendCreateDecoderReply(init_done_msg, false);
  }

#if !defined(OS_WIN)
  // Ensure we will be able to get a GL context at all before initializing
  // non-Windows VDAs.
  if (!make_context_current_.Run()) {
    SendCreateDecoderReply(init_done_msg, false);
    return;
  }
#endif

#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_WIN7) {
    NOTIMPLEMENTED() << "HW video decode acceleration not available.";
    SendCreateDecoderReply(init_done_msg, false);
    return;
  }
  DVLOG(0) << "Initializing DXVA HW decoder for windows.";
  video_decode_accelerator_.reset(
      new DXVAVideoDecodeAccelerator(make_context_current_));
#elif defined(OS_MACOSX)
  video_decode_accelerator_.reset(new VTVideoDecodeAccelerator(
      static_cast<CGLContextObj>(
          stub_->decoder()->GetGLContext()->GetHandle())));
#elif defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL) && defined(USE_X11)
  scoped_ptr<V4L2Device> device = V4L2Device::Create(V4L2Device::kDecoder);
  if (!device.get()) {
    SendCreateDecoderReply(init_done_msg, false);
    return;
  }
  video_decode_accelerator_.reset(new V4L2VideoDecodeAccelerator(
      gfx::GLSurfaceEGL::GetHardwareDisplay(),
      stub_->decoder()->GetGLContext()->GetHandle(),
      weak_factory_for_io_.GetWeakPtr(),
      make_context_current_,
      device.Pass(),
      io_message_loop_));
#elif defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY) && defined(USE_X11)
  if (gfx::GetGLImplementation() != gfx::kGLImplementationDesktopGL) {
    VLOG(1) << "HW video decode acceleration not available without "
               "DesktopGL (GLX).";
    SendCreateDecoderReply(init_done_msg, false);
    return;
  }
  gfx::GLContextGLX* glx_context =
      static_cast<gfx::GLContextGLX*>(stub_->decoder()->GetGLContext());
  video_decode_accelerator_.reset(new VaapiVideoDecodeAccelerator(
      glx_context->display(), make_context_current_));
#elif defined(USE_OZONE)
  media::MediaOzonePlatform* platform =
      media::MediaOzonePlatform::GetInstance();
  video_decode_accelerator_.reset(platform->CreateVideoDecodeAccelerator(
      make_context_current_));
  if (!video_decode_accelerator_) {
    SendCreateDecoderReply(init_done_msg, false);
    return;
  }
#elif defined(OS_ANDROID)
  video_decode_accelerator_.reset(new AndroidVideoDecodeAccelerator(
      stub_->decoder()->AsWeakPtr(),
      make_context_current_));
#else
  NOTIMPLEMENTED() << "HW video decode acceleration not available.";
  SendCreateDecoderReply(init_done_msg, false);
  return;
#endif

  if (video_decode_accelerator_->CanDecodeOnIOThread()) {
    filter_ = new MessageFilter(this, host_route_id_);
    stub_->channel()->AddFilter(filter_.get());
  }

  if (!video_decode_accelerator_->Initialize(profile, this)) {
    SendCreateDecoderReply(init_done_msg, false);
    return;
  }

  SendCreateDecoderReply(init_done_msg, true);
}

// Runs on IO thread if video_decode_accelerator_->CanDecodeOnIOThread() is
// true, otherwise on the main thread.
void GpuVideoDecodeAccelerator::OnDecode(
    base::SharedMemoryHandle handle, int32 id, uint32 size) {
  DCHECK(video_decode_accelerator_.get());
  if (id < 0) {
    DLOG(ERROR) << "BitstreamBuffer id " << id << " out of range";
    if (child_message_loop_->BelongsToCurrentThread()) {
      NotifyError(media::VideoDecodeAccelerator::INVALID_ARGUMENT);
    } else {
      child_message_loop_->PostTask(
          FROM_HERE,
          base::Bind(&GpuVideoDecodeAccelerator::NotifyError,
                     base::Unretained(this),
                     media::VideoDecodeAccelerator::INVALID_ARGUMENT));
    }
    return;
  }
  video_decode_accelerator_->Decode(media::BitstreamBuffer(id, handle, size));
}

void GpuVideoDecodeAccelerator::OnAssignPictureBuffers(
    const std::vector<int32>& buffer_ids,
    const std::vector<uint32>& texture_ids) {
  if (buffer_ids.size() != texture_ids.size()) {
    NotifyError(media::VideoDecodeAccelerator::INVALID_ARGUMENT);
    return;
  }

  gpu::gles2::GLES2Decoder* command_decoder = stub_->decoder();
  gpu::gles2::TextureManager* texture_manager =
      command_decoder->GetContextGroup()->texture_manager();

  std::vector<media::PictureBuffer> buffers;
  std::vector<scoped_refptr<gpu::gles2::TextureRef> > textures;
  for (uint32 i = 0; i < buffer_ids.size(); ++i) {
    if (buffer_ids[i] < 0) {
      DLOG(ERROR) << "Buffer id " << buffer_ids[i] << " out of range";
      NotifyError(media::VideoDecodeAccelerator::INVALID_ARGUMENT);
      return;
    }
    gpu::gles2::TextureRef* texture_ref = texture_manager->GetTexture(
        texture_ids[i]);
    if (!texture_ref) {
      DLOG(ERROR) << "Failed to find texture id " << texture_ids[i];
      NotifyError(media::VideoDecodeAccelerator::INVALID_ARGUMENT);
      return;
    }
    gpu::gles2::Texture* info = texture_ref->texture();
    if (info->target() != texture_target_) {
      DLOG(ERROR) << "Texture target mismatch for texture id "
                  << texture_ids[i];
      NotifyError(media::VideoDecodeAccelerator::INVALID_ARGUMENT);
      return;
    }
    if (texture_target_ == GL_TEXTURE_EXTERNAL_OES) {
      // GL_TEXTURE_EXTERNAL_OES textures have their dimensions defined by the
      // underlying EGLImage.  Use |texture_dimensions_| for this size.
      texture_manager->SetLevelInfo(texture_ref,
                                    GL_TEXTURE_EXTERNAL_OES,
                                    0,
                                    0,
                                    texture_dimensions_.width(),
                                    texture_dimensions_.height(),
                                    1,
                                    0,
                                    0,
                                    0,
                                    false);
    } else {
      // For other targets, texture dimensions should already be defined.
      GLsizei width = 0, height = 0;
      info->GetLevelSize(texture_target_, 0, &width, &height);
      if (width != texture_dimensions_.width() ||
          height != texture_dimensions_.height()) {
        DLOG(ERROR) << "Size mismatch for texture id " << texture_ids[i];
        NotifyError(media::VideoDecodeAccelerator::INVALID_ARGUMENT);
        return;
      }
    }
    uint32 service_texture_id;
    if (!command_decoder->GetServiceTextureId(
            texture_ids[i], &service_texture_id)) {
      DLOG(ERROR) << "Failed to translate texture!";
      NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
      return;
    }
    buffers.push_back(media::PictureBuffer(
        buffer_ids[i], texture_dimensions_, service_texture_id));
    textures.push_back(texture_ref);
  }
  video_decode_accelerator_->AssignPictureBuffers(buffers);
  DebugAutoLock auto_lock(debug_uncleared_textures_lock_);
  for (uint32 i = 0; i < buffer_ids.size(); ++i)
    uncleared_textures_[buffer_ids[i]] = textures[i];
}

void GpuVideoDecodeAccelerator::OnReusePictureBuffer(
    int32 picture_buffer_id) {
  DCHECK(video_decode_accelerator_.get());
  video_decode_accelerator_->ReusePictureBuffer(picture_buffer_id);
}

void GpuVideoDecodeAccelerator::OnFlush() {
  DCHECK(video_decode_accelerator_.get());
  video_decode_accelerator_->Flush();
}

void GpuVideoDecodeAccelerator::OnReset() {
  DCHECK(video_decode_accelerator_.get());
  video_decode_accelerator_->Reset();
}

void GpuVideoDecodeAccelerator::OnDestroy() {
  DCHECK(video_decode_accelerator_.get());
  OnWillDestroyStub();
}

void GpuVideoDecodeAccelerator::OnFilterRemoved() {
  // We're destroying; cancel all callbacks.
  weak_factory_for_io_.InvalidateWeakPtrs();
  filter_removed_.Signal();
}

void GpuVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer(
    int32 bitstream_buffer_id) {
  if (!Send(new AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed(
          host_route_id_, bitstream_buffer_id))) {
    DLOG(ERROR)
        << "Send(AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed) "
        << "failed";
  }
}

void GpuVideoDecodeAccelerator::NotifyFlushDone() {
  if (!Send(new AcceleratedVideoDecoderHostMsg_FlushDone(host_route_id_)))
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_FlushDone) failed";
}

void GpuVideoDecodeAccelerator::NotifyResetDone() {
  if (!Send(new AcceleratedVideoDecoderHostMsg_ResetDone(host_route_id_)))
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_ResetDone) failed";
}

void GpuVideoDecodeAccelerator::OnWillDestroyStub() {
  // The stub is going away, so we have to stop and destroy VDA here, before
  // returning, because the VDA may need the GL context to run and/or do its
  // cleanup. We cannot destroy the VDA before the IO thread message filter is
  // removed however, since we cannot service incoming messages with VDA gone.
  // We cannot simply check for existence of VDA on IO thread though, because
  // we don't want to synchronize the IO thread with the ChildThread.
  // So we have to wait for the RemoveFilter callback here instead and remove
  // the VDA after it arrives and before returning.
  if (filter_.get()) {
    stub_->channel()->RemoveFilter(filter_.get());
    filter_removed_.Wait();
  }

  stub_->channel()->RemoveRoute(host_route_id_);
  stub_->RemoveDestructionObserver(this);

  video_decode_accelerator_.reset();
  delete this;
}

void GpuVideoDecodeAccelerator::SetTextureCleared(
    const media::Picture& picture) {
  DCHECK(child_message_loop_->BelongsToCurrentThread());
  DebugAutoLock auto_lock(debug_uncleared_textures_lock_);
  std::map<int32, scoped_refptr<gpu::gles2::TextureRef> >::iterator it;
  it = uncleared_textures_.find(picture.picture_buffer_id());
  if (it == uncleared_textures_.end())
    return;  // the texture has been cleared

  scoped_refptr<gpu::gles2::TextureRef> texture_ref = it->second;
  GLenum target = texture_ref->texture()->target();
  gpu::gles2::TextureManager* texture_manager =
      stub_->decoder()->GetContextGroup()->texture_manager();
  DCHECK(!texture_ref->texture()->IsLevelCleared(target, 0));
  texture_manager->SetLevelCleared(texture_ref, target, 0, true);
  uncleared_textures_.erase(it);
}

bool GpuVideoDecodeAccelerator::Send(IPC::Message* message) {
  if (filter_.get() && io_message_loop_->BelongsToCurrentThread())
    return filter_->SendOnIOThread(message);
  DCHECK(child_message_loop_->BelongsToCurrentThread());
  return stub_->channel()->Send(message);
}

void GpuVideoDecodeAccelerator::SendCreateDecoderReply(IPC::Message* message,
                                                       bool succeeded) {
  GpuCommandBufferMsg_CreateVideoDecoder::WriteReplyParams(message, succeeded);
  Send(message);
}

}  // namespace content

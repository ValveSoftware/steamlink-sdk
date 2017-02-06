// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/buffer.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/output/context_provider.h"
#include "cc/resources/single_release_callback.h"
#include "cc/resources/texture_mailbox.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace exo {
namespace {

// The amount of time before we wait for release queries using
// GetQueryObjectuivEXT(GL_QUERY_RESULT_EXT).
const int kWaitForReleaseDelayMs = 500;

GLenum GLInternalFormat(gfx::BufferFormat format) {
  const GLenum kGLInternalFormats[] = {
      GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD,  // ATC
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT,     // ATCIA
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT,     // DXT1
      GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,    // DXT5
      GL_ETC1_RGB8_OES,                    // ETC1
      GL_R8_EXT,                           // R_8
      GL_RGB,                              // BGR_565
      GL_RGBA,                             // RGBA_4444
      GL_RGB,                              // RGBX_8888
      GL_RGBA,                             // RGBA_8888
      GL_RGB,                              // BGRX_8888
      GL_BGRA_EXT,                         // BGRA_8888
      GL_RGB_YCRCB_420_CHROMIUM,           // YVU_420
      GL_INVALID_ENUM,                     // YUV_420_BIPLANAR
      GL_RGB_YCBCR_422_CHROMIUM,           // UYVY_422
  };
  static_assert(arraysize(kGLInternalFormats) ==
                    (static_cast<int>(gfx::BufferFormat::LAST) + 1),
                "BufferFormat::LAST must be last value of kGLInternalFormats");

  DCHECK(format <= gfx::BufferFormat::LAST);
  return kGLInternalFormats[static_cast<int>(format)];
}

unsigned CreateGLTexture(gpu::gles2::GLES2Interface* gles2, GLenum target) {
  unsigned texture_id = 0;
  gles2->GenTextures(1, &texture_id);
  gles2->ActiveTexture(GL_TEXTURE0);
  gles2->BindTexture(target, texture_id);
  gles2->TexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gles2->TexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gles2->TexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gles2->TexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return texture_id;
}

void CreateGLTextureMailbox(gpu::gles2::GLES2Interface* gles2,
                            unsigned texture_id,
                            GLenum target,
                            gpu::Mailbox* mailbox) {
  gles2->ActiveTexture(GL_TEXTURE0);
  gles2->BindTexture(target, texture_id);
  gles2->GenMailboxCHROMIUM(mailbox->name);
  gles2->ProduceTextureCHROMIUM(target, mailbox->name);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Buffer::Texture

// Encapsulates the state and logic needed to bind a buffer to a GLES2 texture.
class Buffer::Texture {
 public:
  explicit Texture(cc::ContextProvider* context_provider);
  Texture(cc::ContextProvider* context_provider,
          gfx::GpuMemoryBuffer* gpu_memory_buffer,
          unsigned texture_target,
          unsigned query_type);
  ~Texture();

  // Returns true if GLES2 resources for texture have been lost.
  bool IsLost();

  // Allow texture to be reused after |sync_token| has passed and runs
  // |callback|.
  void Release(const base::Closure& callback,
               const gpu::SyncToken& sync_token,
               bool is_lost);

  // Binds the contents referenced by |image_id_| to the texture returned by
  // mailbox(). Returns a sync token that can be used when accessing texture
  // from a different context.
  gpu::SyncToken BindTexImage();

  // Releases the contents referenced by |image_id_| after |sync_token| has
  // passed and runs |callback| when completed.
  void ReleaseTexImage(const base::Closure& callback,
                       const gpu::SyncToken& sync_token,
                       bool is_lost);

  // Copy the contents of texture to |destination| and runs |callback| when
  // completed. Returns a sync token that can be used when accessing texture
  // from a different context.
  gpu::SyncToken CopyTexImage(Texture* destination,
                              const base::Closure& callback);

  // Returns the mailbox for this texture.
  gpu::Mailbox mailbox() const { return mailbox_; }

 private:
  void ReleaseWhenQueryResultIsAvailable(const base::Closure& callback);
  void Released();
  void ScheduleWaitForRelease(base::TimeDelta delay);
  void WaitForRelease();

  scoped_refptr<cc::ContextProvider> context_provider_;
  const unsigned texture_target_;
  const unsigned query_type_;
  const GLenum internalformat_;
  unsigned image_id_ = 0;
  unsigned query_id_ = 0;
  unsigned texture_id_ = 0;
  gpu::Mailbox mailbox_;
  base::Closure release_callback_;
  base::TimeTicks wait_for_release_time_;
  bool wait_for_release_pending_ = false;
  base::WeakPtrFactory<Texture> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Texture);
};

Buffer::Texture::Texture(cc::ContextProvider* context_provider)
    : context_provider_(context_provider),
      texture_target_(GL_TEXTURE_2D),
      query_type_(GL_COMMANDS_COMPLETED_CHROMIUM),
      internalformat_(GL_RGBA),
      weak_ptr_factory_(this) {
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  texture_id_ = CreateGLTexture(gles2, texture_target_);
  // Generate a crypto-secure random mailbox name.
  CreateGLTextureMailbox(gles2, texture_id_, texture_target_, &mailbox_);
}

Buffer::Texture::Texture(cc::ContextProvider* context_provider,
                         gfx::GpuMemoryBuffer* gpu_memory_buffer,
                         unsigned texture_target,
                         unsigned query_type)
    : context_provider_(context_provider),
      texture_target_(texture_target),
      query_type_(query_type),
      internalformat_(GLInternalFormat(gpu_memory_buffer->GetFormat())),
      weak_ptr_factory_(this) {
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  gfx::Size size = gpu_memory_buffer->GetSize();
  image_id_ =
      gles2->CreateImageCHROMIUM(gpu_memory_buffer->AsClientBuffer(),
                                 size.width(), size.height(), internalformat_);
  DLOG_IF(WARNING, !image_id_) << "Failed to create GLImage";

  gles2->GenQueriesEXT(1, &query_id_);
  texture_id_ = CreateGLTexture(gles2, texture_target_);
}

Buffer::Texture::~Texture() {
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  gles2->DeleteTextures(1, &texture_id_);
  if (query_id_)
    gles2->DeleteQueriesEXT(1, &query_id_);
  if (image_id_)
    gles2->DestroyImageCHROMIUM(image_id_);
}

bool Buffer::Texture::IsLost() {
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  return gles2->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

void Buffer::Texture::Release(const base::Closure& callback,
                              const gpu::SyncToken& sync_token,
                              bool is_lost) {
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  if (sync_token.HasData())
    gles2->WaitSyncTokenCHROMIUM(sync_token.GetConstData());

  // Run callback as texture can be reused immediately after waiting for sync
  // token.
  callback.Run();
}

gpu::SyncToken Buffer::Texture::BindTexImage() {
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  gles2->ActiveTexture(GL_TEXTURE0);
  gles2->BindTexture(texture_target_, texture_id_);
  DCHECK_NE(image_id_, 0u);
  gles2->BindTexImage2DCHROMIUM(texture_target_, image_id_);
  // Generate a crypto-secure random mailbox name if not already done.
  if (mailbox_.IsZero())
    CreateGLTextureMailbox(gles2, texture_id_, texture_target_, &mailbox_);
  // Create and return a sync token that can be used to ensure that the
  // BindTexImage2DCHROMIUM call is processed before issuing any commands
  // that will read from the texture on a different context.
  uint64_t fence_sync = gles2->InsertFenceSyncCHROMIUM();
  gles2->OrderingBarrierCHROMIUM();
  gpu::SyncToken sync_token;
  gles2->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());
  return sync_token;
}

void Buffer::Texture::ReleaseTexImage(const base::Closure& callback,
                                      const gpu::SyncToken& sync_token,
                                      bool is_lost) {
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  if (sync_token.HasData())
    gles2->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  gles2->ActiveTexture(GL_TEXTURE0);
  gles2->BindTexture(texture_target_, texture_id_);
  DCHECK_NE(query_id_, 0u);
  gles2->BeginQueryEXT(query_type_, query_id_);
  gles2->ReleaseTexImage2DCHROMIUM(texture_target_, image_id_);
  gles2->EndQueryEXT(query_type_);
  // Run callback when query result is available and ReleaseTexImage has been
  // handled if sync token has data and buffer has been used. If buffer was
  // never used then run the callback immediately.
  if (sync_token.HasData()) {
    ReleaseWhenQueryResultIsAvailable(callback);
  } else {
    callback.Run();
  }
}

gpu::SyncToken Buffer::Texture::CopyTexImage(Texture* destination,
                                             const base::Closure& callback) {
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  gles2->ActiveTexture(GL_TEXTURE0);
  gles2->BindTexture(texture_target_, texture_id_);
  DCHECK_NE(image_id_, 0u);
  gles2->BindTexImage2DCHROMIUM(texture_target_, image_id_);
  gles2->CopyTextureCHROMIUM(texture_id_, destination->texture_id_,
                             internalformat_, GL_UNSIGNED_BYTE, false, false,
                             false);
  DCHECK_NE(query_id_, 0u);
  gles2->BeginQueryEXT(query_type_, query_id_);
  gles2->ReleaseTexImage2DCHROMIUM(texture_target_, image_id_);
  gles2->EndQueryEXT(query_type_);
  // Run callback when query result is available and ReleaseTexImage has been
  // handled.
  ReleaseWhenQueryResultIsAvailable(callback);
  // Create and return a sync token that can be used to ensure that the
  // CopyTextureCHROMIUM call is processed before issuing any commands
  // that will read from the target texture on a different context.
  uint64_t fence_sync = gles2->InsertFenceSyncCHROMIUM();
  gles2->OrderingBarrierCHROMIUM();
  gpu::SyncToken sync_token;
  gles2->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());
  return sync_token;
}

void Buffer::Texture::ReleaseWhenQueryResultIsAvailable(
    const base::Closure& callback) {
  DCHECK(release_callback_.is_null());
  release_callback_ = callback;
  base::TimeDelta wait_for_release_delay =
      base::TimeDelta::FromMilliseconds(kWaitForReleaseDelayMs);
  wait_for_release_time_ = base::TimeTicks::Now() + wait_for_release_delay;
  ScheduleWaitForRelease(wait_for_release_delay);
  context_provider_->ContextSupport()->SignalQuery(
      query_id_,
      base::Bind(&Buffer::Texture::Released, weak_ptr_factory_.GetWeakPtr()));
}

void Buffer::Texture::Released() {
  if (!release_callback_.is_null())
    base::ResetAndReturn(&release_callback_).Run();
}

void Buffer::Texture::ScheduleWaitForRelease(base::TimeDelta delay) {
  if (wait_for_release_pending_)
    return;

  wait_for_release_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&Buffer::Texture::WaitForRelease,
                            weak_ptr_factory_.GetWeakPtr()),
      delay);
}

void Buffer::Texture::WaitForRelease() {
  DCHECK(wait_for_release_pending_);
  wait_for_release_pending_ = false;

  if (release_callback_.is_null())
    return;

  base::TimeTicks current_time = base::TimeTicks::Now();
  if (current_time < wait_for_release_time_) {
    ScheduleWaitForRelease(wait_for_release_time_ - current_time);
    return;
  }

  base::Closure callback = base::ResetAndReturn(&release_callback_);

  {
    TRACE_EVENT0("exo", "Buffer::Texture::WaitForQueryResult");

    // We need to wait for the result to be available. Getting the result of
    // the query implies waiting for it to become available. The actual result
    // is unimportant and also not well defined.
    unsigned result = 0;
    gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
    gles2->GetQueryObjectuivEXT(query_id_, GL_QUERY_RESULT_EXT, &result);
  }

  callback.Run();
}

////////////////////////////////////////////////////////////////////////////////
// Buffer, public:

Buffer::Buffer(std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer)
    : gpu_memory_buffer_(std::move(gpu_memory_buffer)),
      texture_target_(GL_TEXTURE_2D),
      query_type_(GL_COMMANDS_COMPLETED_CHROMIUM),
      use_zero_copy_(true),
      is_overlay_candidate_(false) {}

Buffer::Buffer(std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
               unsigned texture_target,
               unsigned query_type,
               bool use_zero_copy,
               bool is_overlay_candidate)
    : gpu_memory_buffer_(std::move(gpu_memory_buffer)),
      texture_target_(texture_target),
      query_type_(query_type),
      use_zero_copy_(use_zero_copy),
      is_overlay_candidate_(is_overlay_candidate) {}

Buffer::~Buffer() {}

std::unique_ptr<cc::SingleReleaseCallback> Buffer::ProduceTextureMailbox(
    cc::TextureMailbox* texture_mailbox,
    bool secure_output_only,
    bool client_usage) {
  DCHECK(attach_count_);
  DLOG_IF(WARNING, use_count_ && client_usage)
      << "Producing a texture mailbox for a buffer that has not been released";

  // Some clients think that they can reuse a buffer before it's released by
  // performing a fast blit into the buffer. This behavior is bad as it prevents
  // the client from knowing when the buffer is actually released (e.g. the
  // release notification for the previous use of buffer can arrive after the
  // buffer has been reused). We stop running the release callback when this
  // type of behavior is detected as having the buffer always be busy will
  // result in fewer drawing artifacts.
  if (use_count_ && client_usage)
    release_callback_.Reset();

  // Increment the use count for this buffer.
  ++use_count_;

  // If textures are lost, destroy them to ensure that we create new ones below.
  if (contents_texture_ && contents_texture_->IsLost())
    contents_texture_.reset();
  if (texture_ && texture_->IsLost())
    texture_.reset();

  // Note: This can fail if GPU acceleration has been disabled.
  scoped_refptr<cc::ContextProvider> context_provider =
      aura::Env::GetInstance()
          ->context_factory()
          ->SharedMainThreadContextProvider();
  if (!context_provider) {
    DLOG(WARNING) << "Failed to acquire a context provider";
    Release();  // Decrements the use count
    return nullptr;
  }

  // Create a new image texture for |gpu_memory_buffer_| with |texture_target_|
  // if one doesn't already exist. The contents of this buffer are copied to
  // |texture| using a call to CopyTexImage.
  if (!contents_texture_) {
    contents_texture_ = base::WrapUnique(
        new Texture(context_provider.get(), gpu_memory_buffer_.get(),
                    texture_target_, query_type_));
  }

  if (use_zero_copy_) {
    // Zero-copy means using the contents texture directly.
    Texture* texture = contents_texture_.get();

    // This binds the latest contents of this buffer to |texture|.
    gpu::SyncToken sync_token = texture->BindTexImage();

    *texture_mailbox =
        cc::TextureMailbox(texture->mailbox(), sync_token, texture_target_,
                           gpu_memory_buffer_->GetSize(), is_overlay_candidate_,
                           secure_output_only);
    // The contents texture will be released when no longer used by the
    // compositor.
    return cc::SingleReleaseCallback::Create(
        base::Bind(&Buffer::Texture::ReleaseTexImage, base::Unretained(texture),
                   base::Bind(&Buffer::ReleaseContentsTexture, AsWeakPtr(),
                              base::Passed(&contents_texture_))));
  }

  // Create a mailbox texture that we copy the buffer contents to.
  if (!texture_)
    texture_ = base::WrapUnique(new Texture(context_provider.get()));

  // Copy the contents of |contents_texture| to |texture| and produce a
  // texture mailbox from the result in |texture|.
  Texture* contents_texture = contents_texture_.get();
  Texture* texture = texture_.get();

  // The contents texture will be released when copy has completed.
  gpu::SyncToken sync_token = contents_texture->CopyTexImage(
      texture, base::Bind(&Buffer::ReleaseContentsTexture, AsWeakPtr(),
                          base::Passed(&contents_texture_)));
  *texture_mailbox =
      cc::TextureMailbox(texture->mailbox(), sync_token, GL_TEXTURE_2D,
                         gpu_memory_buffer_->GetSize(),
                         false /* is_overlay_candidate */, secure_output_only);
  // The mailbox texture will be released when no longer used by the
  // compositor.
  return cc::SingleReleaseCallback::Create(
      base::Bind(&Buffer::Texture::Release, base::Unretained(texture),
                 base::Bind(&Buffer::ReleaseTexture, AsWeakPtr(),
                            base::Passed(&texture_))));
}

void Buffer::OnAttach() {
  DLOG_IF(WARNING, attach_count_ > 0u)
      << "Reattaching a buffer that is already attached to another surface.";
  attach_count_++;
}

void Buffer::OnDetach() {
  DCHECK_GT(attach_count_, 0u);
  --attach_count_;
  CheckReleaseCallback();
}

gfx::Size Buffer::GetSize() const {
  return gpu_memory_buffer_->GetSize();
}

std::unique_ptr<base::trace_event::TracedValue> Buffer::AsTracedValue() const {
  std::unique_ptr<base::trace_event::TracedValue> value(
      new base::trace_event::TracedValue());
  gfx::Size size = gpu_memory_buffer_->GetSize();
  value->SetInteger("width", size.width());
  value->SetInteger("height", size.height());
  value->SetInteger("format",
                    static_cast<int>(gpu_memory_buffer_->GetFormat()));
  return value;
}

////////////////////////////////////////////////////////////////////////////////
// Buffer, private:

void Buffer::Release() {
  DCHECK_GT(use_count_, 0u);
  --use_count_;
  CheckReleaseCallback();
}

void Buffer::CheckReleaseCallback() {
  if (attach_count_ || use_count_)
    return;

  // Run release callback to notify the client that buffer has been released.
  if (!release_callback_.is_null())
    release_callback_.Run();
}

void Buffer::ReleaseTexture(std::unique_ptr<Texture> texture) {
  texture_ = std::move(texture);
}

void Buffer::ReleaseContentsTexture(std::unique_ptr<Texture> texture) {
  TRACE_EVENT0("exo", "Buffer::ReleaseContentsTexture");

  contents_texture_ = std::move(texture);
  Release();
}

}  // namespace exo

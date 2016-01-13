// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/video_decoder_resource.h"

#include "base/bind.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_graphics_3d_proxy.h"
#include "ppapi/proxy/serialized_handle.h"
#include "ppapi/proxy/video_decoder_constants.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_graphics_3d_shared.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/thunk/enter.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Graphics3D_API;
using ppapi::thunk::PPB_VideoDecoder_API;

namespace ppapi {
namespace proxy {

VideoDecoderResource::ShmBuffer::ShmBuffer(
    scoped_ptr<base::SharedMemory> shm_ptr,
    uint32_t size,
    uint32_t shm_id)
    : shm(shm_ptr.Pass()), addr(NULL), shm_id(shm_id) {
  if (shm->Map(size))
    addr = shm->memory();
}

VideoDecoderResource::ShmBuffer::~ShmBuffer() {
}

VideoDecoderResource::Texture::Texture(uint32_t texture_target,
                                       const PP_Size& size)
    : texture_target(texture_target), size(size) {
}

VideoDecoderResource::Texture::~Texture() {
}

VideoDecoderResource::Picture::Picture(int32_t decode_id, uint32_t texture_id)
    : decode_id(decode_id), texture_id(texture_id) {
}

VideoDecoderResource::Picture::~Picture() {
}

VideoDecoderResource::VideoDecoderResource(Connection connection,
                                           PP_Instance instance)
    : PluginResource(connection, instance),
      num_decodes_(0),
      get_picture_(NULL),
      gles2_impl_(NULL),
      initialized_(false),
      testing_(false),
      // Set |decoder_last_error_| to PP_OK after successful initialization.
      // This makes error checking a little more concise, since we can check
      // that the decoder has been initialized and hasn't returned an error by
      // just testing |decoder_last_error_|.
      decoder_last_error_(PP_ERROR_FAILED) {
  // Clear the decode_ids_ array.
  memset(decode_ids_, 0, arraysize(decode_ids_));
  SendCreate(RENDERER, PpapiHostMsg_VideoDecoder_Create());
}

VideoDecoderResource::~VideoDecoderResource() {
  // Destroy any textures which haven't been dismissed.
  TextureMap::iterator it = textures_.begin();
  for (; it != textures_.end(); ++it)
    DeleteGLTexture(it->first);
}

PPB_VideoDecoder_API* VideoDecoderResource::AsPPB_VideoDecoder_API() {
  return this;
}

int32_t VideoDecoderResource::Initialize(
    PP_Resource graphics_context,
    PP_VideoProfile profile,
    PP_Bool allow_software_fallback,
    scoped_refptr<TrackedCallback> callback) {
  if (initialized_)
    return PP_ERROR_FAILED;
  if (profile < 0 || profile > PP_VIDEOPROFILE_MAX)
    return PP_ERROR_BADARGUMENT;
  if (initialize_callback_)
    return PP_ERROR_INPROGRESS;
  if (!graphics_context)
    return PP_ERROR_BADRESOURCE;

  HostResource host_resource;
  if (!testing_) {
    // Create a new Graphics3D resource that can create texture resources to
    // share with the plugin. We can't use the plugin's Graphics3D, since we
    // create textures on a proxy thread, and would interfere with the plugin.
    thunk::EnterResourceCreationNoLock enter_create(pp_instance());
    if (enter_create.failed())
      return PP_ERROR_FAILED;
    int32_t attrib_list[] = {PP_GRAPHICS3DATTRIB_NONE};
    graphics3d_ =
        ScopedPPResource(ScopedPPResource::PassRef(),
                         enter_create.functions()->CreateGraphics3D(
                             pp_instance(), graphics_context, attrib_list));
    EnterResourceNoLock<PPB_Graphics3D_API> enter_graphics(graphics3d_.get(),
                                                           false);
    if (enter_graphics.failed())
      return PP_ERROR_BADRESOURCE;

    PPB_Graphics3D_Shared* ppb_graphics3d_shared =
        static_cast<PPB_Graphics3D_Shared*>(enter_graphics.object());
    gles2_impl_ = ppb_graphics3d_shared->gles2_impl();
    host_resource = ppb_graphics3d_shared->host_resource();
  }

  initialize_callback_ = callback;

  Call<PpapiPluginMsg_VideoDecoder_InitializeReply>(
      RENDERER,
      PpapiHostMsg_VideoDecoder_Initialize(
          host_resource, profile, PP_ToBool(allow_software_fallback)),
      base::Bind(&VideoDecoderResource::OnPluginMsgInitializeComplete, this));

  return PP_OK_COMPLETIONPENDING;
}

int32_t VideoDecoderResource::Decode(uint32_t decode_id,
                                     uint32_t size,
                                     const void* buffer,
                                     scoped_refptr<TrackedCallback> callback) {
  if (decoder_last_error_)
    return decoder_last_error_;
  if (flush_callback_ || reset_callback_)
    return PP_ERROR_FAILED;
  if (decode_callback_)
    return PP_ERROR_INPROGRESS;
  if (size > kMaximumBitstreamBufferSize)
    return PP_ERROR_NOMEMORY;

  // If we allow the plugin to call Decode again, we must have somewhere to
  // copy their buffer.
  DCHECK(!available_shm_buffers_.empty() ||
         shm_buffers_.size() < kMaximumPendingDecodes);

  // Count up, wrapping back to 0 before overflowing.
  int32_t uid = ++num_decodes_;
  if (uid == std::numeric_limits<int32_t>::max())
    num_decodes_ = 0;

  // Save decode_id in a ring buffer. The ring buffer is sized to store
  // decode_id for the maximum picture delay.
  decode_ids_[uid % kMaximumPictureDelay] = decode_id;

  if (available_shm_buffers_.empty() ||
      available_shm_buffers_.back()->shm->mapped_size() < size) {
    uint32_t shm_id;
    if (shm_buffers_.size() < kMaximumPendingDecodes) {
      // Signal the host to create a new shm buffer by passing an index outside
      // the legal range.
      shm_id = static_cast<uint32_t>(shm_buffers_.size());
    } else {
      // Signal the host to grow a buffer by passing a legal index. Choose the
      // last available shm buffer for simplicity.
      shm_id = available_shm_buffers_.back()->shm_id;
      available_shm_buffers_.pop_back();
    }

    // Synchronously get shared memory. Use GenericSyncCall so we can get the
    // reply params, which contain the handle.
    uint32_t shm_size = 0;
    IPC::Message reply;
    ResourceMessageReplyParams reply_params;
    int32_t result =
        GenericSyncCall(RENDERER,
                        PpapiHostMsg_VideoDecoder_GetShm(shm_id, size),
                        &reply,
                        &reply_params);
    if (result != PP_OK)
      return PP_ERROR_FAILED;
    if (!UnpackMessage<PpapiPluginMsg_VideoDecoder_GetShmReply>(reply,
                                                                &shm_size))
      return PP_ERROR_FAILED;
    base::SharedMemoryHandle shm_handle = base::SharedMemory::NULLHandle();
    if (!reply_params.TakeSharedMemoryHandleAtIndex(0, &shm_handle))
      return PP_ERROR_NOMEMORY;
    scoped_ptr<base::SharedMemory> shm(
        new base::SharedMemory(shm_handle, false /* read_only */));
    scoped_ptr<ShmBuffer> shm_buffer(
        new ShmBuffer(shm.Pass(), shm_size, shm_id));
    if (!shm_buffer->addr)
      return PP_ERROR_NOMEMORY;

    available_shm_buffers_.push_back(shm_buffer.get());
    if (shm_buffers_.size() < kMaximumPendingDecodes) {
      shm_buffers_.push_back(shm_buffer.release());
    } else {
      // Delete manually since ScopedVector won't delete the existing element if
      // we just assign it.
      delete shm_buffers_[shm_id];
      shm_buffers_[shm_id] = shm_buffer.release();
    }
  }

  // At this point we should have shared memory to hold the plugin's buffer.
  DCHECK(!available_shm_buffers_.empty() &&
         available_shm_buffers_.back()->shm->mapped_size() >= size);

  ShmBuffer* shm_buffer = available_shm_buffers_.back();
  available_shm_buffers_.pop_back();
  memcpy(shm_buffer->addr, buffer, size);

  Call<PpapiPluginMsg_VideoDecoder_DecodeReply>(
      RENDERER,
      PpapiHostMsg_VideoDecoder_Decode(shm_buffer->shm_id, size, uid),
      base::Bind(&VideoDecoderResource::OnPluginMsgDecodeComplete, this));

  // If we have another free buffer, or we can still create new buffers, let
  // the plugin call Decode again.
  if (!available_shm_buffers_.empty() ||
      shm_buffers_.size() < kMaximumPendingDecodes)
    return PP_OK;

  // All buffers are busy and we can't create more. Delay completion until a
  // buffer is available.
  decode_callback_ = callback;
  return PP_OK_COMPLETIONPENDING;
}

int32_t VideoDecoderResource::GetPicture(
    PP_VideoPicture* picture,
    scoped_refptr<TrackedCallback> callback) {
  if (decoder_last_error_)
    return decoder_last_error_;
  if (reset_callback_)
    return PP_ERROR_FAILED;
  if (get_picture_callback_)
    return PP_ERROR_INPROGRESS;

  // If the next picture is ready, return it synchronously.
  if (!received_pictures_.empty()) {
    WriteNextPicture(picture);
    return PP_OK;
  }

  get_picture_callback_ = callback;
  get_picture_ = picture;
  return PP_OK_COMPLETIONPENDING;
}

void VideoDecoderResource::RecyclePicture(const PP_VideoPicture* picture) {
  if (decoder_last_error_)
    return;

  Post(RENDERER, PpapiHostMsg_VideoDecoder_RecyclePicture(picture->texture_id));
}

int32_t VideoDecoderResource::Flush(scoped_refptr<TrackedCallback> callback) {
  if (decoder_last_error_)
    return decoder_last_error_;
  if (reset_callback_)
    return PP_ERROR_FAILED;
  if (flush_callback_)
    return PP_ERROR_INPROGRESS;
  flush_callback_ = callback;

  Call<PpapiPluginMsg_VideoDecoder_FlushReply>(
      RENDERER,
      PpapiHostMsg_VideoDecoder_Flush(),
      base::Bind(&VideoDecoderResource::OnPluginMsgFlushComplete, this));

  return PP_OK_COMPLETIONPENDING;
}

int32_t VideoDecoderResource::Reset(scoped_refptr<TrackedCallback> callback) {
  if (decoder_last_error_)
    return decoder_last_error_;
  if (flush_callback_)
    return PP_ERROR_FAILED;
  if (reset_callback_)
    return PP_ERROR_INPROGRESS;
  reset_callback_ = callback;

  // Cause any pending Decode or GetPicture callbacks to abort after we return,
  // to avoid reentering the plugin.
  if (TrackedCallback::IsPending(decode_callback_))
    decode_callback_->PostAbort();
  decode_callback_ = NULL;
  if (TrackedCallback::IsPending(get_picture_callback_))
    get_picture_callback_->PostAbort();
  get_picture_callback_ = NULL;
  Call<PpapiPluginMsg_VideoDecoder_ResetReply>(
      RENDERER,
      PpapiHostMsg_VideoDecoder_Reset(),
      base::Bind(&VideoDecoderResource::OnPluginMsgResetComplete, this));

  return PP_OK_COMPLETIONPENDING;
}

void VideoDecoderResource::OnReplyReceived(
    const ResourceMessageReplyParams& params,
    const IPC::Message& msg) {
  PPAPI_BEGIN_MESSAGE_MAP(VideoDecoderResource, msg)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_VideoDecoder_RequestTextures, OnPluginMsgRequestTextures)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_VideoDecoder_PictureReady, OnPluginMsgPictureReady)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_VideoDecoder_DismissPicture, OnPluginMsgDismissPicture)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_VideoDecoder_NotifyError, OnPluginMsgNotifyError)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL_UNHANDLED(
        PluginResource::OnReplyReceived(params, msg))
  PPAPI_END_MESSAGE_MAP()
}

void VideoDecoderResource::SetForTest() {
  testing_ = true;
}

void VideoDecoderResource::OnPluginMsgRequestTextures(
    const ResourceMessageReplyParams& params,
    uint32_t num_textures,
    const PP_Size& size,
    uint32_t texture_target,
    const std::vector<gpu::Mailbox>& mailboxes) {
  DCHECK(num_textures);
  DCHECK(mailboxes.empty() || mailboxes.size() == num_textures);
  std::vector<uint32_t> texture_ids(num_textures);
  if (gles2_impl_) {
    gles2_impl_->GenTextures(num_textures, &texture_ids.front());
    for (uint32_t i = 0; i < num_textures; ++i) {
      gles2_impl_->ActiveTexture(GL_TEXTURE0);
      gles2_impl_->BindTexture(texture_target, texture_ids[i]);
      gles2_impl_->TexParameteri(
          texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      gles2_impl_->TexParameteri(
          texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      gles2_impl_->TexParameterf(
          texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      gles2_impl_->TexParameterf(
          texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

      if (texture_target == GL_TEXTURE_2D) {
        gles2_impl_->TexImage2D(texture_target,
                                0,
                                GL_RGBA,
                                size.width,
                                size.height,
                                0,
                                GL_RGBA,
                                GL_UNSIGNED_BYTE,
                                NULL);
      }
      if (!mailboxes.empty()) {
        gles2_impl_->ProduceTextureCHROMIUM(
            GL_TEXTURE_2D, reinterpret_cast<const GLbyte*>(mailboxes[i].name));
      }

      textures_.insert(
          std::make_pair(texture_ids[i], Texture(texture_target, size)));
    }
    gles2_impl_->Flush();
  } else {
    DCHECK(testing_);
    // Create some fake texture ids so we can test picture handling.
    for (uint32_t i = 0; i < num_textures; ++i) {
      texture_ids[i] = i + 1;
      textures_.insert(
          std::make_pair(texture_ids[i], Texture(texture_target, size)));
    }
  }

  Post(RENDERER, PpapiHostMsg_VideoDecoder_AssignTextures(size, texture_ids));
}

void VideoDecoderResource::OnPluginMsgPictureReady(
    const ResourceMessageReplyParams& params,
    int32_t decode_id,
    uint32_t texture_id) {
  received_pictures_.push(Picture(decode_id, texture_id));

  if (TrackedCallback::IsPending(get_picture_callback_)) {
    // The plugin may call GetPicture in its callback.
    scoped_refptr<TrackedCallback> callback;
    callback.swap(get_picture_callback_);
    PP_VideoPicture* picture = get_picture_;
    get_picture_ = NULL;
    WriteNextPicture(picture);
    callback->Run(PP_OK);
  }
}

void VideoDecoderResource::OnPluginMsgDismissPicture(
    const ResourceMessageReplyParams& params,
    uint32_t texture_id) {
  DeleteGLTexture(texture_id);
  textures_.erase(texture_id);
}

void VideoDecoderResource::OnPluginMsgNotifyError(
    const ResourceMessageReplyParams& params,
    int32_t error) {
  decoder_last_error_ = error;
  // Cause any pending callbacks to run immediately. Reentrancy isn't a problem,
  // since the plugin wasn't calling us.
  RunCallbackWithError(&initialize_callback_);
  RunCallbackWithError(&decode_callback_);
  RunCallbackWithError(&get_picture_callback_);
  RunCallbackWithError(&flush_callback_);
  RunCallbackWithError(&reset_callback_);
}

void VideoDecoderResource::OnPluginMsgInitializeComplete(
    const ResourceMessageReplyParams& params) {
  decoder_last_error_ = params.result();
  if (decoder_last_error_ == PP_OK)
    initialized_ = true;

  // Let the plugin call Initialize again from its callback in case of failure.
  scoped_refptr<TrackedCallback> callback;
  callback.swap(initialize_callback_);
  callback->Run(decoder_last_error_);
}

void VideoDecoderResource::OnPluginMsgDecodeComplete(
    const ResourceMessageReplyParams& params,
    uint32_t shm_id) {
  if (shm_id >= shm_buffers_.size()) {
    NOTREACHED();
    return;
  }
  // Make the shm buffer available.
  available_shm_buffers_.push_back(shm_buffers_[shm_id]);
  // If the plugin is waiting, let it call Decode again.
  if (decode_callback_) {
    scoped_refptr<TrackedCallback> callback;
    callback.swap(decode_callback_);
    callback->Run(PP_OK);
  }
}

void VideoDecoderResource::OnPluginMsgFlushComplete(
    const ResourceMessageReplyParams& params) {
  // All shm buffers should have been made available by now.
  DCHECK_EQ(shm_buffers_.size(), available_shm_buffers_.size());

  if (get_picture_callback_) {
    scoped_refptr<TrackedCallback> callback;
    callback.swap(get_picture_callback_);
    callback->Abort();
  }

  scoped_refptr<TrackedCallback> callback;
  callback.swap(flush_callback_);
  callback->Run(params.result());
}

void VideoDecoderResource::OnPluginMsgResetComplete(
    const ResourceMessageReplyParams& params) {
  // All shm buffers should have been made available by now.
  DCHECK_EQ(shm_buffers_.size(), available_shm_buffers_.size());
  // Recycle any pictures which haven't been passed to the plugin.
  while (!received_pictures_.empty()) {
    Post(RENDERER, PpapiHostMsg_VideoDecoder_RecyclePicture(
        received_pictures_.front().texture_id));
    received_pictures_.pop();
  }

  scoped_refptr<TrackedCallback> callback;
  callback.swap(reset_callback_);
  callback->Run(params.result());
}

void VideoDecoderResource::RunCallbackWithError(
    scoped_refptr<TrackedCallback>* callback) {
  if (TrackedCallback::IsPending(*callback)) {
    scoped_refptr<TrackedCallback> temp;
    callback->swap(temp);
    temp->Run(decoder_last_error_);
  }
}

void VideoDecoderResource::DeleteGLTexture(uint32_t id) {
  if (gles2_impl_) {
    gles2_impl_->DeleteTextures(1, &id);
    gles2_impl_->Flush();
  }
}

void VideoDecoderResource::WriteNextPicture(PP_VideoPicture* pp_picture) {
  DCHECK(!received_pictures_.empty());
  Picture& picture = received_pictures_.front();
  // Internally, we identify decodes by a unique id, which the host returns
  // to us in the picture. Use this to get the plugin's decode_id.
  pp_picture->decode_id = decode_ids_[picture.decode_id % kMaximumPictureDelay];
  pp_picture->texture_id = picture.texture_id;
  TextureMap::iterator it = textures_.find(picture.texture_id);
  if (it != textures_.end()) {
    pp_picture->texture_target = it->second.texture_target;
    pp_picture->texture_size = it->second.size;
  } else {
    NOTREACHED();
  }
  received_pictures_.pop();
}

}  // namespace proxy
}  // namespace ppapi

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/gpu_video_decoder.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline_status.h"
#include "media/base/surface_manager.h"
#include "media/base/video_decoder_config.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace media {

const char GpuVideoDecoder::kDecoderName[] = "GpuVideoDecoder";

// Maximum number of concurrent VDA::Decode() operations GVD will maintain.
// Higher values allow better pipelining in the GPU, but also require more
// resources.
enum { kMaxInFlightDecodes = 4 };

// Size of shared-memory segments we allocate.  Since we reuse them we let them
// be on the beefy side.
static const size_t kSharedMemorySegmentBytes = 100 << 10;

GpuVideoDecoder::SHMBuffer::SHMBuffer(std::unique_ptr<base::SharedMemory> m,
                                      size_t s)
    : shm(std::move(m)), size(s) {}

GpuVideoDecoder::SHMBuffer::~SHMBuffer() {}

GpuVideoDecoder::PendingDecoderBuffer::PendingDecoderBuffer(
    SHMBuffer* s,
    const scoped_refptr<DecoderBuffer>& b,
    const DecodeCB& done_cb)
    : shm_buffer(s), buffer(b), done_cb(done_cb) {
}

GpuVideoDecoder::PendingDecoderBuffer::PendingDecoderBuffer(
    const PendingDecoderBuffer& other) = default;

GpuVideoDecoder::PendingDecoderBuffer::~PendingDecoderBuffer() {}

GpuVideoDecoder::BufferData::BufferData(int32_t bbid,
                                        base::TimeDelta ts,
                                        const gfx::Rect& vr,
                                        const gfx::Size& ns)
    : bitstream_buffer_id(bbid),
      timestamp(ts),
      visible_rect(vr),
      natural_size(ns) {}

GpuVideoDecoder::BufferData::~BufferData() {}

GpuVideoDecoder::GpuVideoDecoder(GpuVideoAcceleratorFactories* factories,
                                 const RequestSurfaceCB& request_surface_cb,
                                 scoped_refptr<MediaLog> media_log)
    : needs_bitstream_conversion_(false),
      factories_(factories),
      request_surface_cb_(request_surface_cb),
      media_log_(media_log),
      state_(kNormal),
      decoder_texture_target_(0),
      pixel_format_(PIXEL_FORMAT_UNKNOWN),
      next_picture_buffer_id_(0),
      next_bitstream_buffer_id_(0),
      available_pictures_(0),
      needs_all_picture_buffers_to_decode_(false),
      supports_deferred_initialization_(false),
      requires_texture_copy_(false),
      weak_factory_(this) {
  DCHECK(factories_);
}

void GpuVideoDecoder::Reset(const base::Closure& closure)  {
  DVLOG(3) << "Reset()";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  if (state_ == kDrainingDecoder) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&GpuVideoDecoder::Reset,
                              weak_factory_.GetWeakPtr(), closure));
    return;
  }

  if (!vda_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, closure);
    return;
  }

  DCHECK(pending_reset_cb_.is_null());
  pending_reset_cb_ = BindToCurrentLoop(closure);

  vda_->Reset();
}

static bool IsCodedSizeSupported(const gfx::Size& coded_size,
                                 const gfx::Size& min_resolution,
                                 const gfx::Size& max_resolution) {
  return (coded_size.width() <= max_resolution.width() &&
      coded_size.height() <= max_resolution.height() &&
      coded_size.width() >= min_resolution.width() &&
      coded_size.height() >= min_resolution.height());
}

// Report |success| to UMA and run |cb| with it.  This is super-specific to the
// UMA stat reported because the UMA_HISTOGRAM_ENUMERATION API requires a
// callsite to always be called with the same stat name (can't parameterize it).
static void ReportGpuVideoDecoderInitializeStatusToUMAAndRunCB(
    const VideoDecoder::InitCB& cb,
    scoped_refptr<MediaLog> media_log,
    bool success) {
  // TODO(xhwang): Report |success| directly.
  PipelineStatus status = success ? PIPELINE_OK : DECODER_ERROR_NOT_SUPPORTED;
  UMA_HISTOGRAM_ENUMERATION(
      "Media.GpuVideoDecoderInitializeStatus", status, PIPELINE_STATUS_MAX + 1);

  if (!success) {
    media_log->RecordRapporWithSecurityOrigin(
        "Media.OriginUrl.GpuVideoDecoderInitFailure");
  }

  cb.Run(success);
}

// static
void ReleaseMailboxTrampoline(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const VideoFrame::ReleaseMailboxCB& release_mailbox_cb,
    const gpu::SyncToken& release_sync_token) {
  if (task_runner->BelongsToCurrentThread()) {
    release_mailbox_cb.Run(release_sync_token);
    return;
  }

  task_runner->PostTask(FROM_HERE,
                        base::Bind(release_mailbox_cb, release_sync_token));
}

std::string GpuVideoDecoder::GetDisplayName() const {
  return kDecoderName;
}

void GpuVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                 bool /* low_delay */,
                                 CdmContext* cdm_context,
                                 const InitCB& init_cb,
                                 const OutputCB& output_cb) {
  DVLOG(3) << "Initialize()";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DCHECK(config.IsValidConfig());

  InitCB bound_init_cb =
      base::Bind(&ReportGpuVideoDecoderInitializeStatusToUMAAndRunCB,
                 BindToCurrentLoop(init_cb), media_log_);

#if !defined(OS_ANDROID)
  if (config.is_encrypted()) {
    DVLOG(1) << "Encrypted stream not supported.";
    bound_init_cb.Run(false);
    return;
  }
#endif

  bool previously_initialized = config_.IsValidConfig();
  DVLOG(1) << (previously_initialized ? "Reinitializing" : "Initializing")
           << " GVD with config: " << config.AsHumanReadableString();

  // TODO(posciak): destroy and create a new VDA on codec/profile change
  // (http://crbug.com/260224).
  if (previously_initialized && (config_.profile() != config.profile())) {
    DVLOG(1) << "Codec or profile changed, cannot reinitialize.";
    bound_init_cb.Run(false);
    return;
  }

  // TODO(sandersd): This should be moved to capabilities if we ever have a
  // hardware decoder which supports alpha formats.
  if (config.format() == PIXEL_FORMAT_YV12A) {
    DVLOG(1) << "Alpha transparency formats are not supported.";
    bound_init_cb.Run(false);
    return;
  }

  VideoDecodeAccelerator::Capabilities capabilities =
      factories_->GetVideoDecodeAcceleratorCapabilities();
  if (!IsProfileSupported(capabilities, config.profile(), config.coded_size(),
                          config.is_encrypted())) {
    DVLOG(1) << "Unsupported profile " << config.profile()
             << ", unsupported coded size " << config.coded_size().ToString()
             << ", or accelerator should only be used for encrypted content. "
             << " is_encrypted: " << (config.is_encrypted() ? "yes." : "no.");
    bound_init_cb.Run(false);
    return;
  }

  config_ = config;
  needs_all_picture_buffers_to_decode_ =
      capabilities.flags &
      VideoDecodeAccelerator::Capabilities::NEEDS_ALL_PICTURE_BUFFERS_TO_DECODE;
  needs_bitstream_conversion_ = (config.codec() == kCodecH264);
  requires_texture_copy_ =
      !!(capabilities.flags &
         VideoDecodeAccelerator::Capabilities::REQUIRES_TEXTURE_COPY);
  supports_deferred_initialization_ = !!(
      capabilities.flags &
      VideoDecodeAccelerator::Capabilities::SUPPORTS_DEFERRED_INITIALIZATION);
  output_cb_ = output_cb;

  if (config.is_encrypted() && !supports_deferred_initialization_) {
    DVLOG(1) << __FUNCTION__
             << " Encrypted stream requires deferred initialialization.";
    bound_init_cb.Run(false);
    return;
  }

  if (previously_initialized) {
    DVLOG(3) << __FUNCTION__
             << " Expecting initialized VDA to detect in-stream config change.";
    // Reinitialization with a different config (but same codec and profile).
    // VDA should handle it by detecting this in-stream by itself,
    // no need to notify it.
    bound_init_cb.Run(true);
    return;
  }

  vda_ = factories_->CreateVideoDecodeAccelerator();
  if (!vda_) {
    DVLOG(1) << "Failed to create a VDA.";
    bound_init_cb.Run(false);
    return;
  }

  int cdm_id = CdmContext::kInvalidCdmId;
  if (config.is_encrypted()) {
    DCHECK(cdm_context);
    cdm_id = cdm_context->GetCdmId();
    // No need to store |cdm_context| since it's not needed in reinitialization.
    if (cdm_id == CdmContext::kInvalidCdmId) {
      DVLOG(1) << "CDM ID not available.";
      bound_init_cb.Run(false);
      return;
    }
  }

  init_cb_ = bound_init_cb;

  const bool supports_external_output_surface =
      (capabilities.flags & VideoDecodeAccelerator::Capabilities::
                                SUPPORTS_EXTERNAL_OUTPUT_SURFACE) != 0;
  if (supports_external_output_surface && !request_surface_cb_.is_null()) {
    // If we have a surface request callback we should call it and complete
    // initialization with the returned surface.
    request_surface_cb_.Run(
        BindToCurrentLoop(base::Bind(&GpuVideoDecoder::CompleteInitialization,
                                     weak_factory_.GetWeakPtr(), cdm_id)));
    return;
  }

  // If we don't have to wait for a surface complete initialization with a null
  // surface.
  CompleteInitialization(cdm_id, SurfaceManager::kNoSurfaceID);
}

void GpuVideoDecoder::CompleteInitialization(int cdm_id, int surface_id) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DCHECK(!init_cb_.is_null());

  // It's possible for the vda to become null if NotifyError is called.
  if (!vda_) {
    base::ResetAndReturn(&init_cb_).Run(false);
    return;
  }

  VideoDecodeAccelerator::Config vda_config;
  vda_config.profile = config_.profile();
  vda_config.cdm_id = cdm_id;
  vda_config.is_encrypted = config_.is_encrypted();
  vda_config.surface_id = surface_id;
  vda_config.is_deferred_initialization_allowed = true;
  vda_config.initial_expected_coded_size = config_.coded_size();
  if (!vda_->Initialize(vda_config, this)) {
    DVLOG(1) << "VDA::Initialize failed.";
    base::ResetAndReturn(&init_cb_).Run(false);
    return;
  }

  // If deferred initialization is not supported, initialization is complete.
  // Otherwise, a call to NotifyInitializationComplete will follow with the
  // result of deferred initialization.
  if (!supports_deferred_initialization_)
    base::ResetAndReturn(&init_cb_).Run(true);
}

void GpuVideoDecoder::NotifyInitializationComplete(bool success) {
  DVLOG_IF(1, !success) << __FUNCTION__ << " Deferred initialization failed.";
  DCHECK(!init_cb_.is_null());

  base::ResetAndReturn(&init_cb_).Run(success);
}

void GpuVideoDecoder::DestroyPictureBuffers(PictureBufferMap* buffers) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  for (PictureBufferMap::iterator it = buffers->begin(); it != buffers->end();
       ++it) {
    for (uint32_t id : it->second.texture_ids())
      factories_->DeleteTexture(id);
  }

  buffers->clear();
}

void GpuVideoDecoder::DestroyVDA() {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  vda_.reset();

  // Not destroying PictureBuffers in |picture_buffers_at_display_| yet, since
  // their textures may still be in use by the user of this GpuVideoDecoder.
  for (PictureBufferTextureMap::iterator it =
           picture_buffers_at_display_.begin();
       it != picture_buffers_at_display_.end();
       ++it) {
    assigned_picture_buffers_.erase(it->first);
  }
  DestroyPictureBuffers(&assigned_picture_buffers_);
}

void GpuVideoDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                             const DecodeCB& decode_cb) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DCHECK(pending_reset_cb_.is_null());

  DVLOG(3) << __FUNCTION__ << " " << buffer->AsHumanReadableString();

  DecodeCB bound_decode_cb = BindToCurrentLoop(decode_cb);

  if (state_ == kError || !vda_) {
    bound_decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  switch (state_) {
    case kDecoderDrained:
      state_ = kNormal;
      // Fall-through.
    case kNormal:
      break;
    case kDrainingDecoder:
    case kError:
      NOTREACHED();
      return;
  }

  DCHECK_EQ(state_, kNormal);

  if (buffer->end_of_stream()) {
    DVLOG(3) << __FUNCTION__ << " Initiating Flush for EOS.";
    state_ = kDrainingDecoder;
    eos_decode_cb_ = bound_decode_cb;
    vda_->Flush();
    return;
  }

  size_t size = buffer->data_size();
  std::unique_ptr<SHMBuffer> shm_buffer = GetSHM(size);
  if (!shm_buffer) {
    bound_decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  memcpy(shm_buffer->shm->memory(), buffer->data(), size);
  // AndroidVideoDecodeAccelerator needs the timestamp to output frames in
  // presentation order.
  BitstreamBuffer bitstream_buffer(next_bitstream_buffer_id_,
                                   shm_buffer->shm->handle(), size, 0,
                                   buffer->timestamp());

  if (buffer->decrypt_config())
    bitstream_buffer.SetDecryptConfig(*buffer->decrypt_config());

  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x3FFFFFFF;
  DCHECK(!ContainsKey(bitstream_buffers_in_decoder_, bitstream_buffer.id()));
  bitstream_buffers_in_decoder_.insert(std::make_pair(
      bitstream_buffer.id(),
      PendingDecoderBuffer(shm_buffer.release(), buffer, decode_cb)));
  DCHECK_LE(static_cast<int>(bitstream_buffers_in_decoder_.size()),
            kMaxInFlightDecodes);
  RecordBufferData(bitstream_buffer, *buffer.get());

  vda_->Decode(bitstream_buffer);
}

void GpuVideoDecoder::RecordBufferData(const BitstreamBuffer& bitstream_buffer,
                                       const DecoderBuffer& buffer) {
  input_buffer_data_.push_front(BufferData(bitstream_buffer.id(),
                                           buffer.timestamp(),
                                           config_.visible_rect(),
                                           config_.natural_size()));
  // Why this value?  Because why not.  avformat.h:MAX_REORDER_DELAY is 16, but
  // that's too small for some pathological B-frame test videos.  The cost of
  // using too-high a value is low (192 bits per extra slot).
  static const size_t kMaxInputBufferDataSize = 128;
  // Pop from the back of the list, because that's the oldest and least likely
  // to be useful in the future data.
  if (input_buffer_data_.size() > kMaxInputBufferDataSize)
    input_buffer_data_.pop_back();
}

void GpuVideoDecoder::GetBufferData(int32_t id,
                                    base::TimeDelta* timestamp,
                                    gfx::Rect* visible_rect,
                                    gfx::Size* natural_size) {
  for (std::list<BufferData>::const_iterator it =
           input_buffer_data_.begin(); it != input_buffer_data_.end();
       ++it) {
    if (it->bitstream_buffer_id != id)
      continue;
    *timestamp = it->timestamp;
    *visible_rect = it->visible_rect;
    *natural_size = it->natural_size;
    return;
  }
  NOTREACHED() << "Missing bitstreambuffer id: " << id;
}

bool GpuVideoDecoder::NeedsBitstreamConversion() const {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  return needs_bitstream_conversion_;
}

bool GpuVideoDecoder::CanReadWithoutStalling() const {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  return next_picture_buffer_id_ ==
             0 ||  // Decode() will ProvidePictureBuffers().
         (!needs_all_picture_buffers_to_decode_ && available_pictures_ > 0) ||
         available_pictures_ ==
             static_cast<int>(assigned_picture_buffers_.size());
}

int GpuVideoDecoder::GetMaxDecodeRequests() const {
  return kMaxInFlightDecodes;
}

void GpuVideoDecoder::ProvidePictureBuffers(uint32_t count,
                                            VideoPixelFormat format,
                                            uint32_t textures_per_buffer,
                                            const gfx::Size& size,
                                            uint32_t texture_target) {
  DVLOG(3) << "ProvidePictureBuffers(" << count << ", "
           << size.width() << "x" << size.height() << ")";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  std::vector<uint32_t> texture_ids;
  std::vector<gpu::Mailbox> texture_mailboxes;
  decoder_texture_target_ = texture_target;

  if (format == PIXEL_FORMAT_UNKNOWN) {
    format = IsOpaque(config_.format()) ? PIXEL_FORMAT_XRGB : PIXEL_FORMAT_ARGB;
  }

  // TODO(jbauman): Move decoder_texture_target_ and pixel_format_ to the
  // picture buffer. http://crbug.com/614789
  if ((pixel_format_ != PIXEL_FORMAT_UNKNOWN) && (pixel_format_ != format)) {
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }

  pixel_format_ = format;
  if (!factories_->CreateTextures(count * textures_per_buffer, size,
                                  &texture_ids, &texture_mailboxes,
                                  decoder_texture_target_)) {
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  DCHECK_EQ(count * textures_per_buffer, texture_ids.size());
  DCHECK_EQ(count * textures_per_buffer, texture_mailboxes.size());

  if (!vda_)
    return;

  std::vector<PictureBuffer> picture_buffers;
  size_t index = 0;
  for (size_t i = 0; i < count; ++i) {
    PictureBuffer::TextureIds ids;
    std::vector<gpu::Mailbox> mailboxes;
    for (size_t j = 0; j < textures_per_buffer; j++) {
      ids.push_back(texture_ids[index]);
      mailboxes.push_back(texture_mailboxes[index]);
      index++;
    }

    picture_buffers.push_back(
        PictureBuffer(next_picture_buffer_id_++, size, ids, mailboxes));
    bool inserted = assigned_picture_buffers_.insert(std::make_pair(
        picture_buffers.back().id(), picture_buffers.back())).second;
    DCHECK(inserted);
  }

  available_pictures_ += count;

  vda_->AssignPictureBuffers(picture_buffers);
}

void GpuVideoDecoder::DismissPictureBuffer(int32_t id) {
  DVLOG(3) << "DismissPictureBuffer(" << id << ")";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  PictureBufferMap::iterator it = assigned_picture_buffers_.find(id);
  if (it == assigned_picture_buffers_.end()) {
    NOTREACHED() << "Missing picture buffer: " << id;
    return;
  }

  PictureBuffer buffer_to_dismiss = it->second;
  assigned_picture_buffers_.erase(it);

  if (!picture_buffers_at_display_.count(id)) {
    // We can delete the texture immediately as it's not being displayed.
    for (uint32_t id : buffer_to_dismiss.texture_ids())
      factories_->DeleteTexture(id);
    CHECK_GT(available_pictures_, 0);
    --available_pictures_;
  }
  // Not destroying a texture in display in |picture_buffers_at_display_|.
  // Postpone deletion until after it's returned to us.
}

void GpuVideoDecoder::PictureReady(const media::Picture& picture) {
  DVLOG(3) << "PictureReady()";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  PictureBufferMap::iterator it =
      assigned_picture_buffers_.find(picture.picture_buffer_id());
  if (it == assigned_picture_buffers_.end()) {
    DLOG(ERROR) << "Missing picture buffer: " << picture.picture_buffer_id();
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  PictureBuffer& pb = it->second;
  if (picture.size_changed()) {
    // Update the PictureBuffer size to match that of the Picture. Some VDA's
    // (e.g. Android) will handle resolution changes internally without
    // requesting new PictureBuffers. Sending a Picture of unmatched size is
    // the signal that we should update the size of our PictureBuffer.
    DCHECK(pb.size() != picture.visible_rect().size());
    DVLOG(3) << __FUNCTION__ << " Updating size of PictureBuffer[" << pb.id()
             << "] from:" << pb.size().ToString()
             << " to:" << picture.visible_rect().size().ToString();
    pb.set_size(picture.visible_rect().size());
  }

  // Update frame's timestamp.
  base::TimeDelta timestamp;
  // Some of the VDAs like DXVA, and VTVDA don't support and thus don't provide
  // us with visible size in picture.size, passing (0, 0) instead, so for those
  // cases drop it and use config information instead.
  gfx::Rect visible_rect;
  gfx::Size natural_size;
  GetBufferData(picture.bitstream_buffer_id(), &timestamp, &visible_rect,
                &natural_size);

  if (!picture.visible_rect().IsEmpty()) {
    visible_rect = picture.visible_rect();
  }
  if (!gfx::Rect(pb.size()).Contains(visible_rect)) {
    LOG(WARNING) << "Visible size " << visible_rect.ToString()
                 << " is larger than coded size " << pb.size().ToString();
    visible_rect = gfx::Rect(pb.size());
  }

  DCHECK(decoder_texture_target_);

  gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
  for (size_t i = 0; i < pb.texture_ids().size(); ++i) {
    mailbox_holders[i] = gpu::MailboxHolder(
        pb.texture_mailbox(i), gpu::SyncToken(), decoder_texture_target_);
  }

  scoped_refptr<VideoFrame> frame(VideoFrame::WrapNativeTextures(
      pixel_format_, mailbox_holders,
      base::Bind(&ReleaseMailboxTrampoline, factories_->GetTaskRunner(),
                 base::Bind(&GpuVideoDecoder::ReleaseMailbox,
                            weak_factory_.GetWeakPtr(), factories_,
                            picture.picture_buffer_id(), pb.texture_ids())),
      pb.size(), visible_rect, natural_size, timestamp));
  if (!frame) {
    DLOG(ERROR) << "Create frame failed for: " << picture.picture_buffer_id();
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  if (picture.allow_overlay())
    frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY, true);
#if defined(OS_MACOSX) || defined(OS_WIN)
  frame->metadata()->SetBoolean(VideoFrameMetadata::DECODER_OWNS_FRAME, true);
#endif

  if (requires_texture_copy_)
    frame->metadata()->SetBoolean(VideoFrameMetadata::COPY_REQUIRED, true);

  CHECK_GT(available_pictures_, 0);
  --available_pictures_;

  bool inserted =
      picture_buffers_at_display_
          .insert(std::make_pair(picture.picture_buffer_id(), pb.texture_ids()))
          .second;
  DCHECK(inserted);

  DeliverFrame(frame);
}

void GpuVideoDecoder::DeliverFrame(
    const scoped_refptr<VideoFrame>& frame) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  // During a pending vda->Reset(), we don't accumulate frames.  Drop it on the
  // floor and return.
  if (!pending_reset_cb_.is_null())
    return;

  output_cb_.Run(frame);
}

// static
void GpuVideoDecoder::ReleaseMailbox(
    base::WeakPtr<GpuVideoDecoder> decoder,
    media::GpuVideoAcceleratorFactories* factories,
    int64_t picture_buffer_id,
    PictureBuffer::TextureIds ids,
    const gpu::SyncToken& release_sync_token) {
  DCHECK(factories->GetTaskRunner()->BelongsToCurrentThread());
  factories->WaitSyncToken(release_sync_token);

  if (decoder) {
    decoder->ReusePictureBuffer(picture_buffer_id);
    return;
  }
  // It's the last chance to delete the texture after display,
  // because GpuVideoDecoder was destructed.
  for (uint32_t id : ids)
    factories->DeleteTexture(id);
}

void GpuVideoDecoder::ReusePictureBuffer(int64_t picture_buffer_id) {
  DVLOG(3) << "ReusePictureBuffer(" << picture_buffer_id << ")";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  DCHECK(!picture_buffers_at_display_.empty());
  PictureBufferTextureMap::iterator display_iterator =
      picture_buffers_at_display_.find(picture_buffer_id);
  PictureBuffer::TextureIds ids = display_iterator->second;
  DCHECK(display_iterator != picture_buffers_at_display_.end());
  picture_buffers_at_display_.erase(display_iterator);

  if (!assigned_picture_buffers_.count(picture_buffer_id)) {
    // This picture was dismissed while in display, so we postponed deletion.
    for (uint32_t id : ids)
      factories_->DeleteTexture(id);
    return;
  }

  ++available_pictures_;

  // DestroyVDA() might already have been called.
  if (vda_)
    vda_->ReusePictureBuffer(picture_buffer_id);
}

std::unique_ptr<GpuVideoDecoder::SHMBuffer> GpuVideoDecoder::GetSHM(
    size_t min_size) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  if (available_shm_segments_.empty() ||
      available_shm_segments_.back()->size < min_size) {
    size_t size_to_allocate = std::max(min_size, kSharedMemorySegmentBytes);
    std::unique_ptr<base::SharedMemory> shm =
        factories_->CreateSharedMemory(size_to_allocate);
    // CreateSharedMemory() can return NULL during Shutdown.
    if (!shm)
      return NULL;
    return base::WrapUnique(new SHMBuffer(std::move(shm), size_to_allocate));
  }
  std::unique_ptr<SHMBuffer> ret(available_shm_segments_.back());
  available_shm_segments_.pop_back();
  return ret;
}

void GpuVideoDecoder::PutSHM(std::unique_ptr<SHMBuffer> shm_buffer) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  available_shm_segments_.push_back(shm_buffer.release());
}

void GpuVideoDecoder::NotifyEndOfBitstreamBuffer(int32_t id) {
  DVLOG(3) << "NotifyEndOfBitstreamBuffer(" << id << ")";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  std::map<int32_t, PendingDecoderBuffer>::iterator it =
      bitstream_buffers_in_decoder_.find(id);
  if (it == bitstream_buffers_in_decoder_.end()) {
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    NOTREACHED() << "Missing bitstream buffer: " << id;
    return;
  }

  PutSHM(base::WrapUnique(it->second.shm_buffer));
  it->second.done_cb.Run(state_ == kError ? DecodeStatus::DECODE_ERROR
                                          : DecodeStatus::OK);
  bitstream_buffers_in_decoder_.erase(it);
}

GpuVideoDecoder::~GpuVideoDecoder() {
  DVLOG(3) << __FUNCTION__;
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  if (vda_)
    DestroyVDA();
  DCHECK(assigned_picture_buffers_.empty());

  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(false);
  if (!request_surface_cb_.is_null())
    base::ResetAndReturn(&request_surface_cb_).Run(SurfaceCreatedCB());

  for (size_t i = 0; i < available_shm_segments_.size(); ++i) {
    delete available_shm_segments_[i];
  }
  available_shm_segments_.clear();

  for (std::map<int32_t, PendingDecoderBuffer>::iterator it =
           bitstream_buffers_in_decoder_.begin();
       it != bitstream_buffers_in_decoder_.end(); ++it) {
    delete it->second.shm_buffer;
    it->second.done_cb.Run(DecodeStatus::ABORTED);
  }
  bitstream_buffers_in_decoder_.clear();

  if (!pending_reset_cb_.is_null())
    base::ResetAndReturn(&pending_reset_cb_).Run();
}

void GpuVideoDecoder::NotifyFlushDone() {
  DVLOG(3) << "NotifyFlushDone()";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DCHECK_EQ(state_, kDrainingDecoder);
  state_ = kDecoderDrained;
  base::ResetAndReturn(&eos_decode_cb_).Run(DecodeStatus::OK);
}

void GpuVideoDecoder::NotifyResetDone() {
  DVLOG(3) << "NotifyResetDone()";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DCHECK(bitstream_buffers_in_decoder_.empty());

  // This needs to happen after the Reset() on vda_ is done to ensure pictures
  // delivered during the reset can find their time data.
  input_buffer_data_.clear();

  if (!pending_reset_cb_.is_null())
    base::ResetAndReturn(&pending_reset_cb_).Run();
}

void GpuVideoDecoder::NotifyError(media::VideoDecodeAccelerator::Error error) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  if (!vda_)
    return;

  state_ = kError;

  // If we have any bitstream buffers, then notify one that an error has
  // occurred.  This guarantees that somebody finds out about the error.  If
  // we don't do this, and if the max decodes are already in flight, then there
  // won't be another decode request to report the error.
  if (!bitstream_buffers_in_decoder_.empty()) {
    auto it = bitstream_buffers_in_decoder_.begin();
    it->second.done_cb.Run(DecodeStatus::DECODE_ERROR);
    bitstream_buffers_in_decoder_.erase(it);
  }

  DLOG(ERROR) << "VDA Error: " << error;
  UMA_HISTOGRAM_ENUMERATION("Media.GpuVideoDecoderError", error,
                            media::VideoDecodeAccelerator::ERROR_MAX + 1);

  DestroyVDA();
}

bool GpuVideoDecoder::IsProfileSupported(
    const VideoDecodeAccelerator::Capabilities& capabilities,
    VideoCodecProfile profile,
    const gfx::Size& coded_size,
    bool is_encrypted) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  for (const auto& supported_profile : capabilities.supported_profiles) {
    if (profile == supported_profile.profile) {
      if (supported_profile.encrypted_only && !is_encrypted)
        continue;

      return IsCodedSizeSupported(coded_size,
                                  supported_profile.min_resolution,
                                  supported_profile.max_resolution);
    }
  }
  return false;
}

void GpuVideoDecoder::DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent()
    const {
  DCHECK(factories_->GetTaskRunner()->BelongsToCurrentThread());
}

}  // namespace media

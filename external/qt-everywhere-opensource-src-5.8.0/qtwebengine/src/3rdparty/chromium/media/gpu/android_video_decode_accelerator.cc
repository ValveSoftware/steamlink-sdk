// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android_video_decode_accelerator.h"

#include <stddef.h>

#include <memory>

#include "base/android/build_info.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/limits.h"
#include "media/base/media.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/android_copying_backing_strategy.h"
#include "media/gpu/android_deferred_rendering_backing_strategy.h"
#include "media/gpu/avda_return_on_failure.h"
#include "media/gpu/shared_memory_region.h"
#include "media/video/picture.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"

#if defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
#include "media/mojo/services/mojo_cdm_service.h"
#endif

#define POST_ERROR(error_code, error_message)                 \
  do {                                                        \
    DLOG(ERROR) << error_message;                             \
    PostError(FROM_HERE, VideoDecodeAccelerator::error_code); \
  } while (0)

namespace media {

enum { kNumPictureBuffers = limits::kMaxVideoFrames + 1 };

// Max number of bitstreams notified to the client with
// NotifyEndOfBitstreamBuffer() before getting output from the bitstream.
enum { kMaxBitstreamsNotifiedInAdvance = 32 };

// MediaCodec is only guaranteed to support baseline, but some devices may
// support others. Advertise support for all H264 profiles and let the
// MediaCodec fail when decoding if it's not actually supported. It's assumed
// that consumers won't have software fallback for H264 on Android anyway.
static const VideoCodecProfile kSupportedH264Profiles[] = {
    H264PROFILE_BASELINE,
    H264PROFILE_MAIN,
    H264PROFILE_EXTENDED,
    H264PROFILE_HIGH,
    H264PROFILE_HIGH10PROFILE,
    H264PROFILE_HIGH422PROFILE,
    H264PROFILE_HIGH444PREDICTIVEPROFILE,
    H264PROFILE_SCALABLEBASELINE,
    H264PROFILE_SCALABLEHIGH,
    H264PROFILE_STEREOHIGH,
    H264PROFILE_MULTIVIEWHIGH};

// Because MediaCodec is thread-hostile (must be poked on a single thread) and
// has no callback mechanism (b/11990118), we must drive it by polling for
// complete frames (and available input buffers, when the codec is fully
// saturated).  This function defines the polling delay.  The value used is an
// arbitrary choice that trades off CPU utilization (spinning) against latency.
// Mirrors android_video_encode_accelerator.cc:EncodePollDelay().
static inline const base::TimeDelta DecodePollDelay() {
  // An alternative to this polling scheme could be to dedicate a new thread
  // (instead of using the ChildThread) to run the MediaCodec, and make that
  // thread use the timeout-based flavor of MediaCodec's dequeue methods when it
  // believes the codec should complete "soon" (e.g. waiting for an input
  // buffer, or waiting for a picture when it knows enough complete input
  // pictures have been fed to saturate any internal buffering).  This is
  // speculative and it's unclear that this would be a win (nor that there's a
  // reasonably device-agnostic way to fill in the "believes" above).
  return base::TimeDelta::FromMilliseconds(10);
}

static inline const base::TimeDelta NoWaitTimeOut() {
  return base::TimeDelta::FromMicroseconds(0);
}

static inline const base::TimeDelta IdleTimerTimeOut() {
  return base::TimeDelta::FromSeconds(1);
}

// Time between when we notice an error, and when we actually notify somebody.
// This is to prevent codec errors caused by SurfaceView fullscreen transitions
// from breaking the pipeline, if we're about to be reset anyway.
static inline const base::TimeDelta ErrorPostingDelay() {
  return base::TimeDelta::FromSeconds(2);
}

// For RecordFormatChangedMetric.
enum FormatChangedValue {
  CodecInitialized = false,
  MissingFormatChanged = true
};

// Maximum number of concurrent, incomplete codec creations that we'll allow
// before turning off autodection of codec type.
enum { kMaxConcurrentCodecAutodetections = 4 };

static inline void RecordFormatChangedMetric(FormatChangedValue value) {
  UMA_HISTOGRAM_BOOLEAN("Media.AVDA.MissingFormatChanged", !!value);
}

// Handle OnFrameAvailable callbacks safely.  Since they occur asynchronously,
// we take care that the AVDA that wants them still exists.  A WeakPtr to
// the AVDA would be preferable, except that OnFrameAvailable callbacks can
// occur off the gpu main thread.  We also can't guarantee when the
// SurfaceTexture will quit sending callbacks to coordinate with the
// destruction of the AVDA, so we have a separate object that the cb can own.
class AndroidVideoDecodeAccelerator::OnFrameAvailableHandler
    : public base::RefCountedThreadSafe<OnFrameAvailableHandler> {
 public:
  // We do not retain ownership of |owner|.  It must remain valid until
  // after ClearOwner() is called.  This will register with
  // |surface_texture| to receive OnFrameAvailable callbacks.
  OnFrameAvailableHandler(
      AndroidVideoDecodeAccelerator* owner,
      const scoped_refptr<gl::SurfaceTexture>& surface_texture)
      : owner_(owner) {
    // Note that the callback owns a strong ref to us.
    surface_texture->SetFrameAvailableCallbackOnAnyThread(
        base::Bind(&OnFrameAvailableHandler::OnFrameAvailable,
                   scoped_refptr<OnFrameAvailableHandler>(this)));
  }

  // Forget about our owner, which is required before one deletes it.
  // No further callbacks will happen once this completes.
  void ClearOwner() {
    base::AutoLock lock(lock_);
    // No callback can happen until we release the lock.
    owner_ = nullptr;
  }

  // Call back into our owner if it hasn't been deleted.
  void OnFrameAvailable() {
    base::AutoLock auto_lock(lock_);
    // |owner_| can't be deleted while we have the lock.
    if (owner_)
      owner_->OnFrameAvailable();
  }

 private:
  friend class base::RefCountedThreadSafe<OnFrameAvailableHandler>;
  virtual ~OnFrameAvailableHandler() {}

  // Protects changes to owner_.
  base::Lock lock_;

  // AVDA that wants the OnFrameAvailable callback.
  AndroidVideoDecodeAccelerator* owner_;

  DISALLOW_COPY_AND_ASSIGN(OnFrameAvailableHandler);
};

// Helper class to share an IO timer for DoIOTask() execution; prevents each
// AVDA instance from starting its own high frequency timer.  The intuition
// behind this is that, if we're waiting for long enough, then either (a)
// MediaCodec is broken or (b) MediaCodec is waiting on us to change state
// (e.g., get new demuxed data / get a free picture buffer / return an output
// buffer to MediaCodec).  This is inherently a race, since we don't know if
// MediaCodec is broken or just slow.  Since the MediaCodec API doesn't let
// us wait on MediaCodec state changes prior to L, we more or less have to
// time out or keep polling forever in some common cases.
class AVDATimerManager {
 public:
  // Make sure that the construction thread is started for |avda_instance|.
  bool StartThread(AndroidVideoDecodeAccelerator* avda_instance) {
    DCHECK(thread_checker_.CalledOnValidThread());

    // If we chose not to shut it down due to pending codec constructions, then
    // the thread might already be started even if there are no avda instances.
    // Plus, sometimes we just fail to start the thread.
    if (!construction_thread_.IsRunning()) {
      if (!construction_thread_.Start()) {
        LOG(ERROR) << "Failed to start construction thread.";
        return false;
      }
    }

    thread_avda_instances_.insert(avda_instance);
    return true;
  }

  // |avda_instance| will no longer need the construction thread.  Stop the
  // thread if this is the last instance.
  void StopThread(AndroidVideoDecodeAccelerator* avda_instance) {
    DCHECK(thread_checker_.CalledOnValidThread());

    thread_avda_instances_.erase(avda_instance);
    if (!thread_avda_instances_.empty())
      return;

    // Don't stop the thread if there are outstanding requests, since they
    // might be hung.  They also might simply be incomplete, and the thread
    // will stay running until we try to shut it down again.
    base::AutoLock auto_lock(autodetection_info_.lock_);
    if (autodetection_info_.outstanding_)
      return;

    construction_thread_.Stop();
  }

  // Request periodic callback of |avda_instance|->DoIOTask(). Does nothing if
  // the instance is already registered and the timer started. The first request
  // will start the repeating timer on an interval of DecodePollDelay().
  void StartTimer(AndroidVideoDecodeAccelerator* avda_instance) {
    DCHECK(thread_checker_.CalledOnValidThread());

    timer_avda_instances_.insert(avda_instance);

    // If the timer is running, StopTimer() might have been called earlier, if
    // so remove the instance from the pending erasures.
    if (timer_running_)
      pending_erase_.erase(avda_instance);

    if (io_timer_.IsRunning())
      return;
    io_timer_.Start(FROM_HERE, DecodePollDelay(), this,
                    &AVDATimerManager::RunTimer);
  }

  // Stop callbacks to |avda_instance|->DoIOTask(). Does nothing if the instance
  // is not registered. If there are no instances left, the repeating timer will
  // be stopped.
  void StopTimer(AndroidVideoDecodeAccelerator* avda_instance) {
    DCHECK(thread_checker_.CalledOnValidThread());

    // If the timer is running, defer erasures to avoid iterator invalidation.
    if (timer_running_) {
      pending_erase_.insert(avda_instance);
      return;
    }

    timer_avda_instances_.erase(avda_instance);
    if (timer_avda_instances_.empty())
      io_timer_.Stop();
  }

  // Eventually, we should run the timer on this thread.  For now, we just keep
  // it as a convenience for construction.
  scoped_refptr<base::SingleThreadTaskRunner> ConstructionTaskRunner() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return construction_thread_.task_runner();
  }

  // Called on the main thread when the construction thread will be doing work
  // that can potentially hang (e.g., autodetection).  There may be several
  // calls to this before any call to DoneUsingConstructionThread.
  // Note that this should only be called from the main thread, else it's a race
  // with IsCodecAutodetectionProbablySafe.
  void StartUsingConstructionThread() {
    DCHECK(thread_checker_.CalledOnValidThread());
    base::AutoLock auto_lock(autodetection_info_.lock_);
    ++autodetection_info_.outstanding_;
  }

  // Called on any thread after the potentially dangerous construction thread
  // work completes safely.  May be called on any thread, including the
  // construction thread.
  // This assumes that requests are ordered, so please don't mix sync and async
  // codec construction here.
  void DoneUsingConstructionThread() {
    base::AutoLock auto_lock_l(autodetection_info_.lock_);
    DCHECK_GT(autodetection_info_.outstanding_, 0);
    --autodetection_info_.outstanding_;
  }

  // Return a hint about whether autodetecting the codec type is safe or not.
  bool IsCodecAutodetectionProbablySafe() {
    base::AutoLock auto_lock_l(autodetection_info_.lock_);

    return autodetection_info_.outstanding_ < kMaxConcurrentCodecAutodetections;
  }

  // |avda| would like to use |surface_id|.  If it is not busy, then mark it
  // as busy and return true.  If it is busy, then replace any existing waiter,
  // make |avda| the current waiter, and return false.  Any existing waiter
  // is assumed to be on the way out, so we fail its allocation request.
  bool AllocateSurface(int surface_id, AndroidVideoDecodeAccelerator* avda) {
    // Nobody has to wait for no surface.
    if (surface_id == AndroidVideoDecodeAccelerator::Config::kNoSurfaceID)
      return true;

    auto iter = surface_waiter_map_.find(surface_id);
    if (iter == surface_waiter_map_.end()) {
      // SurfaceView isn't allocated.  Succeed.
      surface_waiter_map_[surface_id].owner = avda;
      return true;
    }

    // SurfaceView is already allocated.
    if (iter->second.waiter) {
      // Some other AVDA is waiting.  |avda| will replace it, so notify it
      // that it will fail.
      iter->second.waiter->OnSurfaceAvailable(false);
      iter->second.waiter = nullptr;
    }

    // |avda| is now waiting.
    iter->second.waiter = avda;
    return false;
  }

  // Clear any waiting request for |surface_id| by |avda|.  It is okay if
  // |waiter| is not waiting and/or isn't the owner of |surface_id|.
  void DeallocateSurface(int surface_id, AndroidVideoDecodeAccelerator* avda) {
    SurfaceWaiterMap::iterator iter = surface_waiter_map_.find(surface_id);
    if (iter == surface_waiter_map_.end())
      return;

    // If |avda| was waiting, then remove it without OnSurfaceAvailable.
    if (iter->second.waiter == avda)
      iter->second.waiter = nullptr;

    // If |avda| is the owner, then let the waiter have it.
    if (iter->second.owner != avda)
      return;

    AndroidVideoDecodeAccelerator* waiter = iter->second.waiter;
    if (!waiter) {
      // No waiter -- remove the record and return explicitly since |iter| is
      // no longer valid.
      surface_waiter_map_.erase(iter);
      return;
    }

    // Promote |waiter| to be the owner.
    iter->second.owner = waiter;
    iter->second.waiter = nullptr;
    waiter->OnSurfaceAvailable(true);
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<AVDATimerManager>;

  AVDATimerManager() : construction_thread_("AVDAThread") {}
  ~AVDATimerManager() { NOTREACHED(); }

  void RunTimer() {
    {
      // Call out to all AVDA instances, some of which may attempt to remove
      // themselves from the list during this operation; those removals will be
      // deferred until after all iterations are complete.
      base::AutoReset<bool> scoper(&timer_running_, true);
      for (auto* avda : timer_avda_instances_)
        avda->DoIOTask(false);
    }

    // Take care of any deferred erasures.
    for (auto* avda : pending_erase_)
      StopTimer(avda);
    pending_erase_.clear();

    // TODO(dalecurtis): We may want to consider chunking this if task execution
    // takes too long for the combined timer.
  }

  // All AVDA instances that would like us to poll DoIOTask.
  std::set<AndroidVideoDecodeAccelerator*> timer_avda_instances_;

  // All AVDA instances that might like to use the construction thread.
  std::set<AndroidVideoDecodeAccelerator*> thread_avda_instances_;

  struct OwnerRecord {
    AndroidVideoDecodeAccelerator* owner = nullptr;
    AndroidVideoDecodeAccelerator* waiter = nullptr;
  };
  // [surface id] = OwnerRecord for that surface.
  using SurfaceWaiterMap = std::map<int, OwnerRecord>;
  SurfaceWaiterMap surface_waiter_map_;

  // Since we can't delete while iterating when using a set, defer erasure until
  // after iteration complete.
  bool timer_running_ = false;
  std::set<AndroidVideoDecodeAccelerator*> pending_erase_;

  // Repeating timer responsible for draining pending IO to the codecs.
  base::RepeatingTimer io_timer_;

  // Data for determining if codec creation is hanging.
  struct {
    // Lock that protects other members of this struct.
    base::Lock lock_;

    // Number of currently pending work items of the construction thread.
    int outstanding_ = 0;
  } autodetection_info_;

  base::Thread construction_thread_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AVDATimerManager);
};

static base::LazyInstance<AVDATimerManager>::Leaky g_avda_timer =
    LAZY_INSTANCE_INITIALIZER;

AndroidVideoDecodeAccelerator::CodecConfig::CodecConfig() {}

AndroidVideoDecodeAccelerator::CodecConfig::~CodecConfig() {}

AndroidVideoDecodeAccelerator::BitstreamRecord::BitstreamRecord(
    const BitstreamBuffer& bitstream_buffer)
    : buffer(bitstream_buffer) {
  if (buffer.id() != -1)
    memory.reset(new SharedMemoryRegion(buffer, true));
}

AndroidVideoDecodeAccelerator::BitstreamRecord::BitstreamRecord(
    BitstreamRecord&& other)
    : buffer(std::move(other.buffer)), memory(std::move(other.memory)) {}

AndroidVideoDecodeAccelerator::BitstreamRecord::~BitstreamRecord() {}

AndroidVideoDecodeAccelerator::AndroidVideoDecodeAccelerator(
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const GetGLES2DecoderCallback& get_gles2_decoder_cb)
    : client_(NULL),
      make_context_current_cb_(make_context_current_cb),
      get_gles2_decoder_cb_(get_gles2_decoder_cb),
      state_(NO_ERROR),
      picturebuffers_requested_(false),
      drain_type_(DRAIN_TYPE_NONE),
      media_drm_bridge_cdm_context_(nullptr),
      cdm_registration_id_(0),
      pending_input_buf_index_(-1),
      error_sequence_token_(0),
      defer_errors_(false),
      deferred_initialization_pending_(false),
      codec_needs_reset_(false),
      weak_this_factory_(this) {}

AndroidVideoDecodeAccelerator::~AndroidVideoDecodeAccelerator() {
  DCHECK(thread_checker_.CalledOnValidThread());
  g_avda_timer.Pointer()->StopTimer(this);
  g_avda_timer.Pointer()->StopThread(this);

#if defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  if (!media_drm_bridge_cdm_context_)
    return;

  DCHECK(cdm_registration_id_);

  // Cancel previously registered callback (if any).
  media_drm_bridge_cdm_context_->SetMediaCryptoReadyCB(
      MediaDrmBridgeCdmContext::MediaCryptoReadyCB());

  media_drm_bridge_cdm_context_->UnregisterPlayer(cdm_registration_id_);
#endif  // defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
}

bool AndroidVideoDecodeAccelerator::Initialize(const Config& config,
                                               Client* client) {
  DVLOG(1) << __FUNCTION__ << ": " << config.AsHumanReadableString();
  TRACE_EVENT0("media", "AVDA::Initialize");
  DCHECK(!media_codec_);
  DCHECK(thread_checker_.CalledOnValidThread());

  if (make_context_current_cb_.is_null() || get_gles2_decoder_cb_.is_null()) {
    DLOG(ERROR) << "GL callbacks are required for this VDA";
    return false;
  }

  if (config.output_mode != Config::OutputMode::ALLOCATE) {
    DLOG(ERROR) << "Only ALLOCATE OutputMode is supported by this VDA";
    return false;
  }

  DCHECK(client);
  client_ = client;
  config_ = config;
  codec_config_ = new CodecConfig();
  codec_config_->codec_ = VideoCodecProfileToVideoCodec(config.profile);
  codec_config_->initial_expected_coded_size_ =
      config.initial_expected_coded_size;

  // We signalled that we support deferred initialization, so see if the client
  // does also.
  deferred_initialization_pending_ = config.is_deferred_initialization_allowed;

  if (config_.is_encrypted && !deferred_initialization_pending_) {
    DLOG(ERROR) << "Deferred initialization must be used for encrypted streams";
    return false;
  }

  if (codec_config_->codec_ != kCodecVP8 &&
      codec_config_->codec_ != kCodecVP9 &&
      codec_config_->codec_ != kCodecH264) {
    LOG(ERROR) << "Unsupported profile: " << config.profile;
    return false;
  }

  // Only use MediaCodec for VP8/9 if it's likely backed by hardware
  // or if the stream is encrypted.
  if ((codec_config_->codec_ == kCodecVP8 ||
       codec_config_->codec_ == kCodecVP9) &&
      !config_.is_encrypted &&
      VideoCodecBridge::IsKnownUnaccelerated(codec_config_->codec_,
                                             MEDIA_CODEC_DECODER)) {
    DVLOG(1) << "Initialization failed: "
             << (codec_config_->codec_ == kCodecVP8 ? "vp8" : "vp9")
             << " is not hardware accelerated";
    return false;
  }

  auto gles_decoder = get_gles2_decoder_cb_.Run();
  if (!gles_decoder) {
    LOG(ERROR) << "Failed to get gles2 decoder instance.";
    return false;
  }

  const gpu::GpuPreferences& gpu_preferences =
      gles_decoder->GetContextGroup()->gpu_preferences();

  if (UseDeferredRenderingStrategy(gpu_preferences)) {
    DVLOG(1) << __FUNCTION__ << ", using deferred rendering strategy.";
    strategy_.reset(new AndroidDeferredRenderingBackingStrategy(this));
  } else {
    DVLOG(1) << __FUNCTION__ << ", using copy back strategy.";
    strategy_.reset(new AndroidCopyingBackingStrategy(this));
  }

  if (!make_context_current_cb_.Run()) {
    LOG(ERROR) << "Failed to make this decoder's GL context current.";
    return false;
  }

  if (g_avda_timer.Pointer()->AllocateSurface(config_.surface_id, this)) {
    // We have succesfully owned the surface, so finish initialization now.
    return InitializeStrategy();
  }

  // We have to wait for some other AVDA instance to free up the surface.
  // OnSurfaceAvailable will be called when it's available.
  return true;
}

void AndroidVideoDecodeAccelerator::OnSurfaceAvailable(bool success) {
  DCHECK(deferred_initialization_pending_);

  if (!success || !InitializeStrategy()) {
    NotifyInitializationComplete(false);
    deferred_initialization_pending_ = false;
  }
}

bool AndroidVideoDecodeAccelerator::InitializeStrategy() {
  codec_config_->surface_ = strategy_->Initialize(config_.surface_id);
  if (codec_config_->surface_.IsEmpty()) {
    LOG(ERROR) << "Failed to initialize the backing strategy. The returned "
                  "Java surface is empty.";
    return false;
  }

  on_destroying_surface_cb_ =
      base::Bind(&AndroidVideoDecodeAccelerator::OnDestroyingSurface,
                 weak_this_factory_.GetWeakPtr());
  AVDASurfaceTracker::GetInstance()->RegisterOnDestroyingSurfaceCallback(
      on_destroying_surface_cb_);

  // TODO(watk,liberato): move this into the strategy.
  scoped_refptr<gl::SurfaceTexture> surface_texture =
      strategy_->GetSurfaceTexture();
  if (surface_texture) {
    on_frame_available_handler_ =
        new OnFrameAvailableHandler(this, surface_texture);
  }

  // Start the thread for async configuration, even if we don't need it now.
  // ResetCodecState might rebuild the codec later, for example.
  if (!g_avda_timer.Pointer()->StartThread(this)) {
    LOG(ERROR) << "Failed to start AVDA thread";
    return false;
  }

  // If we are encrypted, then we aren't able to create the codec yet.
  if (config_.is_encrypted) {
    InitializeCdm();
    return true;
  }

  if (deferred_initialization_pending_) {
    ConfigureMediaCodecAsynchronously();
    return true;
  }

  // If the client doesn't support deferred initialization (WebRTC), then we
  // should complete it now and return a meaningful result.  Note that it would
  // be nice if we didn't have to worry about starting codec configuration at
  // all (::Initialize or the wrapper can do it), but then they have to remember
  // not to start codec config if we have to wait for the cdm.  It's somewhat
  // clearer for us to handle both cases.
  return ConfigureMediaCodecSynchronously();
}

void AndroidVideoDecodeAccelerator::DoIOTask(bool start_timer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("media", "AVDA::DoIOTask");
  if (state_ == ERROR || state_ == WAITING_FOR_CODEC ||
      state_ == SURFACE_DESTROYED) {
    return;
  }

  strategy_->MaybeRenderEarly();
  bool did_work = false, did_input = false, did_output = false;
  do {
    did_input = QueueInput();
    did_output = DequeueOutput();
    if (did_input || did_output)
      did_work = true;
  } while (did_input || did_output);

  ManageTimer(did_work || start_timer);
}

bool AndroidVideoDecodeAccelerator::QueueInput() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("media", "AVDA::QueueInput");
  base::AutoReset<bool> auto_reset(&defer_errors_, true);
  if (state_ == ERROR || state_ == WAITING_FOR_CODEC ||
      state_ == WAITING_FOR_KEY) {
    return false;
  }
  if (bitstreams_notified_in_advance_.size() > kMaxBitstreamsNotifiedInAdvance)
    return false;
  if (pending_bitstream_records_.empty())
    return false;

  int input_buf_index = pending_input_buf_index_;

  // Do not dequeue a new input buffer if we failed with MEDIA_CODEC_NO_KEY.
  // That status does not return this buffer back to the pool of
  // available input buffers. We have to reuse it in QueueSecureInputBuffer().
  if (input_buf_index == -1) {
    MediaCodecStatus status =
        media_codec_->DequeueInputBuffer(NoWaitTimeOut(), &input_buf_index);
    switch (status) {
      case MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER:
        return false;
      case MEDIA_CODEC_ERROR:
        POST_ERROR(PLATFORM_FAILURE, "Failed to DequeueInputBuffer");
        return false;
      case MEDIA_CODEC_OK:
        break;
      default:
        NOTREACHED() << "Unknown DequeueInputBuffer status " << status;
        return false;
    }
  }

  DCHECK_NE(input_buf_index, -1);

  BitstreamBuffer bitstream_buffer = pending_bitstream_records_.front().buffer;

  if (bitstream_buffer.id() == -1) {
    pending_bitstream_records_.pop();
    TRACE_COUNTER1("media", "AVDA::PendingBitstreamBufferCount",
                   pending_bitstream_records_.size());

    media_codec_->QueueEOS(input_buf_index);
    return true;
  }

  std::unique_ptr<SharedMemoryRegion> shm;

  if (pending_input_buf_index_ == -1) {
    // When |pending_input_buf_index_| is not -1, the buffer is already dequeued
    // from MediaCodec, filled with data and bitstream_buffer.handle() is
    // closed.
    shm = std::move(pending_bitstream_records_.front().memory);

    if (!shm->Map()) {
      POST_ERROR(UNREADABLE_INPUT, "Failed to SharedMemoryRegion::Map()");
      return false;
    }
  }

  const base::TimeDelta presentation_timestamp =
      bitstream_buffer.presentation_timestamp();
  DCHECK(presentation_timestamp != kNoTimestamp())
      << "Bitstream buffers must have valid presentation timestamps";

  // There may already be a bitstream buffer with this timestamp, e.g., VP9 alt
  // ref frames, but it's OK to overwrite it because we only expect a single
  // output frame to have that timestamp. AVDA clients only use the bitstream
  // buffer id in the returned Pictures to map a bitstream buffer back to a
  // timestamp on their side, so either one of the bitstream buffer ids will
  // result in them finding the right timestamp.
  bitstream_buffers_in_decoder_[presentation_timestamp] = bitstream_buffer.id();

  // Notice that |memory| will be null if we repeatedly enqueue the same buffer,
  // this happens after MEDIA_CODEC_NO_KEY.
  const uint8_t* memory =
      shm ? static_cast<const uint8_t*>(shm->memory()) : nullptr;
  const std::string& key_id = bitstream_buffer.key_id();
  const std::string& iv = bitstream_buffer.iv();
  const std::vector<SubsampleEntry>& subsamples = bitstream_buffer.subsamples();

  MediaCodecStatus status;
  if (key_id.empty() || iv.empty()) {
    status = media_codec_->QueueInputBuffer(input_buf_index, memory,
                                            bitstream_buffer.size(),
                                            presentation_timestamp);
  } else {
    status = media_codec_->QueueSecureInputBuffer(
        input_buf_index, memory, bitstream_buffer.size(), key_id, iv,
        subsamples, presentation_timestamp);
  }

  DVLOG(2) << __FUNCTION__
           << ": Queue(Secure)InputBuffer: pts:" << presentation_timestamp
           << " status:" << status;

  if (status == MEDIA_CODEC_NO_KEY) {
    // Keep trying to enqueue the same input buffer.
    // The buffer is owned by us (not the MediaCodec) and is filled with data.
    DVLOG(1) << "QueueSecureInputBuffer failed: NO_KEY";
    pending_input_buf_index_ = input_buf_index;
    state_ = WAITING_FOR_KEY;
    return false;
  }

  pending_input_buf_index_ = -1;
  pending_bitstream_records_.pop();
  TRACE_COUNTER1("media", "AVDA::PendingBitstreamBufferCount",
                 pending_bitstream_records_.size());
  // We should call NotifyEndOfBitstreamBuffer(), when no more decoded output
  // will be returned from the bitstream buffer. However, MediaCodec API is
  // not enough to guarantee it.
  // So, here, we calls NotifyEndOfBitstreamBuffer() in advance in order to
  // keep getting more bitstreams from the client, and throttle them by using
  // |bitstreams_notified_in_advance_|.
  // TODO(dwkang): check if there is a way to remove this workaround.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer,
                 weak_this_factory_.GetWeakPtr(), bitstream_buffer.id()));
  bitstreams_notified_in_advance_.push_back(bitstream_buffer.id());

  if (status != MEDIA_CODEC_OK) {
    POST_ERROR(PLATFORM_FAILURE, "Failed to QueueInputBuffer: " << status);
    return false;
  }

  return true;
}

bool AndroidVideoDecodeAccelerator::DequeueOutput() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("media", "AVDA::DequeueOutput");
  base::AutoReset<bool> auto_reset(&defer_errors_, true);
  if (state_ == ERROR || state_ == WAITING_FOR_CODEC)
    return false;
  if (picturebuffers_requested_ && output_picture_buffers_.empty())
    return false;
  if (!output_picture_buffers_.empty() && free_picture_ids_.empty()) {
    // Don't have any picture buffer to send. Need to wait.
    return false;
  }

  bool eos = false;
  base::TimeDelta presentation_timestamp;
  int32_t buf_index = 0;
  do {
    size_t offset = 0;
    size_t size = 0;

    TRACE_EVENT_BEGIN0("media", "AVDA::DequeueOutput");
    MediaCodecStatus status = media_codec_->DequeueOutputBuffer(
        NoWaitTimeOut(), &buf_index, &offset, &size, &presentation_timestamp,
        &eos, NULL);
    TRACE_EVENT_END2("media", "AVDA::DequeueOutput", "status", status,
                     "presentation_timestamp (ms)",
                     presentation_timestamp.InMilliseconds());

    switch (status) {
      case MEDIA_CODEC_ERROR:
        // Do not post an error if we are draining for reset and destroy.
        // Instead, run the drain completion task.
        if (IsDrainingForResetOrDestroy()) {
          DVLOG(1) << __FUNCTION__ << ": error while codec draining";
          state_ = ERROR;
          OnDrainCompleted();
        } else {
          POST_ERROR(PLATFORM_FAILURE, "DequeueOutputBuffer failed.");
        }
        return false;

      case MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER:
        return false;

      case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED: {
        // An OUTPUT_FORMAT_CHANGED is not reported after flush() if the frame
        // size does not change. Therefore we have to keep track on the format
        // even if draining, unless we are draining for destroy.
        if (drain_type_ == DRAIN_FOR_DESTROY)
          return true;  // ignore

        if (media_codec_->GetOutputSize(&size_) != MEDIA_CODEC_OK) {
          POST_ERROR(PLATFORM_FAILURE, "GetOutputSize failed.");
          return false;
        }

        DVLOG(3) << __FUNCTION__
                 << " OUTPUT_FORMAT_CHANGED, new size: " << size_.ToString();

        // Don't request picture buffers if we already have some. This avoids
        // having to dismiss the existing buffers which may actively reference
        // decoded images. Breaking their connection to the decoded image will
        // cause rendering of black frames. Instead, we let the existing
        // PictureBuffers live on and we simply update their size the next time
        // they're attachted to an image of the new resolution. See the
        // size update in |SendDecodedFrameToClient| and https://crbug/587994.
        if (output_picture_buffers_.empty() && !picturebuffers_requested_) {
          picturebuffers_requested_ = true;
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE,
              base::Bind(&AndroidVideoDecodeAccelerator::RequestPictureBuffers,
                         weak_this_factory_.GetWeakPtr()));
          return false;
        }

        return true;
      }

      case MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
        break;

      case MEDIA_CODEC_OK:
        DCHECK_GE(buf_index, 0);
        DVLOG(3) << __FUNCTION__ << ": pts:" << presentation_timestamp
                 << " buf_index:" << buf_index << " offset:" << offset
                 << " size:" << size << " eos:" << eos;
        break;

      default:
        NOTREACHED();
        break;
    }
  } while (buf_index < 0);

  if (eos) {
    OnDrainCompleted();
    return false;
  }

  if (IsDrainingForResetOrDestroy()) {
    media_codec_->ReleaseOutputBuffer(buf_index, false);
    return true;
  }

  if (!picturebuffers_requested_) {
    // If, somehow, we get a decoded frame back before a FORMAT_CHANGED
    // message, then we might not have any picture buffers to use.  This
    // isn't supposed to happen (see EncodeDecodeTest.java#617).
    // Log a metric to see how common this is.
    RecordFormatChangedMetric(FormatChangedValue::MissingFormatChanged);
    media_codec_->ReleaseOutputBuffer(buf_index, false);
    POST_ERROR(PLATFORM_FAILURE, "Dequeued buffers before FORMAT_CHANGED.");
    return false;
  }

  // Get the bitstream buffer id from the timestamp.
  auto it = bitstream_buffers_in_decoder_.find(presentation_timestamp);

  if (it != bitstream_buffers_in_decoder_.end()) {
    const int32_t bitstream_buffer_id = it->second;
    bitstream_buffers_in_decoder_.erase(bitstream_buffers_in_decoder_.begin(),
                                        ++it);
    SendDecodedFrameToClient(buf_index, bitstream_buffer_id);

    // Removes ids former or equal than the id from decoder. Note that
    // |bitstreams_notified_in_advance_| does not mean bitstream ids in decoder
    // because of frame reordering issue. We just maintain this roughly and use
    // it for throttling.
    for (auto bitstream_it = bitstreams_notified_in_advance_.begin();
         bitstream_it != bitstreams_notified_in_advance_.end();
         ++bitstream_it) {
      if (*bitstream_it == bitstream_buffer_id) {
        bitstreams_notified_in_advance_.erase(
            bitstreams_notified_in_advance_.begin(), ++bitstream_it);
        break;
      }
    }
  } else {
    // Normally we assume that the decoder makes at most one output frame for
    // each distinct input timestamp. However MediaCodecBridge uses timestamp
    // correction and provides a non-decreasing timestamp sequence, which might
    // result in timestamp duplicates. Discard the frame if we cannot get the
    // corresponding buffer id.
    DVLOG(3) << __FUNCTION__ << ": Releasing buffer with unexpected PTS: "
             << presentation_timestamp;
    media_codec_->ReleaseOutputBuffer(buf_index, false);
  }

  // We got a decoded frame, so try for another.
  return true;
}

void AndroidVideoDecodeAccelerator::SendDecodedFrameToClient(
    int32_t codec_buffer_index,
    int32_t bitstream_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(bitstream_id, -1);
  DCHECK(!free_picture_ids_.empty());
  TRACE_EVENT0("media", "AVDA::SendDecodedFrameToClient");

  if (!make_context_current_cb_.Run()) {
    POST_ERROR(PLATFORM_FAILURE, "Failed to make the GL context current.");
    return;
  }

  int32_t picture_buffer_id = free_picture_ids_.front();
  free_picture_ids_.pop();
  TRACE_COUNTER1("media", "AVDA::FreePictureIds", free_picture_ids_.size());

  const auto& i = output_picture_buffers_.find(picture_buffer_id);
  if (i == output_picture_buffers_.end()) {
    POST_ERROR(PLATFORM_FAILURE,
               "Can't find PictureBuffer id: " << picture_buffer_id);
    return;
  }

  bool size_changed = false;
  if (i->second.size() != size_) {
    // Size may have changed due to resolution change since the last time this
    // PictureBuffer was used.
    strategy_->UpdatePictureBufferSize(&i->second, size_);
    size_changed = true;
  }

  const bool allow_overlay = strategy_->ArePicturesOverlayable();
  UMA_HISTOGRAM_BOOLEAN("Media.AVDA.FrameSentAsOverlay", allow_overlay);
  Picture picture(picture_buffer_id, bitstream_id, gfx::Rect(size_),
                  allow_overlay);
  picture.set_size_changed(size_changed);

  // Notify picture ready before calling UseCodecBufferForPictureBuffer() since
  // that process may be slow and shouldn't delay delivery of the frame to the
  // renderer. The picture is only used on the same thread as this method is
  // called, so it is safe to do this.
  NotifyPictureReady(picture);

  // Connect the PictureBuffer to the decoded frame, via whatever mechanism the
  // strategy likes.
  strategy_->UseCodecBufferForPictureBuffer(codec_buffer_index, i->second);
}

void AndroidVideoDecodeAccelerator::Decode(
    const BitstreamBuffer& bitstream_buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If we previously deferred a codec restart, take care of it now. This can
  // happen on older devices where configuration changes require a codec reset.
  if (codec_needs_reset_) {
    DCHECK_EQ(drain_type_, DRAIN_TYPE_NONE);
    ResetCodecState();
  }

  if (bitstream_buffer.id() >= 0 && bitstream_buffer.size() > 0) {
    DecodeBuffer(bitstream_buffer);
    return;
  }

  if (base::SharedMemory::IsHandleValid(bitstream_buffer.handle()))
    base::SharedMemory::CloseHandle(bitstream_buffer.handle());

  if (bitstream_buffer.id() < 0) {
    POST_ERROR(INVALID_ARGUMENT,
               "Invalid bistream_buffer, id: " << bitstream_buffer.id());
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer,
                   weak_this_factory_.GetWeakPtr(), bitstream_buffer.id()));
  }
}

void AndroidVideoDecodeAccelerator::DecodeBuffer(
    const BitstreamBuffer& bitstream_buffer) {
  pending_bitstream_records_.push(BitstreamRecord(bitstream_buffer));
  TRACE_COUNTER1("media", "AVDA::PendingBitstreamBufferCount",
                 pending_bitstream_records_.size());

  DoIOTask(true);
}

void AndroidVideoDecodeAccelerator::RequestPictureBuffers() {
  if (client_) {
    client_->ProvidePictureBuffers(kNumPictureBuffers, PIXEL_FORMAT_UNKNOWN, 1,
                                   strategy_->GetPictureBufferSize(),
                                   strategy_->GetTextureTarget());
  }
}

void AndroidVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<PictureBuffer>& buffers) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(output_picture_buffers_.empty());
  DCHECK(free_picture_ids_.empty());

  if (buffers.size() < kNumPictureBuffers) {
    POST_ERROR(INVALID_ARGUMENT, "Not enough picture buffers assigned.");
    return;
  }

  const bool have_context = make_context_current_cb_.Run();
  LOG_IF(WARNING, !have_context)
      << "Failed to make GL context current for Assign, continuing.";

  for (size_t i = 0; i < buffers.size(); ++i) {
    if (buffers[i].size() != strategy_->GetPictureBufferSize()) {
      POST_ERROR(INVALID_ARGUMENT,
                 "Invalid picture buffer size assigned. Wanted "
                     << size_.ToString() << ", but got "
                     << buffers[i].size().ToString());
      return;
    }
    int32_t id = buffers[i].id();
    output_picture_buffers_.insert(std::make_pair(id, buffers[i]));
    free_picture_ids_.push(id);

    strategy_->AssignOnePictureBuffer(buffers[i], have_context);
  }
  TRACE_COUNTER1("media", "AVDA::FreePictureIds", free_picture_ids_.size());
  DoIOTask(true);
}

void AndroidVideoDecodeAccelerator::ReusePictureBuffer(
    int32_t picture_buffer_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  free_picture_ids_.push(picture_buffer_id);
  TRACE_COUNTER1("media", "AVDA::FreePictureIds", free_picture_ids_.size());

  OutputBufferMap::const_iterator i =
      output_picture_buffers_.find(picture_buffer_id);
  if (i == output_picture_buffers_.end()) {
    POST_ERROR(PLATFORM_FAILURE, "Can't find PictureBuffer id "
                                     << picture_buffer_id);
    return;
  }

  strategy_->ReuseOnePictureBuffer(i->second);
  DoIOTask(true);
}

void AndroidVideoDecodeAccelerator::Flush() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == SURFACE_DESTROYED)
    NotifyFlushDone();
  else
    StartCodecDrain(DRAIN_FOR_FLUSH);
}

void AndroidVideoDecodeAccelerator::ConfigureMediaCodecAsynchronously() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // It's probably okay just to return here, since the codec will be configured
  // asynchronously.  It's unclear that any state for the new request could
  // be different, unless somebody modifies |codec_config_| while we're already
  // waiting for a codec.  One shouldn't do that for thread safety.
  DCHECK_NE(state_, WAITING_FOR_CODEC);

  state_ = WAITING_FOR_CODEC;

  // Tell the strategy that we're changing codecs.  The codec itself could be
  // used normally, since we don't replace it until we're back on the main
  // thread.  However, if we're using an output surface, then the incoming codec
  // might access that surface while the main thread is drawing.  Telling the
  // strategy to forget the codec avoids this.
  if (media_codec_) {
    ReleaseMediaCodec();
    strategy_->CodecChanged(nullptr);
  }

  // Choose whether to autodetect the codec type.  Note that we do this after
  // releasing any outgoing codec, so that |codec_config_| still matches the
  // outgoing codec for ReleaseMediaCodec().
  codec_config_->allow_autodetection_ =
      g_avda_timer.Pointer()->IsCodecAutodetectionProbablySafe();
  codec_config_->notify_completion_ = codec_config_->allow_autodetection_;
  if (codec_config_->allow_autodetection_)
    g_avda_timer.Pointer()->StartUsingConstructionThread();

  // If we're not trying autodetection, then use the main thread.  The original
  // might be blocked.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      codec_config_->allow_autodetection_
          ? g_avda_timer.Pointer()->ConstructionTaskRunner()
          : base::ThreadTaskRunnerHandle::Get();
  CHECK(task_runner);

  base::PostTaskAndReplyWithResult(
      task_runner.get(), FROM_HERE,
      base::Bind(&AndroidVideoDecodeAccelerator::ConfigureMediaCodecOnAnyThread,
                 codec_config_),
      base::Bind(&AndroidVideoDecodeAccelerator::OnCodecConfigured,
                 weak_this_factory_.GetWeakPtr()));
}

bool AndroidVideoDecodeAccelerator::ConfigureMediaCodecSynchronously() {
  state_ = WAITING_FOR_CODEC;

  // Decide whether to allow autodetection or not.  Since we're on the main
  // thread, and this request is unordered with respect to pending async config
  // attempts, don't record it.  It may break book-keeping, and there's not
  // much we can do anyway.
  codec_config_->allow_autodetection_ =
      g_avda_timer.Pointer()->IsCodecAutodetectionProbablySafe();
  codec_config_->notify_completion_ = false;

  ReleaseMediaCodec();
  std::unique_ptr<VideoCodecBridge> media_codec =
      ConfigureMediaCodecOnAnyThread(codec_config_);
  OnCodecConfigured(std::move(media_codec));
  return !!media_codec_;
}

std::unique_ptr<VideoCodecBridge>
AndroidVideoDecodeAccelerator::ConfigureMediaCodecOnAnyThread(
    scoped_refptr<CodecConfig> codec_config) {
  TRACE_EVENT0("media", "AVDA::ConfigureMediaCodec");

  jobject media_crypto = codec_config->media_crypto_
                             ? codec_config->media_crypto_->obj()
                             : nullptr;

  // |needs_protected_surface_| implies encrypted stream.
  DCHECK(!codec_config->needs_protected_surface_ || media_crypto);

  const bool require_software_codec = !codec_config->allow_autodetection_;

  std::unique_ptr<VideoCodecBridge> codec(VideoCodecBridge::CreateDecoder(
      codec_config->codec_, codec_config->needs_protected_surface_,
      codec_config->initial_expected_coded_size_,
      codec_config->surface_.j_surface().obj(), media_crypto, true,
      require_software_codec));

  // If we successfully completed after an autodetect, then let the other
  // instances know that we didn't get stuck.
  if (codec_config->notify_completion_)
    g_avda_timer.Pointer()->DoneUsingConstructionThread();

  return codec;
}

void AndroidVideoDecodeAccelerator::OnCodecConfigured(
    std::unique_ptr<VideoCodecBridge> media_codec) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(state_ == WAITING_FOR_CODEC || state_ == SURFACE_DESTROYED);

  // Record one instance of the codec being initialized.
  RecordFormatChangedMetric(FormatChangedValue::CodecInitialized);

  // If we are supposed to notify that initialization is complete, then do so
  // now.  Otherwise, this is a reconfiguration.
  if (deferred_initialization_pending_) {
    // Losing the output surface is not considered an error state, so notify
    // success. The client will destroy this soon.
    NotifyInitializationComplete(state_ == SURFACE_DESTROYED ? true
                                                             : !!media_codec);
    deferred_initialization_pending_ = false;
  }

  // If |state_| changed to SURFACE_DESTROYED while we were configuring a codec,
  // then the codec is already invalid so we return early and drop it.
  if (state_ == SURFACE_DESTROYED)
    return;

  DCHECK(!media_codec_);
  media_codec_ = std::move(media_codec);
  strategy_->CodecChanged(media_codec_.get());
  if (!media_codec_) {
    POST_ERROR(PLATFORM_FAILURE, "Failed to create MediaCodec.");
    return;
  }

  state_ = NO_ERROR;

  ManageTimer(true);
}

void AndroidVideoDecodeAccelerator::StartCodecDrain(DrainType drain_type) {
  DVLOG(2) << __FUNCTION__ << " drain_type:" << drain_type;
  DCHECK(thread_checker_.CalledOnValidThread());

  // We assume that DRAIN_FOR_FLUSH and DRAIN_FOR_RESET cannot come while
  // another drain request is present, but DRAIN_FOR_DESTROY can.
  DCHECK_NE(drain_type, DRAIN_TYPE_NONE);
  DCHECK(drain_type_ == DRAIN_TYPE_NONE || drain_type == DRAIN_FOR_DESTROY)
      << "Unexpected StartCodecDrain() with drain type " << drain_type
      << " while already draining with drain type " << drain_type_;

  const bool enqueue_eos = drain_type_ == DRAIN_TYPE_NONE;
  drain_type_ = drain_type;

  if (enqueue_eos)
    DecodeBuffer(BitstreamBuffer(-1, base::SharedMemoryHandle(), 0));
}

bool AndroidVideoDecodeAccelerator::IsDrainingForResetOrDestroy() const {
  return drain_type_ == DRAIN_FOR_RESET || drain_type_ == DRAIN_FOR_DESTROY;
}

void AndroidVideoDecodeAccelerator::OnDrainCompleted() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // If we were waiting for an EOS, clear the state and reset the MediaCodec
  // as normal.
  //
  // Some Android platforms seem to send an EOS buffer even when we're not
  // expecting it. In this case, destroy and reset the codec but don't notify
  // flush done since it violates the state machine. http://crbug.com/585959.

  switch (drain_type_) {
    case DRAIN_TYPE_NONE:
      // Unexpected EOS.
      state_ = ERROR;
      ResetCodecState();
      break;
    case DRAIN_FOR_FLUSH:
      ResetCodecState();
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&AndroidVideoDecodeAccelerator::NotifyFlushDone,
                                weak_this_factory_.GetWeakPtr()));
      break;
    case DRAIN_FOR_RESET:
      ResetCodecState();
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&AndroidVideoDecodeAccelerator::NotifyResetDone,
                                weak_this_factory_.GetWeakPtr()));
      break;
    case DRAIN_FOR_DESTROY:
      ResetCodecState();
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&AndroidVideoDecodeAccelerator::ActualDestroy,
                                weak_this_factory_.GetWeakPtr()));
      break;
  }
  drain_type_ = DRAIN_TYPE_NONE;
}

void AndroidVideoDecodeAccelerator::ResetCodecState() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If there is already a reset in flight, then that counts.  This can really
  // only happen if somebody calls Reset.
  // If the surface is destroyed there's nothing to do.
  if (state_ == WAITING_FOR_CODEC || state_ == SURFACE_DESTROYED)
    return;

  bitstream_buffers_in_decoder_.clear();

  if (pending_input_buf_index_ != -1) {
    // The data for that index exists in the input buffer, but corresponding
    // shm block been deleted. Check that it is safe to flush the coec, i.e.
    // |pending_bitstream_records_| is empty.
    // TODO(timav): keep shm block for that buffer and remove this restriction.
    DCHECK(pending_bitstream_records_.empty());
    pending_input_buf_index_ = -1;
  }

  const bool did_codec_error_happen = state_ == ERROR;
  state_ = NO_ERROR;

  // Don't reset the codec here if there's no error and we're only flushing;
  // instead defer until the next decode call; this prevents us from unbacking
  // frames that might be out for display at end of stream.
  codec_needs_reset_ = false;
  if (drain_type_ == DRAIN_FOR_FLUSH && !did_codec_error_happen) {
    codec_needs_reset_ = true;
    return;
  }

  // We might increment error_sequence_token here to cancel any delayed errors,
  // but right now it's unclear that it's safe to do so.  If we are in an error
  // state because of a codec error, then it would be okay.  Otherwise, it's
  // less obvious that we are exiting the error state.  Since deferred errors
  // are only intended for fullscreen transitions right now, we take the more
  // conservative approach and let the errors post.
  // TODO(liberato): revisit this once we sort out the error state a bit more.

  // When the codec is not in error state we can flush() for JB-MR2 and beyond.
  // Prior to JB-MR2, flush() had several bugs (b/8125974, b/8347958) so we must
  // delete the MediaCodec and create a new one. The full reconfigure is much
  // slower and may cause visible freezing if done mid-stream.
  if (!did_codec_error_happen &&
      base::android::BuildInfo::GetInstance()->sdk_int() >= 18) {
    DVLOG(3) << __FUNCTION__ << " Flushing MediaCodec.";
    media_codec_->Flush();
    // Since we just flushed all the output buffers, make sure that nothing is
    // using them.
    strategy_->CodecChanged(media_codec_.get());
  } else {
    DVLOG(3) << __FUNCTION__
             << " Deleting the MediaCodec and creating a new one.";
    g_avda_timer.Pointer()->StopTimer(this);
    ConfigureMediaCodecAsynchronously();
  }
}

void AndroidVideoDecodeAccelerator::Reset() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("media", "AVDA::Reset");

  while (!pending_bitstream_records_.empty()) {
    int32_t bitstream_buffer_id =
        pending_bitstream_records_.front().buffer.id();
    pending_bitstream_records_.pop();

    if (bitstream_buffer_id != -1) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer,
                     weak_this_factory_.GetWeakPtr(), bitstream_buffer_id));
    }
  }
  TRACE_COUNTER1("media", "AVDA::PendingBitstreamBufferCount", 0);
  bitstreams_notified_in_advance_.clear();

  // Any error that is waiting to post can be ignored.
  error_sequence_token_++;

  DCHECK(strategy_);
  strategy_->ReleaseCodecBuffers(output_picture_buffers_);

  // Some VP8 files require complete MediaCodec drain before we can call
  // MediaCodec.flush() or MediaCodec.reset(). http://crbug.com/598963.
  if (media_codec_ && codec_config_->codec_ == kCodecVP8 &&
      !bitstream_buffers_in_decoder_.empty()) {
    // Postpone ResetCodecState() after the drain.
    StartCodecDrain(DRAIN_FOR_RESET);
  } else {
    ResetCodecState();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&AndroidVideoDecodeAccelerator::NotifyResetDone,
                              weak_this_factory_.GetWeakPtr()));
  }
}

void AndroidVideoDecodeAccelerator::Destroy() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  bool have_context = make_context_current_cb_.Run();
  if (!have_context)
    LOG(WARNING) << "Failed make GL context current for Destroy, continuing.";

  if (strategy_)
    strategy_->BeginCleanup(have_context, output_picture_buffers_);

  // If we have an OnFrameAvailable handler, tell it that we're going away.
  if (on_frame_available_handler_) {
    on_frame_available_handler_->ClearOwner();
    on_frame_available_handler_ = nullptr;
  }

  client_ = nullptr;

  // Some VP8 files require complete MediaCodec drain before we can call
  // MediaCodec.flush() or MediaCodec.reset(). http://crbug.com/598963.
  if (media_codec_ && codec_config_->codec_ == kCodecVP8) {
    // Clear pending_bitstream_records_.
    while (!pending_bitstream_records_.empty())
      pending_bitstream_records_.pop();

    // Postpone ActualDestroy after the drain.
    StartCodecDrain(DRAIN_FOR_DESTROY);
  } else {
    ActualDestroy();
  }
}

void AndroidVideoDecodeAccelerator::ActualDestroy() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!on_destroying_surface_cb_.is_null()) {
    AVDASurfaceTracker::GetInstance()->UnregisterOnDestroyingSurfaceCallback(
        on_destroying_surface_cb_);
  }

  if (strategy_)
    strategy_->EndCleanup();

  AVDATimerManager* manager = g_avda_timer.Pointer();

  // We no longer care about |surface_id|, in case we did before.  It's okay
  // if we have no surface and/or weren't the owner or a waiter.
  manager->DeallocateSurface(config_.surface_id, this);

  // Note that async codec construction might still be in progress.  In that
  // case, the codec will be deleted when it completes once we invalidate all
  // our weak refs.
  weak_this_factory_.InvalidateWeakPtrs();
  if (media_codec_) {
    manager->StopTimer(this);
    ReleaseMediaCodec();
  }
  delete this;
}

bool AndroidVideoDecodeAccelerator::TryToSetupDecodeOnSeparateThread(
    const base::WeakPtr<Client>& decode_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner) {
  return false;
}

const gfx::Size& AndroidVideoDecodeAccelerator::GetSize() const {
  return size_;
}

const base::ThreadChecker& AndroidVideoDecodeAccelerator::ThreadChecker()
    const {
  return thread_checker_;
}

base::WeakPtr<gpu::gles2::GLES2Decoder>
AndroidVideoDecodeAccelerator::GetGlDecoder() const {
  return get_gles2_decoder_cb_.Run();
}

gpu::gles2::TextureRef* AndroidVideoDecodeAccelerator::GetTextureForPicture(
    const PictureBuffer& picture_buffer) {
  auto gles_decoder = GetGlDecoder();
  RETURN_ON_FAILURE(this, gles_decoder, "Failed to get GL decoder",
                    ILLEGAL_STATE, nullptr);
  RETURN_ON_FAILURE(this, gles_decoder->GetContextGroup(),
                    "Null gles_decoder->GetContextGroup()", ILLEGAL_STATE,
                    nullptr);
  gpu::gles2::TextureManager* texture_manager =
      gles_decoder->GetContextGroup()->texture_manager();
  RETURN_ON_FAILURE(this, texture_manager, "Null texture_manager",
                    ILLEGAL_STATE, nullptr);

  DCHECK_LE(1u, picture_buffer.internal_texture_ids().size());
  gpu::gles2::TextureRef* texture_ref =
      texture_manager->GetTexture(picture_buffer.internal_texture_ids()[0]);
  RETURN_ON_FAILURE(this, texture_manager, "Null texture_ref", ILLEGAL_STATE,
                    nullptr);

  return texture_ref;
}

scoped_refptr<gl::SurfaceTexture>
AndroidVideoDecodeAccelerator::CreateAttachedSurfaceTexture(
    GLuint* service_id) {
  GLuint texture_id;
  glGenTextures(1, &texture_id);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  auto gl_decoder = GetGlDecoder();
  gl_decoder->RestoreTextureUnitBindings(0);
  gl_decoder->RestoreActiveTexture();
  DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  *service_id = texture_id;
  // Previously, to reduce context switching, we used to create an unattached
  // SurfaceTexture and attach it lazily in the compositor's context. But that
  // was flaky because SurfaceTexture#detachFromGLContext() is buggy on a lot of
  // devices. Now we attach it to the current context, which means we might have
  // to context switch later to call updateTexImage(). Fortunately, if virtual
  // contexts are in use, we won't have to context switch.
  return gl::SurfaceTexture::Create(texture_id);
}

void AndroidVideoDecodeAccelerator::OnDestroyingSurface(int surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("media", "AVDA::OnDestroyingSurface");
  DVLOG(1) << __FUNCTION__ << " surface_id: " << surface_id;

  if (surface_id != config_.surface_id)
    return;

  // If we're currently asynchronously configuring a codec, it will be destroyed
  // when configuration completes and it notices that |state_| has changed to
  // SURFACE_DESTROYED.
  state_ = SURFACE_DESTROYED;
  if (media_codec_) {
    ReleaseMediaCodec();
    strategy_->CodecChanged(media_codec_.get());
  }
  // If we're draining, signal completion now because the drain can no longer
  // proceed.
  if (drain_type_ != DRAIN_TYPE_NONE)
    OnDrainCompleted();
}

void AndroidVideoDecodeAccelerator::OnFrameAvailable() {
  // Remember: this may be on any thread.
  DCHECK(strategy_);
  strategy_->OnFrameAvailable();
}

void AndroidVideoDecodeAccelerator::PostError(
    const ::tracked_objects::Location& from_here,
    VideoDecodeAccelerator::Error error) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      from_here,
      base::Bind(&AndroidVideoDecodeAccelerator::NotifyError,
                 weak_this_factory_.GetWeakPtr(), error, error_sequence_token_),
      (defer_errors_ ? ErrorPostingDelay() : base::TimeDelta()));
  state_ = ERROR;
}

void AndroidVideoDecodeAccelerator::InitializeCdm() {
  DVLOG(2) << __FUNCTION__ << ": " << config_.cdm_id;

#if !defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  NOTIMPLEMENTED();
  NotifyInitializationComplete(false);
#else
  // Store the CDM to hold a reference to it.
  cdm_for_reference_holding_only_ =
      MojoCdmService::LegacyGetCdm(config_.cdm_id);
  DCHECK(cdm_for_reference_holding_only_);

  // On Android platform the CdmContext must be a MediaDrmBridgeCdmContext.
  media_drm_bridge_cdm_context_ = static_cast<MediaDrmBridgeCdmContext*>(
      cdm_for_reference_holding_only_->GetCdmContext());
  DCHECK(media_drm_bridge_cdm_context_);

  // Register CDM callbacks. The callbacks registered will be posted back to
  // this thread via BindToCurrentLoop.

  // Since |this| holds a reference to the |cdm_|, by the time the CDM is
  // destructed, UnregisterPlayer() must have been called and |this| has been
  // destructed as well. So the |cdm_unset_cb| will never have a chance to be
  // called.
  // TODO(xhwang): Remove |cdm_unset_cb| after it's not used on all platforms.
  cdm_registration_id_ = media_drm_bridge_cdm_context_->RegisterPlayer(
      BindToCurrentLoop(base::Bind(&AndroidVideoDecodeAccelerator::OnKeyAdded,
                                   weak_this_factory_.GetWeakPtr())),
      base::Bind(&base::DoNothing));

  // Deferred initialization will continue in OnMediaCryptoReady().
  media_drm_bridge_cdm_context_->SetMediaCryptoReadyCB(BindToCurrentLoop(
      base::Bind(&AndroidVideoDecodeAccelerator::OnMediaCryptoReady,
                 weak_this_factory_.GetWeakPtr())));
#endif  // !defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
}

void AndroidVideoDecodeAccelerator::OnMediaCryptoReady(
    MediaDrmBridgeCdmContext::JavaObjectPtr media_crypto,
    bool needs_protected_surface) {
  DVLOG(1) << __FUNCTION__;

  if (!media_crypto) {
    LOG(ERROR) << "MediaCrypto is not available, can't play encrypted stream.";
    cdm_for_reference_holding_only_ = nullptr;
    media_drm_bridge_cdm_context_ = nullptr;
    NotifyInitializationComplete(false);
    return;
  }

  DCHECK(!media_crypto->is_null());

  // We assume this is a part of the initialization process, thus MediaCodec
  // is not created yet.
  DCHECK(!media_codec_);

  codec_config_->media_crypto_ = std::move(media_crypto);
  codec_config_->needs_protected_surface_ = needs_protected_surface;

  // After receiving |media_crypto_| we can configure MediaCodec.
  ConfigureMediaCodecAsynchronously();
}

void AndroidVideoDecodeAccelerator::OnKeyAdded() {
  DVLOG(1) << __FUNCTION__;

  if (state_ == WAITING_FOR_KEY)
    state_ = NO_ERROR;

  DoIOTask(true);
}

void AndroidVideoDecodeAccelerator::NotifyInitializationComplete(bool success) {
  if (client_)
    client_->NotifyInitializationComplete(success);
}

void AndroidVideoDecodeAccelerator::NotifyPictureReady(const Picture& picture) {
  if (client_)
    client_->PictureReady(picture);
}

void AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer(
    int input_buffer_id) {
  if (client_)
    client_->NotifyEndOfBitstreamBuffer(input_buffer_id);
}

void AndroidVideoDecodeAccelerator::NotifyFlushDone() {
  if (client_)
    client_->NotifyFlushDone();
}

void AndroidVideoDecodeAccelerator::NotifyResetDone() {
  if (client_)
    client_->NotifyResetDone();
}

void AndroidVideoDecodeAccelerator::NotifyError(
    VideoDecodeAccelerator::Error error,
    int token) {
  DVLOG(1) << __FUNCTION__ << ": error: " << error << " token: " << token
           << " current: " << error_sequence_token_;
  if (token != error_sequence_token_)
    return;

  if (client_)
    client_->NotifyError(error);
}

void AndroidVideoDecodeAccelerator::ManageTimer(bool did_work) {
  bool should_be_running = true;

  base::TimeTicks now = base::TimeTicks::Now();
  if (!did_work && !most_recent_work_.is_null()) {
    // Make sure that we have done work recently enough, else stop the timer.
    if (now - most_recent_work_ > IdleTimerTimeOut()) {
      most_recent_work_ = base::TimeTicks();
      should_be_running = false;
    }
  } else {
    most_recent_work_ = now;
  }

  if (should_be_running)
    g_avda_timer.Pointer()->StartTimer(this);
  else
    g_avda_timer.Pointer()->StopTimer(this);
}

void AndroidVideoDecodeAccelerator::ReleaseMediaCodec() {
  if (!media_codec_)
    return;

  // If codec construction is broken, then we can't release this codec if it's
  // backed by hardware, else it may hang too.  Post it to the construction
  // thread, and it'll get freed if things start working.  If things are
  // already working, then it'll be freed soon.
  //
  // We require software codecs when |allow_autodetection_| is false, so use
  // the stored value as a proxy for whether the MediaCodec is software backed
  // or not.
  if (!codec_config_->allow_autodetection_) {
    media_codec_.reset();
  } else {
    g_avda_timer.Pointer()->StartUsingConstructionThread();
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        g_avda_timer.Pointer()->ConstructionTaskRunner();
    task_runner->DeleteSoon(FROM_HERE, media_codec_.release());
    task_runner->PostTask(
        FROM_HERE, base::Bind(&AVDATimerManager::DoneUsingConstructionThread,
                              base::Unretained(g_avda_timer.Pointer())));
  }
}

// static
bool AndroidVideoDecodeAccelerator::UseDeferredRenderingStrategy(
    const gpu::GpuPreferences& gpu_preferences) {
  return true;
}

// static
bool AndroidVideoDecodeAccelerator::UseTextureCopyForDeferredStrategy(
    const gpu::GpuPreferences& gpu_preferences) {
  // http://crbug.com/582170
  return gpu_preferences.enable_threaded_texture_mailboxes;
}

// static
VideoDecodeAccelerator::Capabilities
AndroidVideoDecodeAccelerator::GetCapabilities(
    const gpu::GpuPreferences& gpu_preferences) {
  Capabilities capabilities;
  SupportedProfiles& profiles = capabilities.supported_profiles;

  // Only support VP8 on Android versions where we don't have to synchronously
  // tear down the MediaCodec on surface destruction because VP8 requires
  // us to completely drain the decoder before releasing it, which is difficult
  // and time consuming to do while the surface is being destroyed.
  if (base::android::BuildInfo::GetInstance()->sdk_int() >= 18 &&
      MediaCodecUtil::IsVp8DecoderAvailable()) {
    SupportedProfile profile;
    profile.profile = VP8PROFILE_ANY;
    profile.min_resolution.SetSize(0, 0);
    profile.max_resolution.SetSize(3840, 2160);
    // If we know MediaCodec will just create a software codec, prefer our
    // internal software decoder instead. It's more up to date and secured
    // within the renderer sandbox. However if the content is encrypted, we
    // must use MediaCodec anyways since MediaDrm offers no way to decrypt
    // the buffers and let us use our internal software decoders.
    profile.encrypted_only =
        VideoCodecBridge::IsKnownUnaccelerated(kCodecVP8, MEDIA_CODEC_DECODER);
    profiles.push_back(profile);
  }

  if (MediaCodecUtil::IsVp9DecoderAvailable()) {
    SupportedProfile profile;
    profile.min_resolution.SetSize(0, 0);
    profile.max_resolution.SetSize(3840, 2160);
    // If we know MediaCodec will just create a software codec, prefer our
    // internal software decoder instead. It's more up to date and secured
    // within the renderer sandbox. However if the content is encrypted, we
    // must use MediaCodec anyways since MediaDrm offers no way to decrypt
    // the buffers and let us use our internal software decoders.
    profile.encrypted_only =
        VideoCodecBridge::IsKnownUnaccelerated(kCodecVP9, MEDIA_CODEC_DECODER);
    profile.profile = VP9PROFILE_PROFILE0;
    profiles.push_back(profile);
    profile.profile = VP9PROFILE_PROFILE1;
    profiles.push_back(profile);
    profile.profile = VP9PROFILE_PROFILE2;
    profiles.push_back(profile);
    profile.profile = VP9PROFILE_PROFILE3;
    profiles.push_back(profile);
  }

  for (const auto& supported_profile : kSupportedH264Profiles) {
    SupportedProfile profile;
    profile.profile = supported_profile;
    profile.min_resolution.SetSize(0, 0);
    // Advertise support for 4k and let the MediaCodec fail when decoding if it
    // doesn't support the resolution. It's assumed that consumers won't have
    // software fallback for H264 on Android anyway.
    profile.max_resolution.SetSize(3840, 2160);
    profiles.push_back(profile);
  }

  capabilities.flags =
      VideoDecodeAccelerator::Capabilities::SUPPORTS_DEFERRED_INITIALIZATION;
  if (UseDeferredRenderingStrategy(gpu_preferences)) {
    capabilities.flags |= VideoDecodeAccelerator::Capabilities::
        NEEDS_ALL_PICTURE_BUFFERS_TO_DECODE;
    if (UseTextureCopyForDeferredStrategy(gpu_preferences)) {
      capabilities.flags |=
          media::VideoDecodeAccelerator::Capabilities::REQUIRES_TEXTURE_COPY;
    } else if (media::MediaCodecUtil::IsSurfaceViewOutputSupported()) {
      // Fullscreen external SurfaceView is disabled for WebView.
      // http://crbug.com/582170
      capabilities.flags |= media::VideoDecodeAccelerator::Capabilities::
          SUPPORTS_EXTERNAL_OUTPUT_SURFACE;
    }
  }

  return capabilities;
}

}  // namespace media

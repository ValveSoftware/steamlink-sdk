// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_VIDEO_DECODE_ACCELERATOR_H_
#define MEDIA_GPU_ANDROID_VIDEO_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include <list>
#include <map>
#include <queue>
#include <vector>

#include "base/compiler_specific.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "media/base/android/media_drm_bridge_cdm_context.h"
#include "media/base/android/sdk_media_codec_bridge.h"
#include "media/base/media_keys.h"
#include "media/gpu/avda_codec_allocator.h"
#include "media/gpu/avda_picture_buffer_manager.h"
#include "media/gpu/avda_state_provider.h"
#include "media/gpu/avda_surface_tracker.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace gl {
class SurfaceTexture;
}

namespace media {
class SharedMemoryRegion;

// A VideoDecodeAccelerator implementation for Android. This class decodes the
// encded input stream using Android's MediaCodec. It handles the work of
// transferring data to and from MediaCodec, and delegates attaching MediaCodec
// output buffers to PictureBuffers to AVDAPictureBufferManager.
class MEDIA_GPU_EXPORT AndroidVideoDecodeAccelerator
    : public VideoDecodeAccelerator,
      public AVDAStateProvider {
 public:
  static VideoDecodeAccelerator::Capabilities GetCapabilities(
      const gpu::GpuPreferences& gpu_preferences);

  AndroidVideoDecodeAccelerator(
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const GetGLES2DecoderCallback& get_gles2_decoder_cb);

  ~AndroidVideoDecodeAccelerator() override;

  // VideoDecodeAccelerator implementation:
  bool Initialize(const Config& config, Client* client) override;
  void Decode(const BitstreamBuffer& bitstream_buffer) override;
  void AssignPictureBuffers(const std::vector<PictureBuffer>& buffers) override;
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  void Flush() override;
  void Reset() override;
  void SetSurface(int32_t surface_id) override;
  void Destroy() override;
  bool TryToSetupDecodeOnSeparateThread(
      const base::WeakPtr<Client>& decode_client,
      const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner)
      override;

  // AVDAStateProvider implementation:
  const gfx::Size& GetSize() const override;
  base::WeakPtr<gpu::gles2::GLES2Decoder> GetGlDecoder() const override;
  // Notifies the client about the error and sets |state_| to |ERROR|.
  void NotifyError(Error error) override;

 private:
  friend class AVDAManager;

  // TODO(timav): evaluate the need for more states in the AVDA state machine.
  enum State {
    NO_ERROR,
    ERROR,
    // Set when we are asynchronously constructing the codec.  Will transition
    // to NO_ERROR or ERROR depending on success.
    WAITING_FOR_CODEC,
    // Set when we have a codec, but it doesn't yet have a key.
    WAITING_FOR_KEY,
    // The output surface was destroyed. We must not configure a new MediaCodec
    // with the destroyed surface.
    SURFACE_DESTROYED,
  };

  enum DrainType {
    DRAIN_TYPE_NONE,
    DRAIN_FOR_FLUSH,
    DRAIN_FOR_RESET,
    DRAIN_FOR_DESTROY,
  };

  // Configuration info for MediaCodec.
  // This is used to shuttle configuration info between threads without needing
  // to worry about the lifetime of the AVDA instance.  All of these should not
  // be modified while |state_| is WAITING_FOR_CODEC.
  class CodecConfig : public base::RefCountedThreadSafe<CodecConfig> {
   public:
    CodecConfig();

    // Codec type. Used when we configure media codec.
    VideoCodec codec_ = kUnknownVideoCodec;

    // Whether encryption scheme requires to use protected surface.
    bool needs_protected_surface_ = false;

    // The surface that MediaCodec is configured to output to.
    gl::ScopedJavaSurface surface_;

    // The MediaCrypto object is used in the MediaCodec.configure() in case of
    // an encrypted stream.
    MediaDrmBridgeCdmContext::JavaObjectPtr media_crypto_;

    // Initial coded size.  The actual size might change at any time, so this
    // is only a hint.
    gfx::Size initial_expected_coded_size_;

    // The type of allocation to use for this.  We use this to select the right
    // thread for construction / destruction, and to decide if we should
    // restrict the codec to be software only.
    AVDACodecAllocator::TaskType task_type_;

    // Codec specific data (SPS and PPS for H264).
    std::vector<uint8_t> csd0_;
    std::vector<uint8_t> csd1_;

   protected:
    friend class base::RefCountedThreadSafe<CodecConfig>;
    virtual ~CodecConfig();

   private:
    DISALLOW_COPY_AND_ASSIGN(CodecConfig);
  };

  // Callback that is called when the SurfaceView becomes available, if it's
  // not during Initialize.  |success| is true if it is now available, false
  // if initialization should stop.
  void OnSurfaceAvailable(bool success);

  // Initialize of the picture buffer manager.  This is to be called when the
  // SurfaceView in |surface_id_|, if any, is no longer busy.  It will return
  // false on failure, and true if initialization was successful.  This includes
  // synchronous and asynchronous init; the AVDA might not yet have a codec on
  // success, but async init will at least be in progress.
  bool InitializePictureBufferManager();

  // A part of destruction process that is sometimes postponed after the drain.
  void ActualDestroy();

  // Configures |media_codec_| with the given codec parameters from the client.
  // This configuration will (probably) not be complete before this call
  // returns.  Multiple calls before completion will be ignored.  |state_|
  // must be NO_ERROR or WAITING_FOR_CODEC.  Note that, once you call this,
  // you should be careful to avoid modifying members of |codec_config_| until
  // |state_| is no longer WAITING_FOR_CODEC.
  void ConfigureMediaCodecAsynchronously();

  // Like ConfigureMediaCodecAsynchronously, but synchronous.  Returns true if
  // and only if |media_codec_| is non-null.  Since all configuration is done
  // synchronously, there is no concern with modifying |codec_config_| after
  // this returns.
  bool ConfigureMediaCodecSynchronously();

  // Instantiate a media codec using |codec_config|.
  // This may be called on any thread.
  static std::unique_ptr<VideoCodecBridge> ConfigureMediaCodecOnAnyThread(
      scoped_refptr<CodecConfig> codec_config);

  // Called on the main thread to update |media_codec_| and complete codec
  // configuration.  |media_codec| will be null if configuration failed.
  void OnCodecConfigured(std::unique_ptr<VideoCodecBridge> media_codec);

  // Sends the decoded frame specified by |codec_buffer_index| to the client.
  void SendDecodedFrameToClient(int32_t codec_buffer_index,
                                int32_t bitstream_id);

  // Does pending IO tasks if any. Once this is called, it polls |media_codec_|
  // until it finishes pending tasks. For the polling, |kDecodePollDelay| is
  // used.
  void DoIOTask(bool start_timer);

  // Feeds input data to |media_codec_|. This checks
  // |pending_bitstream_buffers_| and queues a buffer to |media_codec_|.
  // Returns true if any input was processed.
  bool QueueInput();

  // Dequeues output from |media_codec_| and feeds the decoded frame to the
  // client.  Returns a hint about whether calling again might produce
  // more output.
  bool DequeueOutput();

  // Requests picture buffers from the client.
  void RequestPictureBuffers();

  // Decode the content in the |bitstream_buffer|. Note that a
  // |bitstream_buffer| of id as -1 indicates a flush command.
  void DecodeBuffer(const BitstreamBuffer& bitstream_buffer);

  // Called during Initialize() for encrypted streams to set up the CDM.
  void InitializeCdm();

  // Called after the CDM obtains a MediaCrypto object.
  void OnMediaCryptoReady(MediaDrmBridgeCdmContext::JavaObjectPtr media_crypto,
                          bool needs_protected_surface);

  // Called when a new key is added to the CDM.
  void OnKeyAdded();

  // Notifies the client of the result of deferred initialization.
  void NotifyInitializationComplete(bool success);

  // Notifies the client about the availability of a picture.
  void NotifyPictureReady(const Picture& picture);

  // Notifies the client that the input buffer identifed by input_buffer_id has
  // been processed.
  void NotifyEndOfBitstreamBuffer(int input_buffer_id);

  // Notifies the client that the decoder was flushed.
  void NotifyFlushDone();

  // Notifies the client that the decoder was reset.
  void NotifyResetDone();

  // Start or stop our work-polling timer based on whether we did any work, and
  // how long it has been since we've done work.  Calling this with true will
  // start the timer.  Calling it with false may stop the timer.
  void ManageTimer(bool did_work);

  // Safely clear |media_codec_|.  Do this instead of calling reset() / assign.
  // Otherwise, the destructor can hang if mediaserver is in a bad state.  This
  // will release immediately if safe, else post to a separate thread.  Either
  // way, |media_codec_| will be null upon return.
  void ReleaseMediaCodec();

  // Start the MediaCodec drain process by adding end_of_stream() buffer to the
  // encoded buffers queue. When we receive EOS from the output buffer the drain
  // process completes and we perform the action depending on the |drain_type|.
  void StartCodecDrain(DrainType drain_type);

  // Returns true if we are currently draining the codec and doing that as part
  // of Reset() or Destroy() VP8 workaround. (http://crbug.com/598963). We won't
  // display any frames and disable normal errors handling.
  bool IsDrainingForResetOrDestroy() const;

  // A helper method that performs the operation required after the drain
  // completion (usually when we receive EOS in the output). The operation
  // itself depends on the |drain_type_|.
  void OnDrainCompleted();

  // Resets MediaCodec and buffers/containers used for storing output. These
  // components need to be reset upon EOS to decode a later stream. Input state
  // (e.g. queued BitstreamBuffers) is not reset, as input following an EOS
  // is still valid and should be processed.
  void ResetCodecState();

  // Registered to be called when surfaces are being destroyed. If |surface_id|
  // is our surface, we should release the MediaCodec before returning from
  // this.
  void OnDestroyingSurface(int surface_id);

  // Indicates if MediaCodec should not be used for software decoding since we
  // have safer versions elsewhere.
  bool IsMediaCodecSoftwareDecodingForbidden() const;

  // On platforms which support seamless surface changes, this will reinitialize
  // the picture buffer manager with the new surface. This function reads and
  // clears the surface id from |pending_surface_id_|. It will issue a decode
  // error if the surface change fails. Returns false on failure.
  bool UpdateSurface();

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // To expose client callbacks from VideoDecodeAccelerator.
  Client* client_;

  // Callback to set the correct gl context.
  MakeGLContextCurrentCallback make_context_current_cb_;

  // Callback to get the GLES2Decoder instance.
  GetGLES2DecoderCallback get_gles2_decoder_cb_;

  // The current state of this class. For now, this is used only for setting
  // error state.
  State state_;

  // The assigned picture buffers by picture buffer id.
  AVDAPictureBufferManager::PictureBufferMap output_picture_buffers_;

  // This keeps the free picture buffer ids which can be used for sending
  // decoded frames to the client.
  std::queue<int32_t> free_picture_ids_;

  // The low-level decoder which Android SDK provides.
  std::unique_ptr<VideoCodecBridge> media_codec_;

  // Set to true after requesting picture buffers to the client.
  bool picturebuffers_requested_;

  // The resolution of the stream.
  gfx::Size size_;

  // Handy structure to remember a BitstreamBuffer and also its shared memory,
  // if any.  The goal is to prevent leaving a BitstreamBuffer's shared memory
  // handle open.
  struct BitstreamRecord {
    BitstreamRecord(const BitstreamBuffer&);
    BitstreamRecord(BitstreamRecord&& other);
    ~BitstreamRecord();

    BitstreamBuffer buffer;

    // |memory| is not mapped, and may be null if buffer has no data.
    std::unique_ptr<SharedMemoryRegion> memory;
  };

  // Encoded bitstream buffers to be passed to media codec, queued until an
  // input buffer is available.
  std::queue<BitstreamRecord> pending_bitstream_records_;

  // A map of presentation timestamp to bitstream buffer id for the bitstream
  // buffers that have been submitted to the decoder but haven't yet produced an
  // output frame with the same timestamp. Note: there will only be one entry
  // for multiple bitstream buffers that have the same presentation timestamp.
  std::map<base::TimeDelta, int32_t> bitstream_buffers_in_decoder_;

  // Keeps track of bitstream ids notified to the client with
  // NotifyEndOfBitstreamBuffer() before getting output from the bitstream.
  std::list<int32_t> bitstreams_notified_in_advance_;

  AVDAPictureBufferManager picture_buffer_manager_;

  // Time at which we last did useful work on io_timer_.
  base::TimeTicks most_recent_work_;

  // Type of a drain request. We need to distinguish between DRAIN_FOR_FLUSH
  // and other types, see IsDrainingForResetOrDestroy().
  DrainType drain_type_;

  // Holds a ref-count to the CDM to avoid using the CDM after it's destroyed.
  scoped_refptr<MediaKeys> cdm_for_reference_holding_only_;

  MediaDrmBridgeCdmContext* media_drm_bridge_cdm_context_;

  // MediaDrmBridge requires registration/unregistration of the player, this
  // registration id is used for this.
  int cdm_registration_id_;

  // Configuration that we use for MediaCodec.
  // Do not update any of its members while |state_| is WAITING_FOR_CODEC.
  scoped_refptr<CodecConfig> codec_config_;

  // Index of the dequeued and filled buffer that we keep trying to enqueue.
  // Such buffer appears in MEDIA_CODEC_NO_KEY processing.
  int pending_input_buf_index_;

  // Monotonically increasing value that is used to prevent old, delayed errors
  // from being sent after a reset.
  int error_sequence_token_;

  // True if and only if VDA initialization is deferred, and we have not yet
  // called NotifyInitializationComplete.
  bool deferred_initialization_pending_;

  // Indicates if ResetCodecState() should be called upon the next call to
  // Decode(). Allows us to avoid trashing the last few frames of a playback
  // when the EOS buffer is received.
  bool codec_needs_reset_;

  // True if surface creation and |picture_buffer_manager_| initialization has
  // been defered until the first Decode() call.
  bool defer_surface_creation_;

  // Has a value if a SetSurface() call has occurred and a new surface should be
  // switched to when possible. Cleared during OnDestroyingSurface() and if all
  // pictures have been rendered in DequeueOutput().
  base::Optional<int32_t> pending_surface_id_;

  // Copy of the VDA::Config we were given.
  Config config_;

  OnDestroyingSurfaceCallback on_destroying_surface_cb_;

  // WeakPtrFactory for posting tasks back to |this|.
  base::WeakPtrFactory<AndroidVideoDecodeAccelerator> weak_this_factory_;

  friend class AndroidVideoDecodeAcceleratorTest;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_VIDEO_DECODE_ACCELERATOR_H_

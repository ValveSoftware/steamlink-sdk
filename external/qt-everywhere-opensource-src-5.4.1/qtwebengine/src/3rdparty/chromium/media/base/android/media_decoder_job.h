// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_DECODER_JOB_H_
#define MEDIA_BASE_ANDROID_MEDIA_DECODER_JOB_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/android/demuxer_stream_player_params.h"
#include "media/base/android/media_codec_bridge.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class MediaDrmBridge;

// Class for managing all the decoding tasks. Each decoding task will be posted
// onto the same thread. The thread will be stopped once Stop() is called.
// Data is stored in 2 chunks. When new data arrives, it is always stored in
// an inactive chunk. And when the current active chunk becomes empty, a new
// data request will be sent to the renderer.
class MediaDecoderJob {
 public:
  struct Deleter {
    inline void operator()(MediaDecoderJob* ptr) const { ptr->Release(); }
  };

  // Callback when a decoder job finishes its work. Args: whether decode
  // finished successfully, current presentation time, max presentation time.
  // If the current presentation time is equal to kNoTimestamp(), the decoder
  // job skipped rendering of the decoded output and the callback target should
  // ignore the timestamps provided.
  typedef base::Callback<void(MediaCodecStatus, base::TimeDelta,
                              base::TimeDelta)> DecoderCallback;
  // Callback when a decoder job finishes releasing the output buffer.
  // Args: current presentation time, max presentation time.
  // If the current presentation time is equal to kNoTimestamp(), the callback
  // target should ignore the timestamps provided.
  typedef base::Callback<void(base::TimeDelta, base::TimeDelta)>
      ReleaseOutputCompletionCallback;

  virtual ~MediaDecoderJob();

  // Called by MediaSourcePlayer when more data for this object has arrived.
  void OnDataReceived(const DemuxerData& data);

  // Prefetch so we know the decoder job has data when we call Decode().
  // |prefetch_cb| - Run when prefetching has completed.
  void Prefetch(const base::Closure& prefetch_cb);

  // Called by MediaSourcePlayer to decode some data.
  // |callback| - Run when decode operation has completed.
  //
  // Returns true if the next decode was started and |callback| will be
  // called when the decode operation is complete.
  // Returns false if |media_codec_bridge_| cannot be created; |callback| is
  // ignored and will not be called.
  bool Decode(base::TimeTicks start_time_ticks,
              base::TimeDelta start_presentation_timestamp,
              const DecoderCallback& callback);

  // Called to stop the last Decode() early.
  // If the decoder is in the process of decoding the next frame, then
  // this method will just allow the decode to complete as normal. If
  // this object is waiting for a data request to complete, then this method
  // will wait for the data to arrive and then call the |callback|
  // passed to Decode() with a status of MEDIA_CODEC_STOPPED. This ensures that
  // the |callback| passed to Decode() is always called and the status
  // reflects whether data was actually decoded or the decode terminated early.
  void StopDecode();

  // Flushes the decoder and abandons all the data that is being decoded.
  virtual void Flush();

  // Enters prerolling state. The job must not currently be decoding.
  void BeginPrerolling(base::TimeDelta preroll_timestamp);

  // Releases all the decoder resources as the current tab is going background.
  virtual void ReleaseDecoderResources();

  // Sets the demuxer configs. Returns true if configs has changed, or false
  // otherwise.
  bool SetDemuxerConfigs(const DemuxerConfigs& configs);

  // Returns whether the decoder has finished decoding all the data.
  bool OutputEOSReached() const;

  // Returns true if the audio/video stream is available, implemented by child
  // classes.
  virtual bool HasStream() const = 0;

  void SetDrmBridge(MediaDrmBridge* drm_bridge);

  bool is_decoding() const { return !decode_cb_.is_null(); }

  bool is_content_encrypted() const { return is_content_encrypted_; }

 protected:
  // Creates a new MediaDecoderJob instance.
  // |decoder_task_runner| - Thread on which the decoder task will run.
  // |request_data_cb| - Callback to request more data for the decoder.
  // |config_changed_cb| - Callback to inform the caller that
  // demuxer config has changed.
  MediaDecoderJob(
      const scoped_refptr<base::SingleThreadTaskRunner>& decoder_task_runner,
      const base::Closure& request_data_cb,
      const base::Closure& config_changed_cb);

  // Release the output buffer at index |output_buffer_index| and render it if
  // |render_output| is true. Upon completion, |callback| will be called.
  virtual void ReleaseOutputBuffer(
      int output_buffer_index,
      size_t size,
      bool render_output,
      base::TimeDelta current_presentation_timestamp,
      const ReleaseOutputCompletionCallback& callback) = 0;

  // Returns true if the "time to render" needs to be computed for frames in
  // this decoder job.
  virtual bool ComputeTimeToRender() const = 0;

  // Gets MediaCrypto object from |drm_bridge_|.
  base::android::ScopedJavaLocalRef<jobject> GetMediaCrypto();

  // Releases the |media_codec_bridge_|.
  void ReleaseMediaCodecBridge();

  MediaDrmBridge* drm_bridge() { return drm_bridge_; }

  void set_is_content_encrypted(bool is_content_encrypted) {
    is_content_encrypted_ = is_content_encrypted;
  }

  bool need_to_reconfig_decoder_job_;

  scoped_ptr<MediaCodecBridge> media_codec_bridge_;

 private:
  friend class MediaSourcePlayerTest;

  // Causes this instance to be deleted on the thread it is bound to.
  void Release();

  // Queues an access unit into |media_codec_bridge_|'s input buffer.
  MediaCodecStatus QueueInputBuffer(const AccessUnit& unit);

  // Returns true if this object has data to decode.
  bool HasData() const;

  // Initiates a request for more data.
  // |done_cb| is called when more data is available in |received_data_|.
  void RequestData(const base::Closure& done_cb);

  // Posts a task to start decoding the current access unit in |received_data_|.
  void DecodeCurrentAccessUnit(
      base::TimeTicks start_time_ticks,
      base::TimeDelta start_presentation_timestamp);

  // Helper function to decode data on |decoder_task_runner_|. |unit| contains
  // the data to be decoded. |start_time_ticks| and
  // |start_presentation_timestamp| represent the system time and the
  // presentation timestamp when the first frame is rendered. We use these
  // information to estimate when the current frame should be rendered.
  // If |needs_flush| is true, codec needs to be flushed at the beginning of
  // this call.
  // It is possible that |stop_decode_pending_| or |release_resources_pending_|
  // becomes true while DecodeInternal() is called. However, they should have
  // no impact on DecodeInternal(). They will be handled after DecoderInternal()
  // finishes and OnDecodeCompleted() is posted on the UI thread.
  void DecodeInternal(const AccessUnit& unit,
                      base::TimeTicks start_time_ticks,
                      base::TimeDelta start_presentation_timestamp,
                      bool needs_flush,
                      const DecoderCallback& callback);

  // Called on the UI thread to indicate that one decode cycle has completed.
  // Completes any pending job destruction or any pending decode stop. If
  // destruction was not pending, passes its arguments to |decode_cb_|.
  void OnDecodeCompleted(MediaCodecStatus status,
                         base::TimeDelta current_presentation_timestamp,
                         base::TimeDelta max_presentation_timestamp);

  // Helper function to get the current access unit that is being decoded.
  const AccessUnit& CurrentAccessUnit() const;

  // Helper function to get the current data chunk index that is being decoded.
  size_t CurrentReceivedDataChunkIndex() const;

  // Check whether a chunk has no remaining access units to decode. If
  // |is_active_chunk| is true, this function returns whether decoder has
  // consumed all data in |received_data_[current_demuxer_data_index_]|.
  // Otherwise, it returns whether decoder has consumed all data in the inactive
  // chunk.
  bool NoAccessUnitsRemainingInChunk(bool is_active_chunk) const;

  // Requests new data for the current chunk if it runs out of data.
  void RequestCurrentChunkIfEmpty();

  // Initializes |received_data_| and |access_unit_index_|.
  void InitializeReceivedData();

  // Called when the decoder is completely drained and is ready to be released.
  void OnDecoderDrained();

  // Creates |media_codec_bridge_| for decoding purpose. Returns true if it is
  // created, or false otherwise.
  bool CreateMediaCodecBridge();

  // Called when an access unit is consumed by the decoder. |is_config_change|
  // indicates whether the current access unit is a config change. If it is
  // true, the next access unit is guarateed to be an I-frame.
  virtual void CurrentDataConsumed(bool is_config_change) {}

  // Called when |media_codec_bridge_| is released
  virtual void OnMediaCodecBridgeReleased() {}

  // Implemented by the child class to create |media_codec_bridge_| for a
  // particular stream. Returns true if it is created, or false otherwise.
  virtual bool CreateMediaCodecBridgeInternal() = 0;

  // Returns true if the |configs| doesn't match the current demuxer configs
  // the decoder job has.
  virtual bool AreDemuxerConfigsChanged(
      const DemuxerConfigs& configs) const = 0;

  // Updates the demuxer configs.
  virtual void UpdateDemuxerConfigs(const DemuxerConfigs& configs) = 0;

  // Returns true if |media_codec_bridge_| needs to be reconfigured for the
  // new DemuxerConfigs, or false otherwise.
  virtual bool IsCodecReconfigureNeeded(const DemuxerConfigs& configs) const;

  // Return the index to |received_data_| that is not currently being decoded.
  size_t inactive_demuxer_data_index() const {
    return 1 - current_demuxer_data_index_;
  }

  // The UI message loop where callbacks should be dispatched.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // The task runner that decoder job runs on.
  scoped_refptr<base::SingleThreadTaskRunner> decoder_task_runner_;

  // Whether the decoder needs to be flushed.
  bool needs_flush_;

  // Whether input EOS is encountered.
  // TODO(wolenetz/qinmin): Protect with a lock. See http://crbug.com/320043.
  bool input_eos_encountered_;

  // Whether output EOS is encountered.
  bool output_eos_encountered_;

  // Tracks whether DecodeInternal() should skip decoding if the first access
  // unit is EOS or empty, and report |MEDIA_CODEC_OUTPUT_END_OF_STREAM|. This
  // is to work around some decoders that could crash otherwise. See
  // http://b/11696552.
  bool skip_eos_enqueue_;

  // The timestamp the decoder needs to preroll to. If an access unit's
  // timestamp is smaller than |preroll_timestamp_|, don't render it.
  // TODO(qinmin): Comparing access unit's timestamp with |preroll_timestamp_|
  // is not very accurate.
  base::TimeDelta preroll_timestamp_;

  // Indicates prerolling state. If true, this job has not yet decoded output
  // that it will render, since the most recent of job construction or
  // BeginPrerolling(). If false, |preroll_timestamp_| has been reached.
  // TODO(qinmin): Comparing access unit's timestamp with |preroll_timestamp_|
  // is not very accurate.
  bool prerolling_;

  // Callback used to request more data.
  base::Closure request_data_cb_;

  // Callback to notify the caller config has changed.
  base::Closure config_changed_cb_;

  // Callback to run when new data has been received.
  base::Closure data_received_cb_;

  // Callback to run when the current Decode() operation completes.
  DecoderCallback decode_cb_;

  // Data received over IPC from last RequestData() operation.
  // We keep 2 chunks at the same time to reduce the IPC latency between chunks.
  // If data inside the current chunk are all decoded, we will request a new
  // chunk from the demuxer and swap the current chunk with the other one.
  // New data will always be stored in the other chunk since the current
  // one may be still in use.
  DemuxerData received_data_[2];

  // Index to the current data chunk that is being decoded.
  size_t current_demuxer_data_index_;

  // Index to the access unit inside each data chunk that is being decoded.
  size_t access_unit_index_[2];

  // The index of input buffer that can be used by QueueInputBuffer().
  // If the index is uninitialized or invalid, it must be -1.
  int input_buf_index_;

  // Indicates whether content is encrypted.
  bool is_content_encrypted_;

  // Indicates the decoder job should stop after decoding the current access
  // unit.
  bool stop_decode_pending_;

  // Indicates that this object should be destroyed once the current
  // Decode() has completed. This gets set when Release() gets called
  // while there is a decode in progress.
  bool destroy_pending_;

  // Indicates whether the decoder is in the middle of requesting new data.
  bool is_requesting_demuxer_data_;

  // Indicates whether the incoming data should be ignored.
  bool is_incoming_data_invalid_;

  // Indicates that |media_codec_bridge_| should be released once the current
  // Decode() has completed. This gets set when ReleaseDecoderResources() gets
  // called while there is a decode in progress.
  bool release_resources_pending_;

  // Pointer to a DRM object that will be used for encrypted streams.
  MediaDrmBridge* drm_bridge_;

  // Indicates whether |media_codec_bridge_| is in the middle of being drained
  // due to a config change.
  bool drain_decoder_;

  // This access unit is passed to the decoder during config changes to drain
  // the decoder.
  AccessUnit eos_unit_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MediaDecoderJob);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_DECODER_JOB_H_

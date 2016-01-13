// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_SOURCE_DELEGATE_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_SOURCE_DELEGATE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer.h"
#include "media/base/media_keys.h"
#include "media/base/pipeline_status.h"
#include "media/base/ranges.h"
#include "media/base/text_track.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"

namespace media {
class ChunkDemuxer;
class DecoderBuffer;
class DecryptingDemuxerStream;
class DemuxerStream;
class MediaLog;
struct DemuxerConfigs;
struct DemuxerData;
}

namespace content {

class RendererDemuxerAndroid;

class MediaSourceDelegate : public media::DemuxerHost {
 public:
  typedef base::Callback<void(blink::WebMediaSource*)>
      MediaSourceOpenedCB;
  typedef base::Callback<void(blink::WebMediaPlayer::NetworkState)>
      UpdateNetworkStateCB;
  typedef base::Callback<void(const base::TimeDelta&)> DurationChangeCB;

  // Helper class used by scoped_ptr to destroy an instance of
  // MediaSourceDelegate.
  class Destroyer {
   public:
    inline void operator()(void* media_source_delegate) const {
      static_cast<MediaSourceDelegate*>(media_source_delegate)->Destroy();
    }
  };

  MediaSourceDelegate(RendererDemuxerAndroid* demuxer_client,
                      int demuxer_client_id,
                      const scoped_refptr<base::MessageLoopProxy>& media_loop,
                      media::MediaLog* media_log);

  // Initialize the MediaSourceDelegate. |media_source| will be owned by
  // this object after this call.
  void InitializeMediaSource(
      const MediaSourceOpenedCB& media_source_opened_cb,
      const media::Demuxer::NeedKeyCB& need_key_cb,
      const media::SetDecryptorReadyCB& set_decryptor_ready_cb,
      const UpdateNetworkStateCB& update_network_state_cb,
      const DurationChangeCB& duration_change_cb);

  blink::WebTimeRanges Buffered() const;
  size_t DecodedFrameCount() const;
  size_t DroppedFrameCount() const;
  size_t AudioDecodedByteCount() const;
  size_t VideoDecodedByteCount() const;

  // In MSE case, calls ChunkDemuxer::CancelPendingSeek(). Also sets the
  // expectation that a regular seek will be arriving and to trivially finish
  // any browser seeks that may be requested prior to the regular seek.
  void CancelPendingSeek(const base::TimeDelta& seek_time);

  // In MSE case, calls ChunkDemuxer::StartWaitingForSeek(), first calling
  // ChunkDemuxer::CancelPendingSeek() if a browser seek is in progress.
  // Also sets the expectation that a regular seek will be arriving and to
  // trivially finish any browser seeks that may be requested prior to the
  // regular seek.
  void StartWaitingForSeek(const base::TimeDelta& seek_time);

  // Seeks the demuxer and later calls OnDemuxerSeekDone() after the seek has
  // been completed. There must be no other seek of the demuxer currently in
  // process when this method is called.
  // If |is_browser_seek| is true, then this is a short-term hack browser
  // seek.
  // TODO(wolenetz): Instead of doing browser seek, browser player should replay
  // cached data since last keyframe. See http://crbug.com/304234.
  void Seek(const base::TimeDelta& seek_time, bool is_browser_seek);

  // Called when DemuxerStreamPlayer needs to read data from ChunkDemuxer.
  void OnReadFromDemuxer(media::DemuxerStream::Type type);

  // Called by the Destroyer to destroy an instance of this object.
  void Destroy();

  // Called on the main thread to check whether the video stream is encrypted.
  bool IsVideoEncrypted();

  // Gets the ChunkDemuxer timeline offset.
  base::Time GetTimelineOffset() const;

 private:
  // This is private to enforce use of the Destroyer.
  virtual ~MediaSourceDelegate();

  // Methods inherited from DemuxerHost.
  virtual void AddBufferedTimeRange(base::TimeDelta start,
                                    base::TimeDelta end) OVERRIDE;
  virtual void SetDuration(base::TimeDelta duration) OVERRIDE;
  virtual void OnDemuxerError(media::PipelineStatus status) OVERRIDE;
  virtual void AddTextStream(media::DemuxerStream* text_stream,
                             const media::TextTrackConfig& config) OVERRIDE;
  virtual void RemoveTextStream(media::DemuxerStream* text_stream) OVERRIDE;

  // Notifies |demuxer_client_| and fires |duration_changed_cb_|.
  void OnDurationChanged(const base::TimeDelta& duration);

  // Callback for ChunkDemuxer initialization.
  void OnDemuxerInitDone(media::PipelineStatus status);

  // Initializes DecryptingDemuxerStreams if audio/video stream is encrypted.
  void InitAudioDecryptingDemuxerStream();
  void InitVideoDecryptingDemuxerStream();

  // Callbacks for DecryptingDemuxerStream::Initialize().
  void OnAudioDecryptingDemuxerStreamInitDone(media::PipelineStatus status);
  void OnVideoDecryptingDemuxerStreamInitDone(media::PipelineStatus status);

  // Callback for ChunkDemuxer::Seek() and callback chain for resetting
  // decrypted audio/video streams if present.
  //
  // Runs on the media thread.
  void OnDemuxerSeekDone(media::PipelineStatus status);
  void ResetAudioDecryptingDemuxerStream();
  void ResetVideoDecryptingDemuxerStream();
  void FinishResettingDecryptingDemuxerStreams();

  // Callback for ChunkDemuxer::Stop() and helper for deleting |this| on the
  // main thread.
  void OnDemuxerStopDone();
  void DeleteSelf();

  void OnDemuxerOpened();
  void OnNeedKey(const std::string& type,
                 const std::vector<uint8>& init_data);
  void NotifyDemuxerReady();

  void StopDemuxer();
  void InitializeDemuxer();
  void SeekInternal(const base::TimeDelta& seek_time);
  // Reads an access unit from the demuxer stream |stream| and stores it in
  // the |index|th access unit in |params|.
  void ReadFromDemuxerStream(media::DemuxerStream::Type type,
                             scoped_ptr<media::DemuxerData> data,
                             size_t index);
  void OnBufferReady(media::DemuxerStream::Type type,
                     scoped_ptr<media::DemuxerData> data,
                     size_t index,
                     media::DemuxerStream::Status status,
                     const scoped_refptr<media::DecoderBuffer>& buffer);

  // Helper function for calculating duration.
  base::TimeDelta GetDuration() const;

  bool IsSeeking() const;

  // Returns |seek_time| if it is still buffered or if there is no currently
  // buffered range including or soon after |seek_time|. If |seek_time| is not
  // buffered, but there is a later range buffered near to |seek_time|, returns
  // next buffered range's start time instead. Only call this for browser seeks.
  // |seeking_lock_| must be held by caller.
  base::TimeDelta FindBufferedBrowserSeekTime_Locked(
      const base::TimeDelta& seek_time) const;

  // Get the demuxer configs for a particular stream identified by |is_audio|.
  // Returns true on success, of false otherwise.
  bool GetDemuxerConfigFromStream(media::DemuxerConfigs* configs,
                                  bool is_audio);

  RendererDemuxerAndroid* demuxer_client_;
  int demuxer_client_id_;

  scoped_refptr<media::MediaLog> media_log_;
  UpdateNetworkStateCB update_network_state_cb_;
  DurationChangeCB duration_change_cb_;

  scoped_ptr<media::ChunkDemuxer> chunk_demuxer_;
  bool is_demuxer_ready_;

  media::SetDecryptorReadyCB set_decryptor_ready_cb_;

  scoped_ptr<media::DecryptingDemuxerStream> audio_decrypting_demuxer_stream_;
  scoped_ptr<media::DecryptingDemuxerStream> video_decrypting_demuxer_stream_;

  media::DemuxerStream* audio_stream_;
  media::DemuxerStream* video_stream_;

  media::PipelineStatistics statistics_;
  media::Ranges<base::TimeDelta> buffered_time_ranges_;

  MediaSourceOpenedCB media_source_opened_cb_;
  media::Demuxer::NeedKeyCB need_key_cb_;

  // Temporary for EME v0.1. In the future the init data type should be passed
  // through GenerateKeyRequest() directly from WebKit.
  std::string init_data_type_;

  // Lock used to serialize access for |seeking_|.
  mutable base::Lock seeking_lock_;
  bool seeking_;

  // Lock used to serialize access for |is_video_encrypted_|.
  mutable base::Lock is_video_encrypted_lock_;
  bool is_video_encrypted_;

  // Track if we are currently performing a browser seek, and track whether or
  // not a regular seek is expected soon. If a regular seek is expected soon,
  // then any in-progress browser seek will be canceled pending the
  // regular seek, if using |chunk_demuxer_|, and any requested browser seek
  // will be trivially finished. Access is serialized by |seeking_lock_|.
  bool doing_browser_seek_;
  base::TimeDelta browser_seek_time_;
  bool expecting_regular_seek_;

  size_t access_unit_size_;

  // Message loop for main renderer and media threads.
  const scoped_refptr<base::MessageLoopProxy> main_loop_;
  const scoped_refptr<base::MessageLoopProxy> media_loop_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaSourceDelegate> main_weak_factory_;
  base::WeakPtrFactory<MediaSourceDelegate> media_weak_factory_;
  base::WeakPtr<MediaSourceDelegate> main_weak_this_;

  DISALLOW_COPY_AND_ASSIGN(MediaSourceDelegate);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_SOURCE_DELEGATE_H_

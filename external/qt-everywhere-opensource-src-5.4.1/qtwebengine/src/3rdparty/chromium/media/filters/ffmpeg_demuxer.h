// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Demuxer interface using FFmpeg's libavformat.  At this time
// will support demuxing any audio/video format thrown at it.  The streams
// output mime types audio/x-ffmpeg and video/x-ffmpeg and include an integer
// key FFmpegCodecID which contains the CodecID enumeration value.  The CodecIDs
// can be used to create and initialize the corresponding FFmpeg decoder.
//
// FFmpegDemuxer sets the duration of pipeline during initialization by using
// the duration of the longest audio/video stream.
//
// NOTE: since FFmpegDemuxer reads packets sequentially without seeking, media
// files with very large drift between audio/video streams may result in
// excessive memory consumption.
//
// When stopped, FFmpegDemuxer and FFmpegDemuxerStream release all callbacks
// and buffered packets.  Reads from a stopped FFmpegDemuxerStream will not be
// replied to.

#ifndef MEDIA_FILTERS_FFMPEG_DEMUXER_H_
#define MEDIA_FILTERS_FFMPEG_DEMUXER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decoder_buffer_queue.h"
#include "media/base/demuxer.h"
#include "media/base/pipeline.h"
#include "media/base/text_track_config.h"
#include "media/base/video_decoder_config.h"
#include "media/ffmpeg/ffmpeg_deleters.h"
#include "media/filters/blocking_url_protocol.h"

// FFmpeg forward declarations.
struct AVPacket;
struct AVRational;
struct AVStream;

namespace media {

class MediaLog;
class FFmpegDemuxer;
class FFmpegGlue;
class FFmpegH264ToAnnexBBitstreamConverter;

typedef scoped_ptr<AVPacket, ScopedPtrAVFreePacket> ScopedAVPacket;

class FFmpegDemuxerStream : public DemuxerStream {
 public:
  // Keeps a copy of |demuxer| and initializes itself using information
  // inside |stream|.  Both parameters must outlive |this|.
  FFmpegDemuxerStream(FFmpegDemuxer* demuxer, AVStream* stream);
  virtual ~FFmpegDemuxerStream();

  // Enqueues the given AVPacket. It is invalid to queue a |packet| after
  // SetEndOfStream() has been called.
  void EnqueuePacket(ScopedAVPacket packet);

  // Enters the end of stream state. After delivering remaining queued buffers
  // only end of stream buffers will be delivered.
  void SetEndOfStream();

  // Drops queued buffers and clears end of stream state.
  void FlushBuffers();

  // Empties the queues and ignores any additional calls to Read().
  void Stop();

  // Returns the duration of this stream.
  base::TimeDelta duration();

  // DemuxerStream implementation.
  virtual Type type() OVERRIDE;
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual void EnableBitstreamConverter() OVERRIDE;
  virtual bool SupportsConfigChanges() OVERRIDE;
  virtual AudioDecoderConfig audio_decoder_config() OVERRIDE;
  virtual VideoDecoderConfig video_decoder_config() OVERRIDE;

  // Returns the range of buffered data in this stream.
  Ranges<base::TimeDelta> GetBufferedRanges() const;

  // Returns elapsed time based on the already queued packets.
  // Used to determine stream duration when it's not known ahead of time.
  base::TimeDelta GetElapsedTime() const;

  // Returns true if this stream has capacity for additional data.
  bool HasAvailableCapacity();

  // Returns the total buffer size FFMpegDemuxerStream is holding onto.
  size_t MemoryUsage() const;

  TextKind GetTextKind() const;

  // Returns the value associated with |key| in the metadata for the avstream.
  // Returns an empty string if the key is not present.
  std::string GetMetadata(const char* key) const;

 private:
  friend class FFmpegDemuxerTest;

  // Runs |read_cb_| if present with the front of |buffer_queue_|, calling
  // NotifyCapacityAvailable() if capacity is still available.
  void SatisfyPendingRead();

  // Converts an FFmpeg stream timestamp into a base::TimeDelta.
  static base::TimeDelta ConvertStreamTimestamp(const AVRational& time_base,
                                                int64 timestamp);

  FFmpegDemuxer* demuxer_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  AVStream* stream_;
  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;
  Type type_;
  base::TimeDelta duration_;
  bool end_of_stream_;
  base::TimeDelta last_packet_timestamp_;
  Ranges<base::TimeDelta> buffered_ranges_;

  DecoderBufferQueue buffer_queue_;
  ReadCB read_cb_;

#if defined(USE_PROPRIETARY_CODECS)
  scoped_ptr<FFmpegH264ToAnnexBBitstreamConverter> bitstream_converter_;
#endif

  bool bitstream_converter_enabled_;

  std::string encryption_key_id_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxerStream);
};

class MEDIA_EXPORT FFmpegDemuxer : public Demuxer {
 public:
  FFmpegDemuxer(const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
                DataSource* data_source,
                const NeedKeyCB& need_key_cb,
                const scoped_refptr<MediaLog>& media_log);
  virtual ~FFmpegDemuxer();

  // Demuxer implementation.
  virtual void Initialize(DemuxerHost* host,
                          const PipelineStatusCB& status_cb,
                          bool enable_text_tracks) OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;
  virtual void Seek(base::TimeDelta time, const PipelineStatusCB& cb) OVERRIDE;
  virtual DemuxerStream* GetStream(DemuxerStream::Type type) OVERRIDE;
  virtual base::TimeDelta GetStartTime() const OVERRIDE;
  virtual base::Time GetTimelineOffset() const OVERRIDE;
  virtual Liveness GetLiveness() const OVERRIDE;

  // Calls |need_key_cb_| with the initialization data encountered in the file.
  void FireNeedKey(const std::string& init_data_type,
                   const std::string& encryption_key_id);

  // Allow FFmpegDemuxerStream to notify us when there is updated information
  // about capacity and what buffered data is available.
  void NotifyCapacityAvailable();
  void NotifyBufferingChanged();

 private:
  // To allow tests access to privates.
  friend class FFmpegDemuxerTest;

  // FFmpeg callbacks during initialization.
  void OnOpenContextDone(const PipelineStatusCB& status_cb, bool result);
  void OnFindStreamInfoDone(const PipelineStatusCB& status_cb, int result);

  // FFmpeg callbacks during seeking.
  void OnSeekFrameDone(const PipelineStatusCB& cb, int result);

  // FFmpeg callbacks during reading + helper method to initiate reads.
  void ReadFrameIfNeeded();
  void OnReadFrameDone(ScopedAVPacket packet, int result);

  // DataSource callbacks during stopping.
  void OnDataSourceStopped(const base::Closure& callback);

  // Returns true iff any stream has additional capacity. Note that streams can
  // go over capacity depending on how the file is muxed.
  bool StreamsHaveAvailableCapacity();

  // Returns true if the maximum allowed memory usage has been reached.
  bool IsMaxMemoryUsageReached() const;

  // Signal all FFmpegDemuxerStreams that the stream has ended.
  void StreamHasEnded();

  // Called by |url_protocol_| whenever |data_source_| returns a read error.
  void OnDataSourceError();

  // Returns the stream from |streams_| that matches |type| as an
  // FFmpegDemuxerStream.
  FFmpegDemuxerStream* GetFFmpegStream(DemuxerStream::Type type) const;

  // Called after the streams have been collected from the media, to allow
  // the text renderer to bind each text stream to the cue rendering engine.
  void AddTextStreams();

  DemuxerHost* host_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Thread on which all blocking FFmpeg operations are executed.
  base::Thread blocking_thread_;

  // Tracks if there's an outstanding av_read_frame() operation.
  //
  // TODO(scherkus): Allow more than one read in flight for higher read
  // throughput using demuxer_bench to verify improvements.
  bool pending_read_;

  // Tracks if there's an outstanding av_seek_frame() operation. Used to discard
  // results of pre-seek av_read_frame() operations.
  bool pending_seek_;

  // |streams_| mirrors the AVStream array in |format_context_|. It contains
  // FFmpegDemuxerStreams encapsluating AVStream objects at the same index.
  //
  // Since we only support a single audio and video stream, |streams_| will
  // contain NULL entries for additional audio/video streams as well as for
  // stream types that we do not currently support.
  //
  // Once initialized, operations on FFmpegDemuxerStreams should be carried out
  // on the demuxer thread.
  typedef ScopedVector<FFmpegDemuxerStream> StreamVector;
  StreamVector streams_;

  // Provides asynchronous IO to this demuxer. Consumed by |url_protocol_| to
  // integrate with libavformat.
  DataSource* data_source_;

  scoped_refptr<MediaLog> media_log_;

  // Derived bitrate after initialization has completed.
  int bitrate_;

  // The first timestamp of the opened media file. This is used to set the
  // starting clock value to match the timestamps in the media file. Default
  // is 0.
  base::TimeDelta start_time_;

  // The Time associated with timestamp 0. Set to a null
  // time if the file doesn't have an association to Time.
  base::Time timeline_offset_;

  // Liveness of the stream.
  Liveness liveness_;

  // Whether text streams have been enabled for this demuxer.
  bool text_enabled_;

  // Set if we know duration of the audio stream. Used when processing end of
  // stream -- at this moment we definitely know duration.
  bool duration_known_;

  // FFmpegURLProtocol implementation and corresponding glue bits.
  scoped_ptr<BlockingUrlProtocol> url_protocol_;
  scoped_ptr<FFmpegGlue> glue_;

  const NeedKeyCB need_key_cb_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<FFmpegDemuxer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_DEMUXER_H_

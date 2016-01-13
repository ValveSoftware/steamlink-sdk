// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_demuxer.h"

#include <algorithm>
#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_byteorder.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/video_decoder_config.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/ffmpeg_h264_to_annex_b_bitstream_converter.h"
#include "media/filters/webvtt_util.h"
#include "media/formats/webm/webm_crypto_helpers.h"

namespace media {

static base::Time ExtractTimelineOffset(AVFormatContext* format_context) {
  if (strstr(format_context->iformat->name, "webm") ||
      strstr(format_context->iformat->name, "matroska")) {
    const AVDictionaryEntry* entry =
        av_dict_get(format_context->metadata, "creation_time", NULL, 0);

    base::Time timeline_offset;
    if (entry != NULL && entry->value != NULL &&
        FFmpegUTCDateToTime(entry->value, &timeline_offset)) {
      return timeline_offset;
    }
  }

  return base::Time();
}

static base::TimeDelta FramesToTimeDelta(int frames, double sample_rate) {
  return base::TimeDelta::FromMicroseconds(
      frames * base::Time::kMicrosecondsPerSecond / sample_rate);
}

//
// FFmpegDemuxerStream
//
FFmpegDemuxerStream::FFmpegDemuxerStream(
    FFmpegDemuxer* demuxer,
    AVStream* stream)
    : demuxer_(demuxer),
      task_runner_(base::MessageLoopProxy::current()),
      stream_(stream),
      type_(UNKNOWN),
      end_of_stream_(false),
      last_packet_timestamp_(kNoTimestamp()),
      bitstream_converter_enabled_(false) {
  DCHECK(demuxer_);

  bool is_encrypted = false;

  // Determine our media format.
  switch (stream->codec->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
      type_ = AUDIO;
      AVStreamToAudioDecoderConfig(stream, &audio_config_, true);
      is_encrypted = audio_config_.is_encrypted();
      break;
    case AVMEDIA_TYPE_VIDEO:
      type_ = VIDEO;
      AVStreamToVideoDecoderConfig(stream, &video_config_, true);
      is_encrypted = video_config_.is_encrypted();
      break;
    case AVMEDIA_TYPE_SUBTITLE:
      type_ = TEXT;
      break;
    default:
      NOTREACHED();
      break;
  }

  // Calculate the duration.
  duration_ = ConvertStreamTimestamp(stream->time_base, stream->duration);

#if defined(USE_PROPRIETARY_CODECS)
  if (stream_->codec->codec_id == AV_CODEC_ID_H264) {
    bitstream_converter_.reset(
        new FFmpegH264ToAnnexBBitstreamConverter(stream_->codec));
  }
#endif

  if (is_encrypted) {
    AVDictionaryEntry* key = av_dict_get(stream->metadata, "enc_key_id", NULL,
                                         0);
    DCHECK(key);
    DCHECK(key->value);
    if (!key || !key->value)
      return;
    base::StringPiece base64_key_id(key->value);
    std::string enc_key_id;
    base::Base64Decode(base64_key_id, &enc_key_id);
    DCHECK(!enc_key_id.empty());
    if (enc_key_id.empty())
      return;

    encryption_key_id_.assign(enc_key_id);
    demuxer_->FireNeedKey(kWebMEncryptInitDataType, enc_key_id);
  }
}

void FFmpegDemuxerStream::EnqueuePacket(ScopedAVPacket packet) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!demuxer_ || end_of_stream_) {
    NOTREACHED() << "Attempted to enqueue packet on a stopped stream";
    return;
  }

#if defined(USE_PROPRIETARY_CODECS)
  // Convert the packet if there is a bitstream filter.
  if (packet->data && bitstream_converter_enabled_ &&
      !bitstream_converter_->ConvertPacket(packet.get())) {
    LOG(ERROR) << "Format conversion failed.";
  }
#endif

  // Get side data if any. For now, the only type of side_data is VP8 Alpha. We
  // keep this generic so that other side_data types in the future can be
  // handled the same way as well.
  av_packet_split_side_data(packet.get());

  scoped_refptr<DecoderBuffer> buffer;

  if (type() == DemuxerStream::TEXT) {
    int id_size = 0;
    uint8* id_data = av_packet_get_side_data(
        packet.get(),
        AV_PKT_DATA_WEBVTT_IDENTIFIER,
        &id_size);

    int settings_size = 0;
    uint8* settings_data = av_packet_get_side_data(
        packet.get(),
        AV_PKT_DATA_WEBVTT_SETTINGS,
        &settings_size);

    std::vector<uint8> side_data;
    MakeSideData(id_data, id_data + id_size,
                 settings_data, settings_data + settings_size,
                 &side_data);

    buffer = DecoderBuffer::CopyFrom(packet.get()->data, packet.get()->size,
                                     side_data.data(), side_data.size());
  } else {
    int side_data_size = 0;
    uint8* side_data = av_packet_get_side_data(
        packet.get(),
        AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL,
        &side_data_size);

    scoped_ptr<DecryptConfig> decrypt_config;
    int data_offset = 0;
    if ((type() == DemuxerStream::AUDIO && audio_config_.is_encrypted()) ||
        (type() == DemuxerStream::VIDEO && video_config_.is_encrypted())) {
      if (!WebMCreateDecryptConfig(
          packet->data, packet->size,
          reinterpret_cast<const uint8*>(encryption_key_id_.data()),
          encryption_key_id_.size(),
          &decrypt_config,
          &data_offset)) {
        LOG(ERROR) << "Creation of DecryptConfig failed.";
      }
    }

    // If a packet is returned by FFmpeg's av_parser_parse2() the packet will
    // reference inner memory of FFmpeg.  As such we should transfer the packet
    // into memory we control.
    if (side_data_size > 0) {
      buffer = DecoderBuffer::CopyFrom(packet.get()->data + data_offset,
                                       packet.get()->size - data_offset,
                                       side_data, side_data_size);
    } else {
      buffer = DecoderBuffer::CopyFrom(packet.get()->data + data_offset,
                                       packet.get()->size - data_offset);
    }

    int skip_samples_size = 0;
    const uint32* skip_samples_ptr =
        reinterpret_cast<const uint32*>(av_packet_get_side_data(
            packet.get(), AV_PKT_DATA_SKIP_SAMPLES, &skip_samples_size));
    const int kSkipSamplesValidSize = 10;
    const int kSkipEndSamplesOffset = 1;
    if (skip_samples_size >= kSkipSamplesValidSize) {
      // Because FFmpeg rolls codec delay and skip samples into one we can only
      // allow front discard padding on the first buffer.  Otherwise the discard
      // helper can't figure out which data to discard.  See AudioDiscardHelper.
      int discard_front_samples = base::ByteSwapToLE32(*skip_samples_ptr);
      if (last_packet_timestamp_ != kNoTimestamp()) {
        DLOG(ERROR) << "Skip samples are only allowed for the first packet.";
        discard_front_samples = 0;
      }

      const int discard_end_samples =
          base::ByteSwapToLE32(*(skip_samples_ptr + kSkipEndSamplesOffset));
      const int samples_per_second =
          audio_decoder_config().samples_per_second();
      buffer->set_discard_padding(std::make_pair(
          FramesToTimeDelta(discard_front_samples, samples_per_second),
          FramesToTimeDelta(discard_end_samples, samples_per_second)));
    }

    if (decrypt_config)
      buffer->set_decrypt_config(decrypt_config.Pass());
  }

  buffer->set_timestamp(ConvertStreamTimestamp(
      stream_->time_base, packet->pts));
  buffer->set_duration(ConvertStreamTimestamp(
      stream_->time_base, packet->duration));
  if (buffer->timestamp() != kNoTimestamp() &&
      last_packet_timestamp_ != kNoTimestamp() &&
      last_packet_timestamp_ < buffer->timestamp()) {
    buffered_ranges_.Add(last_packet_timestamp_, buffer->timestamp());
    demuxer_->NotifyBufferingChanged();
  }
  last_packet_timestamp_ = buffer->timestamp();

  buffer_queue_.Push(buffer);
  SatisfyPendingRead();
}

void FFmpegDemuxerStream::SetEndOfStream() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  end_of_stream_ = true;
  SatisfyPendingRead();
}

void FFmpegDemuxerStream::FlushBuffers() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(read_cb_.is_null()) << "There should be no pending read";
  buffer_queue_.Clear();
  end_of_stream_ = false;
  last_packet_timestamp_ = kNoTimestamp();
}

void FFmpegDemuxerStream::Stop() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  buffer_queue_.Clear();
  if (!read_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(
        DemuxerStream::kOk, DecoderBuffer::CreateEOSBuffer());
  }
  demuxer_ = NULL;
  stream_ = NULL;
  end_of_stream_ = true;
}

base::TimeDelta FFmpegDemuxerStream::duration() {
  return duration_;
}

DemuxerStream::Type FFmpegDemuxerStream::type() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return type_;
}

void FFmpegDemuxerStream::Read(const ReadCB& read_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  CHECK(read_cb_.is_null()) << "Overlapping reads are not supported";
  read_cb_ = BindToCurrentLoop(read_cb);

  // Don't accept any additional reads if we've been told to stop.
  // The |demuxer_| may have been destroyed in the pipeline thread.
  //
  // TODO(scherkus): it would be cleaner to reply with an error message.
  if (!demuxer_) {
    base::ResetAndReturn(&read_cb_).Run(
        DemuxerStream::kOk, DecoderBuffer::CreateEOSBuffer());
    return;
  }

  SatisfyPendingRead();
}

void FFmpegDemuxerStream::EnableBitstreamConverter() {
  DCHECK(task_runner_->BelongsToCurrentThread());

#if defined(USE_PROPRIETARY_CODECS)
  CHECK(bitstream_converter_.get());
  bitstream_converter_enabled_ = true;
#else
  NOTREACHED() << "Proprietary codecs not enabled.";
#endif
}

bool FFmpegDemuxerStream::SupportsConfigChanges() { return false; }

AudioDecoderConfig FFmpegDemuxerStream::audio_decoder_config() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  CHECK_EQ(type_, AUDIO);
  return audio_config_;
}

VideoDecoderConfig FFmpegDemuxerStream::video_decoder_config() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  CHECK_EQ(type_, VIDEO);
  return video_config_;
}

FFmpegDemuxerStream::~FFmpegDemuxerStream() {
  DCHECK(!demuxer_);
  DCHECK(read_cb_.is_null());
  DCHECK(buffer_queue_.IsEmpty());
}

base::TimeDelta FFmpegDemuxerStream::GetElapsedTime() const {
  return ConvertStreamTimestamp(stream_->time_base, stream_->cur_dts);
}

Ranges<base::TimeDelta> FFmpegDemuxerStream::GetBufferedRanges() const {
  return buffered_ranges_;
}

void FFmpegDemuxerStream::SatisfyPendingRead() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!read_cb_.is_null()) {
    if (!buffer_queue_.IsEmpty()) {
      base::ResetAndReturn(&read_cb_).Run(
          DemuxerStream::kOk, buffer_queue_.Pop());
    } else if (end_of_stream_) {
      base::ResetAndReturn(&read_cb_).Run(
          DemuxerStream::kOk, DecoderBuffer::CreateEOSBuffer());
    }
  }

  // Have capacity? Ask for more!
  if (HasAvailableCapacity() && !end_of_stream_) {
    demuxer_->NotifyCapacityAvailable();
  }
}

bool FFmpegDemuxerStream::HasAvailableCapacity() {
  // TODO(scherkus): Remove this return and reenable time-based capacity
  // after our data sources support canceling/concurrent reads, see
  // http://crbug.com/165762 for details.
#if 1
  return !read_cb_.is_null();
#else
  // Try to have one second's worth of encoded data per stream.
  const base::TimeDelta kCapacity = base::TimeDelta::FromSeconds(1);
  return buffer_queue_.IsEmpty() || buffer_queue_.Duration() < kCapacity;
#endif
}

size_t FFmpegDemuxerStream::MemoryUsage() const {
  return buffer_queue_.data_size();
}

TextKind FFmpegDemuxerStream::GetTextKind() const {
  DCHECK_EQ(type_, DemuxerStream::TEXT);

  if (stream_->disposition & AV_DISPOSITION_CAPTIONS)
    return kTextCaptions;

  if (stream_->disposition & AV_DISPOSITION_DESCRIPTIONS)
    return kTextDescriptions;

  if (stream_->disposition & AV_DISPOSITION_METADATA)
    return kTextMetadata;

  return kTextSubtitles;
}

std::string FFmpegDemuxerStream::GetMetadata(const char* key) const {
  const AVDictionaryEntry* entry =
      av_dict_get(stream_->metadata, key, NULL, 0);
  return (entry == NULL || entry->value == NULL) ? "" : entry->value;
}

// static
base::TimeDelta FFmpegDemuxerStream::ConvertStreamTimestamp(
    const AVRational& time_base, int64 timestamp) {
  if (timestamp == static_cast<int64>(AV_NOPTS_VALUE))
    return kNoTimestamp();

  return ConvertFromTimeBase(time_base, timestamp);
}

//
// FFmpegDemuxer
//
FFmpegDemuxer::FFmpegDemuxer(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    DataSource* data_source,
    const NeedKeyCB& need_key_cb,
    const scoped_refptr<MediaLog>& media_log)
    : host_(NULL),
      task_runner_(task_runner),
      blocking_thread_("FFmpegDemuxer"),
      pending_read_(false),
      pending_seek_(false),
      data_source_(data_source),
      media_log_(media_log),
      bitrate_(0),
      start_time_(kNoTimestamp()),
      liveness_(LIVENESS_UNKNOWN),
      text_enabled_(false),
      duration_known_(false),
      need_key_cb_(need_key_cb),
      weak_factory_(this) {
  DCHECK(task_runner_.get());
  DCHECK(data_source_);
}

FFmpegDemuxer::~FFmpegDemuxer() {}

void FFmpegDemuxer::Stop(const base::Closure& callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  url_protocol_->Abort();
  data_source_->Stop(
      BindToCurrentLoop(base::Bind(&FFmpegDemuxer::OnDataSourceStopped,
                                   weak_factory_.GetWeakPtr(),
                                   BindToCurrentLoop(callback))));
  data_source_ = NULL;
}

void FFmpegDemuxer::Seek(base::TimeDelta time, const PipelineStatusCB& cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  CHECK(!pending_seek_);

  // TODO(scherkus): Inspect |pending_read_| and cancel IO via |blocking_url_|,
  // otherwise we can end up waiting for a pre-seek read to complete even though
  // we know we're going to drop it on the floor.

  // Always seek to a timestamp less than or equal to the desired timestamp.
  int flags = AVSEEK_FLAG_BACKWARD;

  // Passing -1 as our stream index lets FFmpeg pick a default stream.  FFmpeg
  // will attempt to use the lowest-index video stream, if present, followed by
  // the lowest-index audio stream.
  pending_seek_ = true;
  base::PostTaskAndReplyWithResult(
      blocking_thread_.message_loop_proxy().get(),
      FROM_HERE,
      base::Bind(&av_seek_frame,
                 glue_->format_context(),
                 -1,
                 time.InMicroseconds(),
                 flags),
      base::Bind(
          &FFmpegDemuxer::OnSeekFrameDone, weak_factory_.GetWeakPtr(), cb));
}

void FFmpegDemuxer::Initialize(DemuxerHost* host,
                               const PipelineStatusCB& status_cb,
                               bool enable_text_tracks) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  host_ = host;
  text_enabled_ = enable_text_tracks;

  url_protocol_.reset(new BlockingUrlProtocol(data_source_, BindToCurrentLoop(
      base::Bind(&FFmpegDemuxer::OnDataSourceError, base::Unretained(this)))));
  glue_.reset(new FFmpegGlue(url_protocol_.get()));
  AVFormatContext* format_context = glue_->format_context();

  // Disable ID3v1 tag reading to avoid costly seeks to end of file for data we
  // don't use.  FFmpeg will only read ID3v1 tags if no other metadata is
  // available, so add a metadata entry to ensure some is always present.
  av_dict_set(&format_context->metadata, "skip_id3v1_tags", "", 0);

  // Open the AVFormatContext using our glue layer.
  CHECK(blocking_thread_.Start());
  base::PostTaskAndReplyWithResult(
      blocking_thread_.message_loop_proxy().get(),
      FROM_HERE,
      base::Bind(&FFmpegGlue::OpenContext, base::Unretained(glue_.get())),
      base::Bind(&FFmpegDemuxer::OnOpenContextDone,
                 weak_factory_.GetWeakPtr(),
                 status_cb));
}

DemuxerStream* FFmpegDemuxer::GetStream(DemuxerStream::Type type) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return GetFFmpegStream(type);
}

FFmpegDemuxerStream* FFmpegDemuxer::GetFFmpegStream(
    DemuxerStream::Type type) const {
  StreamVector::const_iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (*iter && (*iter)->type() == type) {
      return *iter;
    }
  }
  return NULL;
}

base::TimeDelta FFmpegDemuxer::GetStartTime() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return start_time_;
}

base::Time FFmpegDemuxer::GetTimelineOffset() const {
  return timeline_offset_;
}

Demuxer::Liveness FFmpegDemuxer::GetLiveness() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return liveness_;
}

void FFmpegDemuxer::AddTextStreams() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  for (StreamVector::size_type idx = 0; idx < streams_.size(); ++idx) {
    FFmpegDemuxerStream* stream = streams_[idx];
    if (stream == NULL || stream->type() != DemuxerStream::TEXT)
      continue;

    TextKind kind = stream->GetTextKind();
    std::string title = stream->GetMetadata("title");
    std::string language = stream->GetMetadata("language");

    // TODO: Implement "id" metadata in FFMPEG.
    // See: http://crbug.com/323183
    host_->AddTextStream(stream, TextTrackConfig(kind, title, language,
        std::string()));
  }
}

// Helper for calculating the bitrate of the media based on information stored
// in |format_context| or failing that the size and duration of the media.
//
// Returns 0 if a bitrate could not be determined.
static int CalculateBitrate(
    AVFormatContext* format_context,
    const base::TimeDelta& duration,
    int64 filesize_in_bytes) {
  // If there is a bitrate set on the container, use it.
  if (format_context->bit_rate > 0)
    return format_context->bit_rate;

  // Then try to sum the bitrates individually per stream.
  int bitrate = 0;
  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    AVCodecContext* codec_context = format_context->streams[i]->codec;
    bitrate += codec_context->bit_rate;
  }
  if (bitrate > 0)
    return bitrate;

  // See if we can approximate the bitrate as long as we have a filesize and
  // valid duration.
  if (duration.InMicroseconds() <= 0 ||
      duration == kInfiniteDuration() ||
      filesize_in_bytes == 0) {
    return 0;
  }

  // Do math in floating point as we'd overflow an int64 if the filesize was
  // larger than ~1073GB.
  double bytes = filesize_in_bytes;
  double duration_us = duration.InMicroseconds();
  return bytes * 8000000.0 / duration_us;
}

void FFmpegDemuxer::OnOpenContextDone(const PipelineStatusCB& status_cb,
                                      bool result) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!blocking_thread_.IsRunning()) {
    status_cb.Run(PIPELINE_ERROR_ABORT);
    return;
  }

  if (!result) {
    status_cb.Run(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  // Fully initialize AVFormatContext by parsing the stream a little.
  base::PostTaskAndReplyWithResult(
      blocking_thread_.message_loop_proxy().get(),
      FROM_HERE,
      base::Bind(&avformat_find_stream_info,
                 glue_->format_context(),
                 static_cast<AVDictionary**>(NULL)),
      base::Bind(&FFmpegDemuxer::OnFindStreamInfoDone,
                 weak_factory_.GetWeakPtr(),
                 status_cb));
}

void FFmpegDemuxer::OnFindStreamInfoDone(const PipelineStatusCB& status_cb,
                                         int result) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!blocking_thread_.IsRunning() || !data_source_) {
    status_cb.Run(PIPELINE_ERROR_ABORT);
    return;
  }

  if (result < 0) {
    status_cb.Run(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  // Create demuxer stream entries for each possible AVStream. Each stream
  // is examined to determine if it is supported or not (is the codec enabled
  // for it in this release?). Unsupported streams are skipped, allowing for
  // partial playback. At least one audio or video stream must be playable.
  AVFormatContext* format_context = glue_->format_context();
  streams_.resize(format_context->nb_streams);

  AVStream* audio_stream = NULL;
  AudioDecoderConfig audio_config;

  AVStream* video_stream = NULL;
  VideoDecoderConfig video_config;

  base::TimeDelta max_duration;
  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    AVStream* stream = format_context->streams[i];
    AVCodecContext* codec_context = stream->codec;
    AVMediaType codec_type = codec_context->codec_type;

    if (codec_type == AVMEDIA_TYPE_AUDIO) {
      if (audio_stream)
        continue;

      // Log the codec detected, whether it is supported or not.
      UMA_HISTOGRAM_SPARSE_SLOWLY("Media.DetectedAudioCodec",
                                  codec_context->codec_id);
      // Ensure the codec is supported. IsValidConfig() also checks that the
      // channel layout and sample format are valid.
      AVStreamToAudioDecoderConfig(stream, &audio_config, false);
      if (!audio_config.IsValidConfig())
        continue;
      audio_stream = stream;
    } else if (codec_type == AVMEDIA_TYPE_VIDEO) {
      if (video_stream)
        continue;

      // Log the codec detected, whether it is supported or not.
      UMA_HISTOGRAM_SPARSE_SLOWLY("Media.DetectedVideoCodec",
                                  codec_context->codec_id);
      // Ensure the codec is supported. IsValidConfig() also checks that the
      // frame size and visible size are valid.
      AVStreamToVideoDecoderConfig(stream, &video_config, false);

      if (!video_config.IsValidConfig())
        continue;
      video_stream = stream;
    } else if (codec_type == AVMEDIA_TYPE_SUBTITLE) {
      if (codec_context->codec_id != AV_CODEC_ID_WEBVTT || !text_enabled_) {
        continue;
      }
    } else {
      continue;
    }

    streams_[i] = new FFmpegDemuxerStream(this, stream);
    max_duration = std::max(max_duration, streams_[i]->duration());

    if (stream->first_dts != static_cast<int64_t>(AV_NOPTS_VALUE)) {
      const base::TimeDelta first_dts = ConvertFromTimeBase(
          stream->time_base, stream->first_dts);
      if (start_time_ == kNoTimestamp() || first_dts < start_time_)
        start_time_ = first_dts;
    }
  }

  if (!audio_stream && !video_stream) {
    status_cb.Run(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
    return;
  }

  if (text_enabled_)
    AddTextStreams();

  if (format_context->duration != static_cast<int64_t>(AV_NOPTS_VALUE)) {
    // If there is a duration value in the container use that to find the
    // maximum between it and the duration from A/V streams.
    const AVRational av_time_base = {1, AV_TIME_BASE};
    max_duration =
        std::max(max_duration,
                 ConvertFromTimeBase(av_time_base, format_context->duration));
  } else {
    // The duration is unknown, in which case this is likely a live stream.
    max_duration = kInfiniteDuration();
  }

  // Some demuxers, like WAV, do not put timestamps on their frames. We
  // assume the the start time is 0.
  if (start_time_ == kNoTimestamp())
    start_time_ = base::TimeDelta();

  // MPEG-4 B-frames cause grief for a simple container like AVI. Enable PTS
  // generation so we always get timestamps, see http://crbug.com/169570
  if (strcmp(format_context->iformat->name, "avi") == 0)
    format_context->flags |= AVFMT_FLAG_GENPTS;

  timeline_offset_ = ExtractTimelineOffset(format_context);

  if (max_duration == kInfiniteDuration() && !timeline_offset_.is_null()) {
    liveness_ = LIVENESS_LIVE;
  } else if (max_duration != kInfiniteDuration()) {
    liveness_ = LIVENESS_RECORDED;
  } else {
    liveness_ = LIVENESS_UNKNOWN;
  }

  // Good to go: set the duration and bitrate and notify we're done
  // initializing.
  host_->SetDuration(max_duration);
  duration_known_ = (max_duration != kInfiniteDuration());

  int64 filesize_in_bytes = 0;
  url_protocol_->GetSize(&filesize_in_bytes);
  bitrate_ = CalculateBitrate(format_context, max_duration, filesize_in_bytes);
  if (bitrate_ > 0)
    data_source_->SetBitrate(bitrate_);

  // Audio logging
  if (audio_stream) {
    AVCodecContext* audio_codec = audio_stream->codec;
    media_log_->SetBooleanProperty("found_audio_stream", true);

    SampleFormat sample_format = audio_config.sample_format();
    std::string sample_name = SampleFormatToString(sample_format);

    media_log_->SetStringProperty("audio_sample_format", sample_name);

    AVCodec* codec = avcodec_find_decoder(audio_codec->codec_id);
    if (codec) {
      media_log_->SetStringProperty("audio_codec_name", codec->name);
    }

    media_log_->SetIntegerProperty("audio_channels_count",
                                   audio_codec->channels);
    media_log_->SetIntegerProperty("audio_samples_per_second",
                                   audio_config.samples_per_second());
  } else {
    media_log_->SetBooleanProperty("found_audio_stream", false);
  }

  // Video logging
  if (video_stream) {
    AVCodecContext* video_codec = video_stream->codec;
    media_log_->SetBooleanProperty("found_video_stream", true);

    AVCodec* codec = avcodec_find_decoder(video_codec->codec_id);
    if (codec) {
      media_log_->SetStringProperty("video_codec_name", codec->name);
    }

    media_log_->SetIntegerProperty("width", video_codec->width);
    media_log_->SetIntegerProperty("height", video_codec->height);
    media_log_->SetIntegerProperty("coded_width",
                                   video_codec->coded_width);
    media_log_->SetIntegerProperty("coded_height",
                                   video_codec->coded_height);
    media_log_->SetStringProperty(
        "time_base",
        base::StringPrintf("%d/%d",
                           video_codec->time_base.num,
                           video_codec->time_base.den));
    media_log_->SetStringProperty(
        "video_format", VideoFrame::FormatToString(video_config.format()));
    media_log_->SetBooleanProperty("video_is_encrypted",
                                   video_config.is_encrypted());
  } else {
    media_log_->SetBooleanProperty("found_video_stream", false);
  }


  media_log_->SetTimeProperty("max_duration", max_duration);
  media_log_->SetTimeProperty("start_time", start_time_);
  media_log_->SetIntegerProperty("bitrate", bitrate_);

  status_cb.Run(PIPELINE_OK);
}

void FFmpegDemuxer::OnSeekFrameDone(const PipelineStatusCB& cb, int result) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  CHECK(pending_seek_);
  pending_seek_ = false;

  if (!blocking_thread_.IsRunning()) {
    cb.Run(PIPELINE_ERROR_ABORT);
    return;
  }

  if (result < 0) {
    // Use VLOG(1) instead of NOTIMPLEMENTED() to prevent the message being
    // captured from stdout and contaminates testing.
    // TODO(scherkus): Implement this properly and signal error (BUG=23447).
    VLOG(1) << "Not implemented";
  }

  // Tell streams to flush buffers due to seeking.
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (*iter)
      (*iter)->FlushBuffers();
  }

  // Resume reading until capacity.
  ReadFrameIfNeeded();

  // Notify we're finished seeking.
  cb.Run(PIPELINE_OK);
}

void FFmpegDemuxer::ReadFrameIfNeeded() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Make sure we have work to do before reading.
  if (!blocking_thread_.IsRunning() || !StreamsHaveAvailableCapacity() ||
      pending_read_ || pending_seek_) {
    return;
  }

  // Allocate and read an AVPacket from the media. Save |packet_ptr| since
  // evaluation order of packet.get() and base::Passed(&packet) is
  // undefined.
  ScopedAVPacket packet(new AVPacket());
  AVPacket* packet_ptr = packet.get();

  pending_read_ = true;
  base::PostTaskAndReplyWithResult(
      blocking_thread_.message_loop_proxy().get(),
      FROM_HERE,
      base::Bind(&av_read_frame, glue_->format_context(), packet_ptr),
      base::Bind(&FFmpegDemuxer::OnReadFrameDone,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(&packet)));
}

void FFmpegDemuxer::OnReadFrameDone(ScopedAVPacket packet, int result) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(pending_read_);
  pending_read_ = false;

  if (!blocking_thread_.IsRunning() || pending_seek_) {
    return;
  }

  // Consider the stream as ended if:
  // - either underlying ffmpeg returned an error
  // - or FFMpegDemuxer reached the maximum allowed memory usage.
  if (result < 0 || IsMaxMemoryUsageReached()) {
    // Update the duration based on the highest elapsed time across all streams
    // if it was previously unknown.
    if (!duration_known_) {
      base::TimeDelta max_duration;

      for (StreamVector::iterator iter = streams_.begin();
           iter != streams_.end();
           ++iter) {
        if (!*iter)
          continue;

        base::TimeDelta duration = (*iter)->GetElapsedTime();
        if (duration != kNoTimestamp() && duration > max_duration)
          max_duration = duration;
      }

      if (max_duration > base::TimeDelta()) {
        host_->SetDuration(max_duration);
        duration_known_ = true;
      }
    }
    // If we have reached the end of stream, tell the downstream filters about
    // the event.
    StreamHasEnded();
    return;
  }

  // Queue the packet with the appropriate stream.
  DCHECK_GE(packet->stream_index, 0);
  DCHECK_LT(packet->stream_index, static_cast<int>(streams_.size()));

  // Defend against ffmpeg giving us a bad stream index.
  if (packet->stream_index >= 0 &&
      packet->stream_index < static_cast<int>(streams_.size()) &&
      streams_[packet->stream_index]) {
    // TODO(scherkus): Fix demuxing upstream to never return packets w/o data
    // when av_read_frame() returns success code. See bug comment for ideas:
    //
    // https://code.google.com/p/chromium/issues/detail?id=169133#c10
    if (!packet->data) {
      ScopedAVPacket new_packet(new AVPacket());
      av_new_packet(new_packet.get(), 0);
      av_packet_copy_props(new_packet.get(), packet.get());
      packet.swap(new_packet);
    }

    // Special case for opus in ogg.  FFmpeg is pre-trimming the codec delay
    // from the packet timestamp.  Chrome expects to handle this itself inside
    // the decoder, so shift timestamps by the delay in this case.
    // TODO(dalecurtis): Try to get fixed upstream.  See http://crbug.com/328207
    if (strcmp(glue_->format_context()->iformat->name, "ogg") == 0) {
      const AVCodecContext* codec_context =
          glue_->format_context()->streams[packet->stream_index]->codec;
      if (codec_context->codec_id == AV_CODEC_ID_OPUS &&
          codec_context->delay > 0) {
        packet->pts += codec_context->delay;
      }
    }

    FFmpegDemuxerStream* demuxer_stream = streams_[packet->stream_index];
    demuxer_stream->EnqueuePacket(packet.Pass());
  }

  // Keep reading until we've reached capacity.
  ReadFrameIfNeeded();
}

void FFmpegDemuxer::OnDataSourceStopped(const base::Closure& callback) {
  // This will block until all tasks complete. Note that after this returns it's
  // possible for reply tasks (e.g., OnReadFrameDone()) to be queued on this
  // thread. Each of the reply task methods must check whether we've stopped the
  // thread and drop their results on the floor.
  DCHECK(task_runner_->BelongsToCurrentThread());
  blocking_thread_.Stop();

  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (*iter)
      (*iter)->Stop();
  }

  callback.Run();
}

bool FFmpegDemuxer::StreamsHaveAvailableCapacity() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (*iter && (*iter)->HasAvailableCapacity()) {
      return true;
    }
  }
  return false;
}

bool FFmpegDemuxer::IsMaxMemoryUsageReached() const {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Max allowed memory usage, all streams combined.
  const size_t kDemuxerMemoryLimit = 150 * 1024 * 1024;

  size_t memory_left = kDemuxerMemoryLimit;
  for (StreamVector::const_iterator iter = streams_.begin();
       iter != streams_.end(); ++iter) {
    if (!(*iter))
      continue;

    size_t stream_memory_usage = (*iter)->MemoryUsage();
    if (stream_memory_usage > memory_left)
      return true;
    memory_left -= stream_memory_usage;
  }
  return false;
}

void FFmpegDemuxer::StreamHasEnded() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (!*iter)
      continue;
    (*iter)->SetEndOfStream();
  }
}

void FFmpegDemuxer::FireNeedKey(const std::string& init_data_type,
                                const std::string& encryption_key_id) {
  std::vector<uint8> key_id_local(encryption_key_id.begin(),
                                  encryption_key_id.end());
  need_key_cb_.Run(init_data_type, key_id_local);
}

void FFmpegDemuxer::NotifyCapacityAvailable() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  ReadFrameIfNeeded();
}

void FFmpegDemuxer::NotifyBufferingChanged() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  Ranges<base::TimeDelta> buffered;
  FFmpegDemuxerStream* audio = GetFFmpegStream(DemuxerStream::AUDIO);
  FFmpegDemuxerStream* video = GetFFmpegStream(DemuxerStream::VIDEO);
  if (audio && video) {
    buffered = audio->GetBufferedRanges().IntersectionWith(
        video->GetBufferedRanges());
  } else if (audio) {
    buffered = audio->GetBufferedRanges();
  } else if (video) {
    buffered = video->GetBufferedRanges();
  }
  for (size_t i = 0; i < buffered.size(); ++i)
    host_->AddBufferedTimeRange(buffered.start(i), buffered.end(i));
}

void FFmpegDemuxer::OnDataSourceError() {
  host_->OnDemuxerError(PIPELINE_ERROR_READ);
}

}  // namespace media

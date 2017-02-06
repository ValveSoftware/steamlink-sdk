// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/media_source_delegate.h"

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/renderer/media/android/renderer_demuxer_android.h"
#include "media/base/android/demuxer_stream_player_params.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/base/timestamp_constants.h"
#include "media/blink/webmediaplayer_util.h"
#include "media/blink/webmediasource_impl.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"

using media::DemuxerStream;
using media::DemuxerConfigs;
using media::DemuxerData;
using blink::WebMediaPlayer;
using blink::WebString;

namespace {

// The size of the access unit to transfer in an IPC in case of MediaSource.
// 4: approximately 64ms of content in 60 fps movies.
const size_t kAccessUnitSizeForMediaSource = 4;

const uint8_t kVorbisPadding[] = {0xff, 0xff, 0xff, 0xff};

}  // namespace

namespace content {

MediaSourceDelegate::MediaSourceDelegate(
    RendererDemuxerAndroid* demuxer_client,
    int demuxer_client_id,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<media::MediaLog> media_log)
    : demuxer_client_(demuxer_client),
      demuxer_client_id_(demuxer_client_id),
      media_log_(media_log),
      is_demuxer_ready_(false),
      audio_stream_(NULL),
      video_stream_(NULL),
      seeking_(false),
      is_video_encrypted_(false),
      doing_browser_seek_(false),
      browser_seek_time_(media::kNoTimestamp()),
      expecting_regular_seek_(false),
      access_unit_size_(0),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      media_task_runner_(media_task_runner),
      main_weak_factory_(this),
      media_weak_factory_(this) {
  main_weak_this_ = main_weak_factory_.GetWeakPtr();
  DCHECK(main_task_runner_->BelongsToCurrentThread());
}

MediaSourceDelegate::~MediaSourceDelegate() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;
  DCHECK(!chunk_demuxer_);
  DCHECK(!demuxer_client_);
  DCHECK(!audio_decrypting_demuxer_stream_);
  DCHECK(!video_decrypting_demuxer_stream_);
  DCHECK(!audio_stream_);
  DCHECK(!video_stream_);
}

void MediaSourceDelegate::Stop(const base::Closure& stop_cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;

  if (!chunk_demuxer_) {
    DCHECK(!demuxer_client_);
    return;
  }

  duration_change_cb_.Reset();
  update_network_state_cb_.Reset();
  media_source_opened_cb_.Reset();

  main_weak_factory_.InvalidateWeakPtrs();
  DCHECK(!main_weak_factory_.HasWeakPtrs());

  chunk_demuxer_->Shutdown();

  // Continue to stop objects on the media thread.
  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &MediaSourceDelegate::StopDemuxer, base::Unretained(this), stop_cb));
}

bool MediaSourceDelegate::IsVideoEncrypted() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(is_video_encrypted_lock_);
  return is_video_encrypted_;
}

base::Time MediaSourceDelegate::GetTimelineOffset() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (!chunk_demuxer_)
    return base::Time();

  return chunk_demuxer_->GetTimelineOffset();
}

void MediaSourceDelegate::StopDemuxer(const base::Closure& stop_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(chunk_demuxer_);

  demuxer_client_->RemoveDelegate(demuxer_client_id_);
  demuxer_client_ = NULL;

  audio_stream_ = NULL;
  video_stream_ = NULL;
  // TODO(xhwang): Figure out if we need to Reset the DDSs after Seeking or
  // before destroying them.
  audio_decrypting_demuxer_stream_.reset();
  video_decrypting_demuxer_stream_.reset();

  media_weak_factory_.InvalidateWeakPtrs();
  DCHECK(!media_weak_factory_.HasWeakPtrs());

  chunk_demuxer_->Stop();
  chunk_demuxer_.reset();

  // |this| may be destroyed at this point in time as a result of running
  // |stop_cb|.
  stop_cb.Run();
}

void MediaSourceDelegate::InitializeMediaSource(
    const MediaSourceOpenedCB& media_source_opened_cb,
    const media::Demuxer::EncryptedMediaInitDataCB&
        encrypted_media_init_data_cb,
    const SetCdmReadyCB& set_cdm_ready_cb,
    const UpdateNetworkStateCB& update_network_state_cb,
    const DurationChangeCB& duration_change_cb,
    const base::Closure& waiting_for_decryption_key_cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!media_source_opened_cb.is_null());
  DCHECK(!encrypted_media_init_data_cb.is_null());
  DCHECK(!set_cdm_ready_cb.is_null());
  DCHECK(!update_network_state_cb.is_null());
  DCHECK(!duration_change_cb.is_null());
  DCHECK(!waiting_for_decryption_key_cb.is_null());

  media_source_opened_cb_ = media_source_opened_cb;
  encrypted_media_init_data_cb_ = encrypted_media_init_data_cb;
  set_cdm_ready_cb_ = media::BindToCurrentLoop(set_cdm_ready_cb);
  update_network_state_cb_ = media::BindToCurrentLoop(update_network_state_cb);
  duration_change_cb_ = duration_change_cb;
  waiting_for_decryption_key_cb_ =
      media::BindToCurrentLoop(waiting_for_decryption_key_cb);
  access_unit_size_ = kAccessUnitSizeForMediaSource;

  chunk_demuxer_.reset(new media::ChunkDemuxer(
      media::BindToCurrentLoop(
          base::Bind(&MediaSourceDelegate::OnDemuxerOpened, main_weak_this_)),
      media::BindToCurrentLoop(base::Bind(
          &MediaSourceDelegate::OnEncryptedMediaInitData, main_weak_this_)),
      media_log_, false));

  // |this| will be retained until StopDemuxer() is posted, so Unretained() is
  // safe here.
  media_task_runner_->PostTask(FROM_HERE,
                        base::Bind(&MediaSourceDelegate::InitializeDemuxer,
                        base::Unretained(this)));
}

void MediaSourceDelegate::InitializeDemuxer() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  demuxer_client_->AddDelegate(demuxer_client_id_, this);
  chunk_demuxer_->Initialize(this,
                             base::Bind(&MediaSourceDelegate::OnDemuxerInitDone,
                                        media_weak_factory_.GetWeakPtr()),
                             false);
}

blink::WebTimeRanges MediaSourceDelegate::Buffered() const {
  return media::ConvertToWebTimeRanges(buffered_time_ranges_);
}

size_t MediaSourceDelegate::DecodedFrameCount() const {
  return statistics_.video_frames_decoded;
}

size_t MediaSourceDelegate::DroppedFrameCount() const {
  return statistics_.video_frames_dropped;
}

size_t MediaSourceDelegate::AudioDecodedByteCount() const {
  return statistics_.audio_bytes_decoded;
}

size_t MediaSourceDelegate::VideoDecodedByteCount() const {
  return statistics_.video_bytes_decoded;
}

void MediaSourceDelegate::CancelPendingSeek(const base::TimeDelta& seek_time) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << seek_time.InSecondsF() << ") : "
           << demuxer_client_id_;

  if (!chunk_demuxer_)
    return;

  {
    // Remember to trivially finish any newly arriving browser seek requests
    // that may arrive prior to the next regular seek request.
    base::AutoLock auto_lock(seeking_lock_);
    expecting_regular_seek_ = true;
  }

  // Cancel any previously expected or in-progress regular or browser seek.
  // It is possible that we have just finished the seek, but caller does
  // not know this yet. It is still safe to cancel in this case because the
  // caller will always call StartWaitingForSeek() when it is notified of
  // the finished seek.
  chunk_demuxer_->CancelPendingSeek(seek_time);
}

void MediaSourceDelegate::StartWaitingForSeek(
    const base::TimeDelta& seek_time) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << seek_time.InSecondsF() << ") : "
           << demuxer_client_id_;

  if (!chunk_demuxer_)
    return;

  bool cancel_browser_seek = false;
  {
    // Remember to trivially finish any newly arriving browser seek requests
    // that may arrive prior to the next regular seek request.
    base::AutoLock auto_lock(seeking_lock_);
    expecting_regular_seek_ = true;

    // Remember to cancel any in-progress browser seek.
    if (seeking_) {
      DCHECK(doing_browser_seek_);
      cancel_browser_seek = true;
    }
  }

  if (cancel_browser_seek)
    chunk_demuxer_->CancelPendingSeek(seek_time);
  chunk_demuxer_->StartWaitingForSeek(seek_time);
}

void MediaSourceDelegate::Seek(
    const base::TimeDelta& seek_time, bool is_browser_seek) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << seek_time.InSecondsF() << ", "
           << (is_browser_seek ? "browser seek" : "regular seek") << ") : "
           << demuxer_client_id_;

  base::TimeDelta internal_seek_time = seek_time;
  {
    base::AutoLock auto_lock(seeking_lock_);
    DCHECK(!seeking_);
    seeking_ = true;
    doing_browser_seek_ = is_browser_seek;

    if (doing_browser_seek_ && (!chunk_demuxer_ || expecting_regular_seek_)) {
      // Trivially finish the browser seek without actually doing it. Reads will
      // continue to be |kAborted| until the next regular seek is done. Browser
      // seeking is not supported unless using a ChunkDemuxer; browser seeks are
      // trivially finished if |chunk_demuxer_| is NULL.
      seeking_ = false;
      doing_browser_seek_ = false;
      demuxer_client_->DemuxerSeekDone(demuxer_client_id_, seek_time);
      return;
    }

    if (doing_browser_seek_) {
      internal_seek_time = FindBufferedBrowserSeekTime_Locked(seek_time);
      browser_seek_time_ = internal_seek_time;
    } else {
      expecting_regular_seek_ = false;
      browser_seek_time_ = media::kNoTimestamp();
    }
  }

  // Prepare |chunk_demuxer_| for browser seek.
  if (is_browser_seek) {
    chunk_demuxer_->CancelPendingSeek(internal_seek_time);
    chunk_demuxer_->StartWaitingForSeek(internal_seek_time);
  }

  SeekInternal(internal_seek_time);
}

void MediaSourceDelegate::SeekInternal(const base::TimeDelta& seek_time) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(IsSeeking());
  chunk_demuxer_->Seek(seek_time, base::Bind(
      &MediaSourceDelegate::OnDemuxerSeekDone,
      media_weak_factory_.GetWeakPtr()));
}

void MediaSourceDelegate::OnBufferedTimeRangesChanged(
    const media::Ranges<base::TimeDelta>& ranges) {
  buffered_time_ranges_ = ranges;
}

void MediaSourceDelegate::SetDuration(base::TimeDelta duration) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << duration.InSecondsF() << ") : "
           << demuxer_client_id_;

  // Force duration change notification to be async to avoid reentrancy into
  // ChunkDemxuer.
  main_task_runner_->PostTask(FROM_HERE, base::Bind(
      &MediaSourceDelegate::OnDurationChanged, main_weak_this_, duration));
}

void MediaSourceDelegate::OnDurationChanged(const base::TimeDelta& duration) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (demuxer_client_)
    demuxer_client_->DurationChanged(demuxer_client_id_, duration);
  if (!duration_change_cb_.is_null())
    duration_change_cb_.Run(duration);
}

void MediaSourceDelegate::OnReadFromDemuxer(media::DemuxerStream::Type type) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << type << ") : " << demuxer_client_id_;
  if (IsSeeking())
    return;  // Drop the request during seeking.

  DCHECK(type == DemuxerStream::AUDIO || type == DemuxerStream::VIDEO);
  // The access unit size should have been initialized properly at this stage.
  DCHECK_GT(access_unit_size_, 0u);
  std::unique_ptr<DemuxerData> data(new DemuxerData());
  data->type = type;
  data->access_units.resize(access_unit_size_);
  ReadFromDemuxerStream(type, std::move(data), 0);
}

void MediaSourceDelegate::ReadFromDemuxerStream(
    media::DemuxerStream::Type type,
    std::unique_ptr<DemuxerData> data,
    size_t index) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  // DemuxerStream::Read() always returns the read callback asynchronously.
  DemuxerStream* stream =
      (type == DemuxerStream::AUDIO) ? audio_stream_ : video_stream_;
  stream->Read(base::Bind(
      &MediaSourceDelegate::OnBufferReady,
      media_weak_factory_.GetWeakPtr(), type, base::Passed(&data), index));
}

void MediaSourceDelegate::OnBufferReady(
    media::DemuxerStream::Type type,
    std::unique_ptr<DemuxerData> data,
    size_t index,
    DemuxerStream::Status status,
    const scoped_refptr<media::DecoderBuffer>& buffer) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << index << ", " << status << ", "
           << ((!buffer.get() || buffer->end_of_stream())
                   ? -1
                   : buffer->timestamp().InMilliseconds())
           << ") : " << demuxer_client_id_;
  DCHECK(chunk_demuxer_);

  // No new OnReadFromDemuxer() will be called during seeking. So this callback
  // must be from previous OnReadFromDemuxer() call and should be ignored.
  if (IsSeeking()) {
    DVLOG(1) << __FUNCTION__ << ": Ignore previous read during seeking.";
    return;
  }

  bool is_audio = (type == DemuxerStream::AUDIO);
  if (status != DemuxerStream::kAborted &&
      index >= data->access_units.size()) {
    LOG(ERROR) << "The internal state inconsistency onBufferReady: "
               << (is_audio ? "Audio" : "Video") << ", index " << index
               << ", size " << data->access_units.size()
               << ", status " << static_cast<int>(status);
    NOTREACHED();
    return;
  }

  switch (status) {
    case DemuxerStream::kAborted:
      DVLOG(1) << __FUNCTION__ << " : Aborted";
      data->access_units[index].status = status;
      data->access_units.resize(index + 1);
      break;

    case DemuxerStream::kConfigChanged:
      CHECK((is_audio && audio_stream_) || (!is_audio && video_stream_));
      data->demuxer_configs.resize(1);
      CHECK(GetDemuxerConfigFromStream(&data->demuxer_configs[0], is_audio));
      if (!is_audio) {
        gfx::Size size = data->demuxer_configs[0].video_size;
        DVLOG(1) << "Video config is changed: " << size.width() << "x"
                 << size.height();
      }
      data->access_units[index].status = status;
      data->access_units.resize(index + 1);
      break;

    case DemuxerStream::kOk:
      data->access_units[index].status = status;
      if (buffer->end_of_stream()) {
        data->access_units[index].is_end_of_stream = true;
        data->access_units.resize(index + 1);
        break;
      }
      data->access_units[index].is_key_frame = buffer->is_key_frame();
      // TODO(ycheo): We assume that the inputed stream will be decoded
      // right away.
      // Need to implement this properly using MediaPlayer.OnInfoListener.
      if (is_audio) {
        statistics_.audio_bytes_decoded += buffer->data_size();
      } else {
        statistics_.video_bytes_decoded += buffer->data_size();
        statistics_.video_frames_decoded++;
      }
      data->access_units[index].timestamp = buffer->timestamp();

      data->access_units[index].data.assign(
          buffer->data(), buffer->data() + buffer->data_size());
      // Vorbis needs 4 extra bytes padding on Android. Check
      // NuMediaExtractor.cpp in Android source code.
      if (is_audio && media::kCodecVorbis ==
          audio_stream_->audio_decoder_config().codec()) {
        data->access_units[index].data.insert(
            data->access_units[index].data.end(), kVorbisPadding,
            kVorbisPadding + 4);
      }
      if (buffer->decrypt_config()) {
        data->access_units[index].key_id = std::vector<char>(
            buffer->decrypt_config()->key_id().begin(),
            buffer->decrypt_config()->key_id().end());
        data->access_units[index].iv = std::vector<char>(
            buffer->decrypt_config()->iv().begin(),
            buffer->decrypt_config()->iv().end());
        data->access_units[index].subsamples =
            buffer->decrypt_config()->subsamples();
      }
      if (++index < data->access_units.size()) {
        ReadFromDemuxerStream(type, std::move(data), index);
        return;
      }
      break;

    default:
      NOTREACHED();
  }

  if (!IsSeeking() && demuxer_client_)
    demuxer_client_->ReadFromDemuxerAck(demuxer_client_id_, *data);
}

void MediaSourceDelegate::OnDemuxerError(media::PipelineStatus status) {
  DVLOG(1) << __FUNCTION__ << "(" << status << ") : " << demuxer_client_id_;
  // |update_network_state_cb_| is bound to the main thread.
  if (status != media::PIPELINE_OK && !update_network_state_cb_.is_null())
    update_network_state_cb_.Run(PipelineErrorToNetworkState(status));
}

void MediaSourceDelegate::AddTextStream(
    media::DemuxerStream* /* text_stream */ ,
    const media::TextTrackConfig& /* config */ ) {
  // TODO(matthewjheaney): add text stream (http://crbug/322115).
  NOTIMPLEMENTED();
}

void MediaSourceDelegate::RemoveTextStream(
    media::DemuxerStream* /* text_stream */ ) {
  // TODO(matthewjheaney): remove text stream (http://crbug/322115).
  NOTIMPLEMENTED();
}

void MediaSourceDelegate::OnDemuxerInitDone(media::PipelineStatus status) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << status << ") : " << demuxer_client_id_;
  DCHECK(chunk_demuxer_);

  if (status != media::PIPELINE_OK) {
    OnDemuxerError(status);
    return;
  }

  audio_stream_ = chunk_demuxer_->GetStream(DemuxerStream::AUDIO);
  video_stream_ = chunk_demuxer_->GetStream(DemuxerStream::VIDEO);
  DCHECK(audio_stream_ || video_stream_);

  if (HasEncryptedStream()) {
    set_cdm_ready_cb_.Run(BindToCurrentLoop(base::Bind(
        &MediaSourceDelegate::SetCdm, media_weak_factory_.GetWeakPtr())));
    return;
  }

  // Notify demuxer ready when both streams are not encrypted.
  NotifyDemuxerReady(false);
}

bool MediaSourceDelegate::HasEncryptedStream() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(audio_stream_ || video_stream_);

  return (audio_stream_ &&
          audio_stream_->audio_decoder_config().is_encrypted()) ||
         (video_stream_ &&
          video_stream_->video_decoder_config().is_encrypted());
}

void MediaSourceDelegate::SetCdm(media::CdmContext* cdm_context,
                                 const media::CdmAttachedCB& cdm_attached_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(cdm_context);
  DCHECK(!cdm_attached_cb.is_null());
  DCHECK(!is_demuxer_ready_);
  DCHECK(HasEncryptedStream());

  cdm_context_ = cdm_context;
  pending_cdm_attached_cb_ = cdm_attached_cb;

  if (audio_stream_ && audio_stream_->audio_decoder_config().is_encrypted()) {
    InitAudioDecryptingDemuxerStream();
    // InitVideoDecryptingDemuxerStream() will be called in
    // OnAudioDecryptingDemuxerStreamInitDone().
    return;
  }

  if (video_stream_ && video_stream_->video_decoder_config().is_encrypted()) {
    InitVideoDecryptingDemuxerStream();
    return;
  }

  NOTREACHED() << "No encrytped stream.";
}

void MediaSourceDelegate::InitAudioDecryptingDemuxerStream() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;
  DCHECK(cdm_context_);
  audio_decrypting_demuxer_stream_.reset(new media::DecryptingDemuxerStream(
      media_task_runner_, media_log_, waiting_for_decryption_key_cb_));
  audio_decrypting_demuxer_stream_->Initialize(
      audio_stream_, cdm_context_,
      base::Bind(&MediaSourceDelegate::OnAudioDecryptingDemuxerStreamInitDone,
                 media_weak_factory_.GetWeakPtr()));
}

void MediaSourceDelegate::InitVideoDecryptingDemuxerStream() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;
  DCHECK(cdm_context_);

  video_decrypting_demuxer_stream_.reset(new media::DecryptingDemuxerStream(
      media_task_runner_, media_log_, waiting_for_decryption_key_cb_));
  video_decrypting_demuxer_stream_->Initialize(
      video_stream_, cdm_context_,
      base::Bind(&MediaSourceDelegate::OnVideoDecryptingDemuxerStreamInitDone,
                 media_weak_factory_.GetWeakPtr()));
}

void MediaSourceDelegate::OnAudioDecryptingDemuxerStreamInitDone(
    media::PipelineStatus status) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << status << ") : " << demuxer_client_id_;
  DCHECK(chunk_demuxer_);

  if (status != media::PIPELINE_OK) {
    audio_decrypting_demuxer_stream_.reset();
    // Different CDMs are supported differently. For CDMs that support a
    // Decryptor, we'll try to use DecryptingDemuxerStream in the render side.
    // Otherwise, we'll try to use the CDMs in the browser side. Therefore, if
    // DecryptingDemuxerStream initialization failed, it's still possible that
    // we can handle the audio with a CDM in the browser. Declare demuxer ready
    // now to try that path. Note there's no need to try DecryptingDemuxerStream
    // for video here since it is impossible to handle audio in the browser and
    // handle video in the render process.
    NotifyDemuxerReady(false);
    return;
  }

  audio_stream_ = audio_decrypting_demuxer_stream_.get();

  if (video_stream_ && video_stream_->video_decoder_config().is_encrypted()) {
    InitVideoDecryptingDemuxerStream();
    return;
  }

  // Try to notify demuxer ready when audio DDS initialization finished and
  // video is not encrypted.
  NotifyDemuxerReady(true);
}

void MediaSourceDelegate::OnVideoDecryptingDemuxerStreamInitDone(
    media::PipelineStatus status) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << status << ") : " << demuxer_client_id_;
  DCHECK(chunk_demuxer_);

  bool success = status == media::PIPELINE_OK;

  if (!success)
    video_decrypting_demuxer_stream_.reset();
  else
    video_stream_ = video_decrypting_demuxer_stream_.get();

  // Try to notify demuxer ready when video DDS initialization finished.
  NotifyDemuxerReady(success);
}

void MediaSourceDelegate::OnDemuxerSeekDone(media::PipelineStatus status) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << "(" << status << ") : " << demuxer_client_id_;
  DCHECK(IsSeeking());

  if (status != media::PIPELINE_OK) {
    OnDemuxerError(status);
    return;
  }

  ResetAudioDecryptingDemuxerStream();
}

void MediaSourceDelegate::ResetAudioDecryptingDemuxerStream() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;
  if (audio_decrypting_demuxer_stream_) {
    audio_decrypting_demuxer_stream_->Reset(
        base::Bind(&MediaSourceDelegate::ResetVideoDecryptingDemuxerStream,
                   media_weak_factory_.GetWeakPtr()));
    return;
  }

  ResetVideoDecryptingDemuxerStream();
}

void MediaSourceDelegate::ResetVideoDecryptingDemuxerStream() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;
  if (video_decrypting_demuxer_stream_) {
    video_decrypting_demuxer_stream_->Reset(base::Bind(
        &MediaSourceDelegate::FinishResettingDecryptingDemuxerStreams,
        media_weak_factory_.GetWeakPtr()));
    return;
  }

  FinishResettingDecryptingDemuxerStreams();
}

void MediaSourceDelegate::FinishResettingDecryptingDemuxerStreams() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_;

  base::AutoLock auto_lock(seeking_lock_);
  DCHECK(seeking_);
  seeking_ = false;
  doing_browser_seek_ = false;
  demuxer_client_->DemuxerSeekDone(demuxer_client_id_, browser_seek_time_);
}

void MediaSourceDelegate::NotifyDemuxerReady(bool is_cdm_attached) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__ << " : " << demuxer_client_id_
           << ", is_cdm_attached: " << is_cdm_attached;
  DCHECK(!is_demuxer_ready_);

  is_demuxer_ready_ = true;

  if (!pending_cdm_attached_cb_.is_null())
    base::ResetAndReturn(&pending_cdm_attached_cb_).Run(is_cdm_attached);

  std::unique_ptr<DemuxerConfigs> configs(new DemuxerConfigs());
  GetDemuxerConfigFromStream(configs.get(), true);
  GetDemuxerConfigFromStream(configs.get(), false);
  configs->duration = GetDuration();

  if (demuxer_client_)
    demuxer_client_->DemuxerReady(demuxer_client_id_, *configs);

  base::AutoLock auto_lock(is_video_encrypted_lock_);
  is_video_encrypted_ = configs->is_video_encrypted;
}

base::TimeDelta MediaSourceDelegate::GetDuration() const {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  if (!chunk_demuxer_)
    return media::kNoTimestamp();

  double duration = chunk_demuxer_->GetDuration();
  if (duration == std::numeric_limits<double>::infinity())
    return media::kInfiniteDuration();

  return base::TimeDelta::FromSecondsD(duration);
}

void MediaSourceDelegate::OnDemuxerOpened() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (media_source_opened_cb_.is_null())
    return;

  media_source_opened_cb_.Run(
      new media::WebMediaSourceImpl(chunk_demuxer_.get(), media_log_));
}

void MediaSourceDelegate::OnEncryptedMediaInitData(
    media::EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (encrypted_media_init_data_cb_.is_null())
    return;

  encrypted_media_init_data_cb_.Run(init_data_type, init_data);
}

bool MediaSourceDelegate::IsSeeking() const {
  base::AutoLock auto_lock(seeking_lock_);
  return seeking_;
}

base::TimeDelta MediaSourceDelegate::FindBufferedBrowserSeekTime_Locked(
    const base::TimeDelta& seek_time) const {
  seeking_lock_.AssertAcquired();
  DCHECK(seeking_);
  DCHECK(doing_browser_seek_);
  DCHECK(chunk_demuxer_) << "Browser seek requested, but no chunk demuxer";

  media::Ranges<base::TimeDelta> buffered =
      chunk_demuxer_->GetBufferedRanges();

  for (size_t i = 0; i < buffered.size(); ++i) {
    base::TimeDelta range_start = buffered.start(i);
    base::TimeDelta range_end = buffered.end(i);
    if (range_start <= seek_time) {
      if (range_end >= seek_time)
        return seek_time;
      continue;
    }

    // If the start of the next buffered range after |seek_time| is too far
    // into the future, do not jump forward.
    if ((range_start - seek_time) > base::TimeDelta::FromMilliseconds(100))
      break;

    // TODO(wolenetz): Remove possibility that this browser seek jumps
    // into future when the requested range is unbuffered but there is some
    // other buffered range after it. See http://crbug.com/304234.
    return range_start;
  }

  // We found no range containing |seek_time| or beginning shortly after
  // |seek_time|. While possible that such data at and beyond the player's
  // current time have been garbage collected or removed by the web app, this is
  // unlikely. This may cause unexpected playback stall due to seek pending an
  // append for a GOP prior to the last GOP demuxed.
  // TODO(wolenetz): Remove the possibility for this seek to cause unexpected
  // player stall by replaying cached data since last keyframe in browser player
  // rather than issuing browser seek. See http://crbug.com/304234.
  return seek_time;
}

bool MediaSourceDelegate::GetDemuxerConfigFromStream(
    media::DemuxerConfigs* configs, bool is_audio) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  if (!is_demuxer_ready_)
    return false;
  if (is_audio && audio_stream_) {
    media::AudioDecoderConfig config = audio_stream_->audio_decoder_config();
    configs->audio_codec = config.codec();
    configs->audio_channels =
        media::ChannelLayoutToChannelCount(config.channel_layout());
    configs->audio_sampling_rate = config.samples_per_second();
    configs->is_audio_encrypted = config.is_encrypted();
    configs->audio_extra_data = config.extra_data();
    configs->audio_codec_delay_ns = static_cast<int64_t>(
        config.codec_delay()  *
        (static_cast<double>(base::Time::kNanosecondsPerSecond) /
         config.samples_per_second()));
    configs->audio_seek_preroll_ns =
        config.seek_preroll().InMicroseconds() *
        base::Time::kNanosecondsPerMicrosecond;
    return true;
  }
  if (!is_audio && video_stream_) {
    media::VideoDecoderConfig config = video_stream_->video_decoder_config();
    configs->video_codec = config.codec();
    configs->video_size = config.natural_size();
    configs->is_video_encrypted = config.is_encrypted();
    configs->video_extra_data = config.extra_data();
    return true;
  }
  return false;
}

}  // namespace content

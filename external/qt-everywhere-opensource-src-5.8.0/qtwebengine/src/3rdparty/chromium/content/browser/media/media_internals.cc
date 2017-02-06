// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_log_event.h"
#include "media/filters/gpu_video_decoder.h"

#if !defined(OS_ANDROID)
#include "media/filters/decrypting_video_decoder.h"
#endif

namespace {

static base::LazyInstance<content::MediaInternals>::Leaky g_media_internals =
    LAZY_INSTANCE_INITIALIZER;

base::string16 SerializeUpdate(const std::string& function,
                               const base::Value* value) {
  return content::WebUI::GetJavascriptCall(
      function, std::vector<const base::Value*>(1, value));
}

std::string EffectsToString(int effects) {
  if (effects == media::AudioParameters::NO_EFFECTS)
    return "NO_EFFECTS";

  struct {
    int flag;
    const char* name;
  } flags[] = {
    { media::AudioParameters::ECHO_CANCELLER, "ECHO_CANCELLER" },
    { media::AudioParameters::DUCKING, "DUCKING" },
    { media::AudioParameters::KEYBOARD_MIC, "KEYBOARD_MIC" },
    { media::AudioParameters::HOTWORD, "HOTWORD" },
  };

  std::string ret;
  for (size_t i = 0; i < arraysize(flags); ++i) {
    if (effects & flags[i].flag) {
      if (!ret.empty())
        ret += " | ";
      ret += flags[i].name;
      effects &= ~flags[i].flag;
    }
  }

  if (effects) {
    if (!ret.empty())
      ret += " | ";
    ret += base::IntToString(effects);
  }

  return ret;
}

std::string FormatToString(media::AudioParameters::Format format) {
  switch (format) {
    case media::AudioParameters::AUDIO_PCM_LINEAR:
      return "pcm_linear";
    case media::AudioParameters::AUDIO_PCM_LOW_LATENCY:
      return "pcm_low_latency";
    case media::AudioParameters::AUDIO_FAKE:
      return "fake";
  }

  NOTREACHED();
  return "unknown";
}

const char kAudioLogStatusKey[] = "status";
const char kAudioLogUpdateFunction[] = "media.updateAudioComponent";

}  // namespace

namespace content {

class AudioLogImpl : public media::AudioLog {
 public:
  AudioLogImpl(int owner_id,
               media::AudioLogFactory::AudioComponent component,
               content::MediaInternals* media_internals);
  ~AudioLogImpl() override;

  void OnCreated(int component_id,
                 const media::AudioParameters& params,
                 const std::string& device_id) override;
  void OnStarted(int component_id) override;
  void OnStopped(int component_id) override;
  void OnClosed(int component_id) override;
  void OnError(int component_id) override;
  void OnSetVolume(int component_id, double volume) override;
  void OnSwitchOutputDevice(int component_id,
                            const std::string& device_id) override;
  void OnLogMessage(int component_id, const std::string& message) override;

  // Called by MediaInternals to update the WebContents title for a stream.
  void SendWebContentsTitle(int component_id,
                            int render_process_id,
                            int render_frame_id);

 private:
  void SendSingleStringUpdate(int component_id,
                              const std::string& key,
                              const std::string& value);
  void StoreComponentMetadata(int component_id, base::DictionaryValue* dict);
  std::string FormatCacheKey(int component_id);

  static void SendWebContentsTitleHelper(
      const std::string& cache_key,
      std::unique_ptr<base::DictionaryValue> dict,
      int render_process_id,
      int render_frame_id);

  const int owner_id_;
  const media::AudioLogFactory::AudioComponent component_;
  content::MediaInternals* const media_internals_;

  DISALLOW_COPY_AND_ASSIGN(AudioLogImpl);
};

AudioLogImpl::AudioLogImpl(int owner_id,
                           media::AudioLogFactory::AudioComponent component,
                           content::MediaInternals* media_internals)
    : owner_id_(owner_id),
      component_(component),
      media_internals_(media_internals) {}

AudioLogImpl::~AudioLogImpl() {}

void AudioLogImpl::OnCreated(int component_id,
                             const media::AudioParameters& params,
                             const std::string& device_id) {
  base::DictionaryValue dict;
  StoreComponentMetadata(component_id, &dict);

  dict.SetString(kAudioLogStatusKey, "created");
  dict.SetString("device_id", device_id);
  dict.SetString("device_type", FormatToString(params.format()));
  dict.SetInteger("frames_per_buffer", params.frames_per_buffer());
  dict.SetInteger("sample_rate", params.sample_rate());
  dict.SetInteger("channels", params.channels());
  dict.SetString("channel_layout",
                 ChannelLayoutToString(params.channel_layout()));
  dict.SetString("effects", EffectsToString(params.effects()));

  media_internals_->UpdateAudioLog(MediaInternals::CREATE,
                                   FormatCacheKey(component_id),
                                   kAudioLogUpdateFunction, &dict);
}

void AudioLogImpl::OnStarted(int component_id) {
  SendSingleStringUpdate(component_id, kAudioLogStatusKey, "started");
}

void AudioLogImpl::OnStopped(int component_id) {
  SendSingleStringUpdate(component_id, kAudioLogStatusKey, "stopped");
}

void AudioLogImpl::OnClosed(int component_id) {
  base::DictionaryValue dict;
  StoreComponentMetadata(component_id, &dict);
  dict.SetString(kAudioLogStatusKey, "closed");
  media_internals_->UpdateAudioLog(MediaInternals::UPDATE_AND_DELETE,
                                   FormatCacheKey(component_id),
                                   kAudioLogUpdateFunction, &dict);
}

void AudioLogImpl::OnError(int component_id) {
  SendSingleStringUpdate(component_id, "error_occurred", "true");
}

void AudioLogImpl::OnSetVolume(int component_id, double volume) {
  base::DictionaryValue dict;
  StoreComponentMetadata(component_id, &dict);
  dict.SetDouble("volume", volume);
  media_internals_->UpdateAudioLog(MediaInternals::UPDATE_IF_EXISTS,
                                   FormatCacheKey(component_id),
                                   kAudioLogUpdateFunction, &dict);
}

void AudioLogImpl::OnSwitchOutputDevice(int component_id,
                                        const std::string& device_id) {
  base::DictionaryValue dict;
  StoreComponentMetadata(component_id, &dict);
  dict.SetString("device_id", device_id);
  media_internals_->UpdateAudioLog(MediaInternals::UPDATE_IF_EXISTS,
                                   FormatCacheKey(component_id),
                                   kAudioLogUpdateFunction, &dict);
}

void AudioLogImpl::OnLogMessage(int component_id, const std::string& message) {
  MediaStreamManager::SendMessageToNativeLog(message);
}

void AudioLogImpl::SendWebContentsTitle(int component_id,
                                        int render_process_id,
                                        int render_frame_id) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  StoreComponentMetadata(component_id, dict.get());
  SendWebContentsTitleHelper(FormatCacheKey(component_id), std::move(dict),
                             render_process_id, render_frame_id);
}

std::string AudioLogImpl::FormatCacheKey(int component_id) {
  return base::StringPrintf("%d:%d:%d", owner_id_, component_, component_id);
}

// static
void AudioLogImpl::SendWebContentsTitleHelper(
    const std::string& cache_key,
    std::unique_ptr<base::DictionaryValue> dict,
    int render_process_id,
    int render_frame_id) {
  // Page title information can only be retrieved from the UI thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SendWebContentsTitleHelper, cache_key, base::Passed(&dict),
                   render_process_id, render_frame_id));
    return;
  }

  const WebContents* web_contents = WebContents::FromRenderFrameHost(
      RenderFrameHost::FromID(render_process_id, render_frame_id));
  if (!web_contents)
    return;

  // Note: by this point the given audio log entry could have been destroyed, so
  // we use UPDATE_IF_EXISTS to discard such instances.
  dict->SetInteger("render_process_id", render_process_id);
  dict->SetString("web_contents_title", web_contents->GetTitle());
  MediaInternals::GetInstance()->UpdateAudioLog(
      MediaInternals::UPDATE_IF_EXISTS, cache_key, kAudioLogUpdateFunction,
      dict.get());
}

void AudioLogImpl::SendSingleStringUpdate(int component_id,
                                          const std::string& key,
                                          const std::string& value) {
  base::DictionaryValue dict;
  StoreComponentMetadata(component_id, &dict);
  dict.SetString(key, value);
  media_internals_->UpdateAudioLog(MediaInternals::UPDATE_IF_EXISTS,
                                   FormatCacheKey(component_id),
                                   kAudioLogUpdateFunction, &dict);
}

void AudioLogImpl::StoreComponentMetadata(int component_id,
                                          base::DictionaryValue* dict) {
  dict->SetInteger("owner_id", owner_id_);
  dict->SetInteger("component_id", component_id);
  dict->SetInteger("component_type", component_);
}

// This class lives on the browser UI thread.
class MediaInternals::MediaInternalsUMAHandler {
 public:
  MediaInternalsUMAHandler();

  // Called when a render process is terminated. Reports the pipeline status to
  // UMA for every player associated with the renderer process and then deletes
  // the player state.
  void OnProcessTerminated(int render_process_id);

  // Helper function to save the event payload to RendererPlayerMap.
  void SavePlayerState(int render_process_id,
                       const media::MediaLogEvent& event);

 private:
  struct PipelineInfo {
    bool has_pipeline = false;
    media::PipelineStatus last_pipeline_status = media::PIPELINE_OK;
    bool has_audio = false;
    bool has_video = false;
    bool video_dds = false;
    bool video_decoder_changed = false;
    std::string audio_codec_name;
    std::string video_codec_name;
    std::string video_decoder;
  };

  // Helper function to report PipelineStatus associated with a player to UMA.
  void ReportUMAForPipelineStatus(const PipelineInfo& player_info);

  // Helper to generate PipelineStatus UMA name for AudioVideo streams.
  std::string GetUMANameForAVStream(const PipelineInfo& player_info);

  // Key is player id.
  typedef std::map<int, PipelineInfo> PlayerInfoMap;

  // Key is renderer id.
  typedef std::map<int, PlayerInfoMap> RendererPlayerMap;

  // Stores player information per renderer.
  RendererPlayerMap renderer_info_;

  DISALLOW_COPY_AND_ASSIGN(MediaInternalsUMAHandler);
};

MediaInternals::MediaInternalsUMAHandler::MediaInternalsUMAHandler() {
}

void MediaInternals::MediaInternalsUMAHandler::SavePlayerState(
    int render_process_id,
    const media::MediaLogEvent& event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PlayerInfoMap& player_info = renderer_info_[render_process_id];
  switch (event.type) {
    case media::MediaLogEvent::PIPELINE_STATE_CHANGED: {
      player_info[event.id].has_pipeline = true;
      break;
    }
    case media::MediaLogEvent::PIPELINE_ERROR: {
      int status;
      event.params.GetInteger("pipeline_error", &status);
      player_info[event.id].last_pipeline_status =
          static_cast<media::PipelineStatus>(status);
      break;
    }
    case media::MediaLogEvent::PROPERTY_CHANGE:
      if (event.params.HasKey("found_audio_stream")) {
        event.params.GetBoolean("found_audio_stream",
                                &player_info[event.id].has_audio);
      }
      if (event.params.HasKey("found_video_stream")) {
        event.params.GetBoolean("found_video_stream",
                                &player_info[event.id].has_video);
      }
      if (event.params.HasKey("audio_codec_name")) {
        event.params.GetString("audio_codec_name",
                               &player_info[event.id].audio_codec_name);
      }
      if (event.params.HasKey("video_codec_name")) {
        event.params.GetString("video_codec_name",
                               &player_info[event.id].video_codec_name);
      }
      if (event.params.HasKey("video_decoder")) {
        std::string previous_video_decoder(player_info[event.id].video_decoder);
        event.params.GetString("video_decoder",
                               &player_info[event.id].video_decoder);
        if (!previous_video_decoder.empty() &&
            previous_video_decoder != player_info[event.id].video_decoder) {
          player_info[event.id].video_decoder_changed = true;
        }
      }
      if (event.params.HasKey("video_dds")) {
        event.params.GetBoolean("video_dds", &player_info[event.id].video_dds);
      }
      break;
    default:
      break;
  }
  return;
}

std::string MediaInternals::MediaInternalsUMAHandler::GetUMANameForAVStream(
    const PipelineInfo& player_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static const char kPipelineUmaPrefix[] = "Media.PipelineStatus.AudioVideo.";
  std::string uma_name = kPipelineUmaPrefix;
  if (player_info.video_codec_name == "vp8") {
    uma_name += "VP8.";
  } else if (player_info.video_codec_name == "vp9") {
    uma_name += "VP9.";
  } else if (player_info.video_codec_name == "h264") {
    uma_name += "H264.";
  } else {
    return uma_name + "Other";
  }

#if !defined(OS_ANDROID)
  if (player_info.video_decoder ==
      media::DecryptingVideoDecoder::kDecoderName) {
    return uma_name + "DVD";
  }
#endif

  if (player_info.video_dds) {
    uma_name += "DDS.";
  }

  if (player_info.video_decoder == media::GpuVideoDecoder::kDecoderName) {
    uma_name += "HW";
  } else {
    uma_name += "SW";
  }
  return uma_name;
}

void MediaInternals::MediaInternalsUMAHandler::ReportUMAForPipelineStatus(
    const PipelineInfo& player_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Don't log pipeline status for players which don't actually have a pipeline;
  // e.g., the Android MediaSourcePlayer implementation.
  if (!player_info.has_pipeline)
    return;

  if (player_info.has_video && player_info.has_audio) {
    base::LinearHistogram::FactoryGet(
        GetUMANameForAVStream(player_info), 1, media::PIPELINE_STATUS_MAX,
        media::PIPELINE_STATUS_MAX + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(player_info.last_pipeline_status);
  } else if (player_info.has_audio) {
    UMA_HISTOGRAM_ENUMERATION("Media.PipelineStatus.AudioOnly",
                              player_info.last_pipeline_status,
                              media::PIPELINE_STATUS_MAX + 1);
  } else if (player_info.has_video) {
    UMA_HISTOGRAM_ENUMERATION("Media.PipelineStatus.VideoOnly",
                              player_info.last_pipeline_status,
                              media::PIPELINE_STATUS_MAX + 1);
  } else {
    // Note: This metric can be recorded as a result of normal operation with
    // Media Source Extensions. If a site creates a MediaSource object but never
    // creates a source buffer or appends data, PIPELINE_OK will be recorded.
    UMA_HISTOGRAM_ENUMERATION("Media.PipelineStatus.Unsupported",
                              player_info.last_pipeline_status,
                              media::PIPELINE_STATUS_MAX + 1);
  }
  // Report whether video decoder fallback happened, but only if a video decoder
  // was reported.
  if (!player_info.video_decoder.empty()) {
    UMA_HISTOGRAM_BOOLEAN("Media.VideoDecoderFallback",
                          player_info.video_decoder_changed);
  }
}

void MediaInternals::MediaInternalsUMAHandler::OnProcessTerminated(
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto players_it = renderer_info_.find(render_process_id);
  if (players_it == renderer_info_.end())
    return;
  auto it = players_it->second.begin();
  while (it != players_it->second.end()) {
    ReportUMAForPipelineStatus(it->second);
    players_it->second.erase(it++);
  }
  renderer_info_.erase(players_it);
}

MediaInternals* MediaInternals::GetInstance() {
  return g_media_internals.Pointer();
}

MediaInternals::MediaInternals()
    : can_update_(false),
      owner_ids_(),
      uma_handler_(new MediaInternalsUMAHandler()) {
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllBrowserContextsAndSources());
}

MediaInternals::~MediaInternals() {}

void MediaInternals::Observe(int type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(type, NOTIFICATION_RENDERER_PROCESS_TERMINATED);
  RenderProcessHost* process = Source<RenderProcessHost>(source).ptr();

  uma_handler_->OnProcessTerminated(process->GetID());
  pending_events_map_.erase(process->GetID());
}

// Converts the |event| to a |update|. Returns whether the conversion succeeded.
static bool ConvertEventToUpdate(int render_process_id,
                                 const media::MediaLogEvent& event,
                                 base::string16* update) {
  DCHECK(update);

  base::DictionaryValue dict;
  dict.SetInteger("renderer", render_process_id);
  dict.SetInteger("player", event.id);
  dict.SetString("type", media::MediaLog::EventTypeToString(event.type));

  // TODO(dalecurtis): This is technically not correct.  TimeTicks "can't" be
  // converted to to a human readable time format.  See base/time/time.h.
  const double ticks = event.time.ToInternalValue();
  const double ticks_millis = ticks / base::Time::kMicrosecondsPerMillisecond;
  dict.SetDouble("ticksMillis", ticks_millis);

  // Convert PipelineStatus to human readable string
  if (event.type == media::MediaLogEvent::PIPELINE_ERROR) {
    int status;
    if (!event.params.GetInteger("pipeline_error", &status) ||
        status < static_cast<int>(media::PIPELINE_OK) ||
        status > static_cast<int>(media::PIPELINE_STATUS_MAX)) {
      return false;
    }
    media::PipelineStatus error = static_cast<media::PipelineStatus>(status);
    dict.SetString("params.pipeline_error",
                   media::MediaLog::PipelineStatusToString(error));
  } else {
    dict.Set("params", event.params.DeepCopy());
  }

  *update = SerializeUpdate("media.onMediaEvent", &dict);
  return true;
}

void MediaInternals::OnMediaEvents(
    int render_process_id, const std::vector<media::MediaLogEvent>& events) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Notify observers that |event| has occurred.
  for (const auto& event : events) {
    if (CanUpdate()) {
      base::string16 update;
      if (ConvertEventToUpdate(render_process_id, event, &update))
        SendUpdate(update);
    }

    SaveEvent(render_process_id, event);
    uma_handler_->SavePlayerState(render_process_id, event);
  }
}

void MediaInternals::AddUpdateCallback(const UpdateCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  update_callbacks_.push_back(callback);

  base::AutoLock auto_lock(lock_);
  can_update_ = true;
}

void MediaInternals::RemoveUpdateCallback(const UpdateCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (size_t i = 0; i < update_callbacks_.size(); ++i) {
    if (update_callbacks_[i].Equals(callback)) {
      update_callbacks_.erase(update_callbacks_.begin() + i);
      break;
    }
  }

  base::AutoLock auto_lock(lock_);
  can_update_ = !update_callbacks_.empty();
}

bool MediaInternals::CanUpdate() {
  base::AutoLock auto_lock(lock_);
  return can_update_;
}

void MediaInternals::SendHistoricalMediaEvents() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& pending_events : pending_events_map_) {
    for (const auto& event : pending_events.second) {
      base::string16 update;
      if (ConvertEventToUpdate(pending_events.first, event, &update))
        SendUpdate(update);
    }
  }
  // Do not clear the map/list here so that refreshing the UI or opening a
  // second UI still works nicely!
}

void MediaInternals::SendAudioStreamData() {
  base::string16 audio_stream_update;
  {
    base::AutoLock auto_lock(lock_);
    audio_stream_update = SerializeUpdate(
        "media.onReceiveAudioStreamData", &audio_streams_cached_data_);
  }
  SendUpdate(audio_stream_update);
}

void MediaInternals::SendVideoCaptureDeviceCapabilities() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!CanUpdate())
    return;

  SendUpdate(SerializeUpdate("media.onReceiveVideoCaptureCapabilities",
                             &video_capture_capabilities_cached_data_));
}

void MediaInternals::UpdateVideoCaptureDeviceCapabilities(
    const media::VideoCaptureDeviceInfos& video_capture_device_infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  video_capture_capabilities_cached_data_.Clear();

  for (const auto& video_capture_device_info : video_capture_device_infos) {
    base::ListValue* format_list = new base::ListValue();
    // TODO(nisse): Representing format information as a string, to be
    // parsed by the javascript handler, is brittle. Consider passing
    // a list of mappings instead.

    for (const auto& format : video_capture_device_info.supported_formats)
      format_list->AppendString(media::VideoCaptureFormat::ToString(format));

    std::unique_ptr<base::DictionaryValue> device_dict(
        new base::DictionaryValue());
    device_dict->SetString("id", video_capture_device_info.name.id());
    device_dict->SetString(
        "name", video_capture_device_info.name.GetNameAndModel());
    device_dict->Set("formats", format_list);
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_ANDROID)
    device_dict->SetString(
        "captureApi", video_capture_device_info.name.GetCaptureApiTypeString());
#endif
    video_capture_capabilities_cached_data_.Append(std::move(device_dict));
  }

  SendVideoCaptureDeviceCapabilities();
}

std::unique_ptr<media::AudioLog> MediaInternals::CreateAudioLog(
    AudioComponent component) {
  base::AutoLock auto_lock(lock_);
  return std::unique_ptr<media::AudioLog>(
      new AudioLogImpl(owner_ids_[component]++, component, this));
}

void MediaInternals::SetWebContentsTitleForAudioLogEntry(
    int component_id,
    int render_process_id,
    int render_frame_id,
    media::AudioLog* audio_log) {
  static_cast<AudioLogImpl*>(audio_log)
      ->SendWebContentsTitle(component_id, render_process_id, render_frame_id);
}

void MediaInternals::SendUpdate(const base::string16& update) {
  // SendUpdate() may be called from any thread, but must run on the UI thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
        &MediaInternals::SendUpdate, base::Unretained(this), update));
    return;
  }

  for (size_t i = 0; i < update_callbacks_.size(); i++)
    update_callbacks_[i].Run(update);
}

void MediaInternals::SaveEvent(int process_id,
                               const media::MediaLogEvent& event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Max number of saved updates allowed for one process.
  const size_t kMaxNumEvents = 128;

  // Do not save instantaneous events that happen frequently and have little
  // value in the future.
  if (event.type == media::MediaLogEvent::NETWORK_ACTIVITY_SET ||
      event.type == media::MediaLogEvent::BUFFERED_EXTENTS_CHANGED) {
    return;
  }

  auto& pending_events = pending_events_map_[process_id];
  // TODO(xhwang): Notify user that some old logs could have been truncated.
  // See http://crbug.com/498520
  if (pending_events.size() >= kMaxNumEvents)
    pending_events.pop_front();
  pending_events.push_back(event);
}

void MediaInternals::UpdateAudioLog(AudioLogUpdateType type,
                                    const std::string& cache_key,
                                    const std::string& function,
                                    const base::DictionaryValue* value) {
  {
    base::AutoLock auto_lock(lock_);
    const bool has_entry = audio_streams_cached_data_.HasKey(cache_key);
    if ((type == UPDATE_IF_EXISTS || type == UPDATE_AND_DELETE) && !has_entry) {
      return;
    } else if (!has_entry) {
      DCHECK_EQ(type, CREATE);
      audio_streams_cached_data_.Set(cache_key, value->DeepCopy());
    } else if (type == UPDATE_AND_DELETE) {
      std::unique_ptr<base::Value> out_value;
      CHECK(audio_streams_cached_data_.Remove(cache_key, &out_value));
    } else {
      base::DictionaryValue* existing_dict = NULL;
      CHECK(
          audio_streams_cached_data_.GetDictionary(cache_key, &existing_dict));
      existing_dict->MergeDictionary(value);
    }
  }

  if (CanUpdate())
    SendUpdate(SerializeUpdate(function, value));
}

}  // namespace content

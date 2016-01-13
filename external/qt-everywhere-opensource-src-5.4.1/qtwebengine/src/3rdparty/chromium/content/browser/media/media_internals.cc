// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals.h"

#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "media/audio/audio_parameters.h"
#include "media/base/media_log.h"
#include "media/base/media_log_event.h"

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
  };

  std::string ret;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(flags); ++i) {
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

const char kAudioLogStatusKey[] = "status";
const char kAudioLogUpdateFunction[] = "media.updateAudioComponent";

}  // namespace

namespace content {

class AudioLogImpl : public media::AudioLog {
 public:
  AudioLogImpl(int owner_id,
               media::AudioLogFactory::AudioComponent component,
               content::MediaInternals* media_internals);
  virtual ~AudioLogImpl();

  virtual void OnCreated(int component_id,
                         const media::AudioParameters& params,
                         const std::string& device_id) OVERRIDE;
  virtual void OnStarted(int component_id) OVERRIDE;
  virtual void OnStopped(int component_id) OVERRIDE;
  virtual void OnClosed(int component_id) OVERRIDE;
  virtual void OnError(int component_id) OVERRIDE;
  virtual void OnSetVolume(int component_id, double volume) OVERRIDE;

 private:
  void SendSingleStringUpdate(int component_id,
                              const std::string& key,
                              const std::string& value);
  void StoreComponentMetadata(int component_id, base::DictionaryValue* dict);
  std::string FormatCacheKey(int component_id);

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
  dict.SetInteger("input_channels", params.input_channels());
  dict.SetInteger("frames_per_buffer", params.frames_per_buffer());
  dict.SetInteger("sample_rate", params.sample_rate());
  dict.SetInteger("channels", params.channels());
  dict.SetString("channel_layout",
                 ChannelLayoutToString(params.channel_layout()));
  dict.SetString("effects", EffectsToString(params.effects()));

  media_internals_->SendUpdateAndCache(
      FormatCacheKey(component_id), kAudioLogUpdateFunction, &dict);
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
  media_internals_->SendUpdateAndPurgeCache(
      FormatCacheKey(component_id), kAudioLogUpdateFunction, &dict);
}

void AudioLogImpl::OnError(int component_id) {
  SendSingleStringUpdate(component_id, "error_occurred", "true");
}

void AudioLogImpl::OnSetVolume(int component_id, double volume) {
  base::DictionaryValue dict;
  StoreComponentMetadata(component_id, &dict);
  dict.SetDouble("volume", volume);
  media_internals_->SendUpdateAndCache(
      FormatCacheKey(component_id), kAudioLogUpdateFunction, &dict);
}

std::string AudioLogImpl::FormatCacheKey(int component_id) {
  return base::StringPrintf("%d:%d:%d", owner_id_, component_, component_id);
}

void AudioLogImpl::SendSingleStringUpdate(int component_id,
                                          const std::string& key,
                                          const std::string& value) {
  base::DictionaryValue dict;
  StoreComponentMetadata(component_id, &dict);
  dict.SetString(key, value);
  media_internals_->SendUpdateAndCache(
      FormatCacheKey(component_id), kAudioLogUpdateFunction, &dict);
}

void AudioLogImpl::StoreComponentMetadata(int component_id,
                                          base::DictionaryValue* dict) {
  dict->SetInteger("owner_id", owner_id_);
  dict->SetInteger("component_id", component_id);
  dict->SetInteger("component_type", component_);
}

MediaInternals* MediaInternals::GetInstance() {
  return g_media_internals.Pointer();
}

MediaInternals::MediaInternals() : owner_ids_() {}
MediaInternals::~MediaInternals() {}

void MediaInternals::OnMediaEvents(
    int render_process_id, const std::vector<media::MediaLogEvent>& events) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Notify observers that |event| has occurred.
  for (std::vector<media::MediaLogEvent>::const_iterator event = events.begin();
       event != events.end(); ++event) {
    base::DictionaryValue dict;
    dict.SetInteger("renderer", render_process_id);
    dict.SetInteger("player", event->id);
    dict.SetString("type", media::MediaLog::EventTypeToString(event->type));

    // TODO(dalecurtis): This is technically not correct.  TimeTicks "can't" be
    // converted to to a human readable time format.  See base/time/time.h.
    const double ticks = event->time.ToInternalValue();
    const double ticks_millis = ticks / base::Time::kMicrosecondsPerMillisecond;
    dict.SetDouble("ticksMillis", ticks_millis);
    dict.Set("params", event->params.DeepCopy());
    SendUpdate(SerializeUpdate("media.onMediaEvent", &dict));
  }
}

void MediaInternals::AddUpdateCallback(const UpdateCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  update_callbacks_.push_back(callback);
}

void MediaInternals::RemoveUpdateCallback(const UpdateCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  for (size_t i = 0; i < update_callbacks_.size(); ++i) {
    if (update_callbacks_[i].Equals(callback)) {
      update_callbacks_.erase(update_callbacks_.begin() + i);
      return;
    }
  }
  NOTREACHED();
}

void MediaInternals::SendEverything() {
  base::string16 everything_update;
  {
    base::AutoLock auto_lock(lock_);
    everything_update = SerializeUpdate(
        "media.onReceiveEverything", &cached_data_);
  }
  SendUpdate(everything_update);
}

void MediaInternals::SendUpdate(const base::string16& update) {
  // SendUpdate() may be called from any thread, but must run on the IO thread.
  // TODO(dalecurtis): This is pretty silly since the update callbacks simply
  // forward the calls to the UI thread.  We should avoid the extra hop.
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
        &MediaInternals::SendUpdate, base::Unretained(this), update));
    return;
  }

  for (size_t i = 0; i < update_callbacks_.size(); i++)
    update_callbacks_[i].Run(update);
}

scoped_ptr<media::AudioLog> MediaInternals::CreateAudioLog(
    AudioComponent component) {
  base::AutoLock auto_lock(lock_);
  return scoped_ptr<media::AudioLog>(new AudioLogImpl(
      owner_ids_[component]++, component, this));
}

void MediaInternals::SendUpdateAndCache(const std::string& cache_key,
                                        const std::string& function,
                                        const base::DictionaryValue* value) {
  SendUpdate(SerializeUpdate(function, value));

  base::AutoLock auto_lock(lock_);
  if (!cached_data_.HasKey(cache_key)) {
    cached_data_.Set(cache_key, value->DeepCopy());
    return;
  }

  base::DictionaryValue* existing_dict = NULL;
  CHECK(cached_data_.GetDictionary(cache_key, &existing_dict));
  existing_dict->MergeDictionary(value);
}

void MediaInternals::SendUpdateAndPurgeCache(
    const std::string& cache_key,
    const std::string& function,
    const base::DictionaryValue* value) {
  SendUpdate(SerializeUpdate(function, value));

  base::AutoLock auto_lock(lock_);
  scoped_ptr<base::Value> out_value;
  CHECK(cached_data_.Remove(cache_key, &out_value));
}

}  // namespace content

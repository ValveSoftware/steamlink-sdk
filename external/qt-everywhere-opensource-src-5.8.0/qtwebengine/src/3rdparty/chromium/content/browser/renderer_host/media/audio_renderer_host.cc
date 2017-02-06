// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_renderer_host.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram.h"
#include "base/process/process.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/audio_sync_reader.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/media/audio_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_switches.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_streams_tracker.h"
#include "media/base/audio_bus.h"
#include "media/base/limits.h"

using media::AudioBus;
using media::AudioManager;

namespace content {

namespace {

// Tracks the maximum number of simultaneous output streams browser-wide.
// Accessed on IO thread.
base::LazyInstance<media::AudioStreamsTracker> g_audio_streams_tracker =
    LAZY_INSTANCE_INITIALIZER;

std::pair<int, std::pair<bool, std::string>> MakeAuthorizationData(
    int stream_id,
    bool authorized,
    const std::string& device_unique_id) {
  return std::make_pair(stream_id,
                        std::make_pair(authorized, device_unique_id));
}

bool IsValidDeviceId(const std::string& device_id) {
  static const std::string::size_type kValidLength = 64;

  if (device_id.empty() ||
      device_id == media::AudioDeviceDescription::kDefaultDeviceId ||
      device_id == media::AudioDeviceDescription::kCommunicationsDeviceId) {
    return true;
  }

  if (device_id.length() != kValidLength)
    return false;

  for (const char& c : device_id) {
    if ((c < 'a' || c > 'f') && (c < '0' || c > '9'))
      return false;
  }

  return true;
}

AudioOutputDeviceInfo GetDefaultDeviceInfoOnDeviceThread(
    media::AudioManager* audio_manager) {
  DCHECK(audio_manager->GetTaskRunner()->BelongsToCurrentThread());
  AudioOutputDeviceInfo default_device_info = {
      media::AudioDeviceDescription::kDefaultDeviceId,
      media::AudioDeviceDescription::GetDefaultDeviceName(),
      audio_manager->GetDefaultOutputStreamParameters()};

  return default_device_info;
}

void NotifyRenderProcessHostThatAudioStateChanged(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(render_process_id);

  if (render_process_host)
    render_process_host->AudioStateChanged();
}

void MaybeFixAudioParameters(media::AudioParameters* params) {
  // If the number of output channels is greater than the maximum, use the
  // maximum allowed value. Hardware channels are ignored upstream, so it is
  // better to report a valid value if this is the only problem.
  if (params->channels() > media::limits::kMaxChannels)
    params->set_channels_for_discrete(media::limits::kMaxChannels);

  // If hardware parameters are still invalid, use dummy parameters with
  // fake audio path and let the client handle the error.
  if (!params->IsValid())
    *params = media::AudioParameters::UnavailableDeviceParams();
}

void UMALogDeviceAuthorizationTime(base::TimeTicks auth_start_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES("Media.Audio.OutputDeviceAuthorizationTime",
                              base::TimeTicks::Now() - auth_start_time,
                              base::TimeDelta::FromMilliseconds(1),
                              base::TimeDelta::FromMilliseconds(5000), 50);
}

}  // namespace

class AudioRendererHost::AudioEntry
    : public media::AudioOutputController::EventHandler {
 public:
  AudioEntry(AudioRendererHost* host,
             int stream_id,
             int render_frame_id,
             const media::AudioParameters& params,
             const std::string& output_device_id,
             std::unique_ptr<base::SharedMemory> shared_memory,
             std::unique_ptr<media::AudioOutputController::SyncReader> reader);
  ~AudioEntry() override;

  int stream_id() const {
    return stream_id_;
  }

  int render_frame_id() const { return render_frame_id_; }

  media::AudioOutputController* controller() const { return controller_.get(); }

  base::SharedMemory* shared_memory() {
    return shared_memory_.get();
  }

  media::AudioOutputController::SyncReader* reader() const {
    return reader_.get();
  }

  bool playing() const { return playing_; }
  void set_playing(bool playing) { playing_ = playing; }

 private:
  // media::AudioOutputController::EventHandler implementation.
  void OnCreated() override;
  void OnPlaying() override;
  void OnPaused() override;
  void OnError() override;

  AudioRendererHost* const host_;
  const int stream_id_;

  // The routing ID of the source RenderFrame.
  const int render_frame_id_;

  // Shared memory for transmission of the audio data.  Used by |reader_|.
  const std::unique_ptr<base::SharedMemory> shared_memory_;

  // The synchronous reader to be used by |controller_|.
  const std::unique_ptr<media::AudioOutputController::SyncReader> reader_;

  // The AudioOutputController that manages the audio stream.
  const scoped_refptr<media::AudioOutputController> controller_;

  bool playing_;
};

AudioRendererHost::AudioEntry::AudioEntry(
    AudioRendererHost* host,
    int stream_id,
    int render_frame_id,
    const media::AudioParameters& params,
    const std::string& output_device_id,
    std::unique_ptr<base::SharedMemory> shared_memory,
    std::unique_ptr<media::AudioOutputController::SyncReader> reader)
    : host_(host),
      stream_id_(stream_id),
      render_frame_id_(render_frame_id),
      shared_memory_(std::move(shared_memory)),
      reader_(std::move(reader)),
      controller_(media::AudioOutputController::Create(host->audio_manager_,
                                                       this,
                                                       params,
                                                       output_device_id,
                                                       reader_.get())),
      playing_(false) {
  DCHECK(controller_.get());
}

AudioRendererHost::AudioEntry::~AudioEntry() {}

///////////////////////////////////////////////////////////////////////////////
// AudioRendererHost implementations.

AudioRendererHost::AudioRendererHost(int render_process_id,
                                     media::AudioManager* audio_manager,
                                     AudioMirroringManager* mirroring_manager,
                                     MediaInternals* media_internals,
                                     MediaStreamManager* media_stream_manager,
                                     const std::string& salt)
    : BrowserMessageFilter(AudioMsgStart),
      render_process_id_(render_process_id),
      audio_manager_(audio_manager),
      mirroring_manager_(mirroring_manager),
      audio_log_(media_internals->CreateAudioLog(
          media::AudioLogFactory::AUDIO_OUTPUT_CONTROLLER)),
      media_stream_manager_(media_stream_manager),
      num_playing_streams_(0),
      salt_(salt),
      max_simultaneous_streams_(0) {
  DCHECK(audio_manager_);
  DCHECK(media_stream_manager_);
}

AudioRendererHost::~AudioRendererHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  CHECK(audio_entries_.empty());

  // If we had any streams, report UMA stats for the maximum number of
  // simultaneous streams for this render process and for the whole browser
  // process since last reported.
  if (max_simultaneous_streams_ > 0) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Media.AudioRendererIpcStreams",
                                max_simultaneous_streams_, 1, 50, 51);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Media.AudioRendererIpcStreamsTotal",
        g_audio_streams_tracker.Get().max_stream_count(),
        1, 100, 101);
    g_audio_streams_tracker.Get().ResetMaxStreamCount();
  }
}

void AudioRendererHost::GetOutputControllers(
    const RenderProcessHost::GetAudioOutputControllersCallback&
        callback) const {
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AudioRendererHost::DoGetOutputControllers, this), callback);
}

void AudioRendererHost::OnChannelClosing() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Since the IPC sender is gone, close all requested audio streams.
  while (!audio_entries_.empty()) {
    // Note: OnCloseStream() removes the entries from audio_entries_.
    OnCloseStream(audio_entries_.begin()->first);
  }

  // Remove any authorizations for streams that were not yet created
  authorizations_.clear();
}

void AudioRendererHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void AudioRendererHost::AudioEntry::OnCreated() {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&AudioRendererHost::DoCompleteCreation, host_, stream_id_));
}

void AudioRendererHost::AudioEntry::OnPlaying() {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&AudioRendererHost::DoNotifyStreamStateChanged,
                 host_,
                 stream_id_,
                 true));
}

void AudioRendererHost::AudioEntry::OnPaused() {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&AudioRendererHost::DoNotifyStreamStateChanged,
                 host_,
                 stream_id_,
                 false));
}

void AudioRendererHost::AudioEntry::OnError() {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&AudioRendererHost::ReportErrorAndClose, host_, stream_id_));
}

void AudioRendererHost::DoCompleteCreation(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!PeerHandle()) {
    DLOG(WARNING) << "Renderer process handle is invalid.";
    ReportErrorAndClose(stream_id);
    return;
  }

  AudioEntry* const entry = LookupById(stream_id);
  if (!entry) {
    ReportErrorAndClose(stream_id);
    return;
  }

  // Once the audio stream is created then complete the creation process by
  // mapping shared memory and sharing with the renderer process.
  base::SharedMemoryHandle foreign_memory_handle;
  if (!entry->shared_memory()->ShareToProcess(PeerHandle(),
                                              &foreign_memory_handle)) {
    // If we failed to map and share the shared memory then close the audio
    // stream and send an error message.
    ReportErrorAndClose(entry->stream_id());
    return;
  }

  AudioSyncReader* reader = static_cast<AudioSyncReader*>(entry->reader());

  base::SyncSocket::TransitDescriptor socket_descriptor;

  // If we failed to prepare the sync socket for the renderer then we fail
  // the construction of audio stream.
  if (!reader->PrepareForeignSocket(PeerHandle(), &socket_descriptor)) {
    ReportErrorAndClose(entry->stream_id());
    return;
  }

  Send(new AudioMsg_NotifyStreamCreated(
      entry->stream_id(), foreign_memory_handle, socket_descriptor,
      entry->shared_memory()->requested_size()));
}

void AudioRendererHost::DoNotifyStreamStateChanged(int stream_id,
                                                   bool is_playing) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AudioEntry* const entry = LookupById(stream_id);
  if (!entry)
    return;

  Send(new AudioMsg_NotifyStreamStateChanged(
      stream_id,
      is_playing ? media::AUDIO_OUTPUT_IPC_DELEGATE_STATE_PLAYING
                 : media::AUDIO_OUTPUT_IPC_DELEGATE_STATE_PAUSED));

  if (is_playing) {
    AudioStreamMonitor::StartMonitoringStream(
        render_process_id_,
        entry->render_frame_id(),
        entry->stream_id(),
        base::Bind(&media::AudioOutputController::ReadCurrentPowerAndClip,
                   entry->controller()));
  } else {
    AudioStreamMonitor::StopMonitoringStream(
        render_process_id_, entry->render_frame_id(), entry->stream_id());
  }
  UpdateNumPlayingStreams(entry, is_playing);
}

RenderProcessHost::AudioOutputControllerList
AudioRendererHost::DoGetOutputControllers() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RenderProcessHost::AudioOutputControllerList controllers;
  for (AudioEntryMap::const_iterator it = audio_entries_.begin();
       it != audio_entries_.end();
       ++it) {
    controllers.push_back(it->second->controller());
  }

  return controllers;
}

///////////////////////////////////////////////////////////////////////////////
// IPC Messages handler
bool AudioRendererHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AudioRendererHost, message)
    IPC_MESSAGE_HANDLER(AudioHostMsg_RequestDeviceAuthorization,
                        OnRequestDeviceAuthorization)
    IPC_MESSAGE_HANDLER(AudioHostMsg_CreateStream, OnCreateStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_PlayStream, OnPlayStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_PauseStream, OnPauseStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_CloseStream, OnCloseStream)
    IPC_MESSAGE_HANDLER(AudioHostMsg_SetVolume, OnSetVolume)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void AudioRendererHost::OnRequestDeviceAuthorization(
    int stream_id,
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const base::TimeTicks auth_start_time = base::TimeTicks::Now();

  DVLOG(1) << "AudioRendererHost@" << this << "::OnRequestDeviceAuthorization"
           << "(stream_id=" << stream_id
           << ", render_frame_id=" << render_frame_id
           << ", session_id=" << session_id << ", device_id=" << device_id
           << ", security_origin=" << security_origin << ")";

  if (LookupById(stream_id) || IsAuthorizationStarted(stream_id))
    return;

  if (!IsValidDeviceId(device_id)) {
    UMALogDeviceAuthorizationTime(auth_start_time);
    Send(new AudioMsg_NotifyDeviceAuthorized(
        stream_id, media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND,
        media::AudioParameters::UnavailableDeviceParams(), std::string()));
    return;
  }

  // If |session_id should be used for output device selection and such output
  // device is found, reuse the input device permissions.
  if (media::AudioDeviceDescription::UseSessionIdToSelectDevice(session_id,
                                                                device_id)) {
    const StreamDeviceInfo* info =
        media_stream_manager_->audio_input_device_manager()
            ->GetOpenedDeviceInfoById(session_id);
    if (info) {
      media::AudioParameters output_params(
          media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
          static_cast<media::ChannelLayout>(
              info->device.matched_output.channel_layout),
          info->device.matched_output.sample_rate, 16,
          info->device.matched_output.frames_per_buffer);
      output_params.set_effects(info->device.matched_output.effects);
      authorizations_.insert(MakeAuthorizationData(
          stream_id, true, info->device.matched_output_device_id));
      MaybeFixAudioParameters(&output_params);
      UMALogDeviceAuthorizationTime(auth_start_time);
      // Hash matched device id and pass it to the renderer
      Send(new AudioMsg_NotifyDeviceAuthorized(
          stream_id, media::OUTPUT_DEVICE_STATUS_OK, output_params,
          GetHMACForMediaDeviceID(salt_, security_origin,
                                  info->device.matched_output_device_id)));
      return;
    }
  }

  authorizations_.insert(
      MakeAuthorizationData(stream_id, false, std::string()));
  CheckOutputDeviceAccess(
      render_frame_id, device_id, security_origin,
      base::Bind(&AudioRendererHost::OnDeviceAuthorized, this, stream_id,
                 device_id, security_origin, auth_start_time));
}

void AudioRendererHost::OnDeviceAuthorized(int stream_id,
                                           const std::string& device_id,
                                           const url::Origin& security_origin,
                                           base::TimeTicks auth_start_time,
                                           bool have_access) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const auto& auth_data = authorizations_.find(stream_id);

  // A close request was received while access check was in progress.
  if (auth_data == authorizations_.end()) {
    UMALogDeviceAuthorizationTime(auth_start_time);
    return;
  }

  if (!have_access) {
    authorizations_.erase(auth_data);
    UMALogDeviceAuthorizationTime(auth_start_time);
    Send(new AudioMsg_NotifyDeviceAuthorized(
        stream_id, media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED,
        media::AudioParameters::UnavailableDeviceParams(), std::string()));
    return;
  }

  // If enumerator caching is disabled, avoid the enumeration if the default
  // device is requested, since no device ID translation is needed.
  // If enumerator caching is enabled, it is better to use its cache, even
  // for the default device.
  if (media::AudioDeviceDescription::IsDefaultDevice(device_id) &&
      !media_stream_manager_->audio_output_device_enumerator()
           ->IsCacheEnabled()) {
    base::PostTaskAndReplyWithResult(
        audio_manager_->GetTaskRunner(), FROM_HERE,
        base::Bind(&GetDefaultDeviceInfoOnDeviceThread, audio_manager_),
        base::Bind(&AudioRendererHost::OnDeviceIDTranslated, this, stream_id,
                   auth_start_time, true));
  } else {
    media_stream_manager_->audio_output_device_enumerator()->Enumerate(
        base::Bind(&AudioRendererHost::TranslateDeviceID, this, device_id,
                   security_origin,
                   base::Bind(&AudioRendererHost::OnDeviceIDTranslated, this,
                              stream_id, auth_start_time)));
  }
}

void AudioRendererHost::OnDeviceIDTranslated(
    int stream_id,
    base::TimeTicks auth_start_time,
    bool device_found,
    const AudioOutputDeviceInfo& device_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const auto& auth_data = authorizations_.find(stream_id);

  // A close request was received while translation was in progress
  if (auth_data == authorizations_.end()) {
    UMALogDeviceAuthorizationTime(auth_start_time);
    return;
  }

  if (!device_found) {
    authorizations_.erase(auth_data);
    UMALogDeviceAuthorizationTime(auth_start_time);
    Send(new AudioMsg_NotifyDeviceAuthorized(
        stream_id, media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND,
        media::AudioParameters::UnavailableDeviceParams(), std::string()));
    return;
  }

  auth_data->second.first = true;
  auth_data->second.second = device_info.unique_id;

  media::AudioParameters output_params = device_info.output_params;
  MaybeFixAudioParameters(&output_params);
  UMALogDeviceAuthorizationTime(auth_start_time);
  Send(new AudioMsg_NotifyDeviceAuthorized(
      stream_id, media::OUTPUT_DEVICE_STATUS_OK, output_params, std::string()));
}

void AudioRendererHost::OnCreateStream(int stream_id,
                                       int render_frame_id,
                                       const media::AudioParameters& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "AudioRendererHost@" << this << "::OnCreateStream"
           << "(stream_id=" << stream_id << ")";

  const auto& auth_data = authorizations_.find(stream_id);

  // If no previous authorization requested, assume default device
  if (auth_data == authorizations_.end()) {
    DoCreateStream(stream_id, render_frame_id, params, std::string());
    return;
  }

  CHECK(auth_data->second.first);
  DoCreateStream(stream_id, render_frame_id, params, auth_data->second.second);
  authorizations_.erase(auth_data);
}

void AudioRendererHost::DoCreateStream(int stream_id,
                                       int render_frame_id,
                                       const media::AudioParameters& params,
                                       const std::string& device_unique_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // media::AudioParameters is validated in the deserializer.
  if (LookupById(stream_id) != NULL) {
    SendErrorMessage(stream_id);
    return;
  }

  // Create the shared memory and share with the renderer process.
  uint32_t shared_memory_size = sizeof(media::AudioOutputBufferParameters) +
                                AudioBus::CalculateMemorySize(params);
  std::unique_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());
  if (!shared_memory->CreateAndMapAnonymous(shared_memory_size)) {
    SendErrorMessage(stream_id);
    return;
  }

  std::unique_ptr<AudioSyncReader> reader(
      new AudioSyncReader(shared_memory.get(), params));
  if (!reader->Init()) {
    SendErrorMessage(stream_id);
    return;
  }

  MediaObserver* const media_observer =
      GetContentClient()->browser()->GetMediaObserver();
  if (media_observer)
    media_observer->OnCreatingAudioStream(render_process_id_, render_frame_id);

  std::unique_ptr<AudioEntry> entry(
      new AudioEntry(this, stream_id, render_frame_id, params, device_unique_id,
                     std::move(shared_memory), std::move(reader)));
  if (mirroring_manager_) {
    mirroring_manager_->AddDiverter(
        render_process_id_, entry->render_frame_id(), entry->controller());
  }
  audio_entries_.insert(std::make_pair(stream_id, entry.release()));
  g_audio_streams_tracker.Get().IncreaseStreamCount();

  audio_log_->OnCreated(stream_id, params, device_unique_id);
  MediaInternals::GetInstance()->SetWebContentsTitleForAudioLogEntry(
      stream_id, render_process_id_, render_frame_id, audio_log_.get());

  if (audio_entries_.size() > max_simultaneous_streams_)
    max_simultaneous_streams_ = audio_entries_.size();
}

void AudioRendererHost::OnPlayStream(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  entry->controller()->Play();
  audio_log_->OnStarted(stream_id);
}

void AudioRendererHost::OnPauseStream(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  entry->controller()->Pause();
  audio_log_->OnStopped(stream_id);
}

void AudioRendererHost::OnSetVolume(int stream_id, double volume) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AudioEntry* entry = LookupById(stream_id);
  if (!entry) {
    SendErrorMessage(stream_id);
    return;
  }

  // Make sure the volume is valid.
  if (volume < 0 || volume > 1.0)
    return;
  entry->controller()->SetVolume(volume);
  audio_log_->OnSetVolume(stream_id, volume);
}

void AudioRendererHost::SendErrorMessage(int stream_id) {
  Send(new AudioMsg_NotifyStreamStateChanged(
      stream_id, media::AUDIO_OUTPUT_IPC_DELEGATE_STATE_ERROR));
}

void AudioRendererHost::OnCloseStream(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  authorizations_.erase(stream_id);

  // Prevent oustanding callbacks from attempting to close/delete the same
  // AudioEntry twice.
  AudioEntryMap::iterator i = audio_entries_.find(stream_id);
  if (i == audio_entries_.end())
    return;
  std::unique_ptr<AudioEntry> entry(i->second);
  audio_entries_.erase(i);
  g_audio_streams_tracker.Get().DecreaseStreamCount();

  media::AudioOutputController* const controller = entry->controller();
  controller->Close(
      base::Bind(&AudioRendererHost::DeleteEntry, this, base::Passed(&entry)));
  audio_log_->OnClosed(stream_id);
}

void AudioRendererHost::DeleteEntry(std::unique_ptr<AudioEntry> entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // De-register the controller from the AudioMirroringManager now that the
  // controller has closed the AudioOutputStream and shut itself down.  This
  // ensures that calling RemoveDiverter() here won't trigger the controller to
  // re-start the default AudioOutputStream and cause a brief audio blip to come
  // out the user's speakers.  http://crbug.com/474432
  if (mirroring_manager_)
    mirroring_manager_->RemoveDiverter(entry->controller());

  AudioStreamMonitor::StopMonitoringStream(
      render_process_id_, entry->render_frame_id(), entry->stream_id());
  UpdateNumPlayingStreams(entry.get(), false);
}

void AudioRendererHost::ReportErrorAndClose(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Make sure this isn't a stray callback executing after the stream has been
  // closed, so error notifications aren't sent after clients believe the stream
  // is closed.
  if (!LookupById(stream_id))
    return;

  SendErrorMessage(stream_id);

  audio_log_->OnError(stream_id);
  OnCloseStream(stream_id);
}

AudioRendererHost::AudioEntry* AudioRendererHost::LookupById(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AudioEntryMap::const_iterator i = audio_entries_.find(stream_id);
  return i != audio_entries_.end() ? i->second : NULL;
}

void AudioRendererHost::UpdateNumPlayingStreams(AudioEntry* entry,
                                                bool is_playing) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (entry->playing() == is_playing)
    return;

  if (is_playing) {
    entry->set_playing(true);
    base::AtomicRefCountInc(&num_playing_streams_);

    // Inform the RenderProcessHost when audio starts playing for the first
    // time.
    if (base::AtomicRefCountIsOne(&num_playing_streams_)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&NotifyRenderProcessHostThatAudioStateChanged,
                     render_process_id_));
    }
  } else {
    entry->set_playing(false);
    // Inform the RenderProcessHost when there is no more audio playing.
    if (!base::AtomicRefCountDec(&num_playing_streams_)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&NotifyRenderProcessHostThatAudioStateChanged,
                     render_process_id_));
    }
  }
}

bool AudioRendererHost::HasActiveAudio() {
  return !base::AtomicRefCountIsZero(&num_playing_streams_);
}

bool AudioRendererHost::RenderFrameHasActiveAudio(int render_frame_id) const {
  for (AudioEntryMap::const_iterator it = audio_entries_.begin();
       it != audio_entries_.end();
       ++it) {
    AudioEntry* entry = it->second;
    if (entry->render_frame_id() == render_frame_id && entry->playing())
      return true;
  }
  return false;
}

void AudioRendererHost::CheckOutputDeviceAccess(
    int render_frame_id,
    const std::string& device_id,
    const url::Origin& security_origin,
    const OutputDeviceAccessCB& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Check security origin if nondefault device is requested.
  // Ignore check for default device, which is always authorized.
  if (!media::AudioDeviceDescription::IsDefaultDevice(device_id) &&
      !MediaStreamManager::IsOriginAllowed(render_process_id_,
                                           security_origin)) {
    content::bad_message::ReceivedBadMessage(this,
                                             bad_message::ARH_UNAUTHORIZED_URL);
    return;
  }

  if (device_id.empty()) {
    callback.Run(true);
  } else {
    // Check that MediaStream device permissions have been granted,
    // hence the use of a MediaStreamUIProxy.
    std::unique_ptr<MediaStreamUIProxy> ui_proxy = MediaStreamUIProxy::Create();

    // Use MEDIA_DEVICE_AUDIO_CAPTURE instead of MEDIA_DEVICE_AUDIO_OUTPUT
    // because MediaStreamUIProxy::CheckAccess does not currently support
    // MEDIA_DEVICE_AUDIO_OUTPUT.
    // TODO(guidou): Change to MEDIA_DEVICE_AUDIO_OUTPUT when support becomes
    // available. http://crbug.com/498675
    ui_proxy->CheckAccess(security_origin, MEDIA_DEVICE_AUDIO_CAPTURE,
                          render_process_id_, render_frame_id,
                          base::Bind(&AudioRendererHost::AccessChecked, this,
                                     base::Passed(&ui_proxy), callback));
  }
}

void AudioRendererHost::AccessChecked(
    std::unique_ptr<MediaStreamUIProxy> ui_proxy,
    const OutputDeviceAccessCB& callback,
    bool have_access) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(have_access);
}

void AudioRendererHost::TranslateDeviceID(
    const std::string& device_id,
    const url::Origin& security_origin,
    const OutputDeviceInfoCB& callback,
    const AudioOutputDeviceEnumeration& enumeration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const AudioOutputDeviceInfo& device_info : enumeration.devices) {
    if (device_id.empty()) {
      if (device_info.unique_id ==
          media::AudioDeviceDescription::kDefaultDeviceId) {
        callback.Run(true, device_info);
        return;
      }
    } else if (content::DoesMediaDeviceIDMatchHMAC(
                   salt_, security_origin, device_id, device_info.unique_id)) {
      callback.Run(true, device_info);
      return;
    }
  }
  DCHECK(!device_id.empty());  // Default device must always be found
  AudioOutputDeviceInfo device_info = {
      std::string(), std::string(),
      media::AudioParameters::UnavailableDeviceParams()};
  callback.Run(false, device_info);
}

bool AudioRendererHost::IsAuthorizationStarted(int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const auto& i = authorizations_.find(stream_id);
  return i != authorizations_.end();
}

}  // namespace content

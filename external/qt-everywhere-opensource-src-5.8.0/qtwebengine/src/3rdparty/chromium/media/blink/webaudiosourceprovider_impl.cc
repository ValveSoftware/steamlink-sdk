// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webaudiosourceprovider_impl.h"

#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "media/base/bind_to_current_loop.h"
#include "third_party/WebKit/public/platform/WebAudioSourceProviderClient.h"

using blink::WebVector;

namespace media {

namespace {

// Simple helper class for Try() locks.  Lock is Try()'d on construction and
// must be checked via the locked() attribute.  If acquisition was successful
// the lock will be released upon destruction.
// TODO(dalecurtis): This should probably move to base/ if others start using
// this pattern.
class AutoTryLock {
 public:
  explicit AutoTryLock(base::Lock& lock)
      : lock_(lock),
        acquired_(lock_.Try()) {}

  bool locked() const { return acquired_; }

  ~AutoTryLock() {
    if (acquired_) {
      lock_.AssertAcquired();
      lock_.Release();
    }
  }

 private:
  base::Lock& lock_;
  const bool acquired_;
  DISALLOW_COPY_AND_ASSIGN(AutoTryLock);
};

}  // namespace

// TeeFilter is a RenderCallback implementation that allows for a client to get
// a copy of the data being rendered by the |renderer_| on Render(). This class
// also holds on to the necessary audio parameters.
class WebAudioSourceProviderImpl::TeeFilter
    : public AudioRendererSink::RenderCallback {
 public:
  TeeFilter() : renderer_(nullptr), channels_(0), sample_rate_(0) {}
  ~TeeFilter() override {}

  void Initialize(AudioRendererSink::RenderCallback* renderer,
                  int channels,
                  int sample_rate) {
    DCHECK(renderer);
    renderer_ = renderer;
    channels_ = channels;
    sample_rate_ = sample_rate;
  }

  // AudioRendererSink::RenderCallback implementation.
  // These are forwarders to |renderer_| and are here to allow for a client to
  // get a copy of the rendered audio by SetCopyAudioCallback().
  int Render(AudioBus* audio_bus,
             uint32_t frames_delayed,
             uint32_t frames_skipped) override;
  void OnRenderError() override;

  bool IsInitialized() const { return !!renderer_; }
  int channels() const { return channels_; }
  int sample_rate() const { return sample_rate_; }
  void set_copy_audio_bus_callback(const CopyAudioCB& callback) {
    copy_audio_bus_callback_ = callback;
  }

 private:
  AudioRendererSink::RenderCallback* renderer_;
  int channels_;
  int sample_rate_;

  CopyAudioCB copy_audio_bus_callback_;

  DISALLOW_COPY_AND_ASSIGN(TeeFilter);
};

WebAudioSourceProviderImpl::WebAudioSourceProviderImpl(
    const scoped_refptr<SwitchableAudioRendererSink>& sink)
    : volume_(1.0),
      state_(kStopped),
      client_(nullptr),
      sink_(sink),
      tee_filter_(new TeeFilter()),
      weak_factory_(this) {}

WebAudioSourceProviderImpl::~WebAudioSourceProviderImpl() {
}

void WebAudioSourceProviderImpl::setClient(
    blink::WebAudioSourceProviderClient* client) {
  base::AutoLock auto_lock(sink_lock_);
  if (client && client != client_) {
    // Detach the audio renderer from normal playback.
    sink_->Stop();

    // The client will now take control by calling provideInput() periodically.
    client_ = client;

    set_format_cb_ = BindToCurrentLoop(base::Bind(
        &WebAudioSourceProviderImpl::OnSetFormat, weak_factory_.GetWeakPtr()));

    // If |tee_filter_| is Initialize()d - then run |set_format_cb_| to send
    // |client_| the current format info. Otherwise |set_format_cb_| will get
    // called when Initialize() is called. Note: Always using |set_format_cb_|
    // ensures we have the same locking order when calling into |client_|.
    if (tee_filter_->IsInitialized())
      base::ResetAndReturn(&set_format_cb_).Run();
  } else if (!client && client_) {
    // Restore normal playback.
    client_ = nullptr;
    sink_->SetVolume(volume_);
    if (state_ >= kStarted)
      sink_->Start();
    if (state_ >= kPlaying)
      sink_->Play();
  }
}

void WebAudioSourceProviderImpl::provideInput(
    const WebVector<float*>& audio_data, size_t number_of_frames) {
  if (!bus_wrapper_ ||
      static_cast<size_t>(bus_wrapper_->channels()) != audio_data.size()) {
    bus_wrapper_ = AudioBus::CreateWrapper(static_cast<int>(audio_data.size()));
  }

  const int incoming_number_of_frames = static_cast<int>(number_of_frames);
  bus_wrapper_->set_frames(incoming_number_of_frames);
  for (size_t i = 0; i < audio_data.size(); ++i)
    bus_wrapper_->SetChannelData(static_cast<int>(i), audio_data[i]);

  // Use a try lock to avoid contention in the real-time audio thread.
  AutoTryLock auto_try_lock(sink_lock_);
  if (!auto_try_lock.locked() || state_ != kPlaying) {
    // Provide silence if we failed to acquire the lock or the source is not
    // running.
    bus_wrapper_->Zero();
    return;
  }

  DCHECK(client_);
  DCHECK_EQ(tee_filter_->channels(), bus_wrapper_->channels());
  const int frames = tee_filter_->Render(bus_wrapper_.get(), 0, 0);
  if (frames < incoming_number_of_frames)
    bus_wrapper_->ZeroFramesPartial(frames, incoming_number_of_frames - frames);

  bus_wrapper_->Scale(volume_);
}

void WebAudioSourceProviderImpl::Initialize(const AudioParameters& params,
                                            RenderCallback* renderer) {
  base::AutoLock auto_lock(sink_lock_);
  DCHECK_EQ(state_, kStopped);

  tee_filter_->Initialize(renderer, params.channels(), params.sample_rate());

  sink_->Initialize(params, tee_filter_.get());

  if (!set_format_cb_.is_null())
    base::ResetAndReturn(&set_format_cb_).Run();
}

void WebAudioSourceProviderImpl::Start() {
  base::AutoLock auto_lock(sink_lock_);
  DCHECK(tee_filter_);
  DCHECK_EQ(state_, kStopped);
  state_ = kStarted;
  if (!client_)
    sink_->Start();
}

void WebAudioSourceProviderImpl::Stop() {
  base::AutoLock auto_lock(sink_lock_);
  state_ = kStopped;
  if (!client_)
    sink_->Stop();
}

void WebAudioSourceProviderImpl::Play() {
  base::AutoLock auto_lock(sink_lock_);
  DCHECK_EQ(state_, kStarted);
  state_ = kPlaying;
  if (!client_)
    sink_->Play();
}

void WebAudioSourceProviderImpl::Pause() {
  base::AutoLock auto_lock(sink_lock_);
  DCHECK(state_ == kPlaying || state_ == kStarted);
  state_ = kStarted;
  if (!client_)
    sink_->Pause();
}

bool WebAudioSourceProviderImpl::SetVolume(double volume) {
  base::AutoLock auto_lock(sink_lock_);
  volume_ = volume;
  if (!client_)
    sink_->SetVolume(volume);
  return true;
}

media::OutputDeviceInfo WebAudioSourceProviderImpl::GetOutputDeviceInfo() {
  base::AutoLock auto_lock(sink_lock_);
  return sink_->GetOutputDeviceInfo();
}

bool WebAudioSourceProviderImpl::CurrentThreadIsRenderingThread() {
  NOTIMPLEMENTED();
  return false;
}

void WebAudioSourceProviderImpl::SwitchOutputDevice(
    const std::string& device_id,
    const url::Origin& security_origin,
    const OutputDeviceStatusCB& callback) {
  base::AutoLock auto_lock(sink_lock_);
  if (client_)
    callback.Run(media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
  else
    sink_->SwitchOutputDevice(device_id, security_origin, callback);
}

void WebAudioSourceProviderImpl::SetCopyAudioCallback(
    const CopyAudioCB& callback) {
  DCHECK(!callback.is_null());

  // Use |sink_lock_| to protect |tee_filter_| too since they go in lockstep.
  base::AutoLock auto_lock(sink_lock_);

  DCHECK(tee_filter_);
  tee_filter_->set_copy_audio_bus_callback(callback);
}

void WebAudioSourceProviderImpl::ClearCopyAudioCallback() {
  DCHECK(tee_filter_);
  tee_filter_->set_copy_audio_bus_callback(CopyAudioCB());
}

int WebAudioSourceProviderImpl::RenderForTesting(AudioBus* audio_bus) {
  return tee_filter_->Render(audio_bus, 0, 0);
}

void WebAudioSourceProviderImpl::OnSetFormat() {
  base::AutoLock auto_lock(sink_lock_);
  if (!client_)
    return;

  // Inform Blink about the audio stream format.
  client_->setFormat(tee_filter_->channels(), tee_filter_->sample_rate());
}

int WebAudioSourceProviderImpl::TeeFilter::Render(AudioBus* audio_bus,
                                                  uint32_t frames_delayed,
                                                  uint32_t frames_skipped) {
  DCHECK(IsInitialized());

  const int num_rendered_frames =
      renderer_->Render(audio_bus, frames_delayed, frames_skipped);

  if (!copy_audio_bus_callback_.is_null()) {
    std::unique_ptr<AudioBus> bus_copy =
        AudioBus::Create(audio_bus->channels(), audio_bus->frames());
    audio_bus->CopyTo(bus_copy.get());
    copy_audio_bus_callback_.Run(std::move(bus_copy), frames_delayed,
                                 sample_rate_);
  }

  return num_rendered_frames;
}

void WebAudioSourceProviderImpl::TeeFilter::OnRenderError() {
  DCHECK(IsInitialized());
  renderer_->OnRenderError();
}

}  // namespace media

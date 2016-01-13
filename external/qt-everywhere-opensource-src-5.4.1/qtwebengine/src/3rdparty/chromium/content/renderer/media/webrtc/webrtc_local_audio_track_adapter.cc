// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"

#include "base/logging.h"
#include "content/renderer/media/media_stream_audio_processor.h"
#include "content/renderer/media/webrtc/webrtc_audio_sink_adapter.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {

static const char kAudioTrackKind[] = "audio";

scoped_refptr<WebRtcLocalAudioTrackAdapter>
WebRtcLocalAudioTrackAdapter::Create(
    const std::string& label,
    webrtc::AudioSourceInterface* track_source) {
  talk_base::RefCountedObject<WebRtcLocalAudioTrackAdapter>* adapter =
      new talk_base::RefCountedObject<WebRtcLocalAudioTrackAdapter>(
          label, track_source);
  return adapter;
}

WebRtcLocalAudioTrackAdapter::WebRtcLocalAudioTrackAdapter(
    const std::string& label,
    webrtc::AudioSourceInterface* track_source)
    : webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>(label),
      owner_(NULL),
      track_source_(track_source),
      signal_level_(0) {
}

WebRtcLocalAudioTrackAdapter::~WebRtcLocalAudioTrackAdapter() {
}

void WebRtcLocalAudioTrackAdapter::Initialize(WebRtcLocalAudioTrack* owner) {
  DCHECK(!owner_);
  DCHECK(owner);
  owner_ = owner;
}

void WebRtcLocalAudioTrackAdapter::SetAudioProcessor(
    const scoped_refptr<MediaStreamAudioProcessor>& processor) {
  base::AutoLock auto_lock(lock_);
  audio_processor_ = processor;
}

std::string WebRtcLocalAudioTrackAdapter::kind() const {
  return kAudioTrackKind;
}

void WebRtcLocalAudioTrackAdapter::AddSink(
    webrtc::AudioTrackSinkInterface* sink) {
  DCHECK(sink);
#ifndef NDEBUG
  // Verify that |sink| has not been added.
  for (ScopedVector<WebRtcAudioSinkAdapter>::const_iterator it =
           sink_adapters_.begin();
       it != sink_adapters_.end(); ++it) {
    DCHECK(!(*it)->IsEqual(sink));
  }
#endif

  scoped_ptr<WebRtcAudioSinkAdapter> adapter(
      new WebRtcAudioSinkAdapter(sink));
  owner_->AddSink(adapter.get());
  sink_adapters_.push_back(adapter.release());
}

void WebRtcLocalAudioTrackAdapter::RemoveSink(
    webrtc::AudioTrackSinkInterface* sink) {
  DCHECK(sink);
  for (ScopedVector<WebRtcAudioSinkAdapter>::iterator it =
           sink_adapters_.begin();
       it != sink_adapters_.end(); ++it) {
    if ((*it)->IsEqual(sink)) {
      owner_->RemoveSink(*it);
      sink_adapters_.erase(it);
      return;
    }
  }
}

bool WebRtcLocalAudioTrackAdapter::GetSignalLevel(int* level) {
  base::AutoLock auto_lock(lock_);
  // It is required to provide the signal level after audio processing. In
  // case the audio processing is not enabled for the track, we return
  // false here in order not to overwrite the value from WebRTC.
  // TODO(xians): Remove this after we turn on the APM in Chrome by default.
  // http://crbug/365672 .
  if (!MediaStreamAudioProcessor::IsAudioTrackProcessingEnabled())
    return false;

  *level = signal_level_;
  return true;
}

talk_base::scoped_refptr<webrtc::AudioProcessorInterface>
WebRtcLocalAudioTrackAdapter::GetAudioProcessor() {
  base::AutoLock auto_lock(lock_);
  return audio_processor_.get();
}

std::vector<int> WebRtcLocalAudioTrackAdapter::VoeChannels() const {
  base::AutoLock auto_lock(lock_);
  return voe_channels_;
}

void WebRtcLocalAudioTrackAdapter::SetSignalLevel(int signal_level) {
  base::AutoLock auto_lock(lock_);
  signal_level_ = signal_level;
}

void WebRtcLocalAudioTrackAdapter::AddChannel(int channel_id) {
  DVLOG(1) << "WebRtcLocalAudioTrack::AddChannel(channel_id="
           << channel_id << ")";
  base::AutoLock auto_lock(lock_);
  if (std::find(voe_channels_.begin(), voe_channels_.end(), channel_id) !=
      voe_channels_.end()) {
    // We need to handle the case when the same channel is connected to the
    // track more than once.
    return;
  }

  voe_channels_.push_back(channel_id);
}

void WebRtcLocalAudioTrackAdapter::RemoveChannel(int channel_id) {
  DVLOG(1) << "WebRtcLocalAudioTrack::RemoveChannel(channel_id="
           << channel_id << ")";
  base::AutoLock auto_lock(lock_);
  std::vector<int>::iterator iter =
      std::find(voe_channels_.begin(), voe_channels_.end(), channel_id);
  DCHECK(iter != voe_channels_.end());
  voe_channels_.erase(iter);
}

webrtc::AudioSourceInterface* WebRtcLocalAudioTrackAdapter::GetSource() const {
  return track_source_;
}

cricket::AudioRenderer* WebRtcLocalAudioTrackAdapter::GetRenderer() {
  // When the audio track processing is enabled, return a NULL so that capture
  // data goes through Libjingle LocalAudioTrackHandler::LocalAudioSinkAdapter
  // ==> WebRtcVoiceMediaChannel::WebRtcVoiceChannelRenderer ==> WebRTC.
  // When the audio track processing is disabled, WebRtcLocalAudioTrackAdapter
  // is used to pass the channel ids to WebRtcAudioDeviceImpl, the data flow
  // becomes WebRtcAudioDeviceImpl ==> WebRTC.
  // TODO(xians): Only return NULL after the APM in WebRTC is deprecated.
  // See See http://crbug/365672 for details.
  return MediaStreamAudioProcessor::IsAudioTrackProcessingEnabled()?
      NULL : this;
}

}  // namespace content

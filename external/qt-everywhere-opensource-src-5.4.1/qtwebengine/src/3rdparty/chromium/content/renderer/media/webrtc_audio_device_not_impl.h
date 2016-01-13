// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_DEVICE_NOT_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_DEVICE_NOT_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/webrtc/modules/audio_device/include/audio_device.h"

namespace content {

// WebRtcAudioDeviceNotImpl contains default implementations of all methods
// in the webrtc::AudioDeviceModule which are currently not supported in Chrome.
// The real implementation is in WebRtcAudioDeviceImpl and it derives from
// this class. The main purpose of breaking out non-implemented methods into
// a separate unit is to make WebRtcAudioDeviceImpl more readable and easier
// to maintain.
class CONTENT_EXPORT WebRtcAudioDeviceNotImpl
    : NON_EXPORTED_BASE(public webrtc::AudioDeviceModule) {
 public:
  WebRtcAudioDeviceNotImpl();

  // webrtc::Module implementation.
  // TODO(henrika): it is possible to add functionality in these methods.
  // Only adding very basic support for now without triggering any callback
  // in the webrtc::AudioDeviceObserver interface.
  virtual int32_t ChangeUniqueId(const int32_t id) OVERRIDE;
  virtual int32_t TimeUntilNextProcess() OVERRIDE;
  virtual int32_t Process() OVERRIDE;

  // Methods in webrtc::AudioDeviceModule which are not yet implemented.
  // The idea is that we can move methods from this class to the real
  // implementation in WebRtcAudioDeviceImpl when needed.

  virtual int32_t RegisterEventObserver(
      webrtc::AudioDeviceObserver* event_callback) OVERRIDE;
  virtual int32_t ActiveAudioLayer(AudioLayer* audio_layer) const OVERRIDE;
  virtual webrtc::AudioDeviceModule::ErrorCode LastError() const OVERRIDE;
  virtual int16_t PlayoutDevices() OVERRIDE;
  virtual int16_t RecordingDevices() OVERRIDE;
  virtual int32_t PlayoutDeviceName(
      uint16_t index, char name[webrtc::kAdmMaxDeviceNameSize],
      char guid[webrtc::kAdmMaxGuidSize]) OVERRIDE;
  virtual int32_t RecordingDeviceName(
      uint16_t index, char name[webrtc::kAdmMaxDeviceNameSize],
      char guid[webrtc::kAdmMaxGuidSize]) OVERRIDE;
  virtual int32_t SetPlayoutDevice(uint16_t index) OVERRIDE;
  virtual int32_t SetPlayoutDevice(WindowsDeviceType device) OVERRIDE;
  virtual int32_t SetRecordingDevice(uint16_t index) OVERRIDE;
  virtual int32_t SetRecordingDevice(WindowsDeviceType device) OVERRIDE;
  virtual int32_t InitPlayout() OVERRIDE;
  virtual int32_t InitRecording() OVERRIDE;
  virtual int32_t SetWaveOutVolume(uint16_t volume_left,
                                   uint16_t volume_right) OVERRIDE;
  virtual int32_t WaveOutVolume(uint16_t* volume_left,
                                uint16_t* volume_right) const OVERRIDE;
  virtual int32_t InitSpeaker() OVERRIDE;
  virtual bool SpeakerIsInitialized() const OVERRIDE;
  virtual int32_t InitMicrophone() OVERRIDE;
  virtual bool MicrophoneIsInitialized() const OVERRIDE;
  virtual int32_t SpeakerVolumeIsAvailable(bool* available) OVERRIDE;
  virtual int32_t SetSpeakerVolume(uint32_t volume) OVERRIDE;
  virtual int32_t SpeakerVolume(uint32_t* volume) const OVERRIDE;
  virtual int32_t MaxSpeakerVolume(uint32_t* max_volume) const OVERRIDE;
  virtual int32_t MinSpeakerVolume(uint32_t* min_volume) const OVERRIDE;
  virtual int32_t SpeakerVolumeStepSize(uint16_t* step_size) const OVERRIDE;
  virtual int32_t MicrophoneVolumeIsAvailable(bool* available) OVERRIDE;
  virtual int32_t MicrophoneVolumeStepSize(
      uint16_t* step_size) const OVERRIDE;
  virtual int32_t SpeakerMuteIsAvailable(bool* available) OVERRIDE;
  virtual int32_t SetSpeakerMute(bool enable) OVERRIDE;
  virtual int32_t SpeakerMute(bool* enabled) const OVERRIDE;
  virtual int32_t MicrophoneMuteIsAvailable(bool* available) OVERRIDE;
  virtual int32_t SetMicrophoneMute(bool enable) OVERRIDE;
  virtual int32_t MicrophoneMute(bool* enabled) const OVERRIDE;
  virtual int32_t MicrophoneBoostIsAvailable(bool* available) OVERRIDE;
  virtual int32_t SetMicrophoneBoost(bool enable) OVERRIDE;
  virtual int32_t MicrophoneBoost(bool* enabled) const OVERRIDE;
  virtual int32_t SetStereoPlayout(bool enable) OVERRIDE;
  virtual int32_t StereoPlayout(bool* enabled) const OVERRIDE;
  virtual int32_t SetStereoRecording(bool enable) OVERRIDE;
  virtual int32_t StereoRecording(bool* enabled) const OVERRIDE;
  virtual int32_t SetRecordingChannel(const ChannelType channel) OVERRIDE;
  virtual int32_t RecordingChannel(ChannelType* channel) const OVERRIDE;
  virtual int32_t SetPlayoutBuffer(
      const BufferType type, uint16_t size_ms) OVERRIDE;
  virtual int32_t PlayoutBuffer(
      BufferType* type, uint16_t* size_ms) const OVERRIDE;
  virtual int32_t CPULoad(uint16_t* load) const OVERRIDE;
  virtual int32_t StartRawOutputFileRecording(
      const char pcm_file_name_utf8[webrtc::kAdmMaxFileNameSize]) OVERRIDE;
  virtual int32_t StopRawOutputFileRecording() OVERRIDE;
  virtual int32_t StartRawInputFileRecording(
      const char pcm_file_name_utf8[webrtc::kAdmMaxFileNameSize]) OVERRIDE;
  virtual int32_t StopRawInputFileRecording() OVERRIDE;
  virtual int32_t SetRecordingSampleRate(
      const uint32_t samples_per_sec) OVERRIDE;
  virtual int32_t SetPlayoutSampleRate(
      const uint32_t samples_per_sec) OVERRIDE;
  virtual int32_t ResetAudioDevice() OVERRIDE;
  virtual int32_t SetLoudspeakerStatus(bool enable) OVERRIDE;
  virtual int32_t GetLoudspeakerStatus(bool* enabled) const OVERRIDE;
  virtual int32_t SetAGC(bool enable) OVERRIDE;
  virtual bool AGC() const OVERRIDE;

 protected:
  virtual ~WebRtcAudioDeviceNotImpl() {};

 private:
  base::TimeTicks last_process_time_;
  DISALLOW_COPY_AND_ASSIGN(WebRtcAudioDeviceNotImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_DEVICE_NOT_IMPL_H_

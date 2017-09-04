// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_DEVICE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_DEVICE_IMPL_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/renderer/media/webrtc_audio_device_not_impl.h"
#include "ipc/ipc_platform_file.h"

// A WebRtcAudioDeviceImpl instance implements the abstract interface
// webrtc::AudioDeviceModule which makes it possible for a user (e.g. webrtc::
// VoiceEngine) to register this class as an external AudioDeviceModule (ADM).
// Then WebRtcAudioDeviceImpl::SetSessionId() needs to be called to set the
// session id that tells which device to use. The user can then call
// WebRtcAudioDeviceImpl::StartPlayout() and
// WebRtcAudioDeviceImpl::StartRecording() from the render process to initiate
// and start audio rendering and capturing in the browser process. IPC is
// utilized to set up the media streams.
//
// Usage example:
//
//   using namespace webrtc;
//
//   {
//      scoped_refptr<WebRtcAudioDeviceImpl> external_adm;
//      external_adm = new WebRtcAudioDeviceImpl();
//      external_adm->SetSessionId(session_id);
//      VoiceEngine* voe = VoiceEngine::Create();
//      VoEBase* base = VoEBase::GetInterface(voe);
//      base->Init(external_adm);
//      int ch = base->CreateChannel();
//      ...
//      base->StartReceive(ch)
//      base->StartPlayout(ch);
//      base->StartSending(ch);
//      ...
//      <== full-duplex audio session with AGC enabled ==>
//      ...
//      base->DeleteChannel(ch);
//      base->Terminate();
//      base->Release();
//      VoiceEngine::Delete(voe);
//   }
//
// webrtc::VoiceEngine::Init() calls these ADM methods (in this order):
//
//  RegisterAudioCallback(this)
//    webrtc::VoiceEngine is an webrtc::AudioTransport implementation and
//    implements the RecordedDataIsAvailable() and NeedMorePlayData() callbacks.
//
//  Init()
//    Creates and initializes the AudioOutputDevice and AudioInputDevice
//    objects.
//
//  SetAGC(true)
//    Enables the adaptive analog mode of the AGC which ensures that a
//    suitable microphone volume level will be set. This scheme will affect
//    the actual microphone control slider.
//
// AGC overview:
//
// It aims to maintain a constant speech loudness level from the microphone.
// This is done by both controlling the analog microphone gain and applying
// digital gain. The microphone gain on the sound card is slowly
// increased/decreased during speech only. By observing the microphone control
// slider you can see it move when you speak. If you scream, the slider moves
// downwards and then upwards again when you return to normal. It is not
// uncommon that the slider hits the maximum. This means that the maximum
// analog gain is not large enough to give the desired loudness. Nevertheless,
// we can in general still attain the desired loudness. If the microphone
// control slider is moved manually, the gain adaptation restarts and returns
// to roughly the same position as before the change if the circumstances are
// still the same. When the input microphone signal causes saturation, the
// level is decreased dramatically and has to re-adapt towards the old level.
// The adaptation is a slowly varying process and at the beginning of capture
// this is noticed by a slow increase in volume. Smaller changes in microphone
// input level is leveled out by the built-in digital control. For larger
// differences we need to rely on the slow adaptation.
// See http://en.wikipedia.org/wiki/Automatic_gain_control for more details.
//
// AGC implementation details:
//
// The adaptive analog mode of the AGC is always enabled for desktop platforms
// in WebRTC.
//
// Before recording starts, the ADM enables AGC on the AudioInputDevice.
//
// A capture session with AGC is started up as follows (simplified):
//
//                            [renderer]
//                                |
//                     ADM::StartRecording()
//             AudioInputDevice::InitializeOnIOThread()
//           AudioInputHostMsg_CreateStream(..., agc=true)               [IPC]
//                                |
//                       [IPC to the browser]
//                                |
//              AudioInputRendererHost::OnCreateStream()
//              AudioInputController::CreateLowLatency()
//         AudioInputController::DoSetAutomaticGainControl(true)
//            AudioInputStream::SetAutomaticGainControl(true)
//                                |
// AGC is now enabled in the media layer and streaming starts (details omitted).
// The figure below illustrates the AGC scheme which is active in combination
// with the default media flow explained earlier.
//                                |
//                            [browser]
//                                |
//                AudioInputStream::(Capture thread loop)
//  AgcAudioStream<AudioInputStream>::GetAgcVolume() => get latest mic volume
//                 AudioInputData::OnData(..., volume)
//              AudioInputController::OnData(..., volume)
//               AudioInputSyncWriter::Write(..., volume)
//                                |
//      [volume | size | data] is sent to the renderer         [shared memory]
//                                |
//                            [renderer]
//                                |
//          AudioInputDevice::AudioThreadCallback::Process()
//            WebRtcAudioDeviceImpl::Capture(..., volume)
//    AudioTransport::RecordedDataIsAvailable(...,volume, new_volume)
//                                |
// The AGC now uses the current volume input and computes a suitable new
// level given by the |new_level| output. This value is only non-zero if the
// AGC has take a decision that the microphone level should change.
//                                |
//                      if (new_volume != 0)
//              AudioInputDevice::SetVolume(new_volume)
//              AudioInputHostMsg_SetVolume(new_volume)                  [IPC]
//                                |
//                       [IPC to the browser]
//                                |
//                 AudioInputRendererHost::OnSetVolume()
//                  AudioInputController::SetVolume()
//             AudioInputStream::SetVolume(scaled_volume)
//                                |
// Here we set the new microphone level in the media layer and at the same time
// read the new setting (we might not get exactly what is set).
//                                |
//             AudioInputData::OnData(..., updated_volume)
//           AudioInputController::OnData(..., updated_volume)
//                                |
//                                |
// This process repeats until we stop capturing data. Note that, a common
// steady state is that the volume control reaches its max and the new_volume
// value from the AGC is zero. A loud voice input is required to break this
// state and start lowering the level again.
//
// Implementation notes:
//
//  - This class must be created and destroyed on the main render thread and
//    most methods are called on the same thread. However, some methods are
//    also called on a Libjingle worker thread. RenderData is called on the
//    AudioOutputDevice thread and CaptureData on the AudioInputDevice thread.
//    To summarize: this class lives on four different threads, so it is
//    important to be careful with the order in which locks are acquired in
//    order to avoid potential deadlocks.
//  - The webrtc::AudioDeviceModule is reference counted.
//  - AGC is only supported in combination with the WASAPI-based audio layer
//    on Windows, i.e., it is not supported on Windows XP.
//  - All volume levels required for the AGC scheme are transfered in a
//    normalized range [0.0, 1.0]. Scaling takes place in both endpoints
//    (WebRTC client a media layer). This approach ensures that we can avoid
//    transferring maximum levels between the renderer and the browser.
//

namespace media {
class AudioBus;
}

namespace content {

class ProcessedLocalAudioSource;
class WebRtcAudioRenderer;

// TODO(xians): Move the following two interfaces to webrtc so that
// libjingle can own references to the renderer and capturer.
class WebRtcAudioRendererSource {
 public:
  // Callback to get the rendered data.
  virtual void RenderData(media::AudioBus* audio_bus,
                          int sample_rate,
                          int audio_delay_milliseconds,
                          base::TimeDelta* current_time) = 0;

  // Callback to notify the client that the renderer is going away.
  virtual void RemoveAudioRenderer(WebRtcAudioRenderer* renderer) = 0;

  // Callback to notify the client that the audio renderer thread stopped.
  // This function must be called only when that thread is actually stopped.
  // Otherwise a race may occur.
  virtual void AudioRendererThreadStopped() = 0;

 protected:
  virtual ~WebRtcAudioRendererSource() {}
};

// TODO(xians): Merge this interface with WebRtcAudioRendererSource.
// The reason why we could not do it today is that WebRtcAudioRendererSource
// gets the data by pulling, while the data is pushed into
// WebRtcPlayoutDataSource::Sink.
class WebRtcPlayoutDataSource {
 public:
  class Sink {
   public:
    // Callback to get the playout data.
    // Called on the audio render thread.
    virtual void OnPlayoutData(media::AudioBus* audio_bus,
                               int sample_rate,
                               int audio_delay_milliseconds) = 0;

    // Callback to notify the sink that the source has changed.
    // Called on the main render thread.
    virtual void OnPlayoutDataSourceChanged() = 0;

    // Called to notify that the audio render thread has changed, and
    // OnPlayoutData() will from now on be called on the new thread.
    // Called on the new audio render thread.
    virtual void OnRenderThreadChanged() = 0;

   protected:
    virtual ~Sink() {}
  };

  // Adds/Removes the sink of WebRtcAudioRendererSource to the ADM.
  // These methods are used by the MediaStreamAudioProcesssor to get the
  // rendered data for AEC.
  virtual void AddPlayoutSink(Sink* sink) = 0;
  virtual void RemovePlayoutSink(Sink* sink) = 0;

 protected:
  virtual ~WebRtcPlayoutDataSource() {}
};

// Note that this class inherits from webrtc::AudioDeviceModule but due to
// the high number of non-implemented methods, we move the cruft over to the
// WebRtcAudioDeviceNotImpl.
class CONTENT_EXPORT WebRtcAudioDeviceImpl
    : NON_EXPORTED_BASE(public WebRtcAudioDeviceNotImpl),
      NON_EXPORTED_BASE(public WebRtcAudioRendererSource),
      NON_EXPORTED_BASE(public WebRtcPlayoutDataSource) {
 public:
  // The maximum volume value WebRtc uses.
  static const int kMaxVolumeLevel = 255;

  // Instances of this object are created on the main render thread.
  WebRtcAudioDeviceImpl();

  // webrtc::RefCountedModule implementation.
  // The creator must call AddRef() after construction and use Release()
  // to release the reference and delete this object.
  // Called on the main render thread.
  int32_t AddRef() const override;
  int32_t Release() const override;

 private:
  // webrtc::AudioDeviceModule implementation.
  // All implemented methods are called on the main render thread unless
  // anything else is stated.

  int32_t RegisterAudioCallback(
      webrtc::AudioTransport* audio_callback) override;

  int32_t Init() override;
  int32_t Terminate() override;
  bool Initialized() const override;

  int32_t PlayoutIsAvailable(bool* available) override;
  bool PlayoutIsInitialized() const override;
  int32_t RecordingIsAvailable(bool* available) override;
  bool RecordingIsInitialized() const override;

  // All Start/Stop methods are called on a libJingle worker thread.
  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  bool Playing() const override;
  int32_t StartRecording() override;
  int32_t StopRecording() override;
  bool Recording() const override;

  // Called on the AudioInputDevice worker thread.
  int32_t SetMicrophoneVolume(uint32_t volume) override;

  // TODO(henrika): sort out calling thread once we start using this API.
  int32_t MicrophoneVolume(uint32_t* volume) const override;

  int32_t MaxMicrophoneVolume(uint32_t* max_volume) const override;
  int32_t MinMicrophoneVolume(uint32_t* min_volume) const override;
  int32_t StereoPlayoutIsAvailable(bool* available) const override;
  int32_t StereoRecordingIsAvailable(bool* available) const override;
  int32_t PlayoutDelay(uint16_t* delay_ms) const override;
  int32_t RecordingDelay(uint16_t* delay_ms) const override;
  int32_t RecordingSampleRate(uint32_t* sample_rate) const override;
  int32_t PlayoutSampleRate(uint32_t* sample_rate) const override;

 public:
  // Sets the |renderer_|, returns false if |renderer_| already exists.
  // Called on the main renderer thread.
  bool SetAudioRenderer(WebRtcAudioRenderer* renderer);

  // Adds/Removes the |capturer| to the ADM.  Does NOT take ownership.
  // Capturers must remain valid until RemoveAudioCapturer() is called.
  // TODO(xians): Remove these two methods once the ADM does not need to pass
  // hardware information up to WebRtc.
  void AddAudioCapturer(ProcessedLocalAudioSource* capturer);
  void RemoveAudioCapturer(ProcessedLocalAudioSource* capturer);

  // Gets paired device information of the capture device for the audio
  // renderer. This is used to pass on a session id, sample rate and buffer
  // size to a webrtc audio renderer (either local or remote), so that audio
  // will be rendered to a matching output device.
  // Returns true if the capture device has a paired output device, otherwise
  // false. Note that if there are more than one open capture device the
  // function will not be able to pick an appropriate device and return false.
  bool GetAuthorizedDeviceInfoForAudioRenderer(
      int* session_id, int* output_sample_rate, int* output_buffer_size);

  const scoped_refptr<WebRtcAudioRenderer>& renderer() const {
    return renderer_;
  }

 private:
  typedef std::list<ProcessedLocalAudioSource*> CapturerList;
  typedef std::list<WebRtcPlayoutDataSource::Sink*> PlayoutDataSinkList;
  class RenderBuffer;

  // Make destructor private to ensure that we can only be deleted by Release().
  ~WebRtcAudioDeviceImpl() override;

  // WebRtcAudioRendererSource implementation.

  // Called on the AudioOutputDevice worker thread.
  void RenderData(media::AudioBus* audio_bus,
                  int sample_rate,
                  int audio_delay_milliseconds,
                  base::TimeDelta* current_time) override;

  // Called on the main render thread.
  void RemoveAudioRenderer(WebRtcAudioRenderer* renderer) override;
  void AudioRendererThreadStopped() override;

  // WebRtcPlayoutDataSource implementation.
  void AddPlayoutSink(WebRtcPlayoutDataSource::Sink* sink) override;
  void RemovePlayoutSink(WebRtcPlayoutDataSource::Sink* sink) override;

  // Used to check methods that run on the main render thread.
  base::ThreadChecker main_thread_checker_;
  // Used to check methods that are called on libjingle's signaling thread.
  base::ThreadChecker signaling_thread_checker_;
  base::ThreadChecker worker_thread_checker_;
  base::ThreadChecker audio_renderer_thread_checker_;

  mutable int ref_count_;

  // List of captures which provides access to the native audio input layer
  // in the browser process.  The last capturer in this list is considered the
  // "default capturer" by the methods implementing the
  // webrtc::AudioDeviceModule interface.
  CapturerList capturers_;

  // Provides access to the audio renderer in the browser process.
  scoped_refptr<WebRtcAudioRenderer> renderer_;

  // A list of raw pointer of WebRtcPlayoutDataSource::Sink objects which want
  // to get the playout data, the sink need to call RemovePlayoutSink()
  // before it goes away.
  PlayoutDataSinkList playout_sinks_;

  // Weak reference to the audio callback.
  // The webrtc client defines |audio_transport_callback_| by calling
  // RegisterAudioCallback().
  webrtc::AudioTransport* audio_transport_callback_;

  // Cached value of the current audio delay on the output/renderer side.
  int output_delay_ms_;

  // Protects |recording_|, |output_delay_ms_|, |input_delay_ms_|, |renderer_|
  // |recording_|, |microphone_volume_| and |playout_sinks_|.
  mutable base::Lock lock_;

  // Used to protect the racing of calling OnData() since there can be more
  // than one input stream calling OnData().
  mutable base::Lock capture_callback_lock_;

  bool initialized_;
  bool playing_;
  bool recording_;

  // Buffer used for temporary storage during render callback.
  // It is only accessed by the audio render thread.
  std::vector<int16_t> render_buffer_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcAudioDeviceImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_AUDIO_DEVICE_IMPL_H_

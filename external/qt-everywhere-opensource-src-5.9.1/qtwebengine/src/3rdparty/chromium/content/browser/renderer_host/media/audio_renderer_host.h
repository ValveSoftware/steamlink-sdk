// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AudioRendererHost serves audio related requests from AudioRenderer which
// lives inside the render process and provide access to audio hardware.
//
// This class is owned by RenderProcessHostImpl, and instantiated on UI
// thread, but all other operations and method calls happen on IO thread, so we
// need to be extra careful about the lifetime of this object. AudioManager is a
// singleton and created in IO thread, audio output streams are also created in
// the IO thread, so we need to destroy them also in IO thread. After this class
// is created, a task of OnInitialized() is posted on IO thread in which
// singleton of AudioManager is created.
//
// Here's an example of a typical IPC dialog for audio:
//
//   Renderer                     AudioRendererHost
//      |                               |
//      |  RequestDeviceAuthorization > |
//      |    < NotifyDeviceAuthorized   |
//      |                               |
//      |         CreateStream >        |
//      |     < NotifyStreamCreated     |
//      |                               |
//      |          PlayStream >         |
//      |                               |
//      |         PauseStream >         |
//      |                               |
//      |          PlayStream >         |
//      |             ...               |
//      |         CloseStream >         |
//      v                               v
// If there is an error at any point, a NotifyStreamError will
// be sent. Otherwise, the renderer can assume that the actual state
// of the output stream is consistent with the control signals it sends.

// A SyncSocket pair is used to signal buffer readiness between processes.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_RENDERER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_RENDERER_HOST_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/atomic_ref_count.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/browser/renderer_host/media/media_devices_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_logging.h"
#include "media/audio/audio_output_controller.h"
#include "media/audio/simple_sources.h"
#include "url/origin.h"

namespace media {
class AudioManager;
class AudioParameters;
}

namespace content {

class AudioMirroringManager;
class MediaInternals;
class MediaStreamManager;

class CONTENT_EXPORT AudioRendererHost : public BrowserMessageFilter {
 public:
  // Called from UI thread from the owner of this object.
  AudioRendererHost(int render_process_id,
                    media::AudioManager* audio_manager,
                    AudioMirroringManager* mirroring_manager,
                    MediaInternals* media_internals,
                    MediaStreamManager* media_stream_manager,
                    const std::string& salt);

  // Calls |callback| with the list of AudioOutputControllers for this object.
  void GetOutputControllers(
      const RenderProcessHost::GetAudioOutputControllersCallback&
          callback) const;

  // BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // Returns true if any streams managed by this host are actively playing.  Can
  // be called from any thread.
  bool HasActiveAudio();

 private:
  friend class AudioRendererHostTest;
  friend class BrowserThread;
  friend class base::DeleteHelper<AudioRendererHost>;
  friend class MockAudioRendererHost;
  friend class TestAudioRendererHost;
  FRIEND_TEST_ALL_PREFIXES(AudioRendererHostTest, CreateMockStream);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererHostTest, MockStreamDataConversation);

  class AudioEntry;
  typedef std::map<int, AudioEntry*> AudioEntryMap;

  // Internal callback type for access requests to output devices.
  // |have_access| is true only if there is permission to access the device.
  typedef base::Callback<void(bool have_access)> OutputDeviceAccessCB;

  // The type of a function that is run on the UI thread to check whether the
  // routing IDs reference a valid RenderFrameHost. The function then runs
  // |callback| on the IO thread with true/false if valid/invalid.
  using ValidateRenderFrameIdFunction =
      void (*)(int render_process_id,
               int render_frame_id,
               const base::Callback<void(bool)>& callback);

  ~AudioRendererHost() override;

  // Methods called on IO thread ----------------------------------------------

  // Audio related IPC message handlers.

  // Request permission to use an output device for use by a stream produced
  // in the RenderFrame referenced by |render_frame_id|.
  // |session_id| is used for unified IO to find out which input device to be
  // opened for the stream. For clients that do not use unified IO,
  // |session_id| will be ignored and the given |device_id| and
  // |security_origin| will be used to select the output device.
  // Upon completion of the process, the peer is notified with the device output
  // parameters via the NotifyDeviceAuthorized message.
  void OnRequestDeviceAuthorization(int stream_id,
                                    int render_frame_id,
                                    int session_id,
                                    const std::string& device_id,
                                    const url::Origin& security_origin);

  // Creates an audio output stream with the specified format.
  // Upon success/failure, the peer is notified via the NotifyStreamCreated
  // message.
  void OnCreateStream(int stream_id,
                      int render_frame_id,
                      const media::AudioParameters& params);

  // Play the audio stream referenced by |stream_id|.
  void OnPlayStream(int stream_id);

  // Pause the audio stream referenced by |stream_id|.
  void OnPauseStream(int stream_id);

  // Close the audio stream referenced by |stream_id|.
  void OnCloseStream(int stream_id);

  // Set the volume of the audio stream referenced by |stream_id|.
  void OnSetVolume(int stream_id, double volume);

  // Helper methods.

  // Complete the process of creating an audio stream. This will set up the
  // shared memory or shared socket in low latency mode and send the
  // NotifyStreamCreated message to the peer.
  void DoCompleteCreation(int stream_id);

  // Called after the |render_frame_id| provided to OnCreateStream() was
  // validated. When |is_valid| is false, this calls ReportErrorAndClose().
  void DidValidateRenderFrame(int stream_id, bool is_valid);

  // Updates status of stream for AudioStreamMonitor and updates
  // the number of playing streams.
  void StreamStateChanged(int stream_id, bool is_playing);

  RenderProcessHost::AudioOutputControllerList DoGetOutputControllers() const;

  // Send an error message to the renderer.
  void SendErrorMessage(int stream_id);

  // Delete an audio entry, notifying observers first.  This is called by
  // AudioOutputController after it has closed.
  void DeleteEntry(std::unique_ptr<AudioEntry> entry);

  // Send an error message to the renderer, then close the stream.
  void ReportErrorAndClose(int stream_id);

  // A helper method to look up a AudioEntry identified by |stream_id|.
  // Returns NULL if not found.
  AudioEntry* LookupById(int stream_id);

  // A helper method to update the number of playing streams and alert the
  // ResourceScheduler when the renderer starts or stops playing an audiostream.
  void UpdateNumPlayingStreams(AudioEntry* entry, bool is_playing);

  // Check if the renderer process has access to the requested output device.
  void CheckOutputDeviceAccess(int render_frame_id,
                               const std::string& device_id,
                               const url::Origin& security_origin,
                               int stream_id,
                               base::TimeTicks auth_start_time);

  // Proceed with device authorization after checking permissions.
  void AccessChecked(const std::string& device_id,
                     const url::Origin& security_origin,
                     int stream_id,
                     base::TimeTicks auth_start_time,
                     bool have_access);

  // Translate the hashed |device_id| to a unique device ID.
  void TranslateDeviceID(const std::string& device_id,
                         const url::Origin& security_origin,
                         int stream_id,
                         base::TimeTicks auth_start_time,
                         const MediaDeviceEnumeration& enumeration);

  // Get audio hardware parameters on the device thread.
  media::AudioParameters GetDeviceParametersOnDeviceThread(
      const std::string& device_id);

  // Proceed with device authorization after translating device ID and
  // receiving hardware parameters.
  void DeviceParametersReceived(int stream_id,
                                base::TimeTicks auth_start_time,
                                bool device_found,
                                const std::string& unique_id,
                                const media::AudioParameters& output_params);

  // Helper method to check if the authorization procedure for stream
  // |stream_id| has started.
  bool IsAuthorizationStarted(int stream_id);

  // Called from AudioRendererHostTest to override the function that checks for
  // the existence of the RenderFrameHost at stream creation time.
  void set_render_frame_id_validate_function_for_testing(
      ValidateRenderFrameIdFunction function) {
    validate_render_frame_id_function_ = function;
  }

  // ID of the RenderProcessHost that owns this instance.
  const int render_process_id_;

  media::AudioManager* const audio_manager_;
  AudioMirroringManager* const mirroring_manager_;
  std::unique_ptr<media::AudioLog> audio_log_;

  // Used to access to AudioInputDeviceManager.
  MediaStreamManager* media_stream_manager_;

  // A map of stream IDs to audio sources.
  AudioEntryMap audio_entries_;

  // The number of streams in the playing state. Atomic read safe from any
  // thread, but should only be updated from the IO thread.
  base::AtomicRefCount num_playing_streams_;

  // Salt required to translate renderer device IDs to raw device unique IDs
  std::string salt_;

  // Map of device authorizations for streams that are not yet created
  // The key is the stream ID, and the value is a pair. The pair's first element
  // is a bool that is true if the authorization process completes successfully.
  // The second element contains the unique ID of the authorized device.
  std::map<int, std::pair<bool, std::string>> authorizations_;

  // At stream creation time, AudioRendererHost will call this function on the
  // UI thread to validate render frame IDs. A default is set by the
  // constructor, but this can be overridden by unit tests.
  ValidateRenderFrameIdFunction validate_render_frame_id_function_;

  // The maximum number of simultaneous streams during the lifetime of this
  // host. Reported as UMA stat at shutdown.
  size_t max_simultaneous_streams_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_RENDERER_HOST_H_

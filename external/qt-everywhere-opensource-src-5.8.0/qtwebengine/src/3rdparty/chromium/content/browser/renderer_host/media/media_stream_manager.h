// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaStreamManager is used to open/enumerate media capture devices (video
// supported now). Call flow:
// 1. GenerateStream is called when a render process wants to use a capture
//    device.
// 2. MediaStreamManager will ask MediaStreamUIController for permission to
//    use devices and for which device to use.
// 3. MediaStreamManager will request the corresponding media device manager(s)
//    to enumerate available devices. The result will be given to
//    MediaStreamUIController.
// 4. MediaStreamUIController will, by posting the request to UI, let the
//    users to select which devices to use and send callback to
//    MediaStreamManager with the result.
// 5. MediaStreamManager will call the proper media device manager to open the
//    device and let the MediaStreamRequester know it has been done.

// If either user or test harness selects --use-fake-device-for-media-stream,
// a fake video device or devices are used instead of real ones.

// When enumeration and open are done in separate operations,
// MediaStreamUIController is not involved as in steps.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_MANAGER_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/power_monitor/power_observer.h"
#include "base/system_monitor/system_monitor.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/audio_output_device_enumerator.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/browser/media_request_state.h"
#include "content/public/browser/resource_context.h"

namespace media {
class AudioManager;
}

namespace url {
class Origin;
}

namespace content {

class AudioInputDeviceManager;
class AudioOutputDeviceEnumerator;
class BrowserContext;
class FakeMediaStreamUIProxy;
class MediaStreamDeviceSettings;
class MediaStreamRequester;
class MediaStreamUIProxy;
class VideoCaptureManager;

// MediaStreamManager is used to generate and close new media devices, not to
// start the media flow. The classes requesting new media streams are answered
// using MediaStreamRequester.
class CONTENT_EXPORT MediaStreamManager
    : public MediaStreamProviderListener,
      public base::MessageLoop::DestructionObserver,
      public base::PowerObserver,
      public base::SystemMonitor::DevicesChangedObserver {
 public:
  // Callback to deliver the result of a media request.
  typedef base::Callback<void(const MediaStreamDevices& devices,
                              std::unique_ptr<MediaStreamUIProxy> ui)>
      MediaRequestResponseCallback;

  // Adds |message| to native logs for outstanding device requests, for use by
  // render processes hosts whose corresponding render processes are requesting
  // logging from webrtcLoggingPrivate API. Safe to call from any thread.
  static void SendMessageToNativeLog(const std::string& message);

  explicit MediaStreamManager(media::AudioManager* audio_manager);

  ~MediaStreamManager() override;

  // Used to access VideoCaptureManager.
  VideoCaptureManager* video_capture_manager();

  // Used to access AudioInputDeviceManager.
  AudioInputDeviceManager* audio_input_device_manager();

  // Used to access AudioOutputDeviceEnumerator.
  AudioOutputDeviceEnumerator* audio_output_device_enumerator();

  // Creates a new media access request which is identified by a unique string
  // that's returned to the caller. This will trigger the infobar and ask users
  // for access to the device. |render_process_id| and |render_frame_id| are
  // used to determine where the infobar will appear to the user. |callback| is
  // used to send the selected device to the clients. An empty list of device
  // will be returned if the users deny the access.
  std::string MakeMediaAccessRequest(
      int render_process_id,
      int render_frame_id,
      int page_request_id,
      const StreamControls& controls,
      const url::Origin& security_origin,
      const MediaRequestResponseCallback& callback);

  // GenerateStream opens new media devices according to |components|.  It
  // creates a new request which is identified by a unique string that's
  // returned to the caller.  |render_process_id| and |render_frame_id| are used
  // to determine where the infobar will appear to the user.
  void GenerateStream(MediaStreamRequester* requester,
                      int render_process_id,
                      int render_frame_id,
                      const std::string& salt,
                      int page_request_id,
                      const StreamControls& controls,
                      const url::Origin& security_origin,
                      bool user_gesture);

  void CancelRequest(int render_process_id,
                     int render_frame_id,
                     int page_request_id);

  // Cancel an open request identified by |label|.
  virtual void CancelRequest(const std::string& label);

  // Cancel all requests for the given |render_process_id|.
  void CancelAllRequests(int render_process_id);

  // Closes the stream device for a certain render frame. The stream must have
  // been opened by a call to GenerateStream.
  void StopStreamDevice(int render_process_id,
                        int render_frame_id,
                        const std::string& device_id);

  // Gets a list of devices of |type|, which must be MEDIA_DEVICE_AUDIO_CAPTURE
  // or MEDIA_DEVICE_VIDEO_CAPTURE.
  // The request is identified using the string returned to the caller.
  // When the |requester| is NULL, MediaStreamManager will enumerate both audio
  // and video devices and also start monitoring device changes, such as
  // plug/unplug. The new device lists will be delivered via media observer to
  // MediaCaptureDevicesDispatcher.
  virtual std::string EnumerateDevices(MediaStreamRequester* requester,
                                       int render_process_id,
                                       int render_frame_id,
                                       const std::string& salt,
                                       int page_request_id,
                                       MediaStreamType type,
                                       const url::Origin& security_origin);

  // Open a device identified by |device_id|.  |type| must be either
  // MEDIA_DEVICE_AUDIO_CAPTURE or MEDIA_DEVICE_VIDEO_CAPTURE.
  // The request is identified using string returned to the caller.
  void OpenDevice(MediaStreamRequester* requester,
                  int render_process_id,
                  int render_frame_id,
                  const std::string& salt,
                  int page_request_id,
                  const std::string& device_id,
                  MediaStreamType type,
                  const url::Origin& security_origin);

  // Finds and returns the device id corresponding to the given
  // |source_id|. Returns true if there was a raw device id that matched the
  // given |source_id|, false if nothing matched it.
  bool TranslateSourceIdToDeviceId(MediaStreamType stream_type,
                                   const std::string& salt,
                                   const url::Origin& security_origin,
                                   const std::string& source_id,
                                   std::string* device_id) const;

  // Find |device_id| in the list of |requests_|, and returns its session id,
  // or StreamDeviceInfo::kNoId if not found.
  int VideoDeviceIdToSessionId(const std::string& device_id) const;

  // Called by UI to make sure the device monitor is started so that UI receive
  // notifications about device changes.
  void EnsureDeviceMonitorStarted();

  // Implements MediaStreamProviderListener.
  void Opened(MediaStreamType stream_type, int capture_session_id) override;
  void Closed(MediaStreamType stream_type, int capture_session_id) override;
  void DevicesEnumerated(MediaStreamType stream_type,
                         const StreamDeviceInfoArray& devices) override;
  void Aborted(MediaStreamType stream_type, int capture_session_id) override;

  // Implements base::SystemMonitor::DevicesChangedObserver.
  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) override;

  // Returns all devices currently opened by a request with label |label|.
  // If no request with |label| exist, an empty array is returned.
  StreamDeviceInfoArray GetDevicesOpenedByRequest(
      const std::string& label) const;

  // This object gets deleted on the UI thread after the IO thread has been
  // destroyed. So we need to know when IO thread is being destroyed so that
  // we can delete VideoCaptureManager and AudioInputDeviceManager. Normally
  // this is handled by
  // base::MessageLoop::DestructionObserver::WillDestroyCurrentMessageLoop.
  // But for some tests which use TestBrowserThreadBundle, we need to call
  // WillDestroyCurrentMessageLoop explicitly because the notification happens
  // too late. (see http://crbug.com/247525#c14).
  void WillDestroyCurrentMessageLoop() override;

  // Sends log messages to the render process hosts whose corresponding render
  // processes are making device requests, to be used by the
  // webrtcLoggingPrivate API if requested.
  void AddLogMessageOnIOThread(const std::string& message);

  // base::PowerObserver overrides.
  void OnSuspend() override;
  void OnResume() override;

  // Called by the tests to specify a fake UI that should be used for next
  // generated stream (or when using --use-fake-ui-for-media-stream).
  void UseFakeUIForTests(std::unique_ptr<FakeMediaStreamUIProxy> fake_ui);

  // Register and unregister a new callback for receiving native log entries.
  // The registered callback will be invoked on the IO thread.
  // The registration and unregistration will be done asynchronously so it is
  // not guaranteed that when the call returns the operation has completed.
  void RegisterNativeLogCallback(int renderer_host_id,
      const base::Callback<void(const std::string&)>& callback);
  void UnregisterNativeLogCallback(int renderer_host_id);

  // Register and unregister subscribers for device-change notifications.
  // It is an error to try to subscribe a |subscriber| that is already
  // subscribed or to cancel the subscription of a |subscriber| that is not
  // subscribed. Also, subscribers must make sure to invoke
  // CancelDeviceChangeNotifications() before destruction. Otherwise, dangling
  // pointers and use-after-destruction problems will occur.
  void SubscribeToDeviceChangeNotifications(MediaStreamRequester* subscriber);
  void CancelDeviceChangeNotifications(MediaStreamRequester* subscriber);

  // Generates a hash of a device's unique ID usable by one
  // particular security origin.
  static std::string GetHMACForMediaDeviceID(const std::string& salt,
                                             const url::Origin& security_origin,
                                             const std::string& raw_unique_id);

  // Convenience method to check if |device_guid| is an HMAC of
  // |raw_device_id| for |security_origin|.
  static bool DoesMediaDeviceIDMatchHMAC(const std::string& salt,
                                         const url::Origin& security_origin,
                                         const std::string& device_guid,
                                         const std::string& raw_unique_id);

  // Returns true if the renderer process identified with |render_process_id|
  // is allowed to access |origin|.
  static bool IsOriginAllowed(int render_process_id, const url::Origin& origin);

  // Set whether the capturing is secure for the capturing session with given
  // |session_id|, |render_process_id|, and the MediaStreamType |type|.
  void SetCapturingLinkSecured(int render_process_id,
                               int session_id,
                               content::MediaStreamType type,
                               bool is_secure);

 private:
  // Contains all data needed to keep track of requests.
  class DeviceRequest;

  // Cache enumerated device list.
  struct EnumerationCache {
    EnumerationCache();
    ~EnumerationCache();

    bool valid;
    StreamDeviceInfoArray devices;
  };

  // |DeviceRequests| is a list to ensure requests are processed in the order
  // they arrive. The first member of the pair is the label of the
  // |DeviceRequest|.
  using LabeledDeviceRequest = std::pair<std::string, DeviceRequest*>;
  using DeviceRequests = std::list<LabeledDeviceRequest>;

  // Initializes the device managers on IO thread.  Auto-starts the device
  // thread and registers this as a listener with the device managers.
  void InitializeDeviceManagersOnIOThread();

  // Helper for sending up-to-date device lists to media observer when a
  // capture device is plugged in or unplugged.
  void NotifyDevicesChanged(MediaStreamType stream_type,
                            const StreamDeviceInfoArray& devices);

  // |output_parameters| contains real values only if the request requires it.
  void HandleAccessRequestResponse(
      const std::string& label,
      const media::AudioParameters& output_parameters,
      const MediaStreamDevices& devices,
      content::MediaStreamRequestResult result);
  void StopMediaStreamFromBrowser(const std::string& label);

  void DoEnumerateDevices(const std::string& label);

  void AudioOutputDevicesEnumerated(
      const AudioOutputDeviceEnumeration& device_enumeration);

  // Helpers.
  // Checks if all devices that was requested in the request identififed by
  // |label| has been opened and set the request state accordingly.
  void HandleRequestDone(const std::string& label,
                         DeviceRequest* request);
  // Stop the use of the device associated with |session_id| of type |type| in
  // all |requests_|. The device is removed from the request. If a request
  /// doesn't use any devices as a consequence, the request is deleted.
  void StopDevice(MediaStreamType type, int session_id);
  // Calls the correct capture manager and close the device with |session_id|.
  // All requests that uses the device are updated.
  void CloseDevice(MediaStreamType type, int session_id);
  // Returns true if a request for devices has been completed and the devices
  // has either been opened or an error has occurred.
  bool RequestDone(const DeviceRequest& request) const;
  MediaStreamProvider* GetDeviceManager(MediaStreamType stream_type);
  void StartEnumeration(DeviceRequest* request);
  std::string AddRequest(DeviceRequest* request);
  DeviceRequest* FindRequest(const std::string& label) const;
  void DeleteRequest(const std::string& label);
  void ClearEnumerationCache(EnumerationCache* cache);
  // Returns true if the |cache| is invalid, false if it's invalid or if
  // the |stream_type| is MEDIA_NO_SERVICE.
  // On Android, this function will always return true for
  // MEDIA_DEVICE_AUDIO_CAPTURE since we don't have a SystemMonitor to tell
  // us about audio device changes.
  bool EnumerationRequired(EnumerationCache* cache, MediaStreamType type);
  // Prepare the request with label |label| by starting device enumeration if
  // needed.
  void SetupRequest(const std::string& label);
  // Prepare |request| of type MEDIA_DEVICE_AUDIO_CAPTURE and/or
  // MEDIA_DEVICE_VIDEO_CAPTURE for being posted to the UI by parsing
  // StreamControls for requested device IDs.
  bool SetupDeviceCaptureRequest(DeviceRequest* request);
  // Prepare |request| of type MEDIA_TAB_AUDIO_CAPTURE and/or
  // MEDIA_TAB_VIDEO_CAPTURE for being posted to the UI by parsing
  // StreamControls for requested tab capture IDs.
  bool SetupTabCaptureRequest(DeviceRequest* request);
  // Prepare |request| of type MEDIA_DESKTOP_AUDIO_CAPTURE and/or
  // MEDIA_DESKTOP_VIDEO_CAPTURE for being posted to the UI by parsing
  // StreamControls for the requested desktop ID.
  bool SetupScreenCaptureRequest(DeviceRequest* request);
  // Called when a request has been setup and devices have been enumerated if
  // needed.
  void ReadOutputParamsAndPostRequestToUI(const std::string& label,
                                          DeviceRequest* request);
  // Called when audio output parameters have been read if needed.
  void PostRequestToUI(const std::string& label,
                       DeviceRequest* request,
                       const media::AudioParameters& output_parameters);
  // Returns true if a device with |device_id| has already been requested with
  // a render procecss_id and render_frame_id and type equal to the the values
  // in |request|. If it has been requested, |device_info| contain information
  // about the device.
  bool FindExistingRequestedDeviceInfo(
      const DeviceRequest& new_request,
      const MediaStreamDevice& new_device_info,
      StreamDeviceInfo* existing_device_info,
      MediaRequestState* existing_request_state) const;

  void FinalizeGenerateStream(const std::string& label, DeviceRequest* request);
  void FinalizeRequestFailed(const std::string& label,
                             DeviceRequest* request,
                             content::MediaStreamRequestResult result);
  void FinalizeOpenDevice(const std::string& label, DeviceRequest* request);
  void FinalizeMediaAccessRequest(const std::string& label,
                                  DeviceRequest* request,
                                  const MediaStreamDevices& devices);
  void FinalizeEnumerateDevices(const std::string& label,
                                DeviceRequest* request);
  void HandleCheckMediaAccessResponse(const std::string& label,
                                      bool have_access);

  // This method is called when an audio or video device is plugged in or
  // removed. It make sure all MediaStreams that use a removed device is
  // stopped and that the render process is notified. |old_devices| is the list
  // of previously available devices. |new_devices| is the new
  // list of currently available devices.
  void StopRemovedDevices(const StreamDeviceInfoArray& old_devices,
                          const StreamDeviceInfoArray& new_devices);
  // Helper method used by StopRemovedDevices to stop the use of a certain
  // device.
  void StopRemovedDevice(const MediaStreamDevice& device);

  // Helpers to start and stop monitoring devices.
  void StartMonitoring();
  void StopMonitoring();
#if defined(OS_MACOSX)
  void StartMonitoringOnUIThread();
#endif

  // Picks a device ID from a list of required and alternate device IDs,
  // presented as part of a TrackControls structure.
  // Either the required device ID is picked (if present), or the first
  // valid alternate device ID.
  // Returns false if the required device ID is present and invalid.
  // Otherwise, if no valid device is found, device_id is unchanged.
  bool PickDeviceId(MediaStreamType type,
                    const std::string& salt,
                    const url::Origin& security_origin,
                    const TrackControls& controls,
                    std::string* device_id) const;

  // Finds the requested device id from request. The requested device type
  // must be MEDIA_DEVICE_AUDIO_CAPTURE or MEDIA_DEVICE_VIDEO_CAPTURE.
  bool GetRequestedDeviceCaptureId(const DeviceRequest* request,
                                   MediaStreamType type,
                                   std::string* device_id) const;

  void TranslateDeviceIdToSourceId(DeviceRequest* request,
                                   MediaStreamDevice* device);

  // Handles the callback from MediaStreamUIProxy to receive the UI window id,
  // used for excluding the notification window in desktop capturing.
  void OnMediaStreamUIWindowId(MediaStreamType video_type,
                               StreamDeviceInfoArray devices,
                               gfx::NativeViewId window_id);

  // Runs on the IO thread and does the actual [un]registration of callbacks.
  void DoNativeLogCallbackRegistration(int renderer_host_id,
      const base::Callback<void(const std::string&)>& callback);
  void DoNativeLogCallbackUnregistration(int renderer_host_id);

  void NotifyDeviceChangeSubscribers(MediaStreamType type);

  // Task runner shared by VideoCaptureManager and AudioInputDeviceManager and
  // used for enumerating audio output devices.
  // Note: Enumeration tasks may take seconds to complete so must never be run
  // on any of the BrowserThreads (UI, IO, etc).  See http://crbug.com/256945.
  scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;

  media::AudioManager* const audio_manager_;  // not owned
  scoped_refptr<AudioInputDeviceManager> audio_input_device_manager_;
  scoped_refptr<VideoCaptureManager> video_capture_manager_;
  std::unique_ptr<AudioOutputDeviceEnumerator> audio_output_device_enumerator_;
#if defined(OS_WIN)
  base::Thread video_capture_thread_;
#endif

  // Indicator of device monitoring state.
  bool monitoring_started_;

  // Stores most recently enumerated device lists. The cache is cleared when
  // monitoring is stopped or there is no request for that type of device.
  EnumerationCache audio_enumeration_cache_;
  EnumerationCache video_enumeration_cache_;

  // Keeps track of live enumeration commands sent to VideoCaptureManager or
  // AudioInputDeviceManager, in order to only enumerate when necessary.
  int active_enumeration_ref_count_[NUM_MEDIA_TYPES];

  // All non-closed request. Must be accessed on IO thread.
  DeviceRequests requests_;

  bool use_fake_ui_;
  std::unique_ptr<FakeMediaStreamUIProxy> fake_ui_;

  // Maps render process hosts to log callbacks. Used on the IO thread.
  std::map<int, base::Callback<void(const std::string&)>> log_callbacks_;

  // Objects subscribed to changes in the set of media devices.
  std::vector<MediaStreamRequester*> device_change_subscribers_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_MANAGER_H_

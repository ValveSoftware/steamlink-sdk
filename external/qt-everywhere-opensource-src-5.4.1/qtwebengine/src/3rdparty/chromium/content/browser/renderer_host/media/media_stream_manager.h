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
#include <set>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/power_monitor/power_observer.h"
#include "base/system_monitor/system_monitor.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/browser/media_request_state.h"
#include "content/public/browser/resource_context.h"

namespace media {
class AudioManager;
}

namespace content {

class AudioInputDeviceManager;
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
                              scoped_ptr<MediaStreamUIProxy> ui)>
      MediaRequestResponseCallback;

  explicit MediaStreamManager(media::AudioManager* audio_manager);
  virtual ~MediaStreamManager();

  // Used to access VideoCaptureManager.
  VideoCaptureManager* video_capture_manager();

  // Used to access AudioInputDeviceManager.
  AudioInputDeviceManager* audio_input_device_manager();

  // Creates a new media access request which is identified by a unique string
  // that's returned to the caller. This will trigger the infobar and ask users
  // for access to the device. |render_process_id| and |render_view_id| refer
  // to the view where the infobar will appear to the user. |callback| is
  // used to send the selected device to the clients. An empty list of device
  // will be returned if the users deny the access.
  std::string MakeMediaAccessRequest(
      int render_process_id,
      int render_view_id,
      int page_request_id,
      const StreamOptions& options,
      const GURL& security_origin,
      const MediaRequestResponseCallback& callback);

  // GenerateStream opens new media devices according to |components|.  It
  // creates a new request which is identified by a unique string that's
  // returned to the caller.  |render_process_id| and |render_view_id| refer to
  // the view where the infobar will appear to the user.
  void GenerateStream(MediaStreamRequester* requester,
                      int render_process_id,
                      int render_view_id,
                      const ResourceContext::SaltCallback& sc,
                      int page_request_id,
                      const StreamOptions& components,
                      const GURL& security_origin,
                      bool user_gesture);

  void CancelRequest(int render_process_id,
                     int render_view_id,
                     int page_request_id);

  // Cancel an open request identified by |label|.
  virtual void CancelRequest(const std::string& label);

  // Cancel all requests for the given |render_process_id|.
  void CancelAllRequests(int render_process_id);

  // Closes the stream device for a certain render view. The stream must have
  // been opened by a call to GenerateStream.
  void StopStreamDevice(int render_process_id,
                        int render_view_id,
                        const std::string& device_id);

  // Gets a list of devices of |type|, which must be MEDIA_DEVICE_AUDIO_CAPTURE
  // or MEDIA_DEVICE_VIDEO_CAPTURE.
  // The request is identified using the string returned to the caller.
  // When the |requester| is NULL, MediaStreamManager will enumerate both audio
  // and video devices and also start monitoring device changes, such as
  // plug/unplug. The new device lists will be delivered via media observer to
  // MediaCaptureDevicesDispatcher.
  // If |have_permission| is false, we remove the device label from the result.
  virtual std::string EnumerateDevices(MediaStreamRequester* requester,
                                       int render_process_id,
                                       int render_view_id,
                                       const ResourceContext::SaltCallback& sc,
                                       int page_request_id,
                                       MediaStreamType type,
                                       const GURL& security_origin,
                                       bool have_permission);

  // Open a device identified by |device_id|.  |type| must be either
  // MEDIA_DEVICE_AUDIO_CAPTURE or MEDIA_DEVICE_VIDEO_CAPTURE.
  // The request is identified using string returned to the caller.
  void OpenDevice(MediaStreamRequester* requester,
                  int render_process_id,
                  int render_view_id,
                  const ResourceContext::SaltCallback& sc,
                  int page_request_id,
                  const std::string& device_id,
                  MediaStreamType type,
                  const GURL& security_origin);

  // Finds and returns the device id corresponding to the given
  // |source_id|. Returns true if there was a raw device id that matched the
  // given |source_id|, false if nothing matched it.
  bool TranslateSourceIdToDeviceId(
      MediaStreamType stream_type,
      const ResourceContext::SaltCallback& rc,
      const GURL& security_origin,
      const std::string& source_id,
      std::string* device_id) const;

  // Called by UI to make sure the device monitor is started so that UI receive
  // notifications about device changes.
  void EnsureDeviceMonitorStarted();

  // Implements MediaStreamProviderListener.
  virtual void Opened(MediaStreamType stream_type,
                      int capture_session_id) OVERRIDE;
  virtual void Closed(MediaStreamType stream_type,
                      int capture_session_id) OVERRIDE;
  virtual void DevicesEnumerated(MediaStreamType stream_type,
                                 const StreamDeviceInfoArray& devices) OVERRIDE;
  virtual void Aborted(MediaStreamType stream_type,
                       int capture_session_id) OVERRIDE;

  // Implements base::SystemMonitor::DevicesChangedObserver.
  virtual void OnDevicesChanged(
      base::SystemMonitor::DeviceType device_type) OVERRIDE;

  // Called by the tests to specify a fake UI that should be used for next
  // generated stream (or when using --use-fake-ui-for-media-stream).
  void UseFakeUI(scoped_ptr<FakeMediaStreamUIProxy> fake_ui);

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
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  // Sends log messages to the render process hosts whose corresponding render
  // processes are making device requests, to be used by the
  // webrtcLoggingPrivate API if requested.
  void AddLogMessageOnIOThread(const std::string& message);

  // Adds |message| to native logs for outstanding device requests, for use by
  // render processes hosts whose corresponding render processes are requesting
  // logging from webrtcLoggingPrivate API. Safe to call from any thread.
  static void SendMessageToNativeLog(const std::string& message);

  // base::PowerObserver overrides.
  virtual void OnSuspend() OVERRIDE;
  virtual void OnResume() OVERRIDE;

 protected:
  // Used for testing.
  MediaStreamManager();

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
  typedef std::list<std::pair<std::string, DeviceRequest*> > DeviceRequests;

  // Initializes the device managers on IO thread.  Auto-starts the device
  // thread and registers this as a listener with the device managers.
  void InitializeDeviceManagersOnIOThread();

  // Helper for sending up-to-date device lists to media observer when a
  // capture device is plugged in or unplugged.
  void NotifyDevicesChanged(MediaStreamType stream_type,
                            const StreamDeviceInfoArray& devices);

  void HandleAccessRequestResponse(const std::string& label,
                                   const MediaStreamDevices& devices,
                                   content::MediaStreamRequestResult result);
  void StopMediaStreamFromBrowser(const std::string& label);

  void DoEnumerateDevices(const std::string& label);

  // Enumerates audio output devices. No caching.
  void EnumerateAudioOutputDevices(const std::string& label);

  void AudioOutputDevicesEnumerated(const StreamDeviceInfoArray& devices);

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
  // StreamOptions::Constraints for requested device IDs.
  bool SetupDeviceCaptureRequest(DeviceRequest* request);
  // Prepare |request| of type MEDIA_TAB_AUDIO_CAPTURE and/or
  // MEDIA_TAB_VIDEO_CAPTURE for being posted to the UI by parsing
  // StreamOptions::Constraints for requested tab capture IDs.
  bool SetupTabCaptureRequest(DeviceRequest* request);
  // Prepare |request| of type MEDIA_LOOPBACK_AUDIO_CAPTURE and/or
  // MEDIA_DESKTOP_VIDEO_CAPTURE for being posted to the UI by parsing
  // StreamOptions::Constraints for the requested desktop ID.
  bool SetupScreenCaptureRequest(DeviceRequest* request);
  // Called when a request has been setup and devices have been enumerated if
  // needed.
  void PostRequestToUI(const std::string& label, DeviceRequest* request);
  // Returns true if a device with |device_id| has already been requested with
  // a render procecss_id and render_view_id and type equal to the the values
  // in |request|. If it has been requested, |device_info| contain information
  // about the device.
  bool FindExistingRequestedDeviceInfo(
      const DeviceRequest& new_request,
      const MediaStreamDevice& new_device_info,
      StreamDeviceInfo* existing_device_info,
      MediaRequestState* existing_request_state) const;

  void FinalizeGenerateStream(const std::string& label,
                              DeviceRequest* request);
  void FinalizeRequestFailed(const std::string& label,
                             DeviceRequest* request,
                             content::MediaStreamRequestResult result);
  void FinalizeOpenDevice(const std::string& label,
                          DeviceRequest* request);
  void FinalizeMediaAccessRequest(const std::string& label,
                                  DeviceRequest* request,
                                  const MediaStreamDevices& devices);
  void FinalizeEnumerateDevices(const std::string& label,
                                DeviceRequest* request);

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

  // Finds the requested device id from constraints. The requested device type
  // must be MEDIA_DEVICE_AUDIO_CAPTURE or MEDIA_DEVICE_VIDEO_CAPTURE.
  bool GetRequestedDeviceCaptureId(const DeviceRequest* request,
                                   MediaStreamType type,
                                   std::string* device_id) const;

  void TranslateDeviceIdToSourceId(DeviceRequest* request,
                                   MediaStreamDevice* device);

  // Helper method that sends log messages to the render process hosts whose
  // corresponding render processes are in |render_process_ids|, to be used by
  // the webrtcLoggingPrivate API if requested.
  void AddLogMessageOnUIThread(const std::set<int>& render_process_ids,
                               const std::string& message);

  // Handles the callback from MediaStreamUIProxy to receive the UI window id,
  // used for excluding the notification window in desktop capturing.
  void OnMediaStreamUIWindowId(MediaStreamType video_type,
                               StreamDeviceInfoArray devices,
                               gfx::NativeViewId window_id);

  // Task runner shared by VideoCaptureManager and AudioInputDeviceManager and
  // used for enumerating audio output devices.
  // Note: Enumeration tasks may take seconds to complete so must never be run
  // on any of the BrowserThreads (UI, IO, etc).  See http://crbug.com/256945.
  scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;

  media::AudioManager* const audio_manager_;  // not owned
  scoped_refptr<AudioInputDeviceManager> audio_input_device_manager_;
  scoped_refptr<VideoCaptureManager> video_capture_manager_;

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

  // Hold a pointer to the IO loop to check we delete the device thread and
  // managers on the right thread.
  base::MessageLoop* io_loop_;

  bool use_fake_ui_;
  scoped_ptr<FakeMediaStreamUIProxy> fake_ui_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_MANAGER_H_

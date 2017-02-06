// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_manager.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include <cctype>
#include <list>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/power_monitor/power_monitor.h"
#include "base/profiler/scoped_tracker.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_local.h"
#include "build/build_config.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/audio_output_device_enumerator.h"
#include "content/browser/renderer_host/media/media_capture_devices_impl.h"
#include "content/browser/renderer_host/media/media_stream_requester.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/media_request_state.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/media_stream_request.h"
#include "crypto/hmac.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/media_switches.h"
#include "media/capture/video/video_capture_device_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#endif

#if defined(OS_MACOSX)
#include "media/capture/device_monitor_mac.h"
#endif

namespace content {

base::LazyInstance<base::ThreadLocalPointer<MediaStreamManager>>::Leaky
    g_media_stream_manager_tls_ptr = LAZY_INSTANCE_INITIALIZER;

namespace {
// Creates a random label used to identify requests.
std::string RandomLabel() {
  // An earlier PeerConnection spec [1] defined MediaStream::label alphabet as
  // an uuid with characters from range: U+0021, U+0023 to U+0027, U+002A to
  // U+002B, U+002D to U+002E, U+0030 to U+0039, U+0041 to U+005A, U+005E to
  // U+007E. That causes problems with searching for labels in bots, so we use a
  // safe alphanumeric subset |kAlphabet|.
  // [1] http://dev.w3.org/2011/webrtc/editor/webrtc.html
  static const char kAlphabet[] = "0123456789"
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

  static const size_t kRfc4122LengthLabel = 36u;
  std::string label(kRfc4122LengthLabel, ' ');
  for (char& c : label) {
    // Use |arraysize(kAlphabet) - 1| to avoid |kAlphabet|s terminating '\0';
    c = kAlphabet[base::RandGenerator(arraysize(kAlphabet) - 1)];
    DCHECK(std::isalnum(c)) << c;
  }
  return label;
}

void ParseStreamType(const StreamControls& controls,
                     MediaStreamType* audio_type,
                     MediaStreamType* video_type) {
  *audio_type = MEDIA_NO_SERVICE;
  *video_type = MEDIA_NO_SERVICE;
  const bool audio_support_flag_for_desktop_share =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAudioSupportForDesktopShare);
  if (controls.audio.requested) {
    if (!controls.audio.stream_source.empty()) {
       // This is tab or screen capture.
       if (controls.audio.stream_source == kMediaStreamSourceTab) {
         *audio_type = content::MEDIA_TAB_AUDIO_CAPTURE;
       } else if (controls.audio.stream_source == kMediaStreamSourceSystem) {
         *audio_type = content::MEDIA_DESKTOP_AUDIO_CAPTURE;
       } else if (audio_support_flag_for_desktop_share &&
                  controls.audio.stream_source == kMediaStreamSourceDesktop) {
         *audio_type = content::MEDIA_DESKTOP_AUDIO_CAPTURE;
       }
     } else {
       // This is normal audio device capture.
       *audio_type = MEDIA_DEVICE_AUDIO_CAPTURE;
     }
  }
  if (controls.video.requested) {
    if (!controls.video.stream_source.empty()) {
      // This is tab or screen capture.
      if (controls.video.stream_source == kMediaStreamSourceTab) {
        *video_type = content::MEDIA_TAB_VIDEO_CAPTURE;
      } else if (controls.video.stream_source == kMediaStreamSourceScreen) {
        *video_type = content::MEDIA_DESKTOP_VIDEO_CAPTURE;
      } else if (controls.video.stream_source == kMediaStreamSourceDesktop) {
        *video_type = content::MEDIA_DESKTOP_VIDEO_CAPTURE;
      }
    } else {
      // This is normal video device capture.
      *video_type = MEDIA_DEVICE_VIDEO_CAPTURE;
    }
  }
}

// Turns off available audio effects (removes the flag) if the options
// explicitly turn them off.
void FilterAudioEffects(const StreamControls& controls, int* effects) {
  DCHECK(effects);
  // TODO(ajm): Should we handle ECHO_CANCELLER here?
}

// Unlike other effects, hotword is off by default, so turn it on if it's
// requested and available.
void EnableHotwordEffect(const StreamControls& controls, int* effects) {
  DCHECK(effects);
  if (controls.hotword_enabled) {
#if defined(OS_CHROMEOS)
    chromeos::AudioDeviceList devices;
    chromeos::CrasAudioHandler::Get()->GetAudioDevices(&devices);
    // Only enable if a hotword device exists.
    for (const chromeos::AudioDevice& device : devices) {
      if (device.type == chromeos::AUDIO_TYPE_HOTWORD) {
        DCHECK(device.is_input);
        *effects |= media::AudioParameters::HOTWORD;
      }
    }
#endif
  }
}

// Private helper method to generate a string for the log message that lists the
// human readable names of |devices|.
std::string GetLogMessageString(MediaStreamType stream_type,
                                const StreamDeviceInfoArray& device_infos) {
  std::string output_string =
      base::StringPrintf("Getting devices for stream type %d:\n", stream_type);
  if (device_infos.empty())
    return output_string + "No devices found.";
  for (const content::StreamDeviceInfo& device_info : device_infos)
    output_string += "  " + device_info.device.name + "\n";
  return output_string;
}

// Clears the MediaStreamDevice.name from all devices in |devices|.
void ClearDeviceLabels(content::StreamDeviceInfoArray* devices) {
  for (content::StreamDeviceInfo& device_info : *devices)
    device_info.device.name.clear();
}

bool CalledOnIOThread() {
  // Check if this function call is on the IO thread, except for unittests where
  // an IO thread might not have been created.
  return BrowserThread::CurrentlyOn(BrowserThread::IO) ||
         !BrowserThread::IsMessageLoopValid(BrowserThread::IO);
}

GURL ConvertToGURL(const url::Origin& origin) {
  return origin.unique() ? GURL() : GURL(origin.Serialize());
}

}  // namespace


// MediaStreamManager::DeviceRequest represents a request to either enumerate
// available devices or open one or more devices.
// TODO(perkj): MediaStreamManager still needs refactoring. I propose we create
// several subclasses of DeviceRequest and move some of the responsibility of
// the MediaStreamManager to the subclasses to get rid of the way too many if
// statements in MediaStreamManager.
class MediaStreamManager::DeviceRequest {
 public:
  DeviceRequest(MediaStreamRequester* requester,
                int requesting_process_id,
                int requesting_frame_id,
                int page_request_id,
                const url::Origin& security_origin,
                bool user_gesture,
                MediaStreamRequestType request_type,
                const StreamControls& controls,
                const std::string& salt)
      : requester(requester),
        requesting_process_id(requesting_process_id),
        requesting_frame_id(requesting_frame_id),
        page_request_id(page_request_id),
        security_origin(security_origin),
        user_gesture(user_gesture),
        request_type(request_type),
        controls(controls),
        salt(salt),
        state_(NUM_MEDIA_TYPES, MEDIA_REQUEST_STATE_NOT_REQUESTED),
        audio_type_(MEDIA_NO_SERVICE),
        video_type_(MEDIA_NO_SERVICE),
        target_process_id_(-1),
        target_frame_id_(-1) {}

  ~DeviceRequest() {}

  void SetAudioType(MediaStreamType audio_type) {
    DCHECK(IsAudioInputMediaType(audio_type) ||
           audio_type == MEDIA_DEVICE_AUDIO_OUTPUT ||
           audio_type == MEDIA_NO_SERVICE);
    audio_type_ = audio_type;
  }

  MediaStreamType audio_type() const { return audio_type_; }

  void SetVideoType(MediaStreamType video_type) {
    DCHECK(IsVideoMediaType(video_type) || video_type == MEDIA_NO_SERVICE);
    video_type_ = video_type;
  }

  MediaStreamType video_type() const { return video_type_; }

  // Creates a MediaStreamRequest object that is used by this request when UI
  // is asked for permission and device selection.
  void CreateUIRequest(const std::string& requested_audio_device_id,
                       const std::string& requested_video_device_id) {
    DCHECK(!ui_request_);
    target_process_id_ = requesting_process_id;
    target_frame_id_ = requesting_frame_id;
    ui_request_.reset(new MediaStreamRequest(
        requesting_process_id, requesting_frame_id, page_request_id,
        ConvertToGURL(security_origin), user_gesture, request_type,
        requested_audio_device_id, requested_video_device_id, audio_type_,
        video_type_));
  }

  // Creates a tab capture specific MediaStreamRequest object that is used by
  // this request when UI is asked for permission and device selection.
  void CreateTabCaptureUIRequest(int target_render_process_id,
                                 int target_render_frame_id) {
    DCHECK(!ui_request_);
    target_process_id_ = target_render_process_id;
    target_frame_id_ = target_render_frame_id;
    ui_request_.reset(new MediaStreamRequest(
        target_render_process_id, target_render_frame_id, page_request_id,
        ConvertToGURL(security_origin), user_gesture, request_type, "", "",
        audio_type_, video_type_));
  }

  bool HasUIRequest() const { return ui_request_.get() != nullptr; }
  std::unique_ptr<MediaStreamRequest> DetachUIRequest() {
    return std::move(ui_request_);
  }

  // Update the request state and notify observers.
  void SetState(MediaStreamType stream_type, MediaRequestState new_state) {
    if (stream_type == NUM_MEDIA_TYPES) {
      for (int i = MEDIA_NO_SERVICE + 1; i < NUM_MEDIA_TYPES; ++i) {
        const MediaStreamType stream_type = static_cast<MediaStreamType>(i);
        state_[stream_type] = new_state;
      }
    } else {
      state_[stream_type] = new_state;
    }

    MediaObserver* media_observer =
        GetContentClient()->browser()->GetMediaObserver();
    if (!media_observer)
      return;

    media_observer->OnMediaRequestStateChanged(
        target_process_id_, target_frame_id_, page_request_id,
        ConvertToGURL(security_origin), stream_type, new_state);
  }

  MediaRequestState state(MediaStreamType stream_type) const {
    return state_[stream_type];
  }

  void SetCapturingLinkSecured(bool is_secure) {
    MediaObserver* media_observer =
        GetContentClient()->browser()->GetMediaObserver();
    if (!media_observer)
      return;

    media_observer->OnSetCapturingLinkSecured(target_process_id_,
                                              target_frame_id_, page_request_id,
                                              video_type_, is_secure);
  }

  MediaStreamRequester* const requester;  // Can be NULL.


  // The render process id that requested this stream to be generated and that
  // will receive a handle to the MediaStream. This may be different from
  // MediaStreamRequest::render_process_id which in the tab capture case
  // specifies the target renderer from which audio and video is captured.
  const int requesting_process_id;

  // The render frame id that requested this stream to be generated and that
  // will receive a handle to the MediaStream. This may be different from
  // MediaStreamRequest::render_frame_id which in the tab capture case
  // specifies the target renderer from which audio and video is captured.
  const int requesting_frame_id;

  // An ID the render frame provided to identify this request.
  const int page_request_id;

  const url::Origin security_origin;

  const bool user_gesture;

  const MediaStreamRequestType request_type;

  const StreamControls controls;

  const std::string salt;

  StreamDeviceInfoArray devices;

  // Callback to the requester which audio/video devices have been selected.
  // It can be null if the requester has no interest to know the result.
  // Currently it is only used by |DEVICE_ACCESS| type.
  MediaStreamManager::MediaRequestResponseCallback callback;

  std::unique_ptr<MediaStreamUIProxy> ui_proxy;

  std::string tab_capture_device_id;

 private:
  std::vector<MediaRequestState> state_;
  std::unique_ptr<MediaStreamRequest> ui_request_;
  MediaStreamType audio_type_;
  MediaStreamType video_type_;
  int target_process_id_;
  int target_frame_id_;
};

MediaStreamManager::EnumerationCache::EnumerationCache()
    : valid(false) {
}

MediaStreamManager::EnumerationCache::~EnumerationCache() {
}

// static
void MediaStreamManager::SendMessageToNativeLog(const std::string& message) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        base::Bind(&MediaStreamManager::SendMessageToNativeLog, message));
    return;
  }

  MediaStreamManager* msm = g_media_stream_manager_tls_ptr.Pointer()->Get();
  if (!msm) {
    DLOG(ERROR) << "No MediaStreamManager on the IO thread. " << message;
    return;
  }

  msm->AddLogMessageOnIOThread(message);
}

MediaStreamManager::MediaStreamManager(media::AudioManager* audio_manager)
    : audio_manager_(audio_manager),
#if defined(OS_WIN)
      video_capture_thread_("VideoCaptureThread"),
#endif
      monitoring_started_(false),
      use_fake_ui_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForMediaStream)) {
  DCHECK(audio_manager_);
  memset(active_enumeration_ref_count_, 0,
         sizeof(active_enumeration_ref_count_));

  // Some unit tests create the MSM in the IO thread and assumes the
  // initialization is done synchronously.
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    InitializeDeviceManagersOnIOThread();
  } else {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&MediaStreamManager::InitializeDeviceManagersOnIOThread,
                   base::Unretained(this)));
  }

  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  // BrowserMainLoop always creates the PowerMonitor instance before creating
  // MediaStreamManager, but power_monitor may be NULL in unit tests.
  if (power_monitor)
    power_monitor->AddObserver(this);
}

MediaStreamManager::~MediaStreamManager() {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::IO));
  DVLOG(1) << "~MediaStreamManager";
  DCHECK(requests_.empty());
  DCHECK(!device_task_runner_.get());
  DCHECK(device_change_subscribers_.empty());

  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  // The PowerMonitor instance owned by BrowserMainLoops always outlives the
  // MediaStreamManager, but it may be NULL in unit tests.
  if (power_monitor)
    power_monitor->RemoveObserver(this);
}

VideoCaptureManager* MediaStreamManager::video_capture_manager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(video_capture_manager_.get());
  return video_capture_manager_.get();
}

AudioInputDeviceManager* MediaStreamManager::audio_input_device_manager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(audio_input_device_manager_.get());
  return audio_input_device_manager_.get();
}

AudioOutputDeviceEnumerator*
MediaStreamManager::audio_output_device_enumerator() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(audio_output_device_enumerator_.get());
  return audio_output_device_enumerator_.get();
}

std::string MediaStreamManager::MakeMediaAccessRequest(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    const StreamControls& controls,
    const url::Origin& security_origin,
    const MediaRequestResponseCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(perkj): The argument list with NULL parameters to DeviceRequest
  // suggests that this is the wrong design. Can this be refactored?
  DeviceRequest* request =
      new DeviceRequest(NULL, render_process_id, render_frame_id,
                        page_request_id, security_origin,
                        false,  // user gesture
                        MEDIA_DEVICE_ACCESS, controls, std::string());

  const std::string& label = AddRequest(request);

  request->callback = callback;
  // Post a task and handle the request asynchronously. The reason is that the
  // requester won't have a label for the request until this function returns
  // and thus can not handle a response. Using base::Unretained is safe since
  // MediaStreamManager is deleted on the UI thread, after the IO thread has
  // been stopped.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamManager::SetupRequest,
                 base::Unretained(this), label));
  return label;
}

void MediaStreamManager::GenerateStream(MediaStreamRequester* requester,
                                        int render_process_id,
                                        int render_frame_id,
                                        const std::string& salt,
                                        int page_request_id,
                                        const StreamControls& controls,
                                        const url::Origin& security_origin,
                                        bool user_gesture) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "GenerateStream()";

  DeviceRequest* request = new DeviceRequest(
      requester, render_process_id, render_frame_id, page_request_id,
      security_origin, user_gesture, MEDIA_GENERATE_STREAM, controls, salt);

  const std::string& label = AddRequest(request);

  // Post a task and handle the request asynchronously. The reason is that the
  // requester won't have a label for the request until this function returns
  // and thus can not handle a response. Using base::Unretained is safe since
  // MediaStreamManager is deleted on the UI thread, after the IO thread has
  // been stopped.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamManager::SetupRequest,
                 base::Unretained(this), label));
}

void MediaStreamManager::CancelRequest(int render_process_id,
                                       int render_frame_id,
                                       int page_request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const LabeledDeviceRequest& labeled_request : requests_) {
    DeviceRequest* const request = labeled_request.second;
    if (request->requesting_process_id == render_process_id &&
        request->requesting_frame_id == render_frame_id &&
        request->page_request_id == page_request_id) {
      CancelRequest(labeled_request.first);
      return;
    }
  }
  NOTREACHED();
}

void MediaStreamManager::CancelRequest(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "CancelRequest({label = " << label <<  "})";
  DeviceRequest* request = FindRequest(label);
  if (!request) {
    // The request does not exist.
    LOG(ERROR) << "The request with label = " << label  << " does not exist.";
    return;
  }

  if (request->request_type == MEDIA_ENUMERATE_DEVICES) {
    // It isn't an ideal use of "CancelRequest" to make it a requirement
    // for enumeration requests to be deleted via "CancelRequest" _after_
    // the request has been successfully fulfilled.
    // See note in FinalizeEnumerateDevices for a recommendation on how
    // we should refactor this.
    DeleteRequest(label);
    return;
  }

  // This is a request for opening one or more devices.
  for (const StreamDeviceInfo& device_info : request->devices) {
    const MediaRequestState state = request->state(device_info.device.type);
    // If we have not yet requested the device to be opened - just ignore it.
    if (state != MEDIA_REQUEST_STATE_OPENING &&
        state != MEDIA_REQUEST_STATE_DONE) {
      continue;
    }
    // Stop the opening/opened devices of the requests.
    CloseDevice(device_info.device.type, device_info.session_id);
  }

  // Cancel the request if still pending at UI side.
  request->SetState(NUM_MEDIA_TYPES, MEDIA_REQUEST_STATE_CLOSING);
  DeleteRequest(label);
}

void MediaStreamManager::CancelAllRequests(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DeviceRequests::iterator request_it = requests_.begin();
  while (request_it != requests_.end()) {
    if (request_it->second->requesting_process_id != render_process_id) {
      ++request_it;
      continue;
    }
    const std::string label = request_it->first;
    ++request_it;
    CancelRequest(label);
  }
}

void MediaStreamManager::StopStreamDevice(int render_process_id,
                                          int render_frame_id,
                                          const std::string& device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "StopStreamDevice({render_frame_id = " << render_frame_id <<  "} "
           << ", {device_id = " << device_id << "})";
  // Find the first request for this |render_process_id| and |render_frame_id|
  // of type MEDIA_GENERATE_STREAM that has requested to use |device_id| and
  // stop it.
  for (const LabeledDeviceRequest& device_request : requests_) {
    DeviceRequest* const request = device_request.second;
    if (request->requesting_process_id != render_process_id ||
        request->requesting_frame_id != render_frame_id ||
        request->request_type != MEDIA_GENERATE_STREAM) {
      continue;
    }

    for (const StreamDeviceInfo& device_info : request->devices) {
      if (device_info.device.id == device_id) {
        StopDevice(device_info.device.type, device_info.session_id);
        return;
      }
    }
  }
}

int MediaStreamManager::VideoDeviceIdToSessionId(
    const std::string& device_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (const LabeledDeviceRequest& device_request : requests_) {
    for (const StreamDeviceInfo& info : device_request.second->devices) {
      if (info.device.id == device_id) {
        DCHECK_EQ(MEDIA_DEVICE_VIDEO_CAPTURE, info.device.type);
        return info.session_id;
      }
    }
  }
  return StreamDeviceInfo::kNoId;
}

void MediaStreamManager::StopDevice(MediaStreamType type, int session_id) {
  DVLOG(1) << "StopDevice"
           << "{type = " << type << "}"
           << "{session_id = " << session_id << "}";
  DeviceRequests::iterator request_it = requests_.begin();
  while (request_it != requests_.end()) {
    DeviceRequest* request = request_it->second;
    StreamDeviceInfoArray* devices = &request->devices;
    if (devices->empty()) {
      // There is no device in use yet by this request.
      ++request_it;
      continue;
    }
    StreamDeviceInfoArray::iterator device_it = devices->begin();
    while (device_it != devices->end()) {
      if (device_it->device.type != type ||
          device_it->session_id != session_id) {
        ++device_it;
        continue;
      }

      if (request->state(type) == MEDIA_REQUEST_STATE_DONE)
        CloseDevice(type, session_id);
      device_it = devices->erase(device_it);
    }

    // If this request doesn't have any active devices after a device
    // has been stopped above, remove the request. Note that the request is
    // only deleted if a device as been removed from |devices|.
    if (devices->empty()) {
      std::string label = request_it->first;
      ++request_it;
      DeleteRequest(label);
    } else {
      ++request_it;
    }
  }
}

void MediaStreamManager::CloseDevice(MediaStreamType type, int session_id) {
  DVLOG(1) << "CloseDevice("
           << "{type = " << type <<  "} "
           << "{session_id = " << session_id << "})";
  GetDeviceManager(type)->Close(session_id);

  for (const LabeledDeviceRequest& labeled_request : requests_) {
    DeviceRequest* const request = labeled_request.second;
    for (const StreamDeviceInfo& device_info : request->devices) {
      if (device_info.session_id == session_id &&
          device_info.device.type == type) {
        // Notify observers that this device is being closed.
        // Note that only one device per type can be opened.
        request->SetState(type, MEDIA_REQUEST_STATE_CLOSING);
      }
    }
  }
}

std::string MediaStreamManager::EnumerateDevices(
    MediaStreamRequester* requester,
    int render_process_id,
    int render_frame_id,
    const std::string& salt,
    int page_request_id,
    MediaStreamType type,
    const url::Origin& security_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(requester);
  DCHECK(type == MEDIA_DEVICE_AUDIO_CAPTURE ||
         type == MEDIA_DEVICE_VIDEO_CAPTURE ||
         type == MEDIA_DEVICE_AUDIO_OUTPUT);

  DeviceRequest* request =
      new DeviceRequest(requester, render_process_id, render_frame_id,
                        page_request_id, security_origin,
                        false,  // user gesture
                        MEDIA_ENUMERATE_DEVICES, StreamControls(), salt);
  if (IsAudioInputMediaType(type) || type == MEDIA_DEVICE_AUDIO_OUTPUT)
    request->SetAudioType(type);
  else if (IsVideoMediaType(type))
    request->SetVideoType(type);

  const std::string& label = AddRequest(request);
  // Post a task and handle the request asynchronously. The reason is that the
  // requester won't have a label for the request until this function returns
  // and thus can not handle a response. Using base::Unretained is safe since
  // MediaStreamManager is deleted on the UI thread, after the IO thread has
  // been stopped.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamManager::DoEnumerateDevices,
                 base::Unretained(this), label));
  return label;
}

void MediaStreamManager::DoEnumerateDevices(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DeviceRequest* request = FindRequest(label);
  if (!request)
    return;  // This can happen if the request has been canceled.

  if (request->audio_type() == MEDIA_DEVICE_AUDIO_OUTPUT) {
    DCHECK_EQ(MEDIA_NO_SERVICE, request->video_type());
    DCHECK_GE(active_enumeration_ref_count_[MEDIA_DEVICE_AUDIO_OUTPUT], 0);
    request->SetState(MEDIA_DEVICE_AUDIO_OUTPUT, MEDIA_REQUEST_STATE_REQUESTED);
    if (active_enumeration_ref_count_[MEDIA_DEVICE_AUDIO_OUTPUT] == 0) {
      ++active_enumeration_ref_count_[MEDIA_DEVICE_AUDIO_OUTPUT];
      DCHECK(audio_output_device_enumerator_);
      audio_output_device_enumerator_->Enumerate(
          base::Bind(&MediaStreamManager::AudioOutputDevicesEnumerated,
                     base::Unretained(this)));
    }
    return;
  }

  MediaStreamType type;
  EnumerationCache* cache;
  if (request->audio_type() == MEDIA_DEVICE_AUDIO_CAPTURE) {
    DCHECK_EQ(MEDIA_NO_SERVICE, request->video_type());
    type = MEDIA_DEVICE_AUDIO_CAPTURE;
    cache = &audio_enumeration_cache_;
  } else {
    DCHECK_EQ(MEDIA_DEVICE_VIDEO_CAPTURE, request->video_type());
    DCHECK_EQ(MEDIA_NO_SERVICE, request->audio_type());
    type = MEDIA_DEVICE_VIDEO_CAPTURE;
    cache = &video_enumeration_cache_;
  }

  if (!EnumerationRequired(cache, type)) {
    // Cached device list of this type exists. Just send it out.
    request->SetState(type, MEDIA_REQUEST_STATE_REQUESTED);
    request->devices = cache->devices;
    FinalizeEnumerateDevices(label, request);
  } else {
    StartEnumeration(request);
  }
  DVLOG(1) << "Enumerate Devices ({label = " << label <<  "})";
}

void MediaStreamManager::AudioOutputDevicesEnumerated(
    const AudioOutputDeviceEnumeration& device_enumeration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "AudioOutputDevicesEnumerated()";
  StreamDeviceInfoArray device_infos;

  if (device_enumeration.has_actual_devices) {
    for (const auto& entry : device_enumeration.devices) {
      StreamDeviceInfo device_info(MEDIA_DEVICE_AUDIO_OUTPUT, entry.device_name,
                                   entry.unique_id);
      device_infos.push_back(device_info);
    }
  }

  const std::string log_message =
      "New device enumeration result:\n" +
      GetLogMessageString(MEDIA_DEVICE_AUDIO_OUTPUT, device_infos);
  SendMessageToNativeLog(log_message);

  // Publish the result for all requests waiting for device list(s).
  for (const LabeledDeviceRequest& request : requests_) {
    if (request.second->state(MEDIA_DEVICE_AUDIO_OUTPUT) ==
            MEDIA_REQUEST_STATE_REQUESTED &&
        request.second->audio_type() == MEDIA_DEVICE_AUDIO_OUTPUT) {
      DCHECK_EQ(MEDIA_ENUMERATE_DEVICES, request.second->request_type);
      request.second->SetState(MEDIA_DEVICE_AUDIO_OUTPUT,
                               MEDIA_REQUEST_STATE_PENDING_APPROVAL);
      request.second->devices = device_infos;
      FinalizeEnumerateDevices(request.first, request.second);
    }
  }

  --active_enumeration_ref_count_[MEDIA_DEVICE_AUDIO_OUTPUT];
  DCHECK_GE(active_enumeration_ref_count_[MEDIA_DEVICE_AUDIO_OUTPUT], 0);
}

void MediaStreamManager::OpenDevice(MediaStreamRequester* requester,
                                    int render_process_id,
                                    int render_frame_id,
                                    const std::string& salt,
                                    int page_request_id,
                                    const std::string& device_id,
                                    MediaStreamType type,
                                    const url::Origin& security_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(type == MEDIA_DEVICE_AUDIO_CAPTURE ||
         type == MEDIA_DEVICE_VIDEO_CAPTURE);
  DVLOG(1) << "OpenDevice ({page_request_id = " << page_request_id <<  "})";
  StreamControls controls;
  if (IsAudioInputMediaType(type)) {
    controls.audio.requested = true;
    controls.audio.device_ids.push_back(device_id);
  } else if (IsVideoMediaType(type)) {
    controls.video.requested = true;
    controls.video.device_ids.push_back(device_id);
  } else {
    NOTREACHED();
  }
  DeviceRequest* request =
      new DeviceRequest(requester, render_process_id, render_frame_id,
                        page_request_id, security_origin,
                        false,  // user gesture
                        MEDIA_OPEN_DEVICE_PEPPER_ONLY, controls, salt);

  const std::string& label = AddRequest(request);
  // Post a task and handle the request asynchronously. The reason is that the
  // requester won't have a label for the request until this function returns
  // and thus can not handle a response. Using base::Unretained is safe since
  // MediaStreamManager is deleted on the UI thread, after the IO thread has
  // been stopped.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamManager::SetupRequest,
                 base::Unretained(this), label));
}

bool MediaStreamManager::TranslateSourceIdToDeviceId(
    MediaStreamType stream_type,
    const std::string& salt,
    const url::Origin& security_origin,
    const std::string& source_id,
    std::string* device_id) const {
  DCHECK(stream_type == MEDIA_DEVICE_AUDIO_CAPTURE ||
         stream_type == MEDIA_DEVICE_VIDEO_CAPTURE);
  // The source_id can be empty if the constraint is set but empty.
  if (source_id.empty())
    return false;

  const EnumerationCache* cache =
      stream_type == MEDIA_DEVICE_AUDIO_CAPTURE ?
      &audio_enumeration_cache_ : &video_enumeration_cache_;

  // If device monitoring hasn't started, the |device_guid| is not valid.
  if (!cache->valid)
    return false;

  for (const StreamDeviceInfo& device_info : cache->devices) {
    if (DoesMediaDeviceIDMatchHMAC(salt, security_origin, source_id,
                                   device_info.device.id)) {
      *device_id = device_info.device.id;
      return true;
    }
  }
  return false;
}

void MediaStreamManager::EnsureDeviceMonitorStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  StartMonitoring();
}

void MediaStreamManager::StopRemovedDevices(
    const StreamDeviceInfoArray& old_devices,
    const StreamDeviceInfoArray& new_devices) {
  DVLOG(1) << "StopRemovedDevices("
           << "{#old_devices = " << old_devices.size() <<  "} "
           << "{#new_devices = " << new_devices.size() << "})";
  for (const StreamDeviceInfo& old_device_info : old_devices) {
    bool device_found = false;
    for (const StreamDeviceInfo& new_device_info : new_devices) {
      if (old_device_info.device.id == new_device_info.device.id) {
        device_found = true;
        break;
      }
    }

    if (!device_found) {
      // A device has been removed. We need to check if it is used by a
      // MediaStream and in that case cleanup and notify the render process.
      StopRemovedDevice(old_device_info.device);
    }
  }
}

void MediaStreamManager::StopRemovedDevice(const MediaStreamDevice& device) {
  std::vector<int> session_ids;
  for (const LabeledDeviceRequest& labeled_request : requests_) {
    const DeviceRequest* request = labeled_request.second;
    for (const StreamDeviceInfo& device_info : request->devices) {
      const std::string source_id = GetHMACForMediaDeviceID(
          request->salt, request->security_origin, device.id);
      if (device_info.device.id == source_id &&
          device_info.device.type == device.type) {
        session_ids.push_back(device_info.session_id);
        if (labeled_request.second->requester) {
          labeled_request.second->requester->DeviceStopped(
              labeled_request.second->requesting_frame_id,
              labeled_request.first, device_info);
        }
      }
    }
  }
  for (const int session_id : session_ids)
    StopDevice(device.type, session_id);

  AddLogMessageOnIOThread(
      base::StringPrintf(
          "Media input device removed: type = %s, id = %s, name = %s ",
          (device.type == MEDIA_DEVICE_AUDIO_CAPTURE ? "audio" : "video"),
          device.id.c_str(), device.name.c_str()).c_str());
}

void MediaStreamManager::StartMonitoring() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (monitoring_started_)
    return;

  if (!base::SystemMonitor::Get())
    return;

  monitoring_started_ = true;
  base::SystemMonitor::Get()->AddDevicesChangedObserver(this);

  // Enumerate both the audio and video input devices to cache the device lists
  // and send them to media observer.
  ++active_enumeration_ref_count_[MEDIA_DEVICE_AUDIO_CAPTURE];
  audio_input_device_manager_->EnumerateDevices(MEDIA_DEVICE_AUDIO_CAPTURE);
  ++active_enumeration_ref_count_[MEDIA_DEVICE_VIDEO_CAPTURE];
  video_capture_manager_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);

#if defined(OS_MACOSX)
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MediaStreamManager::StartMonitoringOnUIThread,
                 base::Unretained(this)));
#endif
}

void MediaStreamManager::StopMonitoring() {
  DCHECK(CalledOnIOThread());
  if (!monitoring_started_)
    return;
  base::SystemMonitor::Get()->RemoveDevicesChangedObserver(this);
  monitoring_started_ = false;
  ClearEnumerationCache(&audio_enumeration_cache_);
  ClearEnumerationCache(&video_enumeration_cache_);
  audio_output_device_enumerator_->SetCachePolicy(
      AudioOutputDeviceEnumerator::CACHE_POLICY_NO_CACHING);
}

#if defined(OS_MACOSX)
void MediaStreamManager::StartMonitoringOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(erikchen): Remove ScopedTracker below once crbug.com/458404 is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "458404 MediaStreamManager::GetBrowserMainLoop"));
  BrowserMainLoop* browser_main_loop = content::BrowserMainLoop::GetInstance();
  if (!browser_main_loop)
    return;

  // TODO(erikchen): Remove ScopedTracker below once crbug.com/458404 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "458404 MediaStreamManager::GetTaskRunner"));
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      audio_manager_->GetTaskRunner();
  // TODO(erikchen): Remove ScopedTracker below once crbug.com/458404 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "458404 MediaStreamManager::DeviceMonitorMac::StartMonitoring"));
  browser_main_loop->device_monitor_mac()->StartMonitoring(task_runner);
}
#endif

// Pick the first valid (translatable) device ID from lists of required
// and optional IDs.
bool MediaStreamManager::PickDeviceId(MediaStreamType type,
                                      const std::string& salt,
                                      const url::Origin& security_origin,
                                      const TrackControls& controls,
                                      std::string* device_id) const {
  if (!controls.device_ids.empty()) {
    if (controls.device_ids.size() > 1) {
      LOG(ERROR) << "Only one required device ID is supported";
      return false;
    }
    const std::string& candidate_id = controls.device_ids[0];
    if (!TranslateSourceIdToDeviceId(type, salt, security_origin, candidate_id,
                                     device_id)) {
      LOG(WARNING) << "Invalid mandatory capture ID = " << candidate_id;
      return false;
    }
    return true;
  }
  // We don't have a required ID. Look at the alternates.
  for (const std::string& candidate_id : controls.alternate_device_ids) {
    if (TranslateSourceIdToDeviceId(type, salt, security_origin, candidate_id,
                                    device_id)) {
      return true;
    } else {
      LOG(WARNING) << "Invalid optional capture ID = " << candidate_id;
    }
  }
  return true;  // If we get here, device_id is empty.
}

bool MediaStreamManager::GetRequestedDeviceCaptureId(
    const DeviceRequest* request,
    MediaStreamType type,
    std::string* device_id) const {
  if (type == MEDIA_DEVICE_AUDIO_CAPTURE) {
    return PickDeviceId(type, request->salt, request->security_origin,
                        request->controls.audio, device_id);
  } else if (type == MEDIA_DEVICE_VIDEO_CAPTURE) {
    return PickDeviceId(type, request->salt, request->security_origin,
                        request->controls.video, device_id);
  } else {
    NOTREACHED();
  }
  return false;
}

void MediaStreamManager::TranslateDeviceIdToSourceId(
    DeviceRequest* request,
    MediaStreamDevice* device) {
  if (request->audio_type() == MEDIA_DEVICE_AUDIO_CAPTURE ||
      request->audio_type() == MEDIA_DEVICE_AUDIO_OUTPUT ||
      request->video_type() == MEDIA_DEVICE_VIDEO_CAPTURE) {
    device->id = GetHMACForMediaDeviceID(request->salt,
                                         request->security_origin, device->id);
  }
}

void MediaStreamManager::ClearEnumerationCache(EnumerationCache* cache) {
  DCHECK(CalledOnIOThread());
  cache->valid = false;
}

bool MediaStreamManager::EnumerationRequired(EnumerationCache* cache,
                                             MediaStreamType stream_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (stream_type == MEDIA_NO_SERVICE)
    return false;

  DCHECK(stream_type == MEDIA_DEVICE_AUDIO_CAPTURE ||
         stream_type == MEDIA_DEVICE_VIDEO_CAPTURE);

#if defined(OS_ANDROID)
  // There's no SystemMonitor on Android that notifies us when devices are
  // added or removed, so we need to populate the cache on every request.
  // Fortunately, there is an already up-to-date cache in the browser side
  // audio manager that we can rely on, so the performance impact of
  // invalidating the cache like this, is minimal.
  if (stream_type == MEDIA_DEVICE_AUDIO_CAPTURE) {
    // Make sure the cache is marked as invalid so that FinalizeEnumerateDevices
    // will be called at the end of the enumeration.
    ClearEnumerationCache(cache);
  }
#endif
  // If the cache isn't valid, we need to start a full enumeration.
  return !cache->valid;
}

void MediaStreamManager::StartEnumeration(DeviceRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Start monitoring the devices when doing the first enumeration.
  StartMonitoring();

  // Start enumeration for devices of all requested device types.
  const MediaStreamType stream_types[] = {request->audio_type(),
                                          request->video_type()};
  for (const MediaStreamType stream_type : stream_types) {
    if (stream_type == MEDIA_NO_SERVICE)
      continue;
    request->SetState(stream_type, MEDIA_REQUEST_STATE_REQUESTED);
    DCHECK_GE(active_enumeration_ref_count_[stream_type], 0);
    if (active_enumeration_ref_count_[stream_type] == 0) {
      ++active_enumeration_ref_count_[stream_type];
      GetDeviceManager(stream_type)->EnumerateDevices(stream_type);
    }
  }
}

std::string MediaStreamManager::AddRequest(DeviceRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Create a label for this request and verify it is unique.
  std::string unique_label;
  do {
    unique_label = RandomLabel();
  } while (FindRequest(unique_label) != NULL);

  requests_.push_back(std::make_pair(unique_label, request));

  return unique_label;
}

MediaStreamManager::DeviceRequest*
MediaStreamManager::FindRequest(const std::string& label) const {
  for (const LabeledDeviceRequest& labeled_request : requests_) {
    if (labeled_request.first == label)
      return labeled_request.second;
  }
  return NULL;
}

void MediaStreamManager::DeleteRequest(const std::string& label) {
  DVLOG(1) << "DeleteRequest({label= " << label << "})";
  for (DeviceRequests::iterator request_it = requests_.begin();
       request_it != requests_.end(); ++request_it) {
    if (request_it->first == label) {
      std::unique_ptr<DeviceRequest> request(request_it->second);
      requests_.erase(request_it);
      return;
    }
  }
  NOTREACHED();
}

void MediaStreamManager::ReadOutputParamsAndPostRequestToUI(
    const std::string& label,
    DeviceRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Actual audio parameters are required only for MEDIA_TAB_AUDIO_CAPTURE.
  // TODO(guidou): MEDIA_TAB_AUDIO_CAPTURE should not be a special case. See
  // crbug.com/584287.
  if (request->audio_type() == MEDIA_TAB_AUDIO_CAPTURE) {
    // Read output parameters on the correct thread for native audio OS calls.
    // Using base::Unretained is safe since |audio_manager_| is deleted after
    // its task runner, and MediaStreamManager is deleted on the UI thread,
    // after the IO thread has been stopped.
    base::PostTaskAndReplyWithResult(
        audio_manager_->GetTaskRunner(), FROM_HERE,
        base::Bind(&media::AudioManager::GetDefaultOutputStreamParameters,
                   base::Unretained(audio_manager_)),
        base::Bind(&MediaStreamManager::PostRequestToUI, base::Unretained(this),
                   label, request));
  } else {
    PostRequestToUI(label, request, media::AudioParameters());
  }
}

void MediaStreamManager::PostRequestToUI(
    const std::string& label,
    DeviceRequest* request,
    const media::AudioParameters& output_parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request->HasUIRequest());
  DVLOG(1) << "PostRequestToUI({label= " << label << "})";

  const MediaStreamType audio_type = request->audio_type();
  const MediaStreamType video_type = request->video_type();

  // Post the request to UI and set the state.
  if (IsAudioInputMediaType(audio_type))
    request->SetState(audio_type, MEDIA_REQUEST_STATE_PENDING_APPROVAL);
  if (IsVideoMediaType(video_type))
    request->SetState(video_type, MEDIA_REQUEST_STATE_PENDING_APPROVAL);

  // If using the fake UI, it will just auto-select from the available devices.
  // The fake UI doesn't work for desktop sharing requests since we can't see
  // its devices from here; always use the real UI for such requests.
  if (use_fake_ui_ && request->video_type() != MEDIA_DESKTOP_VIDEO_CAPTURE) {
    if (!fake_ui_)
      fake_ui_.reset(new FakeMediaStreamUIProxy());

    MediaStreamDevices devices;
    if (audio_enumeration_cache_.valid) {
      for (const StreamDeviceInfo& device_info :
           audio_enumeration_cache_.devices) {
        devices.push_back(device_info.device);
      }
    }
    if (video_enumeration_cache_.valid) {
      for (const StreamDeviceInfo& device_info :
           video_enumeration_cache_.devices) {
        devices.push_back(device_info.device);
      }
    }

    fake_ui_->SetAvailableDevices(devices);

    request->ui_proxy = std::move(fake_ui_);
  } else {
    request->ui_proxy = MediaStreamUIProxy::Create();
  }

  request->ui_proxy->RequestAccess(
      request->DetachUIRequest(),
      base::Bind(&MediaStreamManager::HandleAccessRequestResponse,
                 base::Unretained(this), label, output_parameters));
}

void MediaStreamManager::SetupRequest(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DeviceRequest* request = FindRequest(label);
  if (!request) {
    DVLOG(1) << "SetupRequest label " << label << " doesn't exist!!";
    return;  // This can happen if the request has been canceled.
  }

  MediaStreamType audio_type = MEDIA_NO_SERVICE;
  MediaStreamType video_type = MEDIA_NO_SERVICE;
  ParseStreamType(request->controls, &audio_type, &video_type);
  request->SetAudioType(audio_type);
  request->SetVideoType(video_type);

  const bool is_web_contents_capture = audio_type == MEDIA_TAB_AUDIO_CAPTURE ||
                                       video_type == MEDIA_TAB_VIDEO_CAPTURE;
  if (is_web_contents_capture && !SetupTabCaptureRequest(request)) {
    FinalizeRequestFailed(label,
                          request,
                          MEDIA_DEVICE_TAB_CAPTURE_FAILURE);
    return;
  }

  const bool is_screen_capture = video_type == MEDIA_DESKTOP_VIDEO_CAPTURE;
  if (is_screen_capture && !SetupScreenCaptureRequest(request)) {
    FinalizeRequestFailed(label,
                          request,
                          MEDIA_DEVICE_SCREEN_CAPTURE_FAILURE);
    return;
  }

  if (!is_web_contents_capture && !is_screen_capture) {
    if (EnumerationRequired(&audio_enumeration_cache_, audio_type) ||
        EnumerationRequired(&video_enumeration_cache_, video_type)) {
      // Enumerate the devices if there is no valid device lists to be used.
      StartEnumeration(request);
      return;
    } else {
        // Cache is valid, so log the cached devices for MediaStream requests.
      if (request->request_type == MEDIA_GENERATE_STREAM) {
        std::string log_message("Using cached devices for request.\n");
        if (audio_type != MEDIA_NO_SERVICE) {
          log_message +=
              GetLogMessageString(audio_type, audio_enumeration_cache_.devices);
        }
        if (video_type != MEDIA_NO_SERVICE) {
          log_message +=
              GetLogMessageString(video_type, video_enumeration_cache_.devices);
        }
        SendMessageToNativeLog(log_message);
      }
    }

    if (!SetupDeviceCaptureRequest(request)) {
      FinalizeRequestFailed(label, request, MEDIA_DEVICE_NO_HARDWARE);
      return;
    }
  }
  ReadOutputParamsAndPostRequestToUI(label, request);
}

bool MediaStreamManager::SetupDeviceCaptureRequest(DeviceRequest* request) {
  DCHECK((request->audio_type() == MEDIA_DEVICE_AUDIO_CAPTURE ||
          request->audio_type() == MEDIA_NO_SERVICE) &&
         (request->video_type() == MEDIA_DEVICE_VIDEO_CAPTURE ||
          request->video_type() == MEDIA_NO_SERVICE));
  std::string audio_device_id;
  if (request->controls.audio.requested &&
      !GetRequestedDeviceCaptureId(request, request->audio_type(),
                                   &audio_device_id)) {
    return false;
  }

  std::string video_device_id;
  if (request->controls.video.requested &&
      !GetRequestedDeviceCaptureId(request, request->video_type(),
                                   &video_device_id)) {
    return false;
  }
  request->CreateUIRequest(audio_device_id, video_device_id);
  DVLOG(3) << "Audio requested " << request->controls.audio.requested
           << " device id = " << audio_device_id << "Video requested "
           << request->controls.video.requested
           << " device id = " << video_device_id;
  return true;
}

bool MediaStreamManager::SetupTabCaptureRequest(DeviceRequest* request) {
  DCHECK(request->audio_type() == MEDIA_TAB_AUDIO_CAPTURE ||
         request->video_type() == MEDIA_TAB_VIDEO_CAPTURE);

  std::string capture_device_id;
  if (!request->controls.audio.device_ids.empty()) {
    capture_device_id = request->controls.audio.device_ids[0];
  } else if (!request->controls.video.device_ids.empty()) {
    capture_device_id = request->controls.video.device_ids[0];
  } else {
    return false;
  }

  // Customize controls for a WebContents based capture.
  int target_render_process_id = 0;
  int target_render_frame_id = 0;

  bool has_valid_device_id = WebContentsMediaCaptureId::ExtractTabCaptureTarget(
      capture_device_id, &target_render_process_id, &target_render_frame_id);
  if (!has_valid_device_id ||
      (request->audio_type() != MEDIA_TAB_AUDIO_CAPTURE &&
       request->audio_type() != MEDIA_NO_SERVICE) ||
      (request->video_type() != MEDIA_TAB_VIDEO_CAPTURE &&
       request->video_type() != MEDIA_NO_SERVICE)) {
    return false;
  }
  request->tab_capture_device_id = capture_device_id;

  request->CreateTabCaptureUIRequest(target_render_process_id,
                                     target_render_frame_id);

  DVLOG(3) << "SetupTabCaptureRequest "
           << ", {capture_device_id = " << capture_device_id <<  "}"
           << ", {target_render_process_id = " << target_render_process_id
           << "}"
           << ", {target_render_frame_id = " << target_render_frame_id << "}";
  return true;
}

bool MediaStreamManager::SetupScreenCaptureRequest(DeviceRequest* request) {
  DCHECK(request->audio_type() == MEDIA_DESKTOP_AUDIO_CAPTURE ||
         request->video_type() == MEDIA_DESKTOP_VIDEO_CAPTURE);

  // For screen capture we only support two valid combinations:
  // (1) screen video capture only, or
  // (2) screen video capture with loopback audio capture.
  if (request->video_type() != MEDIA_DESKTOP_VIDEO_CAPTURE ||
      (request->audio_type() != MEDIA_NO_SERVICE &&
       request->audio_type() != MEDIA_DESKTOP_AUDIO_CAPTURE)) {
    LOG(ERROR) << "Invalid screen capture request.";
    return false;
  }

  std::string video_device_id;
  if (request->video_type() == MEDIA_DESKTOP_VIDEO_CAPTURE) {
    const std::string& video_stream_source =
        request->controls.video.stream_source;

    if (video_stream_source == kMediaStreamSourceDesktop &&
        !request->controls.video.device_ids.empty()) {
      video_device_id = request->controls.video.device_ids[0];
    }
  }

  const std::string audio_device_id =
      request->audio_type() == MEDIA_DESKTOP_AUDIO_CAPTURE ? video_device_id
                                                           : "";

  request->CreateUIRequest(audio_device_id, video_device_id);
  return true;
}

StreamDeviceInfoArray MediaStreamManager::GetDevicesOpenedByRequest(
    const std::string& label) const {
  DeviceRequest* request = FindRequest(label);
  if (!request)
    return StreamDeviceInfoArray();
  return request->devices;
}

bool MediaStreamManager::FindExistingRequestedDeviceInfo(
    const DeviceRequest& new_request,
    const MediaStreamDevice& new_device_info,
    StreamDeviceInfo* existing_device_info,
    MediaRequestState* existing_request_state) const {
  DCHECK(existing_device_info);
  DCHECK(existing_request_state);

  std::string source_id = GetHMACForMediaDeviceID(
      new_request.salt, new_request.security_origin, new_device_info.id);

  for (const LabeledDeviceRequest& labeled_request : requests_) {
    const DeviceRequest* request = labeled_request.second;
    if (request->requesting_process_id == new_request.requesting_process_id &&
        request->requesting_frame_id == new_request.requesting_frame_id &&
        request->request_type == new_request.request_type) {
      for (const StreamDeviceInfo& device_info : request->devices) {
        if (device_info.device.id == source_id &&
            device_info.device.type == new_device_info.type) {
          *existing_device_info = device_info;
          // Make sure that the audio |effects| reflect what the request
          // is set to and not what the capabilities are.
          FilterAudioEffects(request->controls,
                             &existing_device_info->device.input.effects);
          EnableHotwordEffect(request->controls,
                              &existing_device_info->device.input.effects);
          *existing_request_state = request->state(device_info.device.type);
          return true;
        }
      }
    }
  }
  return false;
}

void MediaStreamManager::FinalizeGenerateStream(const std::string& label,
                                                DeviceRequest* request) {
  DVLOG(1) << "FinalizeGenerateStream label " << label;
  // Partition the array of devices into audio vs video.
  StreamDeviceInfoArray audio_devices, video_devices;
  for (const StreamDeviceInfo& device_info : request->devices) {
    if (IsAudioInputMediaType(device_info.device.type))
      audio_devices.push_back(device_info);
    else if (IsVideoMediaType(device_info.device.type))
      video_devices.push_back(device_info);
    else
      NOTREACHED();
  }

  request->requester->StreamGenerated(request->requesting_frame_id,
                                      request->page_request_id, label,
                                      audio_devices, video_devices);
}

void MediaStreamManager::FinalizeRequestFailed(
    const std::string& label,
    DeviceRequest* request,
    content::MediaStreamRequestResult result) {
  if (request->requester)
    request->requester->StreamGenerationFailed(
        request->requesting_frame_id,
        request->page_request_id,
        result);

  if (request->request_type == MEDIA_DEVICE_ACCESS &&
      !request->callback.is_null()) {
    request->callback.Run(MediaStreamDevices(), std::move(request->ui_proxy));
  }

  DeleteRequest(label);
}

void MediaStreamManager::FinalizeOpenDevice(const std::string& label,
                                            DeviceRequest* request) {
  const StreamDeviceInfoArray& requested_devices = request->devices;
  request->requester->DeviceOpened(request->requesting_frame_id,
                                   request->page_request_id,
                                   label, requested_devices.front());
}

void MediaStreamManager::FinalizeEnumerateDevices(const std::string& label,
                                                  DeviceRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(request->request_type, MEDIA_ENUMERATE_DEVICES);
  DCHECK(((request->audio_type() == MEDIA_DEVICE_AUDIO_CAPTURE ||
           request->audio_type() == MEDIA_DEVICE_AUDIO_OUTPUT) &&
          request->video_type() == MEDIA_NO_SERVICE) ||
         (request->audio_type() == MEDIA_NO_SERVICE &&
          request->video_type() == MEDIA_DEVICE_VIDEO_CAPTURE));

  for (StreamDeviceInfo& device_info : request->devices)
    TranslateDeviceIdToSourceId(request, &device_info.device);

  if (use_fake_ui_) {
    if (!fake_ui_)
      fake_ui_.reset(new FakeMediaStreamUIProxy());
    request->ui_proxy = std::move(fake_ui_);
  } else {
    request->ui_proxy = MediaStreamUIProxy::Create();
  }

  // Output label permissions are based on input permission.
  const MediaStreamType type =
      request->audio_type() == MEDIA_DEVICE_AUDIO_CAPTURE ||
      request->audio_type() == MEDIA_DEVICE_AUDIO_OUTPUT
      ? MEDIA_DEVICE_AUDIO_CAPTURE
      : MEDIA_DEVICE_VIDEO_CAPTURE;

  request->ui_proxy->CheckAccess(
      request->security_origin,
      type,
      request->requesting_process_id,
      request->requesting_frame_id,
      base::Bind(&MediaStreamManager::HandleCheckMediaAccessResponse,
                 base::Unretained(this),
                 label));
}

void MediaStreamManager::HandleCheckMediaAccessResponse(
    const std::string& label,
    bool have_access) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DeviceRequest* request = FindRequest(label);
  if (!request) {
    // This can happen if the request was cancelled.
    DVLOG(1) << "The request with label " << label << " does not exist.";
    return;
  }

  if (!have_access)
    ClearDeviceLabels(&request->devices);

  request->requester->DevicesEnumerated(
      request->requesting_frame_id,
      request->page_request_id,
      label,
      request->devices);

  // TODO(tommi):
  // Ideally enumeration requests should be deleted once they have been served
  // (as any request).  However, this implementation mixes requests and
  // notifications together so enumeration requests are kept open by some
  // implementations (only Pepper?) and enumerations are done again when
  // device notifications are fired.
  // Implementations that just want to request the device list and be done
  // (e.g. DeviceRequestMessageFilter), they must (confusingly) call
  // CancelRequest() after the request has been fulfilled.  This is not
  // obvious, not consistent in this class (see e.g. FinalizeMediaAccessRequest)
  // and can lead to subtle bugs (requests not deleted at all deleted too
  // early).
  //
  // Basically, it is not clear that using requests as an additional layer on
  // top of device notifications is necessary or good.
  //
  // To add to this, MediaStreamManager currently relies on the external
  // implementations of MediaStreamRequester to delete enumeration requests via
  // CancelRequest and e.g. DeviceRequestMessageFilter does this.  However the
  // Pepper implementation does not seem to to this at all (and from what I can
  // see, it is the only implementation that uses an enumeration request as a
  // notification mechanism).
  //
  // We should decouple notifications from enumeration requests and once that
  // has been done, remove the requirement to call CancelRequest() to delete
  // enumeration requests and uncomment the following line:
  //
  // DeleteRequest(label);
}

void MediaStreamManager::FinalizeMediaAccessRequest(
    const std::string& label,
    DeviceRequest* request,
    const MediaStreamDevices& devices) {
  if (!request->callback.is_null())
    request->callback.Run(devices, std::move(request->ui_proxy));

  // Delete the request since it is done.
  DeleteRequest(label);
}

void MediaStreamManager::InitializeDeviceManagersOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Store a pointer to |this| on the IO thread to avoid having to jump to the
  // UI thread to fetch a pointer to the MSM. In particular on Android, it can
  // be problematic to post to a UI thread from arbitrary worker threads since
  // attaching to the VM is required and we may have to access the MSM from
  // callback threads that we don't own and don't want to attach.
  g_media_stream_manager_tls_ptr.Pointer()->Set(this);

  // TODO(dalecurtis): Remove ScopedTracker below once crbug.com/457525 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "457525 MediaStreamManager::InitializeDeviceManagersOnIOThread 1"));
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!device_task_runner_.get());
  device_task_runner_ = audio_manager_->GetTaskRunner();

  // TODO(dalecurtis): Remove ScopedTracker below once crbug.com/457525 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "457525 MediaStreamManager::InitializeDeviceManagersOnIOThread 2"));
  audio_input_device_manager_ = new AudioInputDeviceManager(audio_manager_);
  audio_input_device_manager_->Register(this, device_task_runner_);

  // TODO(dalecurtis): Remove ScopedTracker below once crbug.com/457525 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "457525 MediaStreamManager::InitializeDeviceManagersOnIOThread 3"));
  // We want to be notified of IO message loop destruction to delete the thread
  // and the device managers.
  base::MessageLoop::current()->AddDestructionObserver(this);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)) {
    audio_input_device_manager()->UseFakeDevice();
  }

  // TODO(dalecurtis): Remove ScopedTracker below once crbug.com/457525 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile4(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "457525 MediaStreamManager::InitializeDeviceManagersOnIOThread 4"));
  video_capture_manager_ =
      new VideoCaptureManager(media::VideoCaptureDeviceFactory::CreateFactory(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI)));
#if defined(OS_WIN)
  // Use an STA Video Capture Thread to try to avoid crashes on enumeration of
  // buggy third party Direct Show modules, http://crbug.com/428958.
  video_capture_thread_.init_com_with_mta(false);
  CHECK(video_capture_thread_.Start());
  video_capture_manager_->Register(this, video_capture_thread_.task_runner());
#else
  video_capture_manager_->Register(this, device_task_runner_);
#endif

  audio_output_device_enumerator_.reset(new AudioOutputDeviceEnumerator(
      audio_manager_, AudioOutputDeviceEnumerator::CACHE_POLICY_NO_CACHING));
}

void MediaStreamManager::Opened(MediaStreamType stream_type,
                                int capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "Opened({stream_type = " << stream_type <<  "} "
           << "{capture_session_id = " << capture_session_id << "})";
  // Find the request(s) containing this device and mark it as used.
  // It can be used in several requests since the same device can be
  // requested from the same web page.
  for (const LabeledDeviceRequest& labeled_request : requests_) {
    const std::string& label = labeled_request.first;
    DeviceRequest* request = labeled_request.second;
    for (StreamDeviceInfo& device_info : request->devices) {
      if (device_info.device.type == stream_type &&
          device_info.session_id == capture_session_id) {
        CHECK(request->state(device_info.device.type) ==
              MEDIA_REQUEST_STATE_OPENING);
        // We've found a matching request.
        request->SetState(device_info.device.type, MEDIA_REQUEST_STATE_DONE);

        if (IsAudioInputMediaType(device_info.device.type)) {
          // Store the native audio parameters in the device struct.
          // TODO(xians): Handle the tab capture sample rate/channel layout
          // in AudioInputDeviceManager::Open().
          if (device_info.device.type != content::MEDIA_TAB_AUDIO_CAPTURE) {
            const StreamDeviceInfo* info =
                audio_input_device_manager_->GetOpenedDeviceInfoById(
                    device_info.session_id);
            device_info.device.input = info->device.input;

            // Since the audio input device manager will set the input
            // parameters to the default settings (including supported effects),
            // we need to adjust those settings here according to what the
            // request asks for.
            FilterAudioEffects(request->controls,
                               &device_info.device.input.effects);
            EnableHotwordEffect(request->controls,
                                &device_info.device.input.effects);

            device_info.device.matched_output = info->device.matched_output;
          }
        }
        if (RequestDone(*request))
          HandleRequestDone(label, request);
        break;
      }
    }
  }
}

void MediaStreamManager::HandleRequestDone(const std::string& label,
                                           DeviceRequest* request) {
  DCHECK(RequestDone(*request));
  DVLOG(1) << "HandleRequestDone("
           << ", {label = " << label <<  "})";

  switch (request->request_type) {
    case MEDIA_OPEN_DEVICE_PEPPER_ONLY:
      FinalizeOpenDevice(label, request);
      break;
    case MEDIA_GENERATE_STREAM: {
      FinalizeGenerateStream(label, request);
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  if (request->ui_proxy.get()) {
    request->ui_proxy->OnStarted(
        base::Bind(&MediaStreamManager::StopMediaStreamFromBrowser,
                   base::Unretained(this),
                   label),
        base::Bind(&MediaStreamManager::OnMediaStreamUIWindowId,
                   base::Unretained(this),
                   request->video_type(),
                   request->devices));
  }
}

void MediaStreamManager::Closed(MediaStreamType stream_type,
                                int capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void MediaStreamManager::DevicesEnumerated(
    MediaStreamType stream_type, const StreamDeviceInfoArray& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "DevicesEnumerated("
           << "{stream_type = " << stream_type << "})";

  std::string log_message = "New device enumeration result:\n" +
                            GetLogMessageString(stream_type, devices);
  SendMessageToNativeLog(log_message);

  // Only cache the device list when the device list has been changed.
  bool need_update_clients = false;
  bool need_update_device_change_subscribers = false;
  EnumerationCache* cache = stream_type == MEDIA_DEVICE_AUDIO_CAPTURE
                                ? &audio_enumeration_cache_
                                : &video_enumeration_cache_;
  if (!cache->valid || devices.size() != cache->devices.size() ||
      !std::equal(devices.begin(), devices.end(), cache->devices.begin(),
                  StreamDeviceInfo::IsEqual)) {
    StopRemovedDevices(cache->devices, devices);
    need_update_clients = true;
    // Device-change subscribers should not be notified the first time the cache
    // is loaded , as this is not a change in the set of devices. The same
    // applies to enumerations listing no devices when the cache is empty.
    need_update_device_change_subscribers =
        cache->valid && (devices.size() != 0 || cache->devices.size() != 0);
    cache->devices = devices;

    // The device might not be able to be enumerated when it is not warmed up,
    // for example, when the machine just wakes up from sleep. We set the cache
    // to be invalid so that the next media request will trigger the
    // enumeration again. See issue/317673.
    cache->valid = !devices.empty();
  }

  if (need_update_clients && monitoring_started_)
    NotifyDevicesChanged(stream_type, devices);

  if (need_update_device_change_subscribers)
    NotifyDeviceChangeSubscribers(stream_type);

  // Publish the result for all requests waiting for device list(s).
  // Find the requests waiting for this device list, store their labels and
  // release the iterator before calling device settings. We might get a call
  // back from device_settings that will need to iterate through devices.
  std::list<std::string> label_list;
  for (const LabeledDeviceRequest& labeled_request : requests_) {
    DeviceRequest* const request = labeled_request.second;
    if (request->state(stream_type) == MEDIA_REQUEST_STATE_REQUESTED &&
        (request->audio_type() == stream_type ||
         request->video_type() == stream_type)) {
      if (request->request_type != MEDIA_ENUMERATE_DEVICES)
        request->SetState(stream_type, MEDIA_REQUEST_STATE_PENDING_APPROVAL);
      label_list.push_back(labeled_request.first);
    }
  }

  for (const std::string& label : label_list) {
    DeviceRequest* const request = FindRequest(label);
    switch (request->request_type) {
      case MEDIA_ENUMERATE_DEVICES:
        if (need_update_clients && request->requester) {
          request->devices = devices;
          FinalizeEnumerateDevices(label, request);
        }
        break;
      default:
        if (request->state(request->audio_type()) ==
                MEDIA_REQUEST_STATE_REQUESTED ||
            request->state(request->video_type()) ==
                MEDIA_REQUEST_STATE_REQUESTED) {
          // We are doing enumeration for other type of media, wait until it is
          // all done before posting the request to UI because UI needs
          // the device lists to handle the request.
          break;
        }
        if (!SetupDeviceCaptureRequest(request))
          FinalizeRequestFailed(label, request, MEDIA_DEVICE_NO_HARDWARE);
        else
          ReadOutputParamsAndPostRequestToUI(label, request);
        break;
    }
  }
  label_list.clear();
  --active_enumeration_ref_count_[stream_type];
  DCHECK_GE(active_enumeration_ref_count_[stream_type], 0);
}

void MediaStreamManager::Aborted(MediaStreamType stream_type,
                                 int capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "Aborted({stream_type = " << stream_type <<  "} "
           << "{capture_session_id = " << capture_session_id << "})";
  StopDevice(stream_type, capture_session_id);
}

void MediaStreamManager::OnSuspend() {
  SendMessageToNativeLog("Power state suspended.");
}

void MediaStreamManager::OnResume() {
  SendMessageToNativeLog("Power state resumed.");
}

void MediaStreamManager::UseFakeUIForTests(
    std::unique_ptr<FakeMediaStreamUIProxy> fake_ui) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  use_fake_ui_ = true;
  fake_ui_ = std::move(fake_ui);
}

void MediaStreamManager::RegisterNativeLogCallback(
    int renderer_host_id,
    const base::Callback<void(const std::string&)>& callback) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamManager::DoNativeLogCallbackRegistration,
                 base::Unretained(this), renderer_host_id, callback));
}

void MediaStreamManager::UnregisterNativeLogCallback(int renderer_host_id) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamManager::DoNativeLogCallbackUnregistration,
                 base::Unretained(this), renderer_host_id));
}

void MediaStreamManager::AddLogMessageOnIOThread(const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const auto& callback : log_callbacks_)
    callback.second.Run(message);
}

void MediaStreamManager::HandleAccessRequestResponse(
    const std::string& label,
    const media::AudioParameters& output_parameters,
    const MediaStreamDevices& devices,
    content::MediaStreamRequestResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "HandleAccessRequestResponse("
           << ", {label = " << label <<  "})";

  DeviceRequest* request = FindRequest(label);
  if (!request) {
    // The request has been canceled before the UI returned.
    return;
  }

  if (request->request_type == MEDIA_DEVICE_ACCESS) {
    FinalizeMediaAccessRequest(label, request, devices);
    return;
  }

  // Handle the case when the request was denied.
  if (result != MEDIA_DEVICE_OK) {
    FinalizeRequestFailed(label, request, result);
    return;
  }
  DCHECK(!devices.empty());

  // Process all newly-accepted devices for this request.
  bool found_audio = false;
  bool found_video = false;
  for (const MediaStreamDevice& device : devices) {
    StreamDeviceInfo device_info;
    device_info.device = device;

    if (device_info.device.type == content::MEDIA_TAB_VIDEO_CAPTURE ||
        device_info.device.type == content::MEDIA_TAB_AUDIO_CAPTURE) {
      device_info.device.id = request->tab_capture_device_id;
    }

    // Initialize the sample_rate and channel_layout here since for audio
    // mirroring, we don't go through EnumerateDevices where these are usually
    // initialized.
    if (device_info.device.type == content::MEDIA_TAB_AUDIO_CAPTURE ||
        device_info.device.type == content::MEDIA_DESKTOP_AUDIO_CAPTURE) {
      int sample_rate = output_parameters.sample_rate();
      // If we weren't able to get the native sampling rate or the sample_rate
      // is outside the valid range for input devices set reasonable defaults.
      if (sample_rate <= 0 || sample_rate > 96000)
        sample_rate = 44100;

      device_info.device.input.sample_rate = sample_rate;
      device_info.device.input.channel_layout = media::CHANNEL_LAYOUT_STEREO;
    }

    if (device_info.device.type == request->audio_type())
      found_audio = true;
    else if (device_info.device.type == request->video_type())
      found_video = true;

    // If this is request for a new MediaStream, a device is only opened once
    // per render frame. This is so that the permission to use a device can be
    // revoked by a single call to StopStreamDevice regardless of how many
    // MediaStreams it is being used in.
    if (request->request_type == MEDIA_GENERATE_STREAM) {
      MediaRequestState state;
      if (FindExistingRequestedDeviceInfo(*request,
                                          device_info.device,
                                          &device_info,
                                          &state)) {
        request->devices.push_back(device_info);
        request->SetState(device_info.device.type, state);
        DVLOG(1) << "HandleAccessRequestResponse - device already opened "
                 << ", {label = " << label << "}"
                 << ", device_id = " << device.id << "}";
        continue;
      }
    }
    device_info.session_id =
        GetDeviceManager(device_info.device.type)->Open(device_info);
    TranslateDeviceIdToSourceId(request, &device_info.device);
    request->devices.push_back(device_info);

    request->SetState(device_info.device.type, MEDIA_REQUEST_STATE_OPENING);
    DVLOG(1) << "HandleAccessRequestResponse - opening device "
             << ", {label = " << label <<  "}"
             << ", {device_id = " << device_info.device.id << "}"
             << ", {session_id = " << device_info.session_id << "}";
  }

  // Check whether we've received all stream types requested.
  if (!found_audio && IsAudioInputMediaType(request->audio_type())) {
    request->SetState(request->audio_type(), MEDIA_REQUEST_STATE_ERROR);
    DVLOG(1) << "Set no audio found label " << label;
  }

  if (!found_video && IsVideoMediaType(request->video_type()))
    request->SetState(request->video_type(), MEDIA_REQUEST_STATE_ERROR);

  if (RequestDone(*request))
    HandleRequestDone(label, request);
}

void MediaStreamManager::StopMediaStreamFromBrowser(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DeviceRequest* request = FindRequest(label);
  if (!request)
    return;

  // Notify renderers that the devices in the stream will be stopped.
  if (request->requester) {
    for (const StreamDeviceInfo& device : request->devices) {
      request->requester->DeviceStopped(request->requesting_frame_id, label,
                                        device);
    }
  }

  CancelRequest(label);
}

void MediaStreamManager::WillDestroyCurrentMessageLoop() {
  DVLOG(3) << "MediaStreamManager::WillDestroyCurrentMessageLoop()";
  DCHECK(CalledOnIOThread());
  DCHECK(requests_.empty());
  if (device_task_runner_.get()) {
    StopMonitoring();

    video_capture_manager_->Unregister();
    audio_input_device_manager_->Unregister();
    device_task_runner_ = NULL;
  }

  audio_input_device_manager_ = NULL;
  video_capture_manager_ = NULL;
  audio_output_device_enumerator_ = NULL;
  g_media_stream_manager_tls_ptr.Pointer()->Set(NULL);
}

void MediaStreamManager::NotifyDevicesChanged(
    MediaStreamType stream_type,
    const StreamDeviceInfoArray& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaObserver* media_observer =
      GetContentClient()->browser()->GetMediaObserver();

  // Map the devices to MediaStreamDevices.
  MediaStreamDevices new_devices;
  for (const StreamDeviceInfo& device_info : devices)
    new_devices.push_back(device_info.device);

  if (IsAudioInputMediaType(stream_type)) {
    MediaCaptureDevicesImpl::GetInstance()->OnAudioCaptureDevicesChanged(
        new_devices);
    if (media_observer)
      media_observer->OnAudioCaptureDevicesChanged();
  } else if (IsVideoMediaType(stream_type)) {
    MediaCaptureDevicesImpl::GetInstance()->OnVideoCaptureDevicesChanged(
        new_devices);
    if (media_observer)
      media_observer->OnVideoCaptureDevicesChanged();
  } else {
    NOTREACHED();
  }
}

bool MediaStreamManager::RequestDone(const DeviceRequest& request) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const bool requested_audio = IsAudioInputMediaType(request.audio_type());
  const bool requested_video = IsVideoMediaType(request.video_type());

  const bool audio_done =
      !requested_audio ||
      request.state(request.audio_type()) == MEDIA_REQUEST_STATE_DONE ||
      request.state(request.audio_type()) == MEDIA_REQUEST_STATE_ERROR;
  if (!audio_done)
    return false;

  const bool video_done =
      !requested_video ||
      request.state(request.video_type()) == MEDIA_REQUEST_STATE_DONE ||
      request.state(request.video_type()) == MEDIA_REQUEST_STATE_ERROR;
  if (!video_done)
    return false;

  return true;
}

MediaStreamProvider* MediaStreamManager::GetDeviceManager(
    MediaStreamType stream_type) {
  if (IsVideoMediaType(stream_type))
    return video_capture_manager();
  else if (IsAudioInputMediaType(stream_type))
    return audio_input_device_manager();
  NOTREACHED();
  return NULL;
}

void MediaStreamManager::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // NOTE: This method is only called in response to physical audio/video device
  // changes (from the operating system).

  MediaStreamType stream_type;
  if (device_type == base::SystemMonitor::DEVTYPE_AUDIO_CAPTURE) {
    stream_type = MEDIA_DEVICE_AUDIO_CAPTURE;
  } else if (device_type == base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE) {
    stream_type = MEDIA_DEVICE_VIDEO_CAPTURE;
  } else {
    return;  // Uninteresting device change.
  }

  // Always do enumeration even though some enumeration is in progress, because
  // those enumeration commands could be sent before these devices change.
  ++active_enumeration_ref_count_[stream_type];
  GetDeviceManager(stream_type)->EnumerateDevices(stream_type);
}

void MediaStreamManager::OnMediaStreamUIWindowId(MediaStreamType video_type,
                                                 StreamDeviceInfoArray devices,
                                                 gfx::NativeViewId window_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!window_id)
    return;

  if (video_type != MEDIA_DESKTOP_VIDEO_CAPTURE)
    return;

  // Pass along for desktop screen and window capturing when
  // DesktopCaptureDevice is used.
  for (const StreamDeviceInfo& device_info : devices) {
    if (device_info.device.type != MEDIA_DESKTOP_VIDEO_CAPTURE)
      continue;

    DesktopMediaID media_id = DesktopMediaID::Parse(device_info.device.id);
    // WebContentsVideoCaptureDevice is used for tab/webcontents.
    if (media_id.type == DesktopMediaID::TYPE_WEB_CONTENTS)
      continue;
#if defined(USE_AURA)
    // DesktopCaptureDevicAura is used when aura_id is valid.
    if (media_id.aura_id > DesktopMediaID::kNullId)
      continue;
#endif
    video_capture_manager_->SetDesktopCaptureWindowId(device_info.session_id,
                                                      window_id);
    break;
  }
}

void MediaStreamManager::DoNativeLogCallbackRegistration(
    int renderer_host_id,
    const base::Callback<void(const std::string&)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Re-registering (overwriting) is allowed and happens in some tests.
  log_callbacks_[renderer_host_id] = callback;
}

void MediaStreamManager::DoNativeLogCallbackUnregistration(
    int renderer_host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  log_callbacks_.erase(renderer_host_id);
}

void MediaStreamManager::SubscribeToDeviceChangeNotifications(
    MediaStreamRequester* subscriber) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(subscriber);
  DCHECK(std::find_if(device_change_subscribers_.begin(),
                      device_change_subscribers_.end(),
                      [subscriber](MediaStreamRequester* item) {
                        return subscriber == item;
                      }) == device_change_subscribers_.end());
  device_change_subscribers_.push_back(subscriber);
}

void MediaStreamManager::CancelDeviceChangeNotifications(
    MediaStreamRequester* subscriber) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto it = std::find(device_change_subscribers_.begin(),
                      device_change_subscribers_.end(), subscriber);
  CHECK(it != device_change_subscribers_.end());
  device_change_subscribers_.erase(it);
}

void MediaStreamManager::NotifyDeviceChangeSubscribers(MediaStreamType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (auto* subscriber : device_change_subscribers_)
    subscriber->DevicesChanged(type);
}

// static
std::string MediaStreamManager::GetHMACForMediaDeviceID(
    const std::string& salt,
    const url::Origin& security_origin,
    const std::string& raw_unique_id) {
  DCHECK(!raw_unique_id.empty());
  if (raw_unique_id == media::AudioDeviceDescription::kDefaultDeviceId ||
      raw_unique_id == media::AudioDeviceDescription::kCommunicationsDeviceId) {
    return raw_unique_id;
  }

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  const size_t digest_length = hmac.DigestLength();
  std::vector<uint8_t> digest(digest_length);
  bool result = hmac.Init(security_origin.Serialize()) &&
                hmac.Sign(raw_unique_id + salt, &digest[0], digest.size());
  DCHECK(result);
  return base::ToLowerASCII(base::HexEncode(&digest[0], digest.size()));
}

// static
bool MediaStreamManager::DoesMediaDeviceIDMatchHMAC(
    const std::string& salt,
    const url::Origin& security_origin,
    const std::string& device_guid,
    const std::string& raw_unique_id) {
  DCHECK(!raw_unique_id.empty());
  std::string guid_from_raw_device_id =
      GetHMACForMediaDeviceID(salt, security_origin, raw_unique_id);
  return guid_from_raw_device_id == device_guid;
}

// static
bool MediaStreamManager::IsOriginAllowed(int render_process_id,
                                         const url::Origin& origin) {
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          render_process_id, ConvertToGURL(origin))) {
    LOG(ERROR) << "MSM: Renderer requested a URL it's not allowed to use.";
    return false;
  }

  return true;
}

void MediaStreamManager::SetCapturingLinkSecured(int render_process_id,
                                                 int session_id,
                                                 content::MediaStreamType type,
                                                 bool is_secure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (LabeledDeviceRequest& labeled_request : requests_) {
    DeviceRequest* request = labeled_request.second;
    if (request->requesting_process_id != render_process_id)
      continue;

    for (const StreamDeviceInfo& device_info : request->devices) {
      if (device_info.session_id == session_id &&
          device_info.device.type == type) {
        request->SetCapturingLinkSecured(is_secure);
        return;
      }
    }
  }
}

}  // namespace content

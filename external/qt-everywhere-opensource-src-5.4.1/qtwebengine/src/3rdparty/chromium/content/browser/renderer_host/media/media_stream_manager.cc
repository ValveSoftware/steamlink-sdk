// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_manager.h"

#include <list>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/power_monitor/power_monitor.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/capture/web_contents_capture_util.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/device_request_message_filter.h"
#include "content/browser/renderer_host/media/media_capture_devices_impl.h"
#include "content/browser/renderer_host/media/media_stream_requester.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/media_request_state.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/media_stream_request.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/media_switches.h"
#include "media/video/capture/video_capture_device_factory.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace content {

// Forward declaration of DeviceMonitorMac and its only useable method.
class DeviceMonitorMac {
 public:
  void StartMonitoring(
    const scoped_refptr<base::SingleThreadTaskRunner>& device_task_runner);
};

namespace {
// Creates a random label used to identify requests.
std::string RandomLabel() {
  // An earlier PeerConnection spec,
  // http://dev.w3.org/2011/webrtc/editor/webrtc.html, specified the
  // MediaStream::label alphabet as containing 36 characters from
  // range: U+0021, U+0023 to U+0027, U+002A to U+002B, U+002D to U+002E,
  // U+0030 to U+0039, U+0041 to U+005A, U+005E to U+007E.
  // Here we use a safe subset.
  static const char kAlphabet[] = "0123456789"
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

  std::string label(36, ' ');
  for (size_t i = 0; i < label.size(); ++i) {
    int random_char = base::RandGenerator(sizeof(kAlphabet) - 1);
    label[i] = kAlphabet[random_char];
  }
  return label;
}

void ParseStreamType(const StreamOptions& options,
                     MediaStreamType* audio_type,
                     MediaStreamType* video_type) {
  *audio_type = MEDIA_NO_SERVICE;
  *video_type = MEDIA_NO_SERVICE;
  if (options.audio_requested) {
     std::string audio_stream_source;
     bool mandatory = false;
     if (options.GetFirstAudioConstraintByName(kMediaStreamSource,
                                               &audio_stream_source,
                                               &mandatory)) {
       DCHECK(mandatory);
       // This is tab or screen capture.
       if (audio_stream_source == kMediaStreamSourceTab) {
         *audio_type = content::MEDIA_TAB_AUDIO_CAPTURE;
       } else if (audio_stream_source == kMediaStreamSourceSystem) {
         *audio_type = content::MEDIA_LOOPBACK_AUDIO_CAPTURE;
       }
     } else {
       // This is normal audio device capture.
       *audio_type = MEDIA_DEVICE_AUDIO_CAPTURE;
     }
  }
  if (options.video_requested) {
     std::string video_stream_source;
     bool mandatory = false;
     if (options.GetFirstVideoConstraintByName(kMediaStreamSource,
                                               &video_stream_source,
                                               &mandatory)) {
       DCHECK(mandatory);
       // This is tab or screen capture.
       if (video_stream_source == kMediaStreamSourceTab) {
         *video_type = content::MEDIA_TAB_VIDEO_CAPTURE;
       } else if (video_stream_source == kMediaStreamSourceScreen) {
         *video_type = content::MEDIA_DESKTOP_VIDEO_CAPTURE;
       } else if (video_stream_source == kMediaStreamSourceDesktop) {
         *video_type = content::MEDIA_DESKTOP_VIDEO_CAPTURE;
       }
     } else {
       // This is normal video device capture.
       *video_type = MEDIA_DEVICE_VIDEO_CAPTURE;
     }
  }
}

// Private helper method for SendMessageToNativeLog() that obtains the global
// MediaStreamManager instance on the UI thread before sending |message| to the
// webrtcLoggingPrivate API.
void DoAddLogMessage(const std::string& message) {
  // Must be on the UI thread to access BrowserMainLoop.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // May be null in tests.
  // TODO(vrk): Handle this more elegantly by having native log messages become
  // no-ops until MediaStreamManager is aware that a renderer process has
  // started logging. crbug.com/333894
  if (content::BrowserMainLoop::GetInstance()) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&MediaStreamManager::AddLogMessageOnIOThread,
                   base::Unretained(content::BrowserMainLoop::GetInstance()
                                        ->media_stream_manager()),
                   message));
  }
}

// Private helper method to generate a string for the log message that lists the
// human readable names of |devices|.
std::string GetLogMessageString(MediaStreamType stream_type,
                                const StreamDeviceInfoArray& devices) {
  std::string output_string =
      base::StringPrintf("Getting devices for stream type %d:\n", stream_type);
  if (devices.empty()) {
    output_string += "No devices found.";
  } else {
    for (StreamDeviceInfoArray::const_iterator it = devices.begin();
         it != devices.end(); ++it) {
      output_string += "  " + it->device.name + "\n";
    }
  }
  return output_string;
}

// Needed for MediaStreamManager::GenerateStream below.
std::string ReturnEmptySalt() {
  return std::string();
}

// Clears the MediaStreamDevice.name from all devices in |devices|.
static void ClearDeviceLabels(content::StreamDeviceInfoArray* devices) {
  for (content::StreamDeviceInfoArray::iterator device_itr = devices->begin();
       device_itr != devices->end();
       ++device_itr) {
    device_itr->device.name.clear();
  }
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
                int requesting_view_id,
                int page_request_id,
                const GURL& security_origin,
                bool have_permission,
                bool user_gesture,
                MediaStreamRequestType request_type,
                const StreamOptions& options,
                const ResourceContext::SaltCallback& salt_callback)
      : requester(requester),
        requesting_process_id(requesting_process_id),
        requesting_view_id(requesting_view_id),
        page_request_id(page_request_id),
        security_origin(security_origin),
        have_permission(have_permission),
        user_gesture(user_gesture),
        request_type(request_type),
        options(options),
        salt_callback(salt_callback),
        state_(NUM_MEDIA_TYPES, MEDIA_REQUEST_STATE_NOT_REQUESTED),
        audio_type_(MEDIA_NO_SERVICE),
        video_type_(MEDIA_NO_SERVICE) {
  }

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
    ui_request_.reset(new MediaStreamRequest(requesting_process_id,
                                             requesting_view_id,
                                             page_request_id,
                                             security_origin,
                                             user_gesture,
                                             request_type,
                                             requested_audio_device_id,
                                             requested_video_device_id,
                                             audio_type_,
                                             video_type_));
  }

  // Creates a tab capture specific MediaStreamRequest object that is used by
  // this request when UI is asked for permission and device selection.
  void CreateTabCatureUIRequest(int target_render_process_id,
                                int target_render_view_id,
                                const std::string& tab_capture_id) {
    DCHECK(!ui_request_);
    ui_request_.reset(new MediaStreamRequest(target_render_process_id,
                                             target_render_view_id,
                                             page_request_id,
                                             security_origin,
                                             user_gesture,
                                             request_type,
                                             "",
                                             "",
                                             audio_type_,
                                             video_type_));
    ui_request_->tab_capture_device_id = tab_capture_id;
  }

  const MediaStreamRequest* UIRequest() const { return ui_request_.get(); }

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

    // If |ui_request_| doesn't exist, it means that the request has not yet
    // been setup fully and there are no valid observers.
    if (!ui_request_)
      return;

    // If we appended a device_id scheme, we want to remove it when notifying
    // observers which may be in different modules since this scheme is only
    // used internally within the content module.
    std::string device_id =
        WebContentsCaptureUtil::StripWebContentsDeviceScheme(
            ui_request_->tab_capture_device_id);

    media_observer->OnMediaRequestStateChanged(
        ui_request_->render_process_id, ui_request_->render_view_id,
        ui_request_->page_request_id, ui_request_->security_origin,
        MediaStreamDevice(stream_type, device_id, device_id), new_state);
  }

  MediaRequestState state(MediaStreamType stream_type) const {
    return state_[stream_type];
  }

  MediaStreamRequester* const requester;  // Can be NULL.


  // The render process id that requested this stream to be generated and that
  // will receive a handle to the MediaStream. This may be different from
  // MediaStreamRequest::render_process_id which in the tab capture case
  // specifies the target renderer from which audio and video is captured.
  const int requesting_process_id;

  // The render view id that requested this stream to be generated and that
  // will receive a handle to the MediaStream. This may be different from
  // MediaStreamRequest::render_view_id which in the tab capture case
  // specifies the target renderer from which audio and video is captured.
  const int requesting_view_id;

  // An ID the render view provided to identify this request.
  const int page_request_id;

  const GURL security_origin;

  // This is used when enumerating devices; if we don't have device access
  // permission, we remove the device label.
  bool have_permission;

  const bool user_gesture;

  const MediaStreamRequestType request_type;

  const StreamOptions options;

  ResourceContext::SaltCallback salt_callback;

  StreamDeviceInfoArray devices;

  // Callback to the requester which audio/video devices have been selected.
  // It can be null if the requester has no interest to know the result.
  // Currently it is only used by |DEVICE_ACCESS| type.
  MediaStreamManager::MediaRequestResponseCallback callback;

  scoped_ptr<MediaStreamUIProxy> ui_proxy;

 private:
  std::vector<MediaRequestState> state_;
  scoped_ptr<MediaStreamRequest> ui_request_;
  MediaStreamType audio_type_;
  MediaStreamType video_type_;
};

MediaStreamManager::EnumerationCache::EnumerationCache()
    : valid(false) {
}

MediaStreamManager::EnumerationCache::~EnumerationCache() {
}

MediaStreamManager::MediaStreamManager()
    : audio_manager_(NULL),
      monitoring_started_(false),
      io_loop_(NULL),
      use_fake_ui_(false) {}

MediaStreamManager::MediaStreamManager(media::AudioManager* audio_manager)
    : audio_manager_(audio_manager),
      monitoring_started_(false),
      io_loop_(NULL),
      use_fake_ui_(false) {
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
  DVLOG(1) << "~MediaStreamManager";
  DCHECK(requests_.empty());
  DCHECK(!device_task_runner_);

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

std::string MediaStreamManager::MakeMediaAccessRequest(
    int render_process_id,
    int render_view_id,
    int page_request_id,
    const StreamOptions& options,
    const GURL& security_origin,
    const MediaRequestResponseCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(perkj): The argument list with NULL parameters to DeviceRequest
  // suggests that this is the wrong design. Can this be refactored?
  DeviceRequest* request = new DeviceRequest(NULL,
                                             render_process_id,
                                             render_view_id,
                                             page_request_id,
                                             security_origin,
                                             true,
                                             false,  // user gesture
                                             MEDIA_DEVICE_ACCESS,
                                             options,
                                             base::Bind(&ReturnEmptySalt));

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
                                        int render_view_id,
                                        const ResourceContext::SaltCallback& sc,
                                        int page_request_id,
                                        const StreamOptions& options,
                                        const GURL& security_origin,
                                        bool user_gesture) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "GenerateStream()";
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForMediaStream)) {
    UseFakeUI(scoped_ptr<FakeMediaStreamUIProxy>());
  }

  DeviceRequest* request = new DeviceRequest(requester,
                                             render_process_id,
                                             render_view_id,
                                             page_request_id,
                                             security_origin,
                                             true,
                                             user_gesture,
                                             MEDIA_GENERATE_STREAM,
                                             options,
                                             sc);

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
                                       int render_view_id,
                                       int page_request_id) {
  for (DeviceRequests::const_iterator request_it = requests_.begin();
       request_it != requests_.end(); ++request_it) {
    const DeviceRequest* request = request_it->second;
    if (request->requesting_process_id == render_process_id &&
        request->requesting_view_id == render_view_id &&
        request->page_request_id == page_request_id) {
      CancelRequest(request_it->first);
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
  for (StreamDeviceInfoArray::iterator device_it = request->devices.begin();
       device_it != request->devices.end(); ++device_it) {
    MediaRequestState state = request->state(device_it->device.type);
    // If we have not yet requested the device to be opened - just ignore it.
    if (state != MEDIA_REQUEST_STATE_OPENING &&
        state != MEDIA_REQUEST_STATE_DONE) {
      continue;
    }
    // Stop the opening/opened devices of the requests.
    CloseDevice(device_it->device.type, device_it->session_id);
  }

  // Cancel the request if still pending at UI side.
  request->SetState(NUM_MEDIA_TYPES, MEDIA_REQUEST_STATE_CLOSING);
  DeleteRequest(label);
}

void MediaStreamManager::CancelAllRequests(int render_process_id) {
  DeviceRequests::iterator request_it = requests_.begin();
  while (request_it != requests_.end()) {
    if (request_it->second->requesting_process_id != render_process_id) {
      ++request_it;
      continue;
    }

    std::string label = request_it->first;
    ++request_it;
    CancelRequest(label);
  }
}

void MediaStreamManager::StopStreamDevice(int render_process_id,
                                          int render_view_id,
                                          const std::string& device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "StopStreamDevice({render_view_id = " << render_view_id <<  "} "
           << ", {device_id = " << device_id << "})";
  // Find the first request for this |render_process_id| and |render_view_id|
  // of type MEDIA_GENERATE_STREAM that has requested to use |device_id| and
  // stop it.
  for (DeviceRequests::iterator request_it = requests_.begin();
       request_it  != requests_.end(); ++request_it) {
    DeviceRequest* request = request_it->second;
    if (request->requesting_process_id != render_process_id ||
        request->requesting_view_id != render_view_id ||
        request->request_type != MEDIA_GENERATE_STREAM) {
      continue;
    }

    StreamDeviceInfoArray& devices = request->devices;
    for (StreamDeviceInfoArray::iterator device_it = devices.begin();
         device_it != devices.end(); ++device_it) {
      if (device_it->device.id == device_id) {
        StopDevice(device_it->device.type, device_it->session_id);
        return;
      }
    }
  }
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

  for (DeviceRequests::iterator request_it = requests_.begin();
       request_it != requests_.end() ; ++request_it) {
    StreamDeviceInfoArray* devices = &request_it->second->devices;
    for (StreamDeviceInfoArray::iterator device_it = devices->begin();
         device_it != devices->end(); ++device_it) {
      if (device_it->session_id == session_id &&
          device_it->device.type == type) {
        // Notify observers that this device is being closed.
        // Note that only one device per type can be opened.
        request_it->second->SetState(type, MEDIA_REQUEST_STATE_CLOSING);
      }
    }
  }
}

std::string MediaStreamManager::EnumerateDevices(
    MediaStreamRequester* requester,
    int render_process_id,
    int render_view_id,
    const ResourceContext::SaltCallback& sc,
    int page_request_id,
    MediaStreamType type,
    const GURL& security_origin,
    bool have_permission) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(requester);
  DCHECK(type == MEDIA_DEVICE_AUDIO_CAPTURE ||
         type == MEDIA_DEVICE_VIDEO_CAPTURE ||
         type == MEDIA_DEVICE_AUDIO_OUTPUT);

  DeviceRequest* request = new DeviceRequest(requester,
                                             render_process_id,
                                             render_view_id,
                                             page_request_id,
                                             security_origin,
                                             have_permission,
                                             false,  // user gesture
                                             MEDIA_ENUMERATE_DEVICES,
                                             StreamOptions(),
                                             sc);
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
      device_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&MediaStreamManager::EnumerateAudioOutputDevices,
                     base::Unretained(this),
                     label));
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

void MediaStreamManager::EnumerateAudioOutputDevices(
    const std::string& label) {
  DCHECK(device_task_runner_->BelongsToCurrentThread());

  scoped_ptr<media::AudioDeviceNames> device_names(
      new media::AudioDeviceNames());
  audio_manager_->GetAudioOutputDeviceNames(device_names.get());
  StreamDeviceInfoArray devices;
  for (media::AudioDeviceNames::iterator it = device_names->begin();
       it != device_names->end(); ++it) {
    StreamDeviceInfo device(MEDIA_DEVICE_AUDIO_OUTPUT,
                            it->device_name,
                            it->unique_id);
    devices.push_back(device);
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamManager::AudioOutputDevicesEnumerated,
                 base::Unretained(this),
                 devices));
}

void MediaStreamManager::AudioOutputDevicesEnumerated(
    const StreamDeviceInfoArray& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "AudioOutputDevicesEnumerated()";

  std::string log_message = "New device enumeration result:\n" +
                            GetLogMessageString(MEDIA_DEVICE_AUDIO_OUTPUT,
                                                devices);
  SendMessageToNativeLog(log_message);

  // Publish the result for all requests waiting for device list(s).
  for (DeviceRequests::iterator it = requests_.begin(); it != requests_.end();
       ++it) {
    if (it->second->state(MEDIA_DEVICE_AUDIO_OUTPUT) ==
            MEDIA_REQUEST_STATE_REQUESTED &&
        it->second->audio_type() == MEDIA_DEVICE_AUDIO_OUTPUT) {
      DCHECK_EQ(MEDIA_ENUMERATE_DEVICES, it->second->request_type);
      it->second->SetState(MEDIA_DEVICE_AUDIO_OUTPUT,
                           MEDIA_REQUEST_STATE_PENDING_APPROVAL);
      it->second->devices = devices;
      FinalizeEnumerateDevices(it->first, it->second);
    }
  }

  --active_enumeration_ref_count_[MEDIA_DEVICE_AUDIO_OUTPUT];
  DCHECK_GE(active_enumeration_ref_count_[MEDIA_DEVICE_AUDIO_OUTPUT], 0);
}

void MediaStreamManager::OpenDevice(MediaStreamRequester* requester,
                                    int render_process_id,
                                    int render_view_id,
                                    const ResourceContext::SaltCallback& sc,
                                    int page_request_id,
                                    const std::string& device_id,
                                    MediaStreamType type,
                                    const GURL& security_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(type == MEDIA_DEVICE_AUDIO_CAPTURE ||
         type == MEDIA_DEVICE_VIDEO_CAPTURE);
  DVLOG(1) << "OpenDevice ({page_request_id = " << page_request_id <<  "})";
  StreamOptions options;
  if (IsAudioInputMediaType(type)) {
    options.audio_requested = true;
    options.mandatory_audio.push_back(
        StreamOptions::Constraint(kMediaStreamSourceInfoId, device_id));
  } else if (IsVideoMediaType(type)) {
    options.video_requested = true;
    options.mandatory_video.push_back(
        StreamOptions::Constraint(kMediaStreamSourceInfoId, device_id));
  } else {
    NOTREACHED();
  }
  DeviceRequest* request = new DeviceRequest(requester,
                                             render_process_id,
                                             render_view_id,
                                             page_request_id,
                                             security_origin,
                                             true,
                                             false,  // user gesture
                                             MEDIA_OPEN_DEVICE,
                                             options,
                                             sc);

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
    const ResourceContext::SaltCallback& sc,
    const GURL& security_origin,
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

  for (StreamDeviceInfoArray::const_iterator it = cache->devices.begin();
       it != cache->devices.end();
       ++it) {
    if (content::DoesMediaDeviceIDMatchHMAC(sc, security_origin, source_id,
                                            it->device.id)) {
      *device_id = it->device.id;
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
  for (StreamDeviceInfoArray::const_iterator old_dev_it = old_devices.begin();
       old_dev_it != old_devices.end(); ++old_dev_it) {
    bool device_found = false;
    StreamDeviceInfoArray::const_iterator new_dev_it = new_devices.begin();
    for (; new_dev_it != new_devices.end(); ++new_dev_it) {
      if (old_dev_it->device.id == new_dev_it->device.id) {
        device_found = true;
        break;
      }
    }

    if (!device_found) {
      // A device has been removed. We need to check if it is used by a
      // MediaStream and in that case cleanup and notify the render process.
      StopRemovedDevice(old_dev_it->device);
    }
  }
}

void MediaStreamManager::StopRemovedDevice(const MediaStreamDevice& device) {
  std::vector<int> session_ids;
  for (DeviceRequests::const_iterator it = requests_.begin();
       it != requests_.end() ; ++it) {
    const DeviceRequest* request = it->second;
    for (StreamDeviceInfoArray::const_iterator device_it =
             request->devices.begin();
         device_it != request->devices.end(); ++device_it) {
      std::string source_id = content::GetHMACForMediaDeviceID(
          request->salt_callback,
          request->security_origin,
          device.id);
      if (device_it->device.id == source_id &&
          device_it->device.type == device.type) {
        session_ids.push_back(device_it->session_id);
        if (it->second->requester) {
          it->second->requester->DeviceStopped(
              it->second->requesting_view_id,
              it->first,
              *device_it);
        }
      }
    }
  }
  for (std::vector<int>::const_iterator it = session_ids.begin();
       it != session_ids.end(); ++it) {
    StopDevice(device.type, *it);
  }

  std::ostringstream oss;
  oss << "Media input device removed: type = " <<
    (device.type == MEDIA_DEVICE_AUDIO_CAPTURE ? "audio" : "video") <<
    ", id = " << device.id << ", name = " << device.name;
  AddLogMessageOnIOThread(oss.str());
}

void MediaStreamManager::StartMonitoring() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (monitoring_started_)
    return;

  if (!base::SystemMonitor::Get())
    return;

  monitoring_started_ = true;
  base::SystemMonitor::Get()->AddDevicesChangedObserver(this);

  // Enumerate both the audio and video devices to cache the device lists
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

#if defined(OS_MACOSX)
void MediaStreamManager::StartMonitoringOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserMainLoop* browser_main_loop = content::BrowserMainLoop::GetInstance();
  if (browser_main_loop) {
    browser_main_loop->device_monitor_mac()
        ->StartMonitoring(audio_manager_->GetWorkerTaskRunner());
  }
}
#endif

void MediaStreamManager::StopMonitoring() {
  DCHECK_EQ(base::MessageLoop::current(), io_loop_);
  if (monitoring_started_) {
    base::SystemMonitor::Get()->RemoveDevicesChangedObserver(this);
    monitoring_started_ = false;
    ClearEnumerationCache(&audio_enumeration_cache_);
    ClearEnumerationCache(&video_enumeration_cache_);
  }
}

bool MediaStreamManager::GetRequestedDeviceCaptureId(
    const DeviceRequest* request,
    MediaStreamType type,
    std::string* device_id) const {
  DCHECK(type == MEDIA_DEVICE_AUDIO_CAPTURE ||
         type == MEDIA_DEVICE_VIDEO_CAPTURE);
  const StreamOptions::Constraints* mandatory =
      (type == MEDIA_DEVICE_AUDIO_CAPTURE) ?
          &request->options.mandatory_audio : &request->options.mandatory_video;
  const StreamOptions::Constraints* optional =
      (type == MEDIA_DEVICE_AUDIO_CAPTURE) ?
          &request->options.optional_audio : &request->options.optional_video;

  std::vector<std::string> source_ids;
  StreamOptions::GetConstraintsByName(*mandatory,
                                      kMediaStreamSourceInfoId, &source_ids);
  if (source_ids.size() > 1) {
    LOG(ERROR) << "Only one mandatory " << kMediaStreamSourceInfoId
        << " is supported.";
    return false;
  }
  // If a specific device has been requested we need to find the real device
  // id.
  if (source_ids.size() == 1 &&
      !TranslateSourceIdToDeviceId(type,
                                   request->salt_callback,
                                   request->security_origin,
                                   source_ids[0], device_id)) {
    LOG(WARNING) << "Invalid mandatory " << kMediaStreamSourceInfoId
                 << " = " << source_ids[0] << ".";
    return false;
  }
  // Check for optional audio sourceIDs.
  if (device_id->empty()) {
    StreamOptions::GetConstraintsByName(*optional,
                                        kMediaStreamSourceInfoId,
                                        &source_ids);
    // Find the first sourceID that translates to device. Note that only one
    // device per type can call to GenerateStream is ever opened.
    for (std::vector<std::string>::const_iterator it = source_ids.begin();
         it != source_ids.end(); ++it) {
      if (TranslateSourceIdToDeviceId(type,
                                      request->salt_callback,
                                      request->security_origin,
                                      *it,
                                      device_id)) {
        break;
      }
    }
  }
  return true;
}

void MediaStreamManager::TranslateDeviceIdToSourceId(
    DeviceRequest* request,
    MediaStreamDevice* device) {
  if (request->audio_type() == MEDIA_DEVICE_AUDIO_CAPTURE ||
      request->audio_type() == MEDIA_DEVICE_AUDIO_OUTPUT ||
      request->video_type() == MEDIA_DEVICE_VIDEO_CAPTURE) {
    device->id = content::GetHMACForMediaDeviceID(
        request->salt_callback,
        request->security_origin,
        device->id);
  }
}

void MediaStreamManager::ClearEnumerationCache(EnumerationCache* cache) {
  DCHECK_EQ(base::MessageLoop::current(), io_loop_);
  cache->valid = false;
}

bool MediaStreamManager::EnumerationRequired(EnumerationCache* cache,
                                             MediaStreamType stream_type) {
  DCHECK_EQ(base::MessageLoop::current(), io_loop_);
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
  const MediaStreamType streams[] = { request->audio_type(),
                                      request->video_type() };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(streams); ++i) {
    if (streams[i] == MEDIA_NO_SERVICE)
      continue;
    request->SetState(streams[i], MEDIA_REQUEST_STATE_REQUESTED);
    DCHECK_GE(active_enumeration_ref_count_[streams[i]], 0);
    if (active_enumeration_ref_count_[streams[i]] == 0) {
      ++active_enumeration_ref_count_[streams[i]];
      GetDeviceManager(streams[i])->EnumerateDevices(streams[i]);
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
  for (DeviceRequests::const_iterator request_it = requests_.begin();
       request_it != requests_.end(); ++request_it) {
    if (request_it->first == label)
      return request_it->second;
  }
  return NULL;
}

void MediaStreamManager::DeleteRequest(const std::string& label) {
  DVLOG(1) << "DeleteRequest({label= " << label << "})";
  for (DeviceRequests::iterator request_it = requests_.begin();
       request_it != requests_.end(); ++request_it) {
    if (request_it->first == label) {
      scoped_ptr<DeviceRequest> request(request_it->second);
      requests_.erase(request_it);
      return;
    }
  }
  NOTREACHED();
}

void MediaStreamManager::PostRequestToUI(const std::string& label,
                                         DeviceRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request->UIRequest());
  DVLOG(1) << "PostRequestToUI({label= " << label << "})";

  const MediaStreamType audio_type = request->audio_type();
  const MediaStreamType video_type = request->video_type();

  // Post the request to UI and set the state.
  if (IsAudioInputMediaType(audio_type))
    request->SetState(audio_type, MEDIA_REQUEST_STATE_PENDING_APPROVAL);
  if (IsVideoMediaType(video_type))
    request->SetState(video_type, MEDIA_REQUEST_STATE_PENDING_APPROVAL);

  if (use_fake_ui_) {
    if (!fake_ui_)
      fake_ui_.reset(new FakeMediaStreamUIProxy());

    MediaStreamDevices devices;
    if (audio_enumeration_cache_.valid) {
      for (StreamDeviceInfoArray::const_iterator it =
               audio_enumeration_cache_.devices.begin();
           it != audio_enumeration_cache_.devices.end(); ++it) {
        devices.push_back(it->device);
      }
    }
    if (video_enumeration_cache_.valid) {
      for (StreamDeviceInfoArray::const_iterator it =
               video_enumeration_cache_.devices.begin();
           it != video_enumeration_cache_.devices.end(); ++it) {
        devices.push_back(it->device);
      }
    }

    fake_ui_->SetAvailableDevices(devices);

    request->ui_proxy = fake_ui_.Pass();
  } else {
    request->ui_proxy = MediaStreamUIProxy::Create();
  }

  request->ui_proxy->RequestAccess(
      *request->UIRequest(),
      base::Bind(&MediaStreamManager::HandleAccessRequestResponse,
                 base::Unretained(this), label));
}

void MediaStreamManager::SetupRequest(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DeviceRequest* request = FindRequest(label);
  if (!request) {
    DVLOG(1) << "SetupRequest label " << label << " doesn't exist!!";
    return;  // This can happen if the request has been canceled.
  }

  if (!request->security_origin.is_valid()) {
    LOG(ERROR) << "Invalid security origin. "
               << request->security_origin;
    FinalizeRequestFailed(label,
                          request,
                          MEDIA_DEVICE_INVALID_SECURITY_ORIGIN);
    return;
  }

  MediaStreamType audio_type = MEDIA_NO_SERVICE;
  MediaStreamType video_type = MEDIA_NO_SERVICE;
  ParseStreamType(request->options, &audio_type, &video_type);
  request->SetAudioType(audio_type);
  request->SetVideoType(video_type);

  bool is_web_contents_capture =
      audio_type == MEDIA_TAB_AUDIO_CAPTURE ||
      video_type == MEDIA_TAB_VIDEO_CAPTURE;
  if (is_web_contents_capture && !SetupTabCaptureRequest(request)) {
    FinalizeRequestFailed(label,
                          request,
                          MEDIA_DEVICE_TAB_CAPTURE_FAILURE);
    return;
  }

  bool is_screen_capture =
      video_type == MEDIA_DESKTOP_VIDEO_CAPTURE;
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
  PostRequestToUI(label, request);
}

bool MediaStreamManager::SetupDeviceCaptureRequest(DeviceRequest* request) {
  DCHECK((request->audio_type() == MEDIA_DEVICE_AUDIO_CAPTURE ||
          request->audio_type() == MEDIA_NO_SERVICE) &&
         (request->video_type() == MEDIA_DEVICE_VIDEO_CAPTURE ||
          request->video_type() == MEDIA_NO_SERVICE));
  std::string audio_device_id;
  if (request->options.audio_requested &&
      !GetRequestedDeviceCaptureId(request, request->audio_type(),
                                     &audio_device_id)) {
    return false;
  }

  std::string video_device_id;
  if (request->options.video_requested &&
      !GetRequestedDeviceCaptureId(request, request->video_type(),
                                   &video_device_id)) {
    return false;
  }
  request->CreateUIRequest(audio_device_id, video_device_id);
  DVLOG(3) << "Audio requested " << request->options.audio_requested
           << " device id = " << audio_device_id
           << "Video requested " << request->options.video_requested
           << " device id = " << video_device_id;
  return true;
}

bool MediaStreamManager::SetupTabCaptureRequest(DeviceRequest* request) {
  DCHECK(request->audio_type() == MEDIA_TAB_AUDIO_CAPTURE ||
         request->video_type() == MEDIA_TAB_VIDEO_CAPTURE);

  std::string capture_device_id;
  bool mandatory_audio = false;
  bool mandatory_video = false;
  if (!request->options.GetFirstAudioConstraintByName(kMediaStreamSourceId,
                                                      &capture_device_id,
                                                      &mandatory_audio) &&
      !request->options.GetFirstVideoConstraintByName(kMediaStreamSourceId,
                                                      &capture_device_id,
                                                      &mandatory_video)) {
    return false;
  }
  DCHECK(mandatory_audio || mandatory_video);

  // Customize options for a WebContents based capture.
  int target_render_process_id = 0;
  int target_render_view_id = 0;

  // TODO(justinlin): Can't plumb audio mirroring using stream type right
  // now, so plumbing by device_id. Will revisit once it's refactored.
  // http://crbug.com/163100
  std::string tab_capture_device_id =
      WebContentsCaptureUtil::AppendWebContentsDeviceScheme(capture_device_id);

  bool has_valid_device_id = WebContentsCaptureUtil::ExtractTabCaptureTarget(
      tab_capture_device_id, &target_render_process_id,
      &target_render_view_id);
  if (!has_valid_device_id ||
      (request->audio_type() != MEDIA_TAB_AUDIO_CAPTURE &&
       request->audio_type() != MEDIA_NO_SERVICE) ||
      (request->video_type() != MEDIA_TAB_VIDEO_CAPTURE &&
       request->video_type() != MEDIA_NO_SERVICE)) {
    return false;
  }

  request->CreateTabCatureUIRequest(target_render_process_id,
                                    target_render_view_id,
                                    tab_capture_device_id);

  DVLOG(3) << "SetupTabCaptureRequest "
           << ", {tab_capture_device_id = " << tab_capture_device_id <<  "}"
           << ", {target_render_process_id = " << target_render_process_id
           << "}"
           << ", {target_render_view_id = " << target_render_view_id << "}";
  return true;
}

bool MediaStreamManager::SetupScreenCaptureRequest(DeviceRequest* request) {
  DCHECK(request->audio_type() == MEDIA_LOOPBACK_AUDIO_CAPTURE ||
         request->video_type() == MEDIA_DESKTOP_VIDEO_CAPTURE);

  // For screen capture we only support two valid combinations:
  // (1) screen video capture only, or
  // (2) screen video capture with loopback audio capture.
  if (request->video_type() != MEDIA_DESKTOP_VIDEO_CAPTURE ||
      (request->audio_type() != MEDIA_NO_SERVICE &&
       request->audio_type() != MEDIA_LOOPBACK_AUDIO_CAPTURE)) {
    LOG(ERROR) << "Invalid screen capture request.";
    return false;
  }

  std::string video_device_id;
  if (request->video_type() == MEDIA_DESKTOP_VIDEO_CAPTURE) {
    std::string video_stream_source;
    bool mandatory = false;
    if (!request->options.GetFirstVideoConstraintByName(
        kMediaStreamSource,
        &video_stream_source,
        &mandatory)) {
      LOG(ERROR) << kMediaStreamSource << " not found.";
      return false;
    }
    DCHECK(mandatory);

    if (video_stream_source == kMediaStreamSourceDesktop) {
      if (!request->options.GetFirstVideoConstraintByName(
          kMediaStreamSourceId,
          &video_device_id,
          &mandatory)) {
        LOG(ERROR) << kMediaStreamSourceId << " not found.";
        return false;
      }
      DCHECK(mandatory);
    }
  }

  request->CreateUIRequest("", video_device_id);
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

  std::string source_id = content::GetHMACForMediaDeviceID(
      new_request.salt_callback,
      new_request.security_origin,
      new_device_info.id);

  for (DeviceRequests::const_iterator it = requests_.begin();
       it != requests_.end() ; ++it) {
    const DeviceRequest* request = it->second;
    if (request->requesting_process_id == new_request.requesting_process_id &&
        request->requesting_view_id == new_request.requesting_view_id &&
        request->request_type == new_request.request_type) {
      for (StreamDeviceInfoArray::const_iterator device_it =
               request->devices.begin();
           device_it != request->devices.end(); ++device_it) {
        if (device_it->device.id == source_id &&
            device_it->device.type == new_device_info.type) {
            *existing_device_info = *device_it;
            *existing_request_state = request->state(device_it->device.type);
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
  const StreamDeviceInfoArray& requested_devices = request->devices;

  // Partition the array of devices into audio vs video.
  StreamDeviceInfoArray audio_devices, video_devices;
  for (StreamDeviceInfoArray::const_iterator device_it =
           requested_devices.begin();
       device_it != requested_devices.end(); ++device_it) {
    if (IsAudioInputMediaType(device_it->device.type)) {
      audio_devices.push_back(*device_it);
    } else if (IsVideoMediaType(device_it->device.type)) {
      video_devices.push_back(*device_it);
    } else {
      NOTREACHED();
    }
  }

  request->requester->StreamGenerated(
      request->requesting_view_id,
      request->page_request_id,
      label, audio_devices, video_devices);
}

void MediaStreamManager::FinalizeRequestFailed(
    const std::string& label,
    DeviceRequest* request,
    content::MediaStreamRequestResult result) {
  if (request->requester)
    request->requester->StreamGenerationFailed(
        request->requesting_view_id,
        request->page_request_id,
        result);

  if (request->request_type == MEDIA_DEVICE_ACCESS &&
      !request->callback.is_null()) {
    request->callback.Run(MediaStreamDevices(), request->ui_proxy.Pass());
  }

  DeleteRequest(label);
}

void MediaStreamManager::FinalizeOpenDevice(const std::string& label,
                                            DeviceRequest* request) {
  const StreamDeviceInfoArray& requested_devices = request->devices;
  request->requester->DeviceOpened(request->requesting_view_id,
                                   request->page_request_id,
                                   label, requested_devices.front());
}

void MediaStreamManager::FinalizeEnumerateDevices(const std::string& label,
                                                  DeviceRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(request->request_type, MEDIA_ENUMERATE_DEVICES);

  if (request->security_origin.is_valid()) {
    for (StreamDeviceInfoArray::iterator it = request->devices.begin();
         it != request->devices.end(); ++it) {
      TranslateDeviceIdToSourceId(request, &it->device);
    }
  } else {
    request->devices.clear();
  }

  if (!request->have_permission)
    ClearDeviceLabels(&request->devices);

  request->requester->DevicesEnumerated(
      request->requesting_view_id,
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
    request->callback.Run(devices, request->ui_proxy.Pass());

  // Delete the request since it is done.
  DeleteRequest(label);
}

void MediaStreamManager::InitializeDeviceManagersOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (device_task_runner_)
    return;

  device_task_runner_ = audio_manager_->GetWorkerTaskRunner();

  audio_input_device_manager_ = new AudioInputDeviceManager(audio_manager_);
  audio_input_device_manager_->Register(this, device_task_runner_);

  // We want to be notified of IO message loop destruction to delete the thread
  // and the device managers.
  io_loop_ = base::MessageLoop::current();
  io_loop_->AddDestructionObserver(this);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeDeviceForMediaStream)) {
    audio_input_device_manager()->UseFakeDevice();
  }

  video_capture_manager_ =
      new VideoCaptureManager(media::VideoCaptureDeviceFactory::CreateFactory(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI)));
  video_capture_manager_->Register(this, device_task_runner_);
}

void MediaStreamManager::Opened(MediaStreamType stream_type,
                                int capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "Opened({stream_type = " << stream_type <<  "} "
           << "{capture_session_id = " << capture_session_id << "})";
  // Find the request(s) containing this device and mark it as used.
  // It can be used in several requests since the same device can be
  // requested from the same web page.
  for (DeviceRequests::iterator request_it = requests_.begin();
       request_it != requests_.end(); ++request_it) {
    const std::string& label = request_it->first;
    DeviceRequest* request = request_it->second;
    StreamDeviceInfoArray* devices = &(request->devices);
    for (StreamDeviceInfoArray::iterator device_it = devices->begin();
         device_it != devices->end(); ++device_it) {
      if (device_it->device.type == stream_type &&
          device_it->session_id == capture_session_id) {
        CHECK(request->state(device_it->device.type) ==
            MEDIA_REQUEST_STATE_OPENING);
        // We've found a matching request.
        request->SetState(device_it->device.type, MEDIA_REQUEST_STATE_DONE);

        if (IsAudioInputMediaType(device_it->device.type)) {
          // Store the native audio parameters in the device struct.
          // TODO(xians): Handle the tab capture sample rate/channel layout
          // in AudioInputDeviceManager::Open().
          if (device_it->device.type != content::MEDIA_TAB_AUDIO_CAPTURE) {
            const StreamDeviceInfo* info =
                audio_input_device_manager_->GetOpenedDeviceInfoById(
                    device_it->session_id);
            device_it->device.input = info->device.input;
            device_it->device.matched_output = info->device.matched_output;
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
    case MEDIA_OPEN_DEVICE:
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
           << "{stream_type = " << stream_type << "})" << std::endl;

  std::string log_message = "New device enumeration result:\n" +
                            GetLogMessageString(stream_type, devices);
  SendMessageToNativeLog(log_message);

  // Only cache the device list when the device list has been changed.
  bool need_update_clients = false;
  EnumerationCache* cache =
      stream_type == MEDIA_DEVICE_AUDIO_CAPTURE ?
      &audio_enumeration_cache_ : &video_enumeration_cache_;
  if (!cache->valid ||
      devices.size() != cache->devices.size() ||
      !std::equal(devices.begin(), devices.end(), cache->devices.begin(),
                  StreamDeviceInfo::IsEqual)) {
    StopRemovedDevices(cache->devices, devices);
    cache->devices = devices;
    need_update_clients = true;

    // The device might not be able to be enumerated when it is not warmed up,
    // for example, when the machine just wakes up from sleep. We set the cache
    // to be invalid so that the next media request will trigger the
    // enumeration again. See issue/317673.
    cache->valid = !devices.empty();
  }

  if (need_update_clients && monitoring_started_)
    NotifyDevicesChanged(stream_type, devices);

  // Publish the result for all requests waiting for device list(s).
  // Find the requests waiting for this device list, store their labels and
  // release the iterator before calling device settings. We might get a call
  // back from device_settings that will need to iterate through devices.
  std::list<std::string> label_list;
  for (DeviceRequests::iterator it = requests_.begin(); it != requests_.end();
       ++it) {
    if (it->second->state(stream_type) == MEDIA_REQUEST_STATE_REQUESTED &&
        (it->second->audio_type() == stream_type ||
         it->second->video_type() == stream_type)) {
      if (it->second->request_type != MEDIA_ENUMERATE_DEVICES)
        it->second->SetState(stream_type, MEDIA_REQUEST_STATE_PENDING_APPROVAL);
      label_list.push_back(it->first);
    }
  }

  for (std::list<std::string>::iterator it = label_list.begin();
       it != label_list.end(); ++it) {
    DeviceRequest* request = FindRequest(*it);
    switch (request->request_type) {
      case MEDIA_ENUMERATE_DEVICES:
        if (need_update_clients && request->requester) {
          request->devices = devices;
          FinalizeEnumerateDevices(*it, request);
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
        if (!SetupDeviceCaptureRequest(request)) {
          FinalizeRequestFailed(*it,
                                request,
                                MEDIA_DEVICE_NO_HARDWARE);
        } else {
          PostRequestToUI(*it, request);
        }
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

// static
void MediaStreamManager::SendMessageToNativeLog(const std::string& message) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(DoAddLogMessage, message));
}

void MediaStreamManager::OnSuspend() {
  SendMessageToNativeLog("Power state suspended.");
}

void MediaStreamManager::OnResume() {
  SendMessageToNativeLog("Power state resumed.");
}

void MediaStreamManager::AddLogMessageOnIOThread(const std::string& message) {
  // Get render process ids on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Grab all unique process ids that request a MediaStream or have a
  // MediaStream running.
  std::set<int> requesting_process_ids;
  for (DeviceRequests::const_iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    DeviceRequest* request = it->second;
    if (request->request_type == MEDIA_GENERATE_STREAM)
      requesting_process_ids.insert(request->requesting_process_id);
  }

  // MediaStreamManager is a singleton in BrowserMainLoop, which owns the UI
  // thread. MediaStreamManager has the same lifetime as the UI thread, so it is
  // safe to use base::Unretained.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&MediaStreamManager::AddLogMessageOnUIThread,
                 base::Unretained(this),
                 requesting_process_ids,
                 message));
}

void MediaStreamManager::AddLogMessageOnUIThread(
    const std::set<int>& requesting_process_ids,
    const std::string& message) {
#if defined(ENABLE_WEBRTC)
  // Must be on the UI thread to access RenderProcessHost from process ID.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (std::set<int>::const_iterator it = requesting_process_ids.begin();
       it != requesting_process_ids.end(); ++it) {
    // Log the message to all renderers that are requesting a MediaStream or
    // have a MediaStream running.
    content::RenderProcessHostImpl* render_process_host_impl =
        static_cast<content::RenderProcessHostImpl*>(
            content::RenderProcessHost::FromID(*it));
    if (render_process_host_impl)
      render_process_host_impl->WebRtcLogMessage(message);
  }
#endif
}

void MediaStreamManager::HandleAccessRequestResponse(
    const std::string& label,
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
  for (MediaStreamDevices::const_iterator device_it = devices.begin();
       device_it != devices.end(); ++device_it) {
    StreamDeviceInfo device_info;
    device_info.device = *device_it;

    // TODO(justinlin): Nicer way to do this?
    // Re-append the device's id since we lost it when posting request to UI.
    if (device_info.device.type == content::MEDIA_TAB_VIDEO_CAPTURE ||
        device_info.device.type == content::MEDIA_TAB_AUDIO_CAPTURE) {
      device_info.device.id = request->UIRequest()->tab_capture_device_id;

      // Initialize the sample_rate and channel_layout here since for audio
      // mirroring, we don't go through EnumerateDevices where these are usually
      // initialized.
      if (device_info.device.type == content::MEDIA_TAB_AUDIO_CAPTURE) {
        const media::AudioParameters parameters =
            audio_manager_->GetDefaultOutputStreamParameters();
        int sample_rate = parameters.sample_rate();
        // If we weren't able to get the native sampling rate or the sample_rate
        // is outside the valid range for input devices set reasonable defaults.
        if (sample_rate <= 0 || sample_rate > 96000)
          sample_rate = 44100;

        device_info.device.input.sample_rate = sample_rate;
        device_info.device.input.channel_layout = media::CHANNEL_LAYOUT_STEREO;
      }
    }

    if (device_info.device.type == request->audio_type()) {
      found_audio = true;
    } else if (device_info.device.type == request->video_type()) {
      found_video = true;
    }

    // If this is request for a new MediaStream, a device is only opened once
    // per render view. This is so that the permission to use a device can be
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
                 << ", {label = " << label <<  "}"
                 << ", device_id = " << device_it->id << "}";
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
    for (StreamDeviceInfoArray::iterator device_it = request->devices.begin();
         device_it != request->devices.end(); ++device_it) {
      request->requester->DeviceStopped(request->requesting_view_id,
                                        label,
                                        *device_it);
    }
  }

  CancelRequest(label);
}

void MediaStreamManager::UseFakeUI(scoped_ptr<FakeMediaStreamUIProxy> fake_ui) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  use_fake_ui_ = true;
  fake_ui_ = fake_ui.Pass();
}

void MediaStreamManager::WillDestroyCurrentMessageLoop() {
  DVLOG(3) << "MediaStreamManager::WillDestroyCurrentMessageLoop()";
  DCHECK_EQ(base::MessageLoop::current(), io_loop_);
  DCHECK(requests_.empty());
  if (device_task_runner_) {
    StopMonitoring();

    video_capture_manager_->Unregister();
    audio_input_device_manager_->Unregister();
    device_task_runner_ = NULL;
  }

  audio_input_device_manager_ = NULL;
  video_capture_manager_ = NULL;
}

void MediaStreamManager::NotifyDevicesChanged(
    MediaStreamType stream_type,
    const StreamDeviceInfoArray& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaObserver* media_observer =
      GetContentClient()->browser()->GetMediaObserver();

  // Map the devices to MediaStreamDevices.
  MediaStreamDevices new_devices;
  for (StreamDeviceInfoArray::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    new_devices.push_back(it->device);
  }

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
  if (IsVideoMediaType(stream_type)) {
    return video_capture_manager();
  } else if (IsAudioInputMediaType(stream_type)) {
    return audio_input_device_manager();
  }
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

  // Always do enumeration even though some enumeration is in progress,
  // because those enumeration commands could be sent before these devices
  // change.
  ++active_enumeration_ref_count_[stream_type];
  GetDeviceManager(stream_type)->EnumerateDevices(stream_type);
}

void MediaStreamManager::OnMediaStreamUIWindowId(MediaStreamType video_type,
                                                 StreamDeviceInfoArray devices,
                                                 gfx::NativeViewId window_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!window_id)
    return;

  // Pass along for desktop capturing. Ignored for other stream types.
  if (video_type == MEDIA_DESKTOP_VIDEO_CAPTURE) {
    for (StreamDeviceInfoArray::iterator it = devices.begin();
         it != devices.end();
         ++it) {
      if (it->device.type == MEDIA_DESKTOP_VIDEO_CAPTURE) {
        video_capture_manager_->SetDesktopCaptureWindowId(it->session_id,
                                                          window_id);
        break;
      }
    }
  }
}

}  // namespace content

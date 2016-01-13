// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_impl.h"

#include <utility>

#include "base/hash.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/media_stream_video_capturer_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/peer_connection_tracker.h"
#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_logging.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaDeviceInfo.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {
namespace {

void CopyStreamConstraints(const blink::WebMediaConstraints& constraints,
                           StreamOptions::Constraints* mandatory,
                           StreamOptions::Constraints* optional) {
  blink::WebVector<blink::WebMediaConstraint> mandatory_constraints;
  constraints.getMandatoryConstraints(mandatory_constraints);
  for (size_t i = 0; i < mandatory_constraints.size(); i++) {
    mandatory->push_back(StreamOptions::Constraint(
        mandatory_constraints[i].m_name.utf8(),
        mandatory_constraints[i].m_value.utf8()));
  }

  blink::WebVector<blink::WebMediaConstraint> optional_constraints;
  constraints.getOptionalConstraints(optional_constraints);
  for (size_t i = 0; i < optional_constraints.size(); i++) {
    optional->push_back(StreamOptions::Constraint(
        optional_constraints[i].m_name.utf8(),
        optional_constraints[i].m_value.utf8()));
  }
}

static int g_next_request_id  = 0;

}  // namespace

struct MediaStreamImpl::MediaDevicesRequestInfo {
  MediaDevicesRequestInfo(const blink::WebMediaDevicesRequest& request,
                          int audio_input_request_id,
                          int video_input_request_id,
                          int audio_output_request_id)
      : request(request),
        audio_input_request_id(audio_input_request_id),
        video_input_request_id(video_input_request_id),
        audio_output_request_id(audio_output_request_id),
        has_audio_input_returned(false),
        has_video_input_returned(false),
        has_audio_output_returned(false) {}

  blink::WebMediaDevicesRequest request;
  int audio_input_request_id;
  int video_input_request_id;
  int audio_output_request_id;
  bool has_audio_input_returned;
  bool has_video_input_returned;
  bool has_audio_output_returned;
  StreamDeviceInfoArray audio_input_devices;
  StreamDeviceInfoArray video_input_devices;
  StreamDeviceInfoArray audio_output_devices;
};

MediaStreamImpl::MediaStreamImpl(
    RenderView* render_view,
    MediaStreamDispatcher* media_stream_dispatcher,
    PeerConnectionDependencyFactory* dependency_factory)
    : RenderViewObserver(render_view),
      dependency_factory_(dependency_factory),
      media_stream_dispatcher_(media_stream_dispatcher) {
}

MediaStreamImpl::~MediaStreamImpl() {
}

void MediaStreamImpl::requestUserMedia(
    const blink::WebUserMediaRequest& user_media_request) {
  // Save histogram data so we can see how much GetUserMedia is used.
  // The histogram counts the number of calls to the JS API
  // webGetUserMedia.
  UpdateWebRTCMethodCount(WEBKIT_GET_USER_MEDIA);
  DCHECK(CalledOnValidThread());

  if (RenderThreadImpl::current()) {
    RenderThreadImpl::current()->peer_connection_tracker()->TrackGetUserMedia(
        user_media_request);
  }

  int request_id = g_next_request_id++;
  StreamOptions options;
  blink::WebLocalFrame* frame = NULL;
  GURL security_origin;
  bool enable_automatic_output_device_selection = false;

  // |user_media_request| can't be mocked. So in order to test at all we check
  // if it isNull.
  if (user_media_request.isNull()) {
    // We are in a test.
    options.audio_requested = true;
    options.video_requested = true;
  } else {
    if (user_media_request.audio()) {
      options.audio_requested = true;
      CopyStreamConstraints(user_media_request.audioConstraints(),
                            &options.mandatory_audio,
                            &options.optional_audio);

      // Check if this input device should be used to select a matching output
      // device for audio rendering.
      std::string enable;
      if (options.GetFirstAudioConstraintByName(
              kMediaStreamRenderToAssociatedSink, &enable, NULL) &&
          LowerCaseEqualsASCII(enable, "true")) {
        enable_automatic_output_device_selection = true;
      }
    }
    if (user_media_request.video()) {
      options.video_requested = true;
      CopyStreamConstraints(user_media_request.videoConstraints(),
                            &options.mandatory_video,
                            &options.optional_video);
    }

    security_origin = GURL(user_media_request.securityOrigin().toString());
    // Get the WebFrame that requested a MediaStream.
    // The frame is needed to tell the MediaStreamDispatcher when a stream goes
    // out of scope.
    frame = user_media_request.ownerDocument().frame();
    DCHECK(frame);
  }

  DVLOG(1) << "MediaStreamImpl::requestUserMedia(" << request_id << ", [ "
           << "audio=" << (options.audio_requested)
           << " select associated sink: "
           << enable_automatic_output_device_selection
           << ", video=" << (options.video_requested) << " ], "
           << security_origin.spec() << ")";

  std::string audio_device_id;
  bool mandatory_audio;
  options.GetFirstAudioConstraintByName(kMediaStreamSourceInfoId,
                                        &audio_device_id, &mandatory_audio);
  std::string video_device_id;
  bool mandatory_video;
  options.GetFirstVideoConstraintByName(kMediaStreamSourceInfoId,
                                        &video_device_id, &mandatory_video);

  WebRtcLogMessage(base::StringPrintf(
      "MSI::requestUserMedia. request_id=%d"
      ", audio source id=%s mandatory= %s "
      ", video source id=%s mandatory= %s",
      request_id,
      audio_device_id.c_str(),
      mandatory_audio ? "true":"false",
      video_device_id.c_str(),
      mandatory_video ? "true":"false"));

  user_media_requests_.push_back(
      new UserMediaRequestInfo(request_id, frame, user_media_request,
          enable_automatic_output_device_selection));

  media_stream_dispatcher_->GenerateStream(
      request_id,
      AsWeakPtr(),
      options,
      security_origin);
}

void MediaStreamImpl::cancelUserMediaRequest(
    const blink::WebUserMediaRequest& user_media_request) {
  DCHECK(CalledOnValidThread());
  UserMediaRequestInfo* request = FindUserMediaRequestInfo(user_media_request);
  if (request) {
    // We can't abort the stream generation process.
    // Instead, erase the request. Once the stream is generated we will stop the
    // stream if the request does not exist.
    DeleteUserMediaRequestInfo(request);
  }
}

void MediaStreamImpl::requestMediaDevices(
    const blink::WebMediaDevicesRequest& media_devices_request) {
  UpdateWebRTCMethodCount(WEBKIT_GET_MEDIA_DEVICES);
  DCHECK(CalledOnValidThread());

  int audio_input_request_id = g_next_request_id++;
  int video_input_request_id = g_next_request_id++;
  int audio_output_request_id = g_next_request_id++;

  // |media_devices_request| can't be mocked, so in tests it will be empty (the
  // underlying pointer is null). In order to use this function in a test we
  // need to check if it isNull.
  GURL security_origin;
  if (!media_devices_request.isNull())
    security_origin = GURL(media_devices_request.securityOrigin().toString());

  DVLOG(1) << "MediaStreamImpl::requestMediaDevices(" << audio_input_request_id
           << ", " << video_input_request_id << ", " << audio_output_request_id
           << ", " << security_origin.spec() << ")";

  media_devices_requests_.push_back(new MediaDevicesRequestInfo(
      media_devices_request,
      audio_input_request_id,
      video_input_request_id,
      audio_output_request_id));

  media_stream_dispatcher_->EnumerateDevices(
      audio_input_request_id,
      AsWeakPtr(),
      MEDIA_DEVICE_AUDIO_CAPTURE,
      security_origin,
      true);

  media_stream_dispatcher_->EnumerateDevices(
      video_input_request_id,
      AsWeakPtr(),
      MEDIA_DEVICE_VIDEO_CAPTURE,
      security_origin,
      true);

  media_stream_dispatcher_->EnumerateDevices(
      audio_output_request_id,
      AsWeakPtr(),
      MEDIA_DEVICE_AUDIO_OUTPUT,
      security_origin,
      true);
}

void MediaStreamImpl::cancelMediaDevicesRequest(
    const blink::WebMediaDevicesRequest& media_devices_request) {
  DCHECK(CalledOnValidThread());
  MediaDevicesRequestInfo* request =
      FindMediaDevicesRequestInfo(media_devices_request);
  if (!request)
    return;

  // Cancel device enumeration.
  media_stream_dispatcher_->StopEnumerateDevices(
      request->audio_input_request_id,
      AsWeakPtr());
  media_stream_dispatcher_->StopEnumerateDevices(
      request->video_input_request_id,
      AsWeakPtr());
  media_stream_dispatcher_->StopEnumerateDevices(
      request->audio_output_request_id,
      AsWeakPtr());
  DeleteMediaDevicesRequestInfo(request);
}

// Callback from MediaStreamDispatcher.
// The requested stream have been generated by the MediaStreamDispatcher.
void MediaStreamImpl::OnStreamGenerated(
    int request_id,
    const std::string& label,
    const StreamDeviceInfoArray& audio_array,
    const StreamDeviceInfoArray& video_array) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "MediaStreamImpl::OnStreamGenerated stream:" << label;

  UserMediaRequestInfo* request_info = FindUserMediaRequestInfo(request_id);
  if (!request_info) {
    // This can happen if the request is canceled or the frame reloads while
    // MediaStreamDispatcher is processing the request.
    // Only stop the device if the device is not used in another MediaStream.
    for (StreamDeviceInfoArray::const_iterator device_it = audio_array.begin();
         device_it != audio_array.end(); ++device_it) {
      if (!FindLocalSource(*device_it))
        media_stream_dispatcher_->StopStreamDevice(*device_it);
    }

    for (StreamDeviceInfoArray::const_iterator device_it = video_array.begin();
         device_it != video_array.end(); ++device_it) {
      if (!FindLocalSource(*device_it))
        media_stream_dispatcher_->StopStreamDevice(*device_it);
    }

    DVLOG(1) << "Request ID not found";
    return;
  }
  request_info->generated = true;

  // WebUserMediaRequest don't have an implementation in unit tests.
  // Therefore we need to check for isNull here and initialize the
  // constraints.
  blink::WebUserMediaRequest* request = &(request_info->request);
  blink::WebMediaConstraints audio_constraints;
  blink::WebMediaConstraints video_constraints;
  if (request->isNull()) {
    audio_constraints.initialize();
    video_constraints.initialize();
  } else {
    audio_constraints = request->audioConstraints();
    video_constraints = request->videoConstraints();
  }

  blink::WebVector<blink::WebMediaStreamTrack> audio_track_vector(
      audio_array.size());
  CreateAudioTracks(audio_array, audio_constraints, &audio_track_vector,
                    request_info);

  blink::WebVector<blink::WebMediaStreamTrack> video_track_vector(
      video_array.size());
  CreateVideoTracks(video_array, video_constraints, &video_track_vector,
                    request_info);

  blink::WebString webkit_id = base::UTF8ToUTF16(label);
  blink::WebMediaStream* web_stream = &(request_info->web_stream);

  web_stream->initialize(webkit_id, audio_track_vector,
                         video_track_vector);
  web_stream->setExtraData(
      new MediaStream(
          *web_stream));

  // Wait for the tracks to be started successfully or to fail.
  request_info->CallbackOnTracksStarted(
      base::Bind(&MediaStreamImpl::OnCreateNativeTracksCompleted, AsWeakPtr()));
}

// Callback from MediaStreamDispatcher.
// The requested stream failed to be generated.
void MediaStreamImpl::OnStreamGenerationFailed(
    int request_id,
    content::MediaStreamRequestResult result) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "MediaStreamImpl::OnStreamGenerationFailed("
           << request_id << ")";
  UserMediaRequestInfo* request_info = FindUserMediaRequestInfo(request_id);
  if (!request_info) {
    // This can happen if the request is canceled or the frame reloads while
    // MediaStreamDispatcher is processing the request.
    DVLOG(1) << "Request ID not found";
    return;
  }

  GetUserMediaRequestFailed(&request_info->request, result);
  DeleteUserMediaRequestInfo(request_info);
}

// Callback from MediaStreamDispatcher.
// The browser process has stopped a device used by a MediaStream.
void MediaStreamImpl::OnDeviceStopped(
    const std::string& label,
    const StreamDeviceInfo& device_info) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "MediaStreamImpl::OnDeviceStopped("
           << "{device_id = " << device_info.device.id << "})";

  const blink::WebMediaStreamSource* source_ptr = FindLocalSource(device_info);
  if (!source_ptr) {
    // This happens if the same device is used in several guM requests or
    // if a user happen stop a track from JS at the same time
    // as the underlying media device is unplugged from the system.
    return;
  }
  // By creating |source| it is guaranteed that the blink::WebMediaStreamSource
  // object is valid during the cleanup.
  blink::WebMediaStreamSource source(*source_ptr);
  StopLocalSource(source, false);

  for (LocalStreamSources::iterator device_it = local_sources_.begin();
       device_it != local_sources_.end(); ++device_it) {
    if (device_it->source.id() == source.id()) {
      local_sources_.erase(device_it);
      break;
    }
  }
}

void MediaStreamImpl::InitializeSourceObject(
    const StreamDeviceInfo& device,
    blink::WebMediaStreamSource::Type type,
    const blink::WebMediaConstraints& constraints,
    blink::WebFrame* frame,
    blink::WebMediaStreamSource* webkit_source) {
  const blink::WebMediaStreamSource* existing_source =
      FindLocalSource(device);
  if (existing_source) {
    *webkit_source = *existing_source;
    DVLOG(1) << "Source already exist. Reusing source with id "
             << webkit_source->id().utf8();
    return;
  }

  webkit_source->initialize(
      base::UTF8ToUTF16(device.device.id),
      type,
      base::UTF8ToUTF16(device.device.name));

  DVLOG(1) << "Initialize source object :"
           << "id = " << webkit_source->id().utf8()
           << ", name = " << webkit_source->name().utf8();

  if (type == blink::WebMediaStreamSource::TypeVideo) {
    webkit_source->setExtraData(
        CreateVideoSource(
            device,
            base::Bind(&MediaStreamImpl::OnLocalSourceStopped, AsWeakPtr())));
  } else {
    DCHECK_EQ(blink::WebMediaStreamSource::TypeAudio, type);
    MediaStreamAudioSource* audio_source(
        new MediaStreamAudioSource(
            RenderViewObserver::routing_id(),
            device,
            base::Bind(&MediaStreamImpl::OnLocalSourceStopped, AsWeakPtr()),
            dependency_factory_));
    webkit_source->setExtraData(audio_source);
  }
  local_sources_.push_back(LocalStreamSource(frame, *webkit_source));
}

MediaStreamVideoSource* MediaStreamImpl::CreateVideoSource(
    const StreamDeviceInfo& device,
    const MediaStreamSource::SourceStoppedCallback& stop_callback) {
  return new content::MediaStreamVideoCapturerSource(
      device,
      stop_callback,
      new VideoCapturerDelegate(device));
}

void MediaStreamImpl::CreateVideoTracks(
    const StreamDeviceInfoArray& devices,
    const blink::WebMediaConstraints& constraints,
    blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks,
    UserMediaRequestInfo* request) {
  DCHECK_EQ(devices.size(), webkit_tracks->size());

  for (size_t i = 0; i < devices.size(); ++i) {
    blink::WebMediaStreamSource webkit_source;
    InitializeSourceObject(devices[i],
                           blink::WebMediaStreamSource::TypeVideo,
                           constraints,
                           request->frame,
                           &webkit_source);
    (*webkit_tracks)[i] =
        request->CreateAndStartVideoTrack(webkit_source, constraints);
  }
}

void MediaStreamImpl::CreateAudioTracks(
    const StreamDeviceInfoArray& devices,
    const blink::WebMediaConstraints& constraints,
    blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks,
    UserMediaRequestInfo* request) {
  DCHECK_EQ(devices.size(), webkit_tracks->size());

  // Log the device names for this request.
  for (StreamDeviceInfoArray::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    WebRtcLogMessage(base::StringPrintf(
        "Generated media stream for request id %d contains audio device name"
        " \"%s\"",
        request->request_id,
        it->device.name.c_str()));
  }

  StreamDeviceInfoArray overridden_audio_array = devices;
  if (!request->enable_automatic_output_device_selection) {
    // If the GetUserMedia request did not explicitly set the constraint
    // kMediaStreamRenderToAssociatedSink, the output device parameters must
    // be removed.
    for (StreamDeviceInfoArray::iterator it = overridden_audio_array.begin();
         it != overridden_audio_array.end(); ++it) {
      it->device.matched_output_device_id = "";
      it->device.matched_output = MediaStreamDevice::AudioDeviceParameters();
    }
  }

  for (size_t i = 0; i < overridden_audio_array.size(); ++i) {
    blink::WebMediaStreamSource webkit_source;
    InitializeSourceObject(overridden_audio_array[i],
                           blink::WebMediaStreamSource::TypeAudio,
                           constraints,
                           request->frame,
                           &webkit_source);
    (*webkit_tracks)[i].initialize(webkit_source);
    request->StartAudioTrack((*webkit_tracks)[i], constraints);
  }
}

void MediaStreamImpl::OnCreateNativeTracksCompleted(
    UserMediaRequestInfo* request,
    content::MediaStreamRequestResult result) {
  DVLOG(1) << "MediaStreamImpl::OnCreateNativeTracksComplete("
           << "{request_id = " << request->request_id << "} "
           << "{result = " << result << "})";
  if (result == content::MEDIA_DEVICE_OK)
    GetUserMediaRequestSucceeded(request->web_stream, &request->request);
  else
    GetUserMediaRequestFailed(&request->request, result);

  DeleteUserMediaRequestInfo(request);
}

void MediaStreamImpl::OnDevicesEnumerated(
    int request_id,
    const StreamDeviceInfoArray& device_array) {
  DVLOG(1) << "MediaStreamImpl::OnDevicesEnumerated(" << request_id << ")";

  MediaDevicesRequestInfo* request = FindMediaDevicesRequestInfo(request_id);
  DCHECK(request);

  if (request_id == request->audio_input_request_id) {
    request->has_audio_input_returned = true;
    DCHECK(request->audio_input_devices.empty());
    request->audio_input_devices = device_array;
  } else if (request_id == request->video_input_request_id) {
    request->has_video_input_returned = true;
    DCHECK(request->video_input_devices.empty());
    request->video_input_devices = device_array;
  } else {
    DCHECK_EQ(request->audio_output_request_id, request_id);
    request->has_audio_output_returned = true;
    DCHECK(request->audio_output_devices.empty());
    request->audio_output_devices = device_array;
  }

  if (!request->has_audio_input_returned ||
      !request->has_video_input_returned ||
      !request->has_audio_output_returned) {
    // Wait for the rest of the devices to complete.
    return;
  }

  // All devices are ready for copying. We use a hashed audio output device id
  // as the group id for input and output audio devices. If an input device
  // doesn't have an associated output device, we use the input device's own id.
  // We don't support group id for video devices, that's left empty.
  blink::WebVector<blink::WebMediaDeviceInfo>
      devices(request->audio_input_devices.size() +
              request->video_input_devices.size() +
              request->audio_output_devices.size());
  for (size_t i = 0; i  < request->audio_input_devices.size(); ++i) {
    const MediaStreamDevice& device = request->audio_input_devices[i].device;
    DCHECK_EQ(device.type, MEDIA_DEVICE_AUDIO_CAPTURE);
    std::string group_id = base::UintToString(base::Hash(
        !device.matched_output_device_id.empty() ?
            device.matched_output_device_id :
            device.id));
    devices[i].initialize(
        blink::WebString::fromUTF8(device.id),
        blink::WebMediaDeviceInfo::MediaDeviceKindAudioInput,
        blink::WebString::fromUTF8(device.name),
        blink::WebString::fromUTF8(group_id));
  }
  size_t offset = request->audio_input_devices.size();
  for (size_t i = 0; i  < request->video_input_devices.size(); ++i) {
    const MediaStreamDevice& device = request->video_input_devices[i].device;
    DCHECK_EQ(device.type, MEDIA_DEVICE_VIDEO_CAPTURE);
    devices[offset + i].initialize(
        blink::WebString::fromUTF8(device.id),
        blink::WebMediaDeviceInfo::MediaDeviceKindVideoInput,
        blink::WebString::fromUTF8(device.name),
        blink::WebString());
  }
  offset += request->video_input_devices.size();
  for (size_t i = 0; i  < request->audio_output_devices.size(); ++i) {
    const MediaStreamDevice& device = request->audio_output_devices[i].device;
    DCHECK_EQ(device.type, MEDIA_DEVICE_AUDIO_OUTPUT);
    devices[offset + i].initialize(
        blink::WebString::fromUTF8(device.id),
        blink::WebMediaDeviceInfo::MediaDeviceKindAudioOutput,
        blink::WebString::fromUTF8(device.name),
        blink::WebString::fromUTF8(base::UintToString(base::Hash(device.id))));
  }

  EnumerateDevicesSucceded(&request->request, devices);

  // Cancel device enumeration.
  media_stream_dispatcher_->StopEnumerateDevices(
      request->audio_input_request_id,
      AsWeakPtr());
  media_stream_dispatcher_->StopEnumerateDevices(
      request->video_input_request_id,
      AsWeakPtr());
  media_stream_dispatcher_->StopEnumerateDevices(
      request->audio_output_request_id,
      AsWeakPtr());

  DeleteMediaDevicesRequestInfo(request);
}

void MediaStreamImpl::OnDeviceOpened(
    int request_id,
    const std::string& label,
    const StreamDeviceInfo& video_device) {
  DVLOG(1) << "MediaStreamImpl::OnDeviceOpened("
           << request_id << ", " << label << ")";
  NOTIMPLEMENTED();
}

void MediaStreamImpl::OnDeviceOpenFailed(int request_id) {
  DVLOG(1) << "MediaStreamImpl::VideoDeviceOpenFailed("
           << request_id << ")";
  NOTIMPLEMENTED();
}

void MediaStreamImpl::GetUserMediaRequestSucceeded(
    const blink::WebMediaStream& stream,
    blink::WebUserMediaRequest* request_info) {
  DVLOG(1) << "MediaStreamImpl::GetUserMediaRequestSucceeded";
  request_info->requestSucceeded(stream);
}

void MediaStreamImpl::GetUserMediaRequestFailed(
    blink::WebUserMediaRequest* request_info,
    content::MediaStreamRequestResult result) {
  switch (result) {
    case MEDIA_DEVICE_OK:
      NOTREACHED();
      break;
    case MEDIA_DEVICE_PERMISSION_DENIED:
      request_info->requestDenied();
      break;
    case MEDIA_DEVICE_PERMISSION_DISMISSED:
      request_info->requestFailedUASpecific("PermissionDismissedError");
      break;
    case MEDIA_DEVICE_INVALID_STATE:
      request_info->requestFailedUASpecific("InvalidStateError");
      break;
    case MEDIA_DEVICE_NO_HARDWARE:
      request_info->requestFailedUASpecific("DevicesNotFoundError");
      break;
    case MEDIA_DEVICE_INVALID_SECURITY_ORIGIN:
      request_info->requestFailedUASpecific("InvalidSecurityOriginError");
      break;
    case MEDIA_DEVICE_TAB_CAPTURE_FAILURE:
      request_info->requestFailedUASpecific("TabCaptureError");
      break;
    case MEDIA_DEVICE_SCREEN_CAPTURE_FAILURE:
      request_info->requestFailedUASpecific("ScreenCaptureError");
      break;
    case MEDIA_DEVICE_CAPTURE_FAILURE:
      request_info->requestFailedUASpecific("DeviceCaptureError");
      break;
    case MEDIA_DEVICE_TRACK_START_FAILURE:
      request_info->requestFailedUASpecific("TrackStartError");
      break;
    default:
      request_info->requestFailed();
      break;
  }
}

void MediaStreamImpl::EnumerateDevicesSucceded(
    blink::WebMediaDevicesRequest* request,
    blink::WebVector<blink::WebMediaDeviceInfo>& devices) {
  request->requestSucceeded(devices);
}

const blink::WebMediaStreamSource* MediaStreamImpl::FindLocalSource(
    const StreamDeviceInfo& device) const {
  for (LocalStreamSources::const_iterator it = local_sources_.begin();
       it != local_sources_.end(); ++it) {
    MediaStreamSource* source =
        static_cast<MediaStreamSource*>(it->source.extraData());
    const StreamDeviceInfo& active_device = source->device_info();
    if (active_device.device.id == device.device.id &&
        active_device.device.type == device.device.type &&
        active_device.session_id == device.session_id) {
      return &it->source;
    }
  }
  return NULL;
}

MediaStreamImpl::UserMediaRequestInfo*
MediaStreamImpl::FindUserMediaRequestInfo(int request_id) {
  UserMediaRequests::iterator it = user_media_requests_.begin();
  for (; it != user_media_requests_.end(); ++it) {
    if ((*it)->request_id == request_id)
      return (*it);
  }
  return NULL;
}

MediaStreamImpl::UserMediaRequestInfo*
MediaStreamImpl::FindUserMediaRequestInfo(
    const blink::WebUserMediaRequest& request) {
  UserMediaRequests::iterator it = user_media_requests_.begin();
  for (; it != user_media_requests_.end(); ++it) {
    if ((*it)->request == request)
      return (*it);
  }
  return NULL;
}

void MediaStreamImpl::DeleteUserMediaRequestInfo(
    UserMediaRequestInfo* request) {
  UserMediaRequests::iterator it = user_media_requests_.begin();
  for (; it != user_media_requests_.end(); ++it) {
    if ((*it) == request) {
      user_media_requests_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

MediaStreamImpl::MediaDevicesRequestInfo*
MediaStreamImpl::FindMediaDevicesRequestInfo(
    int request_id) {
  MediaDevicesRequests::iterator it = media_devices_requests_.begin();
  for (; it != media_devices_requests_.end(); ++it) {
    if ((*it)->audio_input_request_id == request_id ||
        (*it)->video_input_request_id == request_id ||
        (*it)->audio_output_request_id == request_id) {
      return (*it);
    }
  }
  return NULL;
}

MediaStreamImpl::MediaDevicesRequestInfo*
MediaStreamImpl::FindMediaDevicesRequestInfo(
    const blink::WebMediaDevicesRequest& request) {
  MediaDevicesRequests::iterator it = media_devices_requests_.begin();
  for (; it != media_devices_requests_.end(); ++it) {
    if ((*it)->request == request)
      return (*it);
  }
  return NULL;
}

void MediaStreamImpl::DeleteMediaDevicesRequestInfo(
    MediaDevicesRequestInfo* request) {
  MediaDevicesRequests::iterator it = media_devices_requests_.begin();
  for (; it != media_devices_requests_.end(); ++it) {
    if ((*it) == request) {
      media_devices_requests_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

void MediaStreamImpl::FrameDetached(blink::WebFrame* frame) {
  // Do same thing as FrameWillClose.
  FrameWillClose(frame);
}

void MediaStreamImpl::FrameWillClose(blink::WebFrame* frame) {
  // Loop through all UserMediaRequests and find the requests that belong to the
  // frame that is being closed.
  UserMediaRequests::iterator request_it = user_media_requests_.begin();
  while (request_it != user_media_requests_.end()) {
    if ((*request_it)->frame == frame) {
      DVLOG(1) << "MediaStreamImpl::FrameWillClose: "
               << "Cancel user media request " << (*request_it)->request_id;
      // If the request is not generated, it means that a request
      // has been sent to the MediaStreamDispatcher to generate a stream
      // but MediaStreamDispatcher has not yet responded and we need to cancel
      // the request.
      if (!(*request_it)->generated) {
        media_stream_dispatcher_->CancelGenerateStream(
            (*request_it)->request_id, AsWeakPtr());
      }
      request_it = user_media_requests_.erase(request_it);
    } else {
      ++request_it;
    }
  }

  // Loop through all current local sources and stop the sources that were
  // created by the frame that will be closed.
  LocalStreamSources::iterator sources_it = local_sources_.begin();
  while (sources_it != local_sources_.end()) {
    if (sources_it->frame == frame) {
      StopLocalSource(sources_it->source, true);
      sources_it = local_sources_.erase(sources_it);
    } else {
      ++sources_it;
    }
  }
}

void MediaStreamImpl::OnLocalSourceStopped(
    const blink::WebMediaStreamSource& source) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "MediaStreamImpl::OnLocalSourceStopped";

  bool device_found = false;
  for (LocalStreamSources::iterator device_it = local_sources_.begin();
       device_it != local_sources_.end(); ++device_it) {
    if (device_it->source.id()  == source.id()) {
      device_found = true;
      local_sources_.erase(device_it);
      break;
    }
  }
  CHECK(device_found);

  MediaStreamSource* source_impl =
      static_cast<MediaStreamSource*> (source.extraData());
  media_stream_dispatcher_->StopStreamDevice(source_impl->device_info());
}

void MediaStreamImpl::StopLocalSource(
    const blink::WebMediaStreamSource& source,
    bool notify_dispatcher) {
  MediaStreamSource* source_impl =
      static_cast<MediaStreamSource*> (source.extraData());
  DVLOG(1) << "MediaStreamImpl::StopLocalSource("
           << "{device_id = " << source_impl->device_info().device.id << "})";

  if (notify_dispatcher)
    media_stream_dispatcher_->StopStreamDevice(source_impl->device_info());

  source_impl->ResetSourceStoppedCallback();
  source_impl->StopSource();
}

MediaStreamImpl::UserMediaRequestInfo::UserMediaRequestInfo(
    int request_id,
    blink::WebFrame* frame,
    const blink::WebUserMediaRequest& request,
    bool enable_automatic_output_device_selection)
    : request_id(request_id),
      generated(false),
      enable_automatic_output_device_selection(
          enable_automatic_output_device_selection),
      frame(frame),
      request(request),
      request_failed_(false) {
}

MediaStreamImpl::UserMediaRequestInfo::~UserMediaRequestInfo() {
  DVLOG(1) << "~UserMediaRequestInfo";
}

void MediaStreamImpl::UserMediaRequestInfo::StartAudioTrack(
    const blink::WebMediaStreamTrack& track,
    const blink::WebMediaConstraints& constraints) {
  DCHECK(track.source().type() == blink::WebMediaStreamSource::TypeAudio);
  MediaStreamAudioSource* native_source =
      static_cast <MediaStreamAudioSource*>(track.source().extraData());
  DCHECK(native_source);

  sources_.push_back(track.source());
  sources_waiting_for_callback_.push_back(native_source);
  native_source->AddTrack(
      track, constraints, base::Bind(
          &MediaStreamImpl::UserMediaRequestInfo::OnTrackStarted,
          AsWeakPtr()));
}

blink::WebMediaStreamTrack
MediaStreamImpl::UserMediaRequestInfo::CreateAndStartVideoTrack(
    const blink::WebMediaStreamSource& source,
    const blink::WebMediaConstraints& constraints) {
  DCHECK(source.type() == blink::WebMediaStreamSource::TypeVideo);
  MediaStreamVideoSource* native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);
  sources_.push_back(source);
  sources_waiting_for_callback_.push_back(native_source);
  return MediaStreamVideoTrack::CreateVideoTrack(
      native_source, constraints, base::Bind(
          &MediaStreamImpl::UserMediaRequestInfo::OnTrackStarted,
          AsWeakPtr()),
      true);
}

void MediaStreamImpl::UserMediaRequestInfo::CallbackOnTracksStarted(
    const ResourcesReady& callback) {
  DCHECK(ready_callback_.is_null());
  ready_callback_ = callback;
  CheckAllTracksStarted();
}

void MediaStreamImpl::UserMediaRequestInfo::OnTrackStarted(
    MediaStreamSource* source, bool success) {
  DVLOG(1) << "OnTrackStarted result " << success;
  std::vector<MediaStreamSource*>::iterator it =
      std::find(sources_waiting_for_callback_.begin(),
                sources_waiting_for_callback_.end(),
                source);
  DCHECK(it != sources_waiting_for_callback_.end());
  sources_waiting_for_callback_.erase(it);
  // All tracks must be started successfully. Otherwise the request is a
  // failure.
  if (!success)
    request_failed_ = true;
  CheckAllTracksStarted();
}

void MediaStreamImpl::UserMediaRequestInfo::CheckAllTracksStarted() {
  if (!ready_callback_.is_null() && sources_waiting_for_callback_.empty()) {
    ready_callback_.Run(
        this,
        request_failed_ ? MEDIA_DEVICE_TRACK_START_FAILURE : MEDIA_DEVICE_OK);
  }
}

bool MediaStreamImpl::UserMediaRequestInfo::IsSourceUsed(
    const blink::WebMediaStreamSource& source) const {
  for (std::vector<blink::WebMediaStreamSource>::const_iterator source_it =
           sources_.begin();
       source_it != sources_.end(); ++source_it) {
    if (source_it->id() == source.id())
      return true;
  }
  return false;
}

void MediaStreamImpl::UserMediaRequestInfo::RemoveSource(
    const blink::WebMediaStreamSource& source) {
  for (std::vector<blink::WebMediaStreamSource>::iterator it =
           sources_.begin();
       it != sources_.end(); ++it) {
    if (source.id() == it->id()) {
      sources_.erase(it);
      return;
    }
  }
}

}  // namespace content

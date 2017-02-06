// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_USER_MEDIA_CLIENT_IMPL_H_
#define CONTENT_RENDERER_MEDIA_USER_MEDIA_CLIENT_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"
#include "content/renderer/media/media_stream_source.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebSourceInfo.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebMediaDeviceChangeObserver.h"
#include "third_party/WebKit/public/web/WebMediaDevicesRequest.h"
#include "third_party/WebKit/public/web/WebUserMediaClient.h"
#include "third_party/WebKit/public/web/WebUserMediaRequest.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

namespace content {
class PeerConnectionDependencyFactory;
class MediaStreamAudioSource;
class MediaStreamDispatcher;
class MediaStreamVideoSource;
class VideoCapturerDelegate;

// UserMediaClientImpl is a delegate for the Media Stream GetUserMedia API.
// It ties together WebKit and MediaStreamManager
// (via MediaStreamDispatcher and MediaStreamDispatcherHost)
// in the browser process. It must be created, called and destroyed on the
// render thread.
class CONTENT_EXPORT UserMediaClientImpl
    : public RenderFrameObserver,
      NON_EXPORTED_BASE(public blink::WebUserMediaClient),
      public MediaStreamDispatcherEventHandler,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  // |render_frame| and |dependency_factory| must outlive this instance.
  UserMediaClientImpl(
      RenderFrame* render_frame,
      PeerConnectionDependencyFactory* dependency_factory,
      std::unique_ptr<MediaStreamDispatcher> media_stream_dispatcher);
  ~UserMediaClientImpl() override;

  MediaStreamDispatcher* media_stream_dispatcher() const {
    return media_stream_dispatcher_.get();
  }

  // blink::WebUserMediaClient implementation
  void requestUserMedia(
      const blink::WebUserMediaRequest& user_media_request) override;
  void cancelUserMediaRequest(
      const blink::WebUserMediaRequest& user_media_request) override;
  void requestMediaDevices(
      const blink::WebMediaDevicesRequest& media_devices_request) override;
  void cancelMediaDevicesRequest(
      const blink::WebMediaDevicesRequest& media_devices_request) override;
  void requestSources(
      const blink::WebMediaStreamTrackSourcesRequest& sources_request) override;
  void setMediaDeviceChangeObserver(
      const blink::WebMediaDeviceChangeObserver& observer) override;

  // MediaStreamDispatcherEventHandler implementation.
  void OnStreamGenerated(int request_id,
                         const std::string& label,
                         const StreamDeviceInfoArray& audio_array,
                         const StreamDeviceInfoArray& video_array) override;
  void OnStreamGenerationFailed(int request_id,
                                MediaStreamRequestResult result) override;
  void OnDeviceStopped(const std::string& label,
                       const StreamDeviceInfo& device_info) override;
  void OnDevicesEnumerated(int request_id,
                           const StreamDeviceInfoArray& device_array) override;
  void OnDeviceOpened(int request_id,
                      const std::string& label,
                      const StreamDeviceInfo& device_info) override;
  void OnDeviceOpenFailed(int request_id) override;
  void OnDevicesChanged() override;

  // RenderFrameObserver override
  void FrameWillClose() override;

 protected:
  // Called when |source| has been stopped from JavaScript.
  void OnLocalSourceStopped(const blink::WebMediaStreamSource& source);

  // These methods are virtual for test purposes. A test can override them to
  // test requesting local media streams. The function notifies WebKit that the
  // |request| have completed.
  virtual void GetUserMediaRequestSucceeded(
       const blink::WebMediaStream& stream,
       blink::WebUserMediaRequest request_info);
  void DelayedGetUserMediaRequestSucceeded(
      const blink::WebMediaStream& stream,
      blink::WebUserMediaRequest request_info);
  virtual void GetUserMediaRequestFailed(
      blink::WebUserMediaRequest request_info,
      MediaStreamRequestResult result,
      const blink::WebString& result_name);
  void DelayedGetUserMediaRequestFailed(
      blink::WebUserMediaRequest request_info,
      MediaStreamRequestResult result,
      const blink::WebString& result_name);

  virtual void EnumerateDevicesSucceded(
      blink::WebMediaDevicesRequest* request,
      blink::WebVector<blink::WebMediaDeviceInfo>& devices);
  virtual void EnumerateSourcesSucceded(
      blink::WebMediaStreamTrackSourcesRequest* request,
      blink::WebVector<blink::WebSourceInfo>& sources);

  // Creates a MediaStreamAudioSource/MediaStreamVideoSource objects.
  // These are virtual for test purposes.
  virtual MediaStreamAudioSource* CreateAudioSource(
      const StreamDeviceInfo& device,
      const blink::WebMediaConstraints& constraints);
  virtual MediaStreamVideoSource* CreateVideoSource(
      const StreamDeviceInfo& device,
      const MediaStreamSource::SourceStoppedCallback& stop_callback);

  // Class for storing information about a WebKit request to create a
  // MediaStream.
  class UserMediaRequestInfo
      : public base::SupportsWeakPtr<UserMediaRequestInfo> {
   public:
    typedef base::Callback<void(UserMediaRequestInfo* request_info,
                                MediaStreamRequestResult result,
                                const blink::WebString& result_name)>
      ResourcesReady;

    UserMediaRequestInfo(int request_id,
                         const blink::WebUserMediaRequest& request,
                         bool enable_automatic_output_device_selection);
    ~UserMediaRequestInfo();
    int request_id;
    // True if MediaStreamDispatcher has generated the stream, see
    // OnStreamGenerated.
    bool generated;
    const bool enable_automatic_output_device_selection;
    blink::WebMediaStream web_stream;
    blink::WebUserMediaRequest request;

    void StartAudioTrack(const blink::WebMediaStreamTrack& track);

    blink::WebMediaStreamTrack CreateAndStartVideoTrack(
        const blink::WebMediaStreamSource& source,
        const blink::WebMediaConstraints& constraints);

    // Triggers |callback| when all sources used in this request have either
    // successfully started, or a source has failed to start.
    void CallbackOnTracksStarted(const ResourcesReady& callback);

    bool IsSourceUsed(const blink::WebMediaStreamSource& source) const;
    void RemoveSource(const blink::WebMediaStreamSource& source);

    bool HasPendingSources() const;

   private:
    void OnTrackStarted(
        MediaStreamSource* source,
        MediaStreamRequestResult result,
        const blink::WebString& result_name);
    void CheckAllTracksStarted();

    ResourcesReady ready_callback_;
    MediaStreamRequestResult request_result_;
    blink::WebString request_result_name_;
    // Sources used in this request.
    std::vector<blink::WebMediaStreamSource> sources_;
    std::vector<MediaStreamSource*> sources_waiting_for_callback_;
  };
  typedef ScopedVector<UserMediaRequestInfo> UserMediaRequests;

 protected:
  // These methods can be accessed in unit tests.
  UserMediaRequestInfo* FindUserMediaRequestInfo(int request_id);
  UserMediaRequestInfo* FindUserMediaRequestInfo(
      const blink::WebUserMediaRequest& request);

  void DeleteUserMediaRequestInfo(UserMediaRequestInfo* request);

 private:
  typedef std::vector<blink::WebMediaStreamSource> LocalStreamSources;
  struct MediaDevicesRequestInfo;
  typedef ScopedVector<MediaDevicesRequestInfo> MediaDevicesRequests;

  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // Creates a WebKit representation of stream sources based on
  // |devices| from the MediaStreamDispatcher.
  void InitializeSourceObject(
      const StreamDeviceInfo& device,
      blink::WebMediaStreamSource::Type type,
      const blink::WebMediaConstraints& constraints,
      blink::WebMediaStreamSource* webkit_source);

  void CreateVideoTracks(
      const StreamDeviceInfoArray& devices,
      const blink::WebMediaConstraints& constraints,
      blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks,
      UserMediaRequestInfo* request);

  void CreateAudioTracks(
      const StreamDeviceInfoArray& devices,
      const blink::WebMediaConstraints& constraints,
      blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks,
      UserMediaRequestInfo* request);

  // Callback function triggered when all native versions of the
  // underlying media sources and tracks have been created and started.
  void OnCreateNativeTracksCompleted(
      UserMediaRequestInfo* request,
      MediaStreamRequestResult result,
      const blink::WebString& result_name);

  void OnStreamGeneratedForCancelledRequest(
      const StreamDeviceInfoArray& audio_array,
      const StreamDeviceInfoArray& video_array);

  void FinalizeEnumerateDevices(MediaDevicesRequestInfo* request);
  void FinalizeEnumerateSources(MediaDevicesRequestInfo* request);

  void DeleteAllUserMediaRequests();

  MediaDevicesRequestInfo* FindMediaDevicesRequestInfo(int request_id);
  MediaDevicesRequestInfo* FindMediaDevicesRequestInfo(
      const blink::WebMediaDevicesRequest& request);
  void CancelAndDeleteMediaDevicesRequest(MediaDevicesRequestInfo* request);

  // Returns the source that use a device with |device.session_id|
  // and |device.device.id|. NULL if such source doesn't exist.
  const blink::WebMediaStreamSource* FindLocalSource(
      const StreamDeviceInfo& device) const;

  // Returns true if we do find and remove the |source|.
  // Otherwise returns false.
  bool RemoveLocalSource(const blink::WebMediaStreamSource& source);

  void StopLocalSource(const blink::WebMediaStreamSource& source,
                       bool notify_dispatcher);

  // Weak ref to a PeerConnectionDependencyFactory, owned by the RenderThread.
  // It's valid for the lifetime of RenderThread.
  // TODO(xians): Remove this dependency once audio do not need it for local
  // audio.
  PeerConnectionDependencyFactory* const dependency_factory_;

  // UserMediaClientImpl owns MediaStreamDispatcher instead of RenderFrameImpl
  // (or RenderFrameObserver) to ensure tear-down occurs in the right order.
  const std::unique_ptr<MediaStreamDispatcher> media_stream_dispatcher_;

  LocalStreamSources local_sources_;

  UserMediaRequests user_media_requests_;

  // Requests to enumerate media devices.
  MediaDevicesRequests media_devices_requests_;

  blink::WebMediaDeviceChangeObserver media_device_change_observer_;

  // Note: This member must be the last to ensure all outstanding weak pointers
  // are invalidated first.
  base::WeakPtrFactory<UserMediaClientImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserMediaClientImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_USER_MEDIA_CLIENT_IMPL_H_

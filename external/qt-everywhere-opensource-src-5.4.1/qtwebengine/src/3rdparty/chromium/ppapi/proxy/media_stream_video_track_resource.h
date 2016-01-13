// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_MEDIA_STREAM_VIDEO_TRACK_RESOURCE_H_
#define PPAPI_PROXY_MEDIA_STREAM_VIDEO_TRACK_RESOURCE_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "ppapi/proxy/media_stream_track_resource_base.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_media_stream_video_track_api.h"

namespace ppapi {
namespace proxy {

class VideoFrameResource;

class PPAPI_PROXY_EXPORT MediaStreamVideoTrackResource
    : public MediaStreamTrackResourceBase,
      public thunk::PPB_MediaStreamVideoTrack_API {
 public:
  MediaStreamVideoTrackResource(Connection connection,
                                PP_Instance instance,
                                int pending_renderer_id,
                                const std::string& id);

  MediaStreamVideoTrackResource(Connection connection, PP_Instance instance);

  virtual ~MediaStreamVideoTrackResource();

  // Resource overrides:
  virtual thunk::PPB_MediaStreamVideoTrack_API*
  AsPPB_MediaStreamVideoTrack_API() OVERRIDE;

  // PPB_MediaStreamVideoTrack_API overrides:
  virtual PP_Var GetId() OVERRIDE;
  virtual PP_Bool HasEnded() OVERRIDE;
  virtual int32_t Configure(const int32_t attrib_list[],
                            scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t GetAttrib(PP_MediaStreamVideoTrack_Attrib attrib,
                            int32_t* value) OVERRIDE;
  virtual int32_t GetFrame(PP_Resource* frame,
                           scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t RecycleFrame(PP_Resource frame) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual int32_t GetEmptyFrame(
      PP_Resource* frame, scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t PutFrame(PP_Resource frame) OVERRIDE;

  // MediaStreamBufferManager::Delegate overrides:
  virtual void OnNewBufferEnqueued() OVERRIDE;

 private:
  PP_Resource GetVideoFrame();

  void ReleaseFrames();

  // IPC message handlers.
  void OnPluginMsgConfigureReply(const ResourceMessageReplyParams& params,
                                 const std::string& track_id);

  // Allocated frame resources by |GetFrame()|.
  typedef std::map<PP_Resource, scoped_refptr<VideoFrameResource> > FrameMap;
  FrameMap frames_;

  PP_Resource* get_frame_output_;
  scoped_refptr<TrackedCallback> get_frame_callback_;

  scoped_refptr<TrackedCallback> configure_callback_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoTrackResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_MEDIA_STREAM_VIDEO_TRACK_RESOURCE_H_

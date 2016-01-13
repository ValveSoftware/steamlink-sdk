// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_MEDIA_STREAM_AUDIO_TRACK_RESOURCE_H_
#define PPAPI_PROXY_MEDIA_STREAM_AUDIO_TRACK_RESOURCE_H_

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "ppapi/proxy/media_stream_track_resource_base.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_media_stream_audio_track_api.h"

namespace ppapi {
namespace proxy {

class AudioBufferResource;

class PPAPI_PROXY_EXPORT MediaStreamAudioTrackResource
    : public MediaStreamTrackResourceBase,
      public thunk::PPB_MediaStreamAudioTrack_API {
 public:
  MediaStreamAudioTrackResource(Connection connection,
                                PP_Instance instance,
                                int pending_renderer_id,
                                const std::string& id);

  virtual ~MediaStreamAudioTrackResource();

  // Resource overrides:
  virtual thunk::PPB_MediaStreamAudioTrack_API*
  AsPPB_MediaStreamAudioTrack_API() OVERRIDE;

  // PPB_MediaStreamAudioTrack_API overrides:
  virtual PP_Var GetId() OVERRIDE;
  virtual PP_Bool HasEnded() OVERRIDE;
  virtual int32_t Configure(const int32_t attrib_list[],
                            scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t GetAttrib(PP_MediaStreamAudioTrack_Attrib attrib,
                            int32_t* value) OVERRIDE;
  virtual int32_t GetBuffer(
      PP_Resource* buffer,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t RecycleBuffer(PP_Resource buffer) OVERRIDE;
  virtual void Close() OVERRIDE;

  // MediaStreamBufferManager::Delegate overrides:
  virtual void OnNewBufferEnqueued() OVERRIDE;

 private:
  PP_Resource GetAudioBuffer();

  void ReleaseBuffers();

  // IPC message handlers.
  void OnPluginMsgConfigureReply(const ResourceMessageReplyParams& params);

  // Allocated buffer resources by |GetBuffer()|.
  typedef std::map<PP_Resource, scoped_refptr<AudioBufferResource> > BufferMap;
  BufferMap buffers_;

  PP_Resource* get_buffer_output_;

  scoped_refptr<TrackedCallback> configure_callback_;

  scoped_refptr<TrackedCallback> get_buffer_callback_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamAudioTrackResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_MEDIA_STREAM_AUDIO_TRACK_RESOURCE_H_

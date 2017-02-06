// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEDIA_CMA_MESSAGE_FILTER_PROXY_H_
#define CHROMECAST_RENDERER_MEDIA_CMA_MESSAGE_FILTER_PROXY_H_

#include <memory>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/sync_socket.h"
#include "chromecast/common/media/cma_ipc_common.h"
#include "chromecast/media/cma/pipeline/av_pipeline_client.h"
#include "chromecast/media/cma/pipeline/media_pipeline_client.h"
#include "chromecast/media/cma/pipeline/video_pipeline_client.h"
#include "ipc/message_filter.h"
#include "media/base/buffering_state.h"
#include "media/base/pipeline_status.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace chromecast {
namespace media {

typedef base::Callback<void(
    bool, base::SharedMemoryHandle, base::FileDescriptor)> AvPipeCB;

class CmaMessageFilterProxy : public IPC::MessageFilter {
 public:
  struct MediaDelegate {
    MediaDelegate();
    ~MediaDelegate();

    base::Closure flush_cb;
    MediaPipelineClient client;
  };

  struct AudioDelegate {
    AudioDelegate();
    ~AudioDelegate();

    AvPipeCB av_pipe_cb;
    base::Closure pipe_read_cb;
    ::media::PipelineStatusCB state_changed_cb;
    AvPipelineClient client;
  };

  struct VideoDelegate {
    VideoDelegate();
    ~VideoDelegate();

    AvPipeCB av_pipe_cb;
    base::Closure pipe_read_cb;
    ::media::PipelineStatusCB state_changed_cb;
    VideoPipelineClient client;
  };

  explicit CmaMessageFilterProxy(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);

  // Getter for the one CmaMessageFilterHost object.
  static CmaMessageFilterProxy* Get();

  int CreateChannel();
  void DestroyChannel(int id);

  // For adding/removing delegates.
  bool SetMediaDelegate(int id, const MediaDelegate& media_delegate);
  bool SetAudioDelegate(int id, const AudioDelegate& audio_delegate);
  bool SetVideoDelegate(int id, const VideoDelegate& video_delegate);

  // Sends an IPC message using |channel_|.
  bool Send(std::unique_ptr<IPC::Message> message);

  // IPC::ChannelProxy::MessageFilter implementation. Called on IO thread.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnFilterRemoved() override;
  void OnChannelClosing() override;

 protected:
  ~CmaMessageFilterProxy() override;

 private:
  struct DelegateEntry {
    DelegateEntry();
    ~DelegateEntry();

    MediaDelegate media_delegate;
    AudioDelegate audio_delegate;
    VideoDelegate video_delegate;
  };

  void OnAvPipeCreated(int id,
                       TrackId track_id,
                       bool status,
                       base::SharedMemoryHandle shared_memory_handle,
                       base::FileDescriptor socket_handle);
  void OnPipeRead(int id, TrackId track_id);
  void OnFlushDone(int id);
  void OnTrackStateChanged(int id, TrackId track_id,
                           ::media::PipelineStatus status);
  void OnWaitForKey(int id, TrackId track_id);
  void OnEos(int id, TrackId track_id);
  void OnTimeUpdate(int id,
                    base::TimeDelta time,
                    base::TimeDelta max_time,
                    base::TimeTicks stc);
  void OnBufferingNotification(int id, ::media::BufferingState state);
  void OnPlaybackError(int id, TrackId track_id,
                       ::media::PipelineStatus status);
  void OnStatisticsUpdated(int id, TrackId track_id,
                           const ::media::PipelineStatistics& stats);
  void OnNaturalSizeChanged(int id, TrackId track_id,
                            const gfx::Size& natural_size);

  // The singleton instance for this filter.
  static CmaMessageFilterProxy* filter_;

  // A map of media ids to delegates.
  IDMap<DelegateEntry> delegates_;

  IPC::Sender* sender_;

  scoped_refptr<base::SingleThreadTaskRunner> const io_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(CmaMessageFilterProxy);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEDIA_CMA_MESSAGE_FILTER_PROXY_H_

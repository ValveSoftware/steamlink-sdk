// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/cma_message_filter_host.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/memory/shared_memory.h"
#include "base/sync_socket.h"
#include "base/threading/thread_checker.h"
#include "chromecast/browser/media/media_pipeline_host.h"
#include "chromecast/common/media/cma_messages.h"
#include "chromecast/media/cdm/cast_cdm_proxy.h"
#include "chromecast/media/cma/pipeline/av_pipeline_client.h"
#include "chromecast/media/cma/pipeline/media_pipeline_client.h"
#include "chromecast/media/cma/pipeline/video_pipeline_client.h"
#include "chromecast/public/graphics_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "media/base/bind_to_current_loop.h"

namespace chromecast {
namespace media {

#define FORWARD_CALL(arg_pipeline, arg_fn, ...) \
  task_runner_->PostTask( \
      FROM_HERE, \
      base::Bind(&MediaPipelineHost::arg_fn, \
                 base::Unretained(arg_pipeline), __VA_ARGS__))

namespace {

const size_t kMaxSharedMem = 8 * 1024 * 1024;

// Map of MediaPipelineHost instances that is accessed only from the CMA thread.
// The existence of a MediaPipelineHost* in this map implies that the instance
// is still valid.
class MediaPipelineCmaMap {
 public:
  MediaPipelineCmaMap() { thread_checker_.DetachFromThread(); }

  MediaPipelineHost* GetMediaPipeline(int process_id, int media_id) {
    DCHECK(thread_checker_.CalledOnValidThread());
    uint64_t pipeline_id = GetPipelineId(process_id, media_id);
    auto it = id_pipeline_map_.find(pipeline_id);
    return it != id_pipeline_map_.end() ? it->second : nullptr;
  }

  void SetMediaPipeline(int process_id, int media_id, MediaPipelineHost* host) {
    DCHECK(thread_checker_.CalledOnValidThread());
    uint64_t pipeline_id = GetPipelineId(process_id, media_id);
    auto ret = id_pipeline_map_.insert(std::make_pair(pipeline_id, host));

    // Check there is no other entry with the same ID.
    DCHECK(ret.second != false);
  }

  void DestroyMediaPipeline(int process_id,
                            int media_id,
                            std::unique_ptr<MediaPipelineHost> media_pipeline) {
    DCHECK(thread_checker_.CalledOnValidThread());
    uint64_t pipeline_id = GetPipelineId(process_id, media_id);
    auto it = id_pipeline_map_.find(pipeline_id);
    if (it != id_pipeline_map_.end())
      id_pipeline_map_.erase(it);
  }

 private:
  uint64_t GetPipelineId(int process_id, int media_id) {
    return (static_cast<uint64_t>(process_id) << 32) +
           static_cast<uint64_t>(media_id);
  }

  base::ThreadChecker thread_checker_;
  std::map<uint64_t, MediaPipelineHost*> id_pipeline_map_;
};

base::LazyInstance<MediaPipelineCmaMap> g_pipeline_map =
    LAZY_INSTANCE_INITIALIZER;

void SetCdmOnCmaThread(int render_process_id, int media_id, CastCdm* cdm) {
  MediaPipelineHost* pipeline =
      g_pipeline_map.Get().GetMediaPipeline(render_process_id, media_id);
  if (!pipeline) {
    LOG(WARNING) << "MediaPipelineHost not alive: " << render_process_id << ","
                 << media_id;
    return;
  }

  pipeline->SetCdm(static_cast<CastCdmContext*>(cdm->GetCdmContext()));
}

// BrowserCdm instance must be retrieved/accessed on the UI thread, then
// passed to MediaPipelineHost on CMA thread.
void SetCdmOnUiThread(
    int render_process_id,
    int render_frame_id,
    int media_id,
    int cdm_id,
    scoped_refptr<base::SingleThreadTaskRunner> cma_task_runner) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!host) {
    LOG(ERROR) << "RenderProcessHost not alive for ID: " << render_process_id;
    return;
  }

  scoped_refptr<::media::MediaKeys> cdm = host->GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    LOG(WARNING) << "Could not find BrowserCdm (" << render_frame_id << ","
                 << cdm_id << ")";
    return;
  }

  CastCdm* cast_cdm = static_cast<CastCdmProxy*>(cdm.get())->cast_cdm();

  // base::Unretained is safe to use here, as all calls on |cast_cdm|,
  // including the destructor, will happen on the CMA thread.
  cma_task_runner->PostTask(
      FROM_HERE, base::Bind(&SetCdmOnCmaThread, render_process_id, media_id,
                            base::Unretained(cast_cdm)));
}

}  // namespace

CmaMessageFilterHost::CmaMessageFilterHost(
    int render_process_id,
    const CreateMediaPipelineBackendCB& create_backend_cb,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaResourceTracker* resource_tracker)
    : content::BrowserMessageFilter(CastMediaMsgStart),
      process_id_(render_process_id),
      create_backend_cb_(create_backend_cb),
      task_runner_(task_runner),
      resource_tracker_(resource_tracker),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

CmaMessageFilterHost::~CmaMessageFilterHost() {
  DCHECK(media_pipelines_.empty());
}

void CmaMessageFilterHost::OnChannelClosing() {
  content::BrowserMessageFilter::OnChannelClosing();
  DeleteEntries();
}

void CmaMessageFilterHost::OnDestruct() const {
  content::BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool CmaMessageFilterHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CmaMessageFilterHost, message)
    IPC_MESSAGE_HANDLER(CmaHostMsg_CreateMedia, CreateMedia)
    IPC_MESSAGE_HANDLER(CmaHostMsg_DestroyMedia, DestroyMedia)
    IPC_MESSAGE_HANDLER(CmaHostMsg_SetCdm, SetCdm)
    IPC_MESSAGE_HANDLER(CmaHostMsg_CreateAvPipe, CreateAvPipe)
    IPC_MESSAGE_HANDLER(CmaHostMsg_AudioInitialize, AudioInitialize)
    IPC_MESSAGE_HANDLER(CmaHostMsg_VideoInitialize, VideoInitialize)
    IPC_MESSAGE_HANDLER(CmaHostMsg_StartPlayingFrom, StartPlayingFrom)
    IPC_MESSAGE_HANDLER(CmaHostMsg_Flush, Flush)
    IPC_MESSAGE_HANDLER(CmaHostMsg_Stop, Stop)
    IPC_MESSAGE_HANDLER(CmaHostMsg_SetPlaybackRate, SetPlaybackRate)
    IPC_MESSAGE_HANDLER(CmaHostMsg_SetVolume, SetVolume)
    IPC_MESSAGE_HANDLER(CmaHostMsg_NotifyPipeWrite, NotifyPipeWrite)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void CmaMessageFilterHost::DeleteEntries() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  for (MediaPipelineMap::iterator it = media_pipelines_.begin();
       it != media_pipelines_.end(); ) {
    int media_id = it->first;
    std::unique_ptr<MediaPipelineHost> media_pipeline(it->second);
    media_pipelines_.erase(it++);
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MediaPipelineCmaMap::DestroyMediaPipeline,
                   base::Unretained(g_pipeline_map.Pointer()), process_id_,
                   media_id, base::Passed(&media_pipeline)));
  }
}

MediaPipelineHost* CmaMessageFilterHost::LookupById(int media_id) {
  MediaPipelineMap::iterator it = media_pipelines_.find(media_id);
  if (it == media_pipelines_.end())
    return NULL;
  return it->second;
}


// *** Handle incoming messages ***

void CmaMessageFilterHost::CreateMedia(int media_id, LoadType load_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::unique_ptr<MediaPipelineHost> media_pipeline_host(
      new MediaPipelineHost());
  MediaPipelineClient client;
  client.time_update_cb = ::media::BindToCurrentLoop(base::Bind(
      &CmaMessageFilterHost::OnTimeUpdate, weak_this_, media_id));
  client.buffering_state_cb = ::media::BindToCurrentLoop(base::Bind(
      &CmaMessageFilterHost::OnBufferingNotification, weak_this_, media_id));
  client.error_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnPlaybackError,
                 weak_this_, media_id, media::kNoTrackId));
  client.pipeline_backend_created_cb =
      base::Bind(&MediaResourceTracker::IncrementUsageCount,
                 base::Unretained(resource_tracker_));
  client.pipeline_backend_destroyed_cb =
      base::Bind(&MediaResourceTracker::DecrementUsageCount,
                 base::Unretained(resource_tracker_));

  task_runner_->PostTask(
      FROM_HERE, base::Bind(&MediaPipelineCmaMap::SetMediaPipeline,
                            base::Unretained(g_pipeline_map.Pointer()),
                            process_id_, media_id, media_pipeline_host.get()));
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&MediaPipelineHost::Initialize,
                                    base::Unretained(media_pipeline_host.get()),
                                    load_type, client, create_backend_cb_));
  std::pair<MediaPipelineMap::iterator, bool> ret =
    media_pipelines_.insert(
        std::make_pair(media_id, media_pipeline_host.release()));

  // Check there is no other entry with the same ID.
  DCHECK(ret.second != false);
}

void CmaMessageFilterHost::DestroyMedia(int media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  MediaPipelineMap::iterator it = media_pipelines_.find(media_id);
  if (it == media_pipelines_.end())
    return;

  std::unique_ptr<MediaPipelineHost> media_pipeline(it->second);
  media_pipelines_.erase(it);
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MediaPipelineCmaMap::DestroyMediaPipeline,
                 base::Unretained(g_pipeline_map.Pointer()), process_id_,
                 media_id, base::Passed(&media_pipeline)));
}

void CmaMessageFilterHost::SetCdm(int media_id,
                                  int render_frame_id,
                                  int cdm_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline)
    return;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&SetCdmOnUiThread, process_id_, render_frame_id, media_id,
                 cdm_id, task_runner_));
}

void CmaMessageFilterHost::CreateAvPipe(
    int media_id, TrackId track_id, size_t shared_mem_size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::FileDescriptor foreign_socket_handle;
  base::SharedMemoryHandle foreign_memory_handle;

  // A few sanity checks before allocating resources.
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline || !PeerHandle() || shared_mem_size > kMaxSharedMem) {
    Send(new CmaMsg_AvPipeCreated(
       media_id, track_id, false,
       foreign_memory_handle, foreign_socket_handle));
    return;
  }

  // Create the local/foreign sockets to signal media message
  // consune/feed events.
  // Use CancelableSyncSocket so that write is always non-blocking.
  std::unique_ptr<base::CancelableSyncSocket> local_socket(
      new base::CancelableSyncSocket());
  std::unique_ptr<base::CancelableSyncSocket> foreign_socket(
      new base::CancelableSyncSocket());
  if (!base::CancelableSyncSocket::CreatePair(local_socket.get(),
                                              foreign_socket.get()) ||
      foreign_socket->handle() == -1) {
    Send(new CmaMsg_AvPipeCreated(
       media_id, track_id, false,
       foreign_memory_handle, foreign_socket_handle));
    return;
  }

  // Shared memory used to convey media messages.
  std::unique_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());
  if (!shared_memory->CreateAndMapAnonymous(shared_mem_size) ||
      !shared_memory->ShareToProcess(PeerHandle(), &foreign_memory_handle)) {
    Send(new CmaMsg_AvPipeCreated(
       media_id, track_id, false,
       foreign_memory_handle, foreign_socket_handle));
    return;
  }

  // Note: the IPC message can be sent only once the pipe has been fully
  // configured. Part of this configuration is done in
  // |MediaPipelineHost::SetAvPipe|.
  // TODO(erickung): investigate possible memory leak here.
  // If the weak pointer in |av_pipe_set_cb| gets invalidated,
  // then |foreign_memory_handle| leaks.
  base::Closure pipe_read_activity_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnPipeReadActivity, weak_this_,
                 media_id, track_id));
  base::Closure av_pipe_set_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnAvPipeSet, weak_this_,
                 media_id, track_id,
                 foreign_memory_handle, base::Passed(&foreign_socket)));
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MediaPipelineHost::SetAvPipe,
                 base::Unretained(media_pipeline),
                 track_id,
                 base::Passed(&shared_memory),
                 pipe_read_activity_cb,
                 av_pipe_set_cb));
}

void CmaMessageFilterHost::OnAvPipeSet(
    int media_id,
    TrackId track_id,
    base::SharedMemoryHandle foreign_memory_handle,
    std::unique_ptr<base::CancelableSyncSocket> foreign_socket) {
  base::FileDescriptor foreign_socket_handle;
  foreign_socket_handle.fd = foreign_socket->handle();
  foreign_socket_handle.auto_close = false;

  // This message can only be set once the pipe has fully been configured
  // by |MediaPipelineHost|.
  Send(new CmaMsg_AvPipeCreated(
      media_id, track_id, true, foreign_memory_handle, foreign_socket_handle));
}

void CmaMessageFilterHost::AudioInitialize(
    int media_id, TrackId track_id, const ::media::AudioDecoderConfig& config) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline) {
    Send(new CmaMsg_TrackStateChanged(
        media_id, track_id, ::media::PIPELINE_ERROR_ABORT));
    return;
  }

  AvPipelineClient client;
  client.wait_for_key_cb = ::media::BindToCurrentLoop(base::Bind(
      &CmaMessageFilterHost::OnWaitForKey, weak_this_, media_id, track_id));
  client.eos_cb = ::media::BindToCurrentLoop(base::Bind(
      &CmaMessageFilterHost::OnEos, weak_this_, media_id, track_id));
  client.playback_error_cb = ::media::BindToCurrentLoop(base::Bind(
      &CmaMessageFilterHost::OnPlaybackError, weak_this_, media_id, track_id));
  client.statistics_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnStatisticsUpdated, weak_this_,
                 media_id, track_id));

  ::media::PipelineStatusCB pipeline_status_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnTrackStateChanged, weak_this_,
                 media_id, track_id));
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MediaPipelineHost::AudioInitialize,
                 base::Unretained(media_pipeline),
                 track_id, client, config, pipeline_status_cb));
}

void CmaMessageFilterHost::VideoInitialize(
    int media_id, TrackId track_id,
    const std::vector<::media::VideoDecoderConfig>& configs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline) {
    Send(new CmaMsg_TrackStateChanged(
        media_id, track_id, ::media::PIPELINE_ERROR_ABORT));
    return;
  }

  VideoPipelineClient client;
  client.av_pipeline_client.wait_for_key_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnWaitForKey, weak_this_,
                 media_id, track_id));
  client.av_pipeline_client.eos_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnEos, weak_this_,
                 media_id, track_id));
  client.av_pipeline_client.playback_error_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnPlaybackError, weak_this_,
                 media_id, track_id));
  client.av_pipeline_client.statistics_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnStatisticsUpdated, weak_this_,
                 media_id, track_id));
  client.natural_size_changed_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnNaturalSizeChanged, weak_this_,
                 media_id, track_id));

  ::media::PipelineStatusCB pipeline_status_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnTrackStateChanged, weak_this_,
                 media_id, track_id));
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MediaPipelineHost::VideoInitialize,
                 base::Unretained(media_pipeline),
                 track_id, client, configs, pipeline_status_cb));
}

void CmaMessageFilterHost::StartPlayingFrom(
    int media_id, base::TimeDelta time) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline)
    return;
  FORWARD_CALL(media_pipeline, StartPlayingFrom, time);
}

void CmaMessageFilterHost::Flush(int media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline) {
    Send(new CmaMsg_FlushDone(media_id));
    return;
  }
  base::Closure flush_cb = ::media::BindToCurrentLoop(
      base::Bind(&CmaMessageFilterHost::OnFlushDone, weak_this_, media_id));
  FORWARD_CALL(media_pipeline, Flush, flush_cb);
}

void CmaMessageFilterHost::Stop(int media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline)
    return;
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MediaPipelineHost::Stop,
                 base::Unretained(media_pipeline)));
}

void CmaMessageFilterHost::SetPlaybackRate(
    int media_id, double playback_rate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline)
    return;
  FORWARD_CALL(media_pipeline, SetPlaybackRate, playback_rate);
}

void CmaMessageFilterHost::SetVolume(
    int media_id, TrackId track_id, float volume) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline)
    return;
  FORWARD_CALL(media_pipeline, SetVolume, track_id, volume);
}

void CmaMessageFilterHost::NotifyPipeWrite(int media_id, TrackId track_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MediaPipelineHost* media_pipeline = LookupById(media_id);
  if (!media_pipeline)
    return;
  FORWARD_CALL(media_pipeline, NotifyPipeWrite, track_id);
}

// *** Browser to renderer messages ***

void CmaMessageFilterHost::OnFlushDone(int media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_FlushDone(media_id));
}

void CmaMessageFilterHost::OnTrackStateChanged(
    int media_id, TrackId track_id, ::media::PipelineStatus status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_TrackStateChanged(media_id, track_id, status));
}

void CmaMessageFilterHost::OnPipeReadActivity(int media_id, TrackId track_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_NotifyPipeRead(media_id, track_id));
}

void CmaMessageFilterHost::OnTimeUpdate(
    int media_id,
    base::TimeDelta media_time,
    base::TimeDelta max_media_time,
    base::TimeTicks stc) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_TimeUpdate(media_id,
                             media_time, max_media_time, stc));
}

void CmaMessageFilterHost::OnBufferingNotification(
    int media_id, ::media::BufferingState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_BufferingNotification(media_id, state));
}

void CmaMessageFilterHost::OnWaitForKey(int media_id, TrackId track_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_WaitForKey(media_id, track_id));
}

void CmaMessageFilterHost::OnEos(int media_id, TrackId track_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_Eos(media_id, track_id));
}

void CmaMessageFilterHost::OnPlaybackError(
    int media_id, TrackId track_id, ::media::PipelineStatus status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_PlaybackError(media_id, track_id, status));
}

void CmaMessageFilterHost::OnStatisticsUpdated(
    int media_id, TrackId track_id, const ::media::PipelineStatistics& stats) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_PlaybackStatistics(media_id, track_id, stats));
}

void CmaMessageFilterHost::OnNaturalSizeChanged(
    int media_id, TrackId track_id, const gfx::Size& size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Send(new CmaMsg_NaturalSizeChanged(media_id, track_id, size));
}

}  // namespace media
}  // namespace chromecast

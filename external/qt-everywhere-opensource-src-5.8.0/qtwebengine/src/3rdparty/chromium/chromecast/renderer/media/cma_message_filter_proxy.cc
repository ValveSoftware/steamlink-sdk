// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/media/cma_message_filter_proxy.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromecast/common/media/cma_messages.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_sender.h"

namespace chromecast {
namespace media {

CmaMessageFilterProxy::MediaDelegate::MediaDelegate() {
}

CmaMessageFilterProxy::MediaDelegate::~MediaDelegate() {
}

CmaMessageFilterProxy::AudioDelegate::AudioDelegate() {
}

CmaMessageFilterProxy::AudioDelegate::~AudioDelegate() {
}

CmaMessageFilterProxy::VideoDelegate::VideoDelegate() {
}

CmaMessageFilterProxy::VideoDelegate::~VideoDelegate() {
}

CmaMessageFilterProxy::DelegateEntry::DelegateEntry() {
}

CmaMessageFilterProxy::DelegateEntry::~DelegateEntry() {
}

CmaMessageFilterProxy* CmaMessageFilterProxy::filter_ = NULL;

// static
CmaMessageFilterProxy* CmaMessageFilterProxy::Get() {
  return filter_;
}

CmaMessageFilterProxy::CmaMessageFilterProxy(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : sender_(NULL), io_task_runner_(io_task_runner) {
  DCHECK(!filter_);
  filter_ = this;
}

int CmaMessageFilterProxy::CreateChannel() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DelegateEntry* entry = new DelegateEntry();
  int id = delegates_.Add(entry);
  return id;
}

void CmaMessageFilterProxy::DestroyChannel(int id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DelegateEntry* entry = delegates_.Lookup(id);
  if (!entry)
    return;
  delegates_.Remove(id);
  delete entry;
}

bool CmaMessageFilterProxy::SetMediaDelegate(
    int id, const MediaDelegate& media_delegate) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DelegateEntry* entry = delegates_.Lookup(id);
  if (!entry)
    return false;
  entry->media_delegate = media_delegate;
  return true;
}

bool CmaMessageFilterProxy::SetAudioDelegate(
    int id, const AudioDelegate& audio_delegate) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DelegateEntry* entry = delegates_.Lookup(id);
  if (!entry)
    return false;
  entry->audio_delegate = audio_delegate;
  return true;
}

bool CmaMessageFilterProxy::SetVideoDelegate(
    int id, const VideoDelegate& video_delegate) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DelegateEntry* entry = delegates_.Lookup(id);
  if (!entry)
    return false;
  entry->video_delegate = video_delegate;
  return true;
}

bool CmaMessageFilterProxy::Send(std::unique_ptr<IPC::Message> message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!sender_)
    return false;
  bool status = sender_->Send(message.release());
  return status;
}

bool CmaMessageFilterProxy::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CmaMessageFilterProxy, message)
    IPC_MESSAGE_HANDLER(CmaMsg_AvPipeCreated, OnAvPipeCreated)
    IPC_MESSAGE_HANDLER(CmaMsg_NotifyPipeRead, OnPipeRead)
    IPC_MESSAGE_HANDLER(CmaMsg_FlushDone, OnFlushDone)
    IPC_MESSAGE_HANDLER(CmaMsg_TrackStateChanged, OnTrackStateChanged)
    IPC_MESSAGE_HANDLER(CmaMsg_WaitForKey, OnWaitForKey)
    IPC_MESSAGE_HANDLER(CmaMsg_Eos, OnEos)
    IPC_MESSAGE_HANDLER(CmaMsg_TimeUpdate, OnTimeUpdate)
    IPC_MESSAGE_HANDLER(CmaMsg_BufferingNotification, OnBufferingNotification)
    IPC_MESSAGE_HANDLER(CmaMsg_PlaybackError, OnPlaybackError)
    IPC_MESSAGE_HANDLER(CmaMsg_PlaybackStatistics, OnStatisticsUpdated)
    IPC_MESSAGE_HANDLER(CmaMsg_NaturalSizeChanged, OnNaturalSizeChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CmaMessageFilterProxy::OnFilterAdded(IPC::Sender* sender) {
  sender_ = sender;
}

void CmaMessageFilterProxy::OnFilterRemoved() {
  sender_ = NULL;
}

void CmaMessageFilterProxy::OnChannelClosing() {
  sender_ = NULL;
}

CmaMessageFilterProxy::~CmaMessageFilterProxy() {
  DCHECK(filter_);
  filter_ = NULL;
}

void CmaMessageFilterProxy::OnAvPipeCreated(
    int id,
    TrackId track_id,
    bool status,
    base::SharedMemoryHandle shared_memory_handle,
    base::FileDescriptor socket_handle) {
  DelegateEntry* entry = delegates_.Lookup(id);
  if (!entry)
    return;
  const AvPipeCB& cb = (track_id == kAudioTrackId) ?
      entry->audio_delegate.av_pipe_cb :
      entry->video_delegate.av_pipe_cb;
  if (!cb.is_null())
    cb.Run(status, shared_memory_handle, socket_handle);
}

void CmaMessageFilterProxy::OnPipeRead(
    int id, TrackId track_id) {
  DelegateEntry* entry = delegates_.Lookup(id);
  if (!entry)
    return;
  const base::Closure& cb = (track_id == kAudioTrackId) ?
      entry->audio_delegate.pipe_read_cb :
      entry->video_delegate.pipe_read_cb;
  if (!cb.is_null())
    cb.Run();
}

void CmaMessageFilterProxy::OnFlushDone(int id) {
  DelegateEntry* entry = delegates_.Lookup(id);
  if (!entry)
    return;
  const base::Closure& cb = entry->media_delegate.flush_cb;
  if (!cb.is_null())
    cb.Run();
}

void CmaMessageFilterProxy::OnTrackStateChanged(
    int id, TrackId track_id, ::media::PipelineStatus status) {
  DelegateEntry* entry = delegates_.Lookup(id);
  if (!entry)
    return;
  const ::media::PipelineStatusCB& cb = (track_id == kAudioTrackId) ?
      entry->audio_delegate.state_changed_cb :
      entry->video_delegate.state_changed_cb;
  if (!cb.is_null())
    cb.Run(status);
}

void CmaMessageFilterProxy::OnWaitForKey(int id, TrackId track_id) {
  DelegateEntry* entry = delegates_.Lookup(id);
  if (!entry)
    return;
  const base::Closure& cb = (track_id == kAudioTrackId) ?
      entry->audio_delegate.client.wait_for_key_cb :
      entry->video_delegate.client.av_pipeline_client.wait_for_key_cb;
  if (!cb.is_null())
    cb.Run();
}

void CmaMessageFilterProxy::OnEos(int id, TrackId track_id) {
  DelegateEntry* entry = delegates_.Lookup(id);
  if (!entry)
    return;
  const base::Closure& cb = (track_id == kAudioTrackId) ?
      entry->audio_delegate.client.eos_cb :
      entry->video_delegate.client.av_pipeline_client.eos_cb;
  if (!cb.is_null())
    cb.Run();
}

void CmaMessageFilterProxy::OnTimeUpdate(
    int id,
    base::TimeDelta time,
    base::TimeDelta max_time,
    base::TimeTicks stc) {
  DelegateEntry* entry = delegates_.Lookup(id);
  if (!entry)
    return;
  const MediaPipelineClient::TimeUpdateCB& cb =
      entry->media_delegate.client.time_update_cb;
  if (!cb.is_null())
    cb.Run(time, max_time, stc);
}

void CmaMessageFilterProxy::OnBufferingNotification(
    int id, ::media::BufferingState buffering_state) {
  DelegateEntry* entry = delegates_.Lookup(id);
  if (!entry)
    return;
  const ::media::BufferingStateCB& cb =
      entry->media_delegate.client.buffering_state_cb;
  if (!cb.is_null())
    cb.Run(buffering_state);
}

void CmaMessageFilterProxy::OnPlaybackError(
    int id, TrackId track_id, ::media::PipelineStatus status) {
  DelegateEntry* entry = delegates_.Lookup(id);
  if (!entry)
    return;
  const ::media::PipelineStatusCB& cb =
      (track_id == kNoTrackId) ? entry->media_delegate.client.error_cb :
      (track_id == kAudioTrackId) ?
          entry->audio_delegate.client.playback_error_cb :
          entry->video_delegate.client.av_pipeline_client.playback_error_cb;
  if (!cb.is_null())
    cb.Run(status);
}

void CmaMessageFilterProxy::OnStatisticsUpdated(
    int id, TrackId track_id, const ::media::PipelineStatistics& stats) {
  DelegateEntry* entry = delegates_.Lookup(id);
  if (!entry)
    return;
  const ::media::StatisticsCB& cb = (track_id == kAudioTrackId) ?
      entry->audio_delegate.client.statistics_cb :
      entry->video_delegate.client.av_pipeline_client.statistics_cb;
  if (!cb.is_null())
    cb.Run(stats);
}

void CmaMessageFilterProxy::OnNaturalSizeChanged(
    int id, TrackId track_id, const gfx::Size& size) {
  DelegateEntry* entry = delegates_.Lookup(id);
  if (!entry)
    return;
  if (track_id == kAudioTrackId)
    return;
  const VideoPipelineClient::NaturalSizeChangedCB& cb =
      entry->video_delegate.client.natural_size_changed_cb;
  if (!cb.is_null())
    cb.Run(size);
}

}  // namespace media
}  // namespace chromecast

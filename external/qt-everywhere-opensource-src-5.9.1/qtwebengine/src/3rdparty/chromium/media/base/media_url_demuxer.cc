// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_url_demuxer.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"

namespace media {

MediaUrlDemuxer::MediaUrlDemuxer(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const GURL& media_url,
    const GURL& first_party_for_cookies)
    : params_{media_url, first_party_for_cookies}, task_runner_(task_runner) {}

MediaUrlDemuxer::~MediaUrlDemuxer() {}

// Should never be called since DemuxerStreamProvider::Type is URL.
DemuxerStream* MediaUrlDemuxer::GetStream(DemuxerStream::Type type) {
  NOTREACHED();
  return nullptr;
}

MediaUrlParams MediaUrlDemuxer::GetMediaUrlParams() const {
  return params_;
}

DemuxerStreamProvider::Type MediaUrlDemuxer::GetType() const {
  return DemuxerStreamProvider::Type::URL;
}

std::string MediaUrlDemuxer::GetDisplayName() const {
  return "MediaUrlDemuxer";
}

void MediaUrlDemuxer::Initialize(DemuxerHost* host,
                                 const PipelineStatusCB& status_cb,
                                 bool enable_text_tracks) {
  DVLOG(1) << __FUNCTION__;
  task_runner_->PostTask(FROM_HERE, base::Bind(status_cb, PIPELINE_OK));
}

void MediaUrlDemuxer::StartWaitingForSeek(base::TimeDelta seek_time) {}

void MediaUrlDemuxer::CancelPendingSeek(base::TimeDelta seek_time) {}

void MediaUrlDemuxer::Seek(base::TimeDelta time,
                           const PipelineStatusCB& status_cb) {
  task_runner_->PostTask(FROM_HERE, base::Bind(status_cb, PIPELINE_OK));
}

void MediaUrlDemuxer::Stop() {}

void MediaUrlDemuxer::AbortPendingReads() {}

base::TimeDelta MediaUrlDemuxer::GetStartTime() const {
  // TODO(tguilbert): Investigate if we need to fetch information from the
  // MediaPlayerRender in order to return a sensible value here.
  return base::TimeDelta();
}
base::Time MediaUrlDemuxer::GetTimelineOffset() const {
  return base::Time();
}

int64_t MediaUrlDemuxer::GetMemoryUsage() const {
  return 0;
}

void MediaUrlDemuxer::OnEnabledAudioTracksChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta currTime) {
  NOTIMPLEMENTED();
}
void MediaUrlDemuxer::OnSelectedVideoTrackChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta currTime) {
  NOTIMPLEMENTED();
}

}  // namespace media

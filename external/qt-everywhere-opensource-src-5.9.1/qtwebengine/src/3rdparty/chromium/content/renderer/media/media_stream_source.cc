// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_source.h"

#include "base/callback_helpers.h"

namespace content {

const char MediaStreamSource::kSourceId[] = "sourceId";

MediaStreamSource::MediaStreamSource() {
}

MediaStreamSource::~MediaStreamSource() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(stop_callback_.is_null());
}

void MediaStreamSource::StopSource() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DoStopSource();
  if (!stop_callback_.is_null())
    base::ResetAndReturn(&stop_callback_).Run(owner());
  owner().setReadyState(blink::WebMediaStreamSource::ReadyStateEnded);
}

void MediaStreamSource::SetDeviceInfo(const StreamDeviceInfo& device_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  device_info_ = device_info;
}

void MediaStreamSource::SetStopCallback(
    const SourceStoppedCallback& stop_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(stop_callback_.is_null());
  stop_callback_ = stop_callback;
}

void MediaStreamSource::ResetSourceStoppedCallback() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!stop_callback_.is_null());
  stop_callback_.Reset();
}

}  // namespace content

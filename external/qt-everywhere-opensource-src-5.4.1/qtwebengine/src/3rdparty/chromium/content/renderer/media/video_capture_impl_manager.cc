// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation notes about interactions with VideoCaptureImpl.
//
// How is VideoCaptureImpl used:
//
// VideoCaptureImpl is an IO thread object while VideoCaptureImplManager
// lives only on the render thread. It is only possible to access an
// object of VideoCaptureImpl via a task on the IO thread.
//
// How is VideoCaptureImpl deleted:
//
// A task is posted to the IO thread to delete a VideoCaptureImpl.
// Immediately after that the pointer to it is dropped. This means no
// access to this VideoCaptureImpl object is possible on the render
// thread. Also note that VideoCaptureImpl does not post task to itself.
//
// The use of Unretained:
//
// We make sure deletion is the last task on the IO thread for a
// VideoCaptureImpl object. This allows the use of Unretained() binding.

#include "content/renderer/media/video_capture_impl_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/child/child_process.h"
#include "content/renderer/media/video_capture_impl.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "media/base/bind_to_current_loop.h"

namespace content {

VideoCaptureImplManager::VideoCaptureImplManager()
    : next_client_id_(0),
      filter_(new VideoCaptureMessageFilter()),
      weak_factory_(this) {
}

VideoCaptureImplManager::~VideoCaptureImplManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (devices_.empty())
    return;
  // Forcibly release all video capture resources.
  for (VideoCaptureDeviceMap::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    VideoCaptureImpl* impl = it->second.second;
    ChildProcess::current()->io_message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&VideoCaptureImpl::DeInit,
                   base::Unretained(impl)));
    ChildProcess::current()->io_message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&base::DeletePointer<VideoCaptureImpl>,
                   base::Unretained(impl)));
  }
  devices_.clear();
}

base::Closure VideoCaptureImplManager::UseDevice(
    media::VideoCaptureSessionId id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VideoCaptureImpl* impl = NULL;
  VideoCaptureDeviceMap::iterator it = devices_.find(id);
  if (it == devices_.end()) {
    impl = CreateVideoCaptureImplForTesting(id, filter_.get());
    if (!impl)
      impl = new VideoCaptureImpl(id, filter_.get());
    devices_[id] = std::make_pair(1, impl);
    ChildProcess::current()->io_message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&VideoCaptureImpl::Init,
                   base::Unretained(impl)));
  } else {
    ++it->second.first;
  }
  return base::Bind(&VideoCaptureImplManager::UnrefDevice,
                    weak_factory_.GetWeakPtr(), id);
}

base::Closure VideoCaptureImplManager::StartCapture(
    media::VideoCaptureSessionId id,
    const media::VideoCaptureParams& params,
    const VideoCaptureStateUpdateCB& state_update_cb,
    const VideoCaptureDeliverFrameCB& deliver_frame_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VideoCaptureDeviceMap::iterator it = devices_.find(id);
  DCHECK(it != devices_.end());
  VideoCaptureImpl* impl = it->second.second;

  // This ID is used to identify a client of VideoCaptureImpl.
  const int client_id = ++next_client_id_;

  ChildProcess::current()->io_message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureImpl::StartCapture,
                 base::Unretained(impl),
                 client_id,
                 params,
                 state_update_cb,
                 deliver_frame_cb));
  return base::Bind(&VideoCaptureImplManager::StopCapture,
                    weak_factory_.GetWeakPtr(),
                    client_id, id);
}

void VideoCaptureImplManager::GetDeviceSupportedFormats(
    media::VideoCaptureSessionId id,
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VideoCaptureDeviceMap::iterator it = devices_.find(id);
  DCHECK(it != devices_.end());
  VideoCaptureImpl* impl = it->second.second;
  ChildProcess::current()->io_message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureImpl::GetDeviceSupportedFormats,
                 base::Unretained(impl), callback));
}

void VideoCaptureImplManager::GetDeviceFormatsInUse(
    media::VideoCaptureSessionId id,
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VideoCaptureDeviceMap::iterator it = devices_.find(id);
  DCHECK(it != devices_.end());
  VideoCaptureImpl* impl = it->second.second;
  ChildProcess::current()->io_message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureImpl::GetDeviceFormatsInUse,
                 base::Unretained(impl), callback));
}

VideoCaptureImpl*
VideoCaptureImplManager::CreateVideoCaptureImplForTesting(
    media::VideoCaptureSessionId id,
    VideoCaptureMessageFilter* filter) const {
  return NULL;
}

void VideoCaptureImplManager::StopCapture(
    int client_id, media::VideoCaptureSessionId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VideoCaptureDeviceMap::iterator it = devices_.find(id);
  DCHECK(it != devices_.end());
  VideoCaptureImpl* impl = it->second.second;
  ChildProcess::current()->io_message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureImpl::StopCapture,
                 base::Unretained(impl), client_id));
}

void VideoCaptureImplManager::UnrefDevice(
    media::VideoCaptureSessionId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VideoCaptureDeviceMap::iterator it = devices_.find(id);
  DCHECK(it != devices_.end());
  VideoCaptureImpl* impl = it->second.second;

  // Unref and destroy on the IO thread if there's no more client.
  DCHECK(it->second.first);
  --it->second.first;
  if (!it->second.first) {
    devices_.erase(id);
    ChildProcess::current()->io_message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&VideoCaptureImpl::DeInit,
                   base::Unretained(impl)));
    ChildProcess::current()->io_message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&base::DeletePointer<VideoCaptureImpl>,
                   base::Unretained(impl)));
  }
}

void VideoCaptureImplManager::SuspendDevices(bool suspend) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (VideoCaptureDeviceMap::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    VideoCaptureImpl* impl = it->second.second;
    ChildProcess::current()->io_message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&VideoCaptureImpl::SuspendCapture,
                   base::Unretained(impl), suspend));
  }
}

}  // namespace content

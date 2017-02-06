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
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/child/child_process.h"
#include "content/renderer/media/video_capture_impl.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "media/base/bind_to_current_loop.h"

namespace content {

VideoCaptureImplManager::VideoCaptureImplManager()
    : next_client_id_(0),
      filter_(new VideoCaptureMessageFilter()),
      render_main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {}

VideoCaptureImplManager::~VideoCaptureImplManager() {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  if (devices_.empty())
    return;
  // Forcibly release all video capture resources.
  for (const auto& device : devices_) {
    VideoCaptureImpl* const impl = device.second.second;
    ChildProcess::current()->io_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&VideoCaptureImpl::DeInit, base::Unretained(impl)));
    ChildProcess::current()->io_task_runner()->DeleteSoon(FROM_HERE, impl);
  }
  devices_.clear();
}

base::Closure VideoCaptureImplManager::UseDevice(
    media::VideoCaptureSessionId id) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  VideoCaptureImpl* impl = NULL;
  const VideoCaptureDeviceMap::iterator it = devices_.find(id);
  if (it == devices_.end()) {
    impl = CreateVideoCaptureImplForTesting(id, filter_.get());
    if (!impl)
      impl = new VideoCaptureImpl(id, filter_.get());
    devices_[id] = std::make_pair(1, impl);
    ChildProcess::current()->io_task_runner()->PostTask(
        FROM_HERE, base::Bind(&VideoCaptureImpl::Init, base::Unretained(impl)));
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
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const VideoCaptureDeviceMap::const_iterator it = devices_.find(id);
  DCHECK(it != devices_.end());
  VideoCaptureImpl* const impl = it->second.second;

  // This ID is used to identify a client of VideoCaptureImpl.
  const int client_id = ++next_client_id_;

  ChildProcess::current()->io_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureImpl::StartCapture, base::Unretained(impl),
                 client_id, params, state_update_cb, deliver_frame_cb));
  return base::Bind(&VideoCaptureImplManager::StopCapture,
                    weak_factory_.GetWeakPtr(), client_id, id);
}

void VideoCaptureImplManager::RequestRefreshFrame(
    media::VideoCaptureSessionId id) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const VideoCaptureDeviceMap::const_iterator it = devices_.find(id);
  DCHECK(it != devices_.end());
  VideoCaptureImpl* const impl = it->second.second;
  ChildProcess::current()->io_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureImpl::RequestRefreshFrame,
                 base::Unretained(impl)));
}

void VideoCaptureImplManager::GetDeviceSupportedFormats(
    media::VideoCaptureSessionId id,
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const VideoCaptureDeviceMap::const_iterator it = devices_.find(id);
  DCHECK(it != devices_.end());
  VideoCaptureImpl* const impl = it->second.second;
  ChildProcess::current()->io_task_runner()->PostTask(
      FROM_HERE, base::Bind(&VideoCaptureImpl::GetDeviceSupportedFormats,
                            base::Unretained(impl), callback));
}

void VideoCaptureImplManager::GetDeviceFormatsInUse(
    media::VideoCaptureSessionId id,
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const VideoCaptureDeviceMap::const_iterator it = devices_.find(id);
  DCHECK(it != devices_.end());
  VideoCaptureImpl* const impl = it->second.second;
  ChildProcess::current()->io_task_runner()->PostTask(
      FROM_HERE, base::Bind(&VideoCaptureImpl::GetDeviceFormatsInUse,
                            base::Unretained(impl), callback));
}

VideoCaptureImpl*
VideoCaptureImplManager::CreateVideoCaptureImplForTesting(
    media::VideoCaptureSessionId id,
    VideoCaptureMessageFilter* filter) const {
  return NULL;
}

void VideoCaptureImplManager::StopCapture(int client_id,
                                          media::VideoCaptureSessionId id) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const VideoCaptureDeviceMap::const_iterator it = devices_.find(id);
  DCHECK(it != devices_.end());
  VideoCaptureImpl* const impl = it->second.second;
  ChildProcess::current()->io_task_runner()->PostTask(
      FROM_HERE, base::Bind(&VideoCaptureImpl::StopCapture,
                            base::Unretained(impl), client_id));
}

void VideoCaptureImplManager::UnrefDevice(
    media::VideoCaptureSessionId id) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const VideoCaptureDeviceMap::iterator it = devices_.find(id);
  DCHECK(it != devices_.end());
  VideoCaptureImpl* const impl = it->second.second;

  // Unref and destroy on the IO thread if there's no more client.
  DCHECK(it->second.first);
  --it->second.first;
  if (!it->second.first) {
    devices_.erase(id);
    ChildProcess::current()->io_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&VideoCaptureImpl::DeInit, base::Unretained(impl)));
    ChildProcess::current()->io_task_runner()->DeleteSoon(FROM_HERE, impl);
  }
}

void VideoCaptureImplManager::SuspendDevices(bool suspend) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  for (const auto& device : devices_) {
    VideoCaptureImpl* const impl = device.second.second;
    ChildProcess::current()->io_task_runner()->PostTask(
        FROM_HERE, base::Bind(&VideoCaptureImpl::SuspendCapture,
                              base::Unretained(impl), suspend));
  }
}

}  // namespace content

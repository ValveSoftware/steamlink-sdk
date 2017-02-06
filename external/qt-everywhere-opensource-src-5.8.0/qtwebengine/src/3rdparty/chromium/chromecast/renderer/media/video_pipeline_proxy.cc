// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/media/video_pipeline_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_checker.h"
#include "chromecast/common/media/cma_ipc_common.h"
#include "chromecast/common/media/cma_messages.h"
#include "chromecast/common/media/shared_memory_chunk.h"
#include "chromecast/media/cma/base/buffering_defs.h"
#include "chromecast/media/cma/base/cma_logging.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "chromecast/media/cma/ipc/media_message_fifo.h"
#include "chromecast/media/cma/ipc_streamer/av_streamer_proxy.h"
#include "chromecast/media/cma/pipeline/video_pipeline_client.h"
#include "chromecast/renderer/media/cma_message_filter_proxy.h"
#include "chromecast/renderer/media/media_channel_proxy.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/pipeline_status.h"

namespace chromecast {
namespace media {

namespace {

void IgnoreResult() {
}

}  // namespace

// VideoPipelineProxyInternal -
// This class is not thread safe and should run on the same thread
// as the media channel proxy.
class VideoPipelineProxyInternal {
 public:
  typedef base::Callback<void(std::unique_ptr<base::SharedMemory>)> SharedMemCB;

  static void Release(std::unique_ptr<VideoPipelineProxyInternal> proxy);

  explicit VideoPipelineProxyInternal(
      scoped_refptr<MediaChannelProxy> media_channel_proxy);
  virtual ~VideoPipelineProxyInternal();

  // Notify the other side (browser process) of some activity on the video pipe.
  // TODO(erickung): either send an IPC message or write a byte on the
  // SyncSocket.
  void NotifyPipeWrite();

  // These functions are almost a one to one correspondence with VideoPipeline
  // but this is an internal class and there is no reason to derive from
  // VideoPipeline.
  void SetClient(const base::Closure& pipe_read_cb,
                 const VideoPipelineClient& client);
  void CreateAvPipe(const SharedMemCB& shared_mem_cb);
  void Initialize(const std::vector<::media::VideoDecoderConfig>& configs,
                  const ::media::PipelineStatusCB& status_cb);

 private:
  void Shutdown();

  // Callbacks for CmaMessageFilterHost::VideoDelegate.
  void OnAvPipeCreated(bool status,
                       base::SharedMemoryHandle shared_mem_handle,
                       base::FileDescriptor socket);
  void OnStateChanged(::media::PipelineStatus status);

  base::ThreadChecker thread_checker_;

  scoped_refptr<MediaChannelProxy> media_channel_proxy_;

  // Store the callback for a pending state transition.
  ::media::PipelineStatusCB status_cb_;

  SharedMemCB shared_mem_cb_;

  DISALLOW_COPY_AND_ASSIGN(VideoPipelineProxyInternal);
};

// static
void VideoPipelineProxyInternal::Release(
    std::unique_ptr<VideoPipelineProxyInternal> proxy) {
  proxy->Shutdown();
}

VideoPipelineProxyInternal::VideoPipelineProxyInternal(
    scoped_refptr<MediaChannelProxy> media_channel_proxy)
    : media_channel_proxy_(media_channel_proxy) {
  DCHECK(media_channel_proxy.get());

  // Creation can be done on a different thread.
  thread_checker_.DetachFromThread();
}

VideoPipelineProxyInternal::~VideoPipelineProxyInternal() {
}

void VideoPipelineProxyInternal::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Remove any callback on VideoPipelineProxyInternal.
  media_channel_proxy_->SetVideoDelegate(
      CmaMessageFilterProxy::VideoDelegate());
}

void VideoPipelineProxyInternal::NotifyPipeWrite() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(damienv): An alternative way would be to use a dedicated socket for
  // this event.
  bool success = media_channel_proxy_->Send(
      std::unique_ptr<IPC::Message>(new CmaHostMsg_NotifyPipeWrite(
          media_channel_proxy_->GetId(), kVideoTrackId)));
  VLOG_IF(4, !success) << "Sending msg failed";
}

void VideoPipelineProxyInternal::SetClient(
    const base::Closure& pipe_read_cb,
    const VideoPipelineClient& video_client) {
  DCHECK(thread_checker_.CalledOnValidThread());

  CmaMessageFilterProxy::VideoDelegate delegate;
  delegate.av_pipe_cb =
      base::Bind(&VideoPipelineProxyInternal::OnAvPipeCreated,
                 base::Unretained(this));
  delegate.state_changed_cb =
      base::Bind(&VideoPipelineProxyInternal::OnStateChanged,
                 base::Unretained(this));
  delegate.pipe_read_cb = pipe_read_cb;
  delegate.client = video_client;
  bool success = media_channel_proxy_->SetVideoDelegate(delegate);
  CHECK(success);
}

void VideoPipelineProxyInternal::CreateAvPipe(
    const SharedMemCB& shared_mem_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(shared_mem_cb_.is_null());
  bool success = media_channel_proxy_->Send(
      std::unique_ptr<IPC::Message>(new CmaHostMsg_CreateAvPipe(
          media_channel_proxy_->GetId(), kVideoTrackId, kAppVideoBufferSize)));
  if (!success) {
    shared_mem_cb.Run(std::unique_ptr<base::SharedMemory>());
    return;
  }
  shared_mem_cb_ = shared_mem_cb;
}

void VideoPipelineProxyInternal::OnAvPipeCreated(
    bool success,
    base::SharedMemoryHandle shared_mem_handle,
    base::FileDescriptor socket) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!shared_mem_cb_.is_null());
  if (!success) {
    shared_mem_cb_.Run(std::unique_ptr<base::SharedMemory>());
    return;
  }

  CHECK(base::SharedMemory::IsHandleValid(shared_mem_handle));
  shared_mem_cb_.Run(std::unique_ptr<base::SharedMemory>(
      new base::SharedMemory(shared_mem_handle, false)));
}

void VideoPipelineProxyInternal::Initialize(
    const std::vector<::media::VideoDecoderConfig>& configs,
    const ::media::PipelineStatusCB& status_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool success = media_channel_proxy_->Send(
      std::unique_ptr<IPC::Message>(new CmaHostMsg_VideoInitialize(
          media_channel_proxy_->GetId(), kVideoTrackId, configs)));
  if (!success) {
    status_cb.Run( ::media::PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }
  DCHECK(status_cb_.is_null());
  status_cb_ = status_cb;
}

void VideoPipelineProxyInternal::OnStateChanged(
    ::media::PipelineStatus status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!status_cb_.is_null());
  base::ResetAndReturn(&status_cb_).Run(status);
}

// A macro runs current member function on |io_task_runner_| thread.
#define FORWARD_ON_IO_THREAD(param_fn, ...)                        \
  io_task_runner_->PostTask(                                       \
      FROM_HERE, base::Bind(&VideoPipelineProxyInternal::param_fn, \
                            base::Unretained(proxy_.get()), ##__VA_ARGS__))

VideoPipelineProxy::VideoPipelineProxy(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<MediaChannelProxy> media_channel_proxy)
    : io_task_runner_(io_task_runner),
      proxy_(new VideoPipelineProxyInternal(media_channel_proxy)),
      video_streamer_(new AvStreamerProxy()),
      weak_factory_(this) {
  DCHECK(io_task_runner_.get());
  weak_this_ = weak_factory_.GetWeakPtr();
  thread_checker_.DetachFromThread();
}

VideoPipelineProxy::~VideoPipelineProxy() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Release the underlying object on the right thread.
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoPipelineProxyInternal::Release, base::Passed(&proxy_)));
}

void VideoPipelineProxy::SetClient(const VideoPipelineClient& video_client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::Closure pipe_read_cb =
      ::media::BindToCurrentLoop(
          base::Bind(&VideoPipelineProxy::OnPipeRead, weak_this_));
  FORWARD_ON_IO_THREAD(SetClient, pipe_read_cb, video_client);
}

void VideoPipelineProxy::Initialize(
    const std::vector<::media::VideoDecoderConfig>& configs,
    std::unique_ptr<CodedFrameProvider> frame_provider,
    const ::media::PipelineStatusCB& status_cb) {
  CMALOG(kLogControl) << "VideoPipelineProxy::Initialize";
  DCHECK(thread_checker_.CalledOnValidThread());
  video_streamer_->SetCodedFrameProvider(std::move(frame_provider));

  VideoPipelineProxyInternal::SharedMemCB shared_mem_cb =
      ::media::BindToCurrentLoop(base::Bind(
          &VideoPipelineProxy::OnAvPipeCreated, weak_this_,
          configs, status_cb));
  FORWARD_ON_IO_THREAD(CreateAvPipe, shared_mem_cb);
}

void VideoPipelineProxy::OnAvPipeCreated(
    const std::vector<::media::VideoDecoderConfig>& configs,
    const ::media::PipelineStatusCB& status_cb,
    std::unique_ptr<base::SharedMemory> shared_memory) {
  CMALOG(kLogControl) << "VideoPipelineProxy::OnAvPipeCreated";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!shared_memory ||
      !shared_memory->Map(kAppVideoBufferSize)) {
    status_cb.Run(::media::PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }
  CHECK(shared_memory->memory());

  std::unique_ptr<MediaMemoryChunk> shared_memory_chunk(
      new SharedMemoryChunk(std::move(shared_memory), kAppVideoBufferSize));
  std::unique_ptr<MediaMessageFifo> video_pipe(
      new MediaMessageFifo(std::move(shared_memory_chunk), false));
  video_pipe->ObserveWriteActivity(
      base::Bind(&VideoPipelineProxy::OnPipeWrite, weak_this_));

  video_streamer_->SetMediaMessageFifo(std::move(video_pipe));

  // Now proceed to the decoder/renderer initialization.
  FORWARD_ON_IO_THREAD(Initialize, configs, status_cb);
}

void VideoPipelineProxy::StartFeeding() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(video_streamer_);
  video_streamer_->Start();
}

void VideoPipelineProxy::Flush(const base::Closure& done_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(video_streamer_);
  video_streamer_->StopAndFlush(done_cb);
}

void VideoPipelineProxy::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!video_streamer_)
    return;
  video_streamer_->StopAndFlush(base::Bind(&IgnoreResult));
}

void VideoPipelineProxy::OnPipeWrite() {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_IO_THREAD(NotifyPipeWrite);
}

void VideoPipelineProxy::OnPipeRead() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (video_streamer_)
    video_streamer_->OnFifoReadEvent();
}

}  // namespace media
}  // namespace chromecast

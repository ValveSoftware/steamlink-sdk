// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_input_message_filter.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/stringprintf.h"
#include "content/common/media/audio_messages.h"
#include "content/renderer/media/webrtc_logging.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_sender.h"

namespace content {

namespace {
const int kStreamIDNotSet = -1;
}

class AudioInputMessageFilter::AudioInputIPCImpl
    : public NON_EXPORTED_BASE(media::AudioInputIPC) {
 public:
  AudioInputIPCImpl(const scoped_refptr<AudioInputMessageFilter>& filter,
                    int render_view_id);
  virtual ~AudioInputIPCImpl();

  // media::AudioInputIPC implementation.
  virtual void CreateStream(media::AudioInputIPCDelegate* delegate,
                            int session_id,
                            const media::AudioParameters& params,
                            bool automatic_gain_control,
                            uint32 total_segments) OVERRIDE;
  virtual void RecordStream() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual void CloseStream() OVERRIDE;

 private:
  const scoped_refptr<AudioInputMessageFilter> filter_;
  const int render_view_id_;
  int stream_id_;
};

AudioInputMessageFilter* AudioInputMessageFilter::g_filter = NULL;

AudioInputMessageFilter::AudioInputMessageFilter(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop)
    : sender_(NULL),
      io_message_loop_(io_message_loop) {
  DCHECK(!g_filter);
  g_filter = this;
}

AudioInputMessageFilter::~AudioInputMessageFilter() {
  DCHECK_EQ(g_filter, this);
  g_filter = NULL;
}

// static
AudioInputMessageFilter* AudioInputMessageFilter::Get() {
  return g_filter;
}

void AudioInputMessageFilter::Send(IPC::Message* message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  if (!sender_) {
    delete message;
  } else {
    sender_->Send(message);
  }
}

bool AudioInputMessageFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AudioInputMessageFilter, message)
    IPC_MESSAGE_HANDLER(AudioInputMsg_NotifyStreamCreated,
                        OnStreamCreated)
    IPC_MESSAGE_HANDLER(AudioInputMsg_NotifyStreamVolume, OnStreamVolume)
    IPC_MESSAGE_HANDLER(AudioInputMsg_NotifyStreamStateChanged,
                        OnStreamStateChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AudioInputMessageFilter::OnFilterAdded(IPC::Sender* sender) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());

  // Captures the sender for IPC.
  sender_ = sender;
}

void AudioInputMessageFilter::OnFilterRemoved() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());

  // Once removed, a filter will not be used again.  At this time all
  // delegates must be notified so they release their reference.
  OnChannelClosing();
}

void AudioInputMessageFilter::OnChannelClosing() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  sender_ = NULL;

  DLOG_IF(WARNING, !delegates_.IsEmpty())
      << "Not all audio devices have been closed.";

  IDMap<media::AudioInputIPCDelegate>::iterator it(&delegates_);
  while (!it.IsAtEnd()) {
    it.GetCurrentValue()->OnIPCClosed();
    delegates_.Remove(it.GetCurrentKey());
    it.Advance();
  }
}

void AudioInputMessageFilter::OnStreamCreated(
    int stream_id,
    base::SharedMemoryHandle handle,
#if defined(OS_WIN)
    base::SyncSocket::Handle socket_handle,
#else
    base::FileDescriptor socket_descriptor,
#endif
    uint32 length,
    uint32 total_segments) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());

  WebRtcLogMessage(base::StringPrintf(
      "AIMF::OnStreamCreated. stream_id=%d",
      stream_id));

#if !defined(OS_WIN)
  base::SyncSocket::Handle socket_handle = socket_descriptor.fd;
#endif
  media::AudioInputIPCDelegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
                  << " audio capturer (stream_id=" << stream_id << ").";
    base::SharedMemory::CloseHandle(handle);
    base::SyncSocket socket(socket_handle);
    return;
  }
  // Forward message to the stream delegate.
  delegate->OnStreamCreated(handle, socket_handle, length, total_segments);
}

void AudioInputMessageFilter::OnStreamVolume(int stream_id, double volume) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  media::AudioInputIPCDelegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
                  << " audio capturer.";
    return;
  }
  delegate->OnVolume(volume);
}

void AudioInputMessageFilter::OnStreamStateChanged(
    int stream_id, media::AudioInputIPCDelegate::State state) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  media::AudioInputIPCDelegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
                  << " audio renderer.";
    return;
  }
  delegate->OnStateChanged(state);
}

AudioInputMessageFilter::AudioInputIPCImpl::AudioInputIPCImpl(
    const scoped_refptr<AudioInputMessageFilter>& filter, int render_view_id)
    : filter_(filter),
      render_view_id_(render_view_id),
      stream_id_(kStreamIDNotSet) {}

AudioInputMessageFilter::AudioInputIPCImpl::~AudioInputIPCImpl() {}

scoped_ptr<media::AudioInputIPC> AudioInputMessageFilter::CreateAudioInputIPC(
    int render_view_id) {
  DCHECK_GT(render_view_id, 0);
  return scoped_ptr<media::AudioInputIPC>(
      new AudioInputIPCImpl(this, render_view_id));
}

void AudioInputMessageFilter::AudioInputIPCImpl::CreateStream(
    media::AudioInputIPCDelegate* delegate,
    int session_id,
    const media::AudioParameters& params,
    bool automatic_gain_control,
    uint32 total_segments) {
  DCHECK(filter_->io_message_loop_->BelongsToCurrentThread());
  DCHECK(delegate);

  stream_id_ = filter_->delegates_.Add(delegate);

  AudioInputHostMsg_CreateStream_Config config;
  config.params = params;
  config.automatic_gain_control = automatic_gain_control;
  config.shared_memory_count = total_segments;
  filter_->Send(new AudioInputHostMsg_CreateStream(
      stream_id_, render_view_id_, session_id, config));
}

void AudioInputMessageFilter::AudioInputIPCImpl::RecordStream() {
  DCHECK_NE(stream_id_, kStreamIDNotSet);
  filter_->Send(new AudioInputHostMsg_RecordStream(stream_id_));
}

void AudioInputMessageFilter::AudioInputIPCImpl::SetVolume(double volume) {
  DCHECK_NE(stream_id_, kStreamIDNotSet);
  filter_->Send(new AudioInputHostMsg_SetVolume(stream_id_, volume));
}

void AudioInputMessageFilter::AudioInputIPCImpl::CloseStream() {
  DCHECK(filter_->io_message_loop_->BelongsToCurrentThread());
  DCHECK_NE(stream_id_, kStreamIDNotSet);
  filter_->Send(new AudioInputHostMsg_CloseStream(stream_id_));
  filter_->delegates_.Remove(stream_id_);
  stream_id_ = kStreamIDNotSet;
}

}  // namespace content

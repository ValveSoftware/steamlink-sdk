// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_message_filter.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/stringprintf.h"
#include "content/common/media/audio_messages.h"
#include "content/renderer/media/webrtc_logging.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_logging.h"

namespace content {

namespace {
const int kStreamIDNotSet = -1;
}

class AudioMessageFilter::AudioOutputIPCImpl
    : public NON_EXPORTED_BASE(media::AudioOutputIPC) {
 public:
  AudioOutputIPCImpl(const scoped_refptr<AudioMessageFilter>& filter,
                     int render_view_id,
                     int render_frame_id);
  virtual ~AudioOutputIPCImpl();

  // media::AudioOutputIPC implementation.
  virtual void CreateStream(media::AudioOutputIPCDelegate* delegate,
                            const media::AudioParameters& params,
                            int session_id) OVERRIDE;
  virtual void PlayStream() OVERRIDE;
  virtual void PauseStream() OVERRIDE;
  virtual void CloseStream() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;

 private:
  const scoped_refptr<AudioMessageFilter> filter_;
  const int render_view_id_;
  const int render_frame_id_;
  int stream_id_;
};

AudioMessageFilter* AudioMessageFilter::g_filter = NULL;

AudioMessageFilter::AudioMessageFilter(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop)
    : sender_(NULL),
      audio_hardware_config_(NULL),
      io_message_loop_(io_message_loop) {
  DCHECK(!g_filter);
  g_filter = this;
}

AudioMessageFilter::~AudioMessageFilter() {
  DCHECK_EQ(g_filter, this);
  g_filter = NULL;
}

// static
AudioMessageFilter* AudioMessageFilter::Get() {
  return g_filter;
}

AudioMessageFilter::AudioOutputIPCImpl::AudioOutputIPCImpl(
    const scoped_refptr<AudioMessageFilter>& filter,
    int render_view_id,
    int render_frame_id)
    : filter_(filter),
      render_view_id_(render_view_id),
      render_frame_id_(render_frame_id),
      stream_id_(kStreamIDNotSet) {}

AudioMessageFilter::AudioOutputIPCImpl::~AudioOutputIPCImpl() {}

scoped_ptr<media::AudioOutputIPC> AudioMessageFilter::CreateAudioOutputIPC(
    int render_view_id, int render_frame_id) {
  DCHECK_GT(render_view_id, 0);
  return scoped_ptr<media::AudioOutputIPC>(
      new AudioOutputIPCImpl(this, render_view_id, render_frame_id));
}

void AudioMessageFilter::AudioOutputIPCImpl::CreateStream(
    media::AudioOutputIPCDelegate* delegate,
    const media::AudioParameters& params,
    int session_id) {
  DCHECK(filter_->io_message_loop_->BelongsToCurrentThread());
  DCHECK(delegate);
  DCHECK_EQ(stream_id_, kStreamIDNotSet);
  stream_id_ = filter_->delegates_.Add(delegate);
  filter_->Send(new AudioHostMsg_CreateStream(
      stream_id_, render_view_id_, render_frame_id_, session_id, params));
}

void AudioMessageFilter::AudioOutputIPCImpl::PlayStream() {
  DCHECK_NE(stream_id_, kStreamIDNotSet);
  filter_->Send(new AudioHostMsg_PlayStream(stream_id_));
}

void AudioMessageFilter::AudioOutputIPCImpl::PauseStream() {
  DCHECK_NE(stream_id_, kStreamIDNotSet);
  filter_->Send(new AudioHostMsg_PauseStream(stream_id_));
}

void AudioMessageFilter::AudioOutputIPCImpl::CloseStream() {
  DCHECK(filter_->io_message_loop_->BelongsToCurrentThread());
  DCHECK_NE(stream_id_, kStreamIDNotSet);
  filter_->Send(new AudioHostMsg_CloseStream(stream_id_));
  filter_->delegates_.Remove(stream_id_);
  stream_id_ = kStreamIDNotSet;
}

void AudioMessageFilter::AudioOutputIPCImpl::SetVolume(double volume) {
  DCHECK_NE(stream_id_, kStreamIDNotSet);
  filter_->Send(new AudioHostMsg_SetVolume(stream_id_, volume));
}

void AudioMessageFilter::Send(IPC::Message* message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  if (!sender_) {
    delete message;
  } else {
    sender_->Send(message);
  }
}

bool AudioMessageFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AudioMessageFilter, message)
    IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamCreated, OnStreamCreated)
    IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamStateChanged, OnStreamStateChanged)
    IPC_MESSAGE_HANDLER(AudioMsg_NotifyDeviceChanged, OnOutputDeviceChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AudioMessageFilter::OnFilterAdded(IPC::Sender* sender) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  sender_ = sender;
}

void AudioMessageFilter::OnFilterRemoved() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());

  // Once removed, a filter will not be used again.  At this time all
  // delegates must be notified so they release their reference.
  OnChannelClosing();
}

void AudioMessageFilter::OnChannelClosing() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  sender_ = NULL;

  DLOG_IF(WARNING, !delegates_.IsEmpty())
      << "Not all audio devices have been closed.";

  IDMap<media::AudioOutputIPCDelegate>::iterator it(&delegates_);
  while (!it.IsAtEnd()) {
    it.GetCurrentValue()->OnIPCClosed();
    delegates_.Remove(it.GetCurrentKey());
    it.Advance();
  }
}

void AudioMessageFilter::OnStreamCreated(
    int stream_id,
    base::SharedMemoryHandle handle,
#if defined(OS_WIN)
    base::SyncSocket::Handle socket_handle,
#else
    base::FileDescriptor socket_descriptor,
#endif
    uint32 length) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());

  WebRtcLogMessage(base::StringPrintf(
      "AMF::OnStreamCreated. stream_id=%d",
      stream_id));

#if !defined(OS_WIN)
  base::SyncSocket::Handle socket_handle = socket_descriptor.fd;
#endif

  media::AudioOutputIPCDelegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got OnStreamCreated() event for a non-existent or removed"
                  << " audio renderer. (stream_id=" << stream_id << ").";
    base::SharedMemory::CloseHandle(handle);
    base::SyncSocket socket(socket_handle);
    return;
  }
  delegate->OnStreamCreated(handle, socket_handle, length);
}

void AudioMessageFilter::OnStreamStateChanged(
    int stream_id, media::AudioOutputIPCDelegate::State state) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  media::AudioOutputIPCDelegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got OnStreamStateChanged() event for a non-existent or"
                  << " removed audio renderer.  State: " << state;
    return;
  }
  delegate->OnStateChanged(state);
}

void AudioMessageFilter::OnOutputDeviceChanged(int stream_id,
                                               int new_buffer_size,
                                               int new_sample_rate) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);

  WebRtcLogMessage(base::StringPrintf(
      "AMF::OnOutputDeviceChanged. stream_id=%d"
      ", new_buffer_size=%d, new_sample_rate=%d",
      stream_id,
      new_buffer_size,
      new_sample_rate));

  // Ignore the message if an audio hardware config hasn't been created; this
  // can occur if the renderer is using the high latency audio path.
  CHECK(audio_hardware_config_);

  // TODO(crogers): fix OnOutputDeviceChanged() to pass AudioParameters.
  media::ChannelLayout channel_layout =
      audio_hardware_config_->GetOutputChannelLayout();
  int channels = audio_hardware_config_->GetOutputChannels();

  media::AudioParameters output_params;
  output_params.Reset(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      channel_layout,
      channels,
      0,
      new_sample_rate,
      16,
      new_buffer_size);

  audio_hardware_config_->UpdateOutputConfig(output_params);
}

void AudioMessageFilter::SetAudioHardwareConfig(
    media::AudioHardwareConfig* config) {
  base::AutoLock auto_lock(lock_);
  audio_hardware_config_ = config;
}

}  // namespace content

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_INPUT_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_INPUT_MESSAGE_FILTER_H_

#include <stdint.h>

#include <memory>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/sync_socket.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "ipc/message_filter.h"
#include "media/audio/audio_input_ipc.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

// MessageFilter that handles audio input messages and delegates them to
// audio capturers. Created on render thread, AudioMessageFilter is operated on
// IO thread (secondary thread of render process), it intercepts audio messages
// and process them on IO thread since these messages are time critical.
class CONTENT_EXPORT AudioInputMessageFilter : public IPC::MessageFilter {
 public:
  explicit AudioInputMessageFilter(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  // Getter for the one AudioInputMessageFilter object.
  static AudioInputMessageFilter* Get();

  // Create an AudioInputIPC to be owned by one delegate.  |render_frame_id|
  // refers to the RenderFrame containing the entity consuming the audio.
  //
  // The returned object is not thread-safe, and must be used on
  // |io_task_runner|.
  std::unique_ptr<media::AudioInputIPC> CreateAudioInputIPC(
      int render_frame_id);

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner() const {
    return io_task_runner_;
  }

 private:
  // Implementation of media::AudioInputIPC which augments IPC calls with
  // stream_id and the destination render_frame_id.
  class AudioInputIPCImpl;

  ~AudioInputMessageFilter() override;

  // Sends an IPC message using |channel_|.
  void Send(IPC::Message* message);

  // IPC::MessageFilter override. Called on |io_task_runner_|.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnFilterRemoved() override;
  void OnChannelClosing() override;

  // Received when browser process has created an audio input stream.
  void OnStreamCreated(int stream_id,
                       base::SharedMemoryHandle handle,
#if defined(OS_WIN)
                       base::SyncSocket::Handle socket_handle,
#else
                       base::FileDescriptor socket_descriptor,
#endif
                       uint32_t length,
                       uint32_t total_segments);

  // Notification of volume property of an audio input stream.
  void OnStreamVolume(int stream_id, double volume);

  // Received when internal state of browser process' audio input stream has
  // changed.
  void OnStreamStateChanged(int stream_id,
                            media::AudioInputIPCDelegateState state);

  // A map of stream ids to delegates.
  IDMap<media::AudioInputIPCDelegate> delegates_;

  // IPC sender for Send(), must only be accesed on |io_task_runner_|.
  IPC::Sender* sender_;

  // Task runner on which IPC calls are executed.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The singleton instance for this filter.
  static AudioInputMessageFilter* g_filter;

  DISALLOW_COPY_AND_ASSIGN(AudioInputMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_INPUT_MESSAGE_FILTER_H_

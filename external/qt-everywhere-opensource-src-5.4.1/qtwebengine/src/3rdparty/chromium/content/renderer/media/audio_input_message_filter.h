// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_INPUT_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_INPUT_MESSAGE_FILTER_H_

#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/sync_socket.h"
#include "content/common/content_export.h"
#include "ipc/message_filter.h"
#include "media/audio/audio_input_ipc.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

// MessageFilter that handles audio input messages and delegates them to
// audio capturers. Created on render thread, AudioMessageFilter is operated on
// IO thread (secondary thread of render process), it intercepts audio messages
// and process them on IO thread since these messages are time critical.
class CONTENT_EXPORT AudioInputMessageFilter : public IPC::MessageFilter {
 public:
  explicit AudioInputMessageFilter(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop);

  // Getter for the one AudioInputMessageFilter object.
  static AudioInputMessageFilter* Get();

  // Create an AudioInputIPC to be owned by one delegate.  |render_view_id| is
  // the render view containing the entity consuming the audio.
  //
  // The returned object is not thread-safe, and must be used on
  // |io_message_loop|.
  scoped_ptr<media::AudioInputIPC> CreateAudioInputIPC(int render_view_id);

  scoped_refptr<base::MessageLoopProxy> io_message_loop() const {
    return io_message_loop_;
  }

 private:
  // Implementation of media::AudioInputIPC which augments IPC calls with
  // stream_id and the destination render_view_id.
  class AudioInputIPCImpl;

  virtual ~AudioInputMessageFilter();

  // Sends an IPC message using |channel_|.
  void Send(IPC::Message* message);

  // IPC::MessageFilter override. Called on |io_message_loop_|.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnFilterAdded(IPC::Sender* sender) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

  // Received when browser process has created an audio input stream.
  void OnStreamCreated(int stream_id,
                       base::SharedMemoryHandle handle,
#if defined(OS_WIN)
                       base::SyncSocket::Handle socket_handle,
#else
                       base::FileDescriptor socket_descriptor,
#endif
                       uint32 length,
                       uint32 total_segments);

  // Notification of volume property of an audio input stream.
  void OnStreamVolume(int stream_id, double volume);

  // Received when internal state of browser process' audio input stream has
  // changed.
  void OnStreamStateChanged(int stream_id,
                            media::AudioInputIPCDelegate::State state);

  // A map of stream ids to delegates.
  IDMap<media::AudioInputIPCDelegate> delegates_;

  // IPC sender for Send(), must only be accesed on |io_message_loop_|.
  IPC::Sender* sender_;

  // Message loop on which IPC calls are driven.
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  // The singleton instance for this filter.
  static AudioInputMessageFilter* g_filter;

  DISALLOW_COPY_AND_ASSIGN(AudioInputMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_INPUT_MESSAGE_FILTER_H_

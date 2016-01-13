// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_MESSAGE_FILTER_H_

#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/sync_socket.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "ipc/message_filter.h"
#include "media/audio/audio_output_ipc.h"
#include "media/base/audio_hardware_config.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

// MessageFilter that handles audio messages and delegates them to audio
// renderers. Created on render thread, AudioMessageFilter is operated on
// IO thread (secondary thread of render process) it intercepts audio messages
// and process them on IO thread since these messages are time critical.
class CONTENT_EXPORT AudioMessageFilter : public IPC::MessageFilter {
 public:
  explicit AudioMessageFilter(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop);

  // Getter for the one AudioMessageFilter object.
  static AudioMessageFilter* Get();

  // Create an AudioOutputIPC to be owned by one delegate.  |render_view_id| and
  // |render_frame_id| are the render view and render frame containing the
  // entity producing the audio.
  // TODO(jam): remove render_view_id
  //
  // The returned object is not thread-safe, and must be used on
  // |io_message_loop|.
  scoped_ptr<media::AudioOutputIPC> CreateAudioOutputIPC(int render_view_id,
                                                         int render_frame_id);

  // When set, AudioMessageFilter will update the AudioHardwareConfig with new
  // configuration values as received by OnOutputDeviceChanged().  The provided
  // |config| must outlive AudioMessageFilter.
  void SetAudioHardwareConfig(media::AudioHardwareConfig* config);

  // IO message loop associated with this message filter.
  scoped_refptr<base::MessageLoopProxy> io_message_loop() const {
    return io_message_loop_;
  }

 protected:
  virtual ~AudioMessageFilter();

 private:
  FRIEND_TEST_ALL_PREFIXES(AudioMessageFilterTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(AudioMessageFilterTest, Delegates);

  // Implementation of media::AudioOutputIPC which augments IPC calls with
  // stream_id and the source render_view_id.
  class AudioOutputIPCImpl;

  // Sends an IPC message using |sender_|.
  void Send(IPC::Message* message);

  // IPC::MessageFilter override. Called on |io_message_loop|.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnFilterAdded(IPC::Sender* sender) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

  // Received when browser process has created an audio output stream.
  void OnStreamCreated(int stream_id, base::SharedMemoryHandle handle,
#if defined(OS_WIN)
                       base::SyncSocket::Handle socket_handle,
#else
                       base::FileDescriptor socket_descriptor,
#endif
                       uint32 length);

  // Received when internal state of browser process' audio output device has
  // changed.
  void OnStreamStateChanged(int stream_id,
                            media::AudioOutputIPCDelegate::State state);

  // Received when the browser process detects an output device change.
  void OnOutputDeviceChanged(int stream_id, int new_buffer_size,
                             int new_sample_rate);

  // IPC sender for Send(); must only be accesed on |io_message_loop_|.
  IPC::Sender* sender_;

  // A map of stream ids to delegates; must only be accessed on
  // |io_message_loop_|.
  IDMap<media::AudioOutputIPCDelegate> delegates_;

  // Audio hardware configuration to update when OnOutputDeviceChanged() fires.
  // Access is guarded by |lock_|.
  base::Lock lock_;
  media::AudioHardwareConfig* audio_hardware_config_;

  // Message loop on which IPC calls are driven.
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  // The singleton instance for this filter.
  static AudioMessageFilter* g_filter;

  DISALLOW_COPY_AND_ASSIGN(AudioMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_MESSAGE_FILTER_H_

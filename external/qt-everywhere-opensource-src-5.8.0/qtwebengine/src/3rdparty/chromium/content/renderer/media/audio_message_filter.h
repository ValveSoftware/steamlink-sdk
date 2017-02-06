// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_MESSAGE_FILTER_H_

#include <stdint.h>

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/sync_socket.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "ipc/message_filter.h"
#include "media/audio/audio_output_ipc.h"
#include "media/base/audio_hardware_config.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

// MessageFilter that handles audio messages and delegates them to audio
// renderers. Created on render thread, AudioMessageFilter is operated on
// IO thread (secondary thread of render process) it intercepts audio messages
// and process them on IO thread since these messages are time critical.
class CONTENT_EXPORT AudioMessageFilter : public IPC::MessageFilter {
 public:
  explicit AudioMessageFilter(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  // Getter for the one AudioMessageFilter object.
  static AudioMessageFilter* Get();

  // Create an AudioOutputIPC to be owned by one delegate.  |render_frame_id| is
  // the RenderFrame containing the entity producing the audio.
  //
  // The returned object is not thread-safe, and must be used on
  // |io_task_runner|.
  std::unique_ptr<media::AudioOutputIPC> CreateAudioOutputIPC(
      int render_frame_id);

  // IO task runner associated with this message filter.
  base::SingleThreadTaskRunner* io_task_runner() const {
    return io_task_runner_.get();
  }

 protected:
  ~AudioMessageFilter() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AudioMessageFilterTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(AudioMessageFilterTest, Delegates);

  // Implementation of media::AudioOutputIPC which augments IPC calls with
  // stream_id and the source render_frame_id.
  class AudioOutputIPCImpl;

  // Sends an IPC message using |sender_|.
  void Send(IPC::Message* message);

  // IPC::MessageFilter override. Called on |io_task_runner_|.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnFilterRemoved() override;
  void OnChannelClosing() override;

  // Received when the browser process has checked authorization to use an
  // audio output device.
  void OnDeviceAuthorized(int stream_id,
                          media::OutputDeviceStatus device_status,
                          const media::AudioParameters& output_params,
                          const std::string& matched_device_id);

  // Received when browser process has created an audio output stream.
  void OnStreamCreated(int stream_id,
                       base::SharedMemoryHandle handle,
                       base::SyncSocket::TransitDescriptor socket_descriptor,
                       uint32_t length);

  // Received when internal state of browser process' audio output device has
  // changed.
  void OnStreamStateChanged(int stream_id,
                            media::AudioOutputIPCDelegateState state);

  // IPC sender for Send(); must only be accessed on |io_task_runner_|.
  IPC::Sender* sender_;

  // A map of stream ids to delegates; must only be accessed on
  // |io_task_runner_|.
  IDMap<media::AudioOutputIPCDelegate> delegates_;

  // Task runner on which IPC calls are executed.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The singleton instance for this filter.
  static AudioMessageFilter* g_filter;

  DISALLOW_COPY_AND_ASSIGN(AudioMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_MESSAGE_FILTER_H_

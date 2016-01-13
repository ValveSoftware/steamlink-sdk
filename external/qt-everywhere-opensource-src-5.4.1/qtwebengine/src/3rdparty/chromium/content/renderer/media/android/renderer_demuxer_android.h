// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_DEMUXER_ANDROID_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_DEMUXER_ANDROID_H_

#include "base/atomic_sequence_num.h"
#include "base/id_map.h"
#include "ipc/message_filter.h"
#include "media/base/android/demuxer_stream_player_params.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class MediaSourceDelegate;
class ThreadSafeSender;

// Represents the renderer process half of an IPC-based implementation of
// media::DemuxerAndroid.
//
// Refer to BrowserDemuxerAndroid for the browser process half.
class RendererDemuxerAndroid : public IPC::MessageFilter {
 public:
  RendererDemuxerAndroid();

  // Returns the next available demuxer client ID for use in IPC messages.
  //
  // Safe to call on any thread.
  int GetNextDemuxerClientID();

  // Associates |delegate| with |demuxer_client_id| for handling incoming IPC
  // messages.
  //
  // Must be called on media thread.
  void AddDelegate(int demuxer_client_id, MediaSourceDelegate* delegate);

  // Removes the association created by AddDelegate().
  //
  // Must be called on media thread.
  void RemoveDelegate(int demuxer_client_id);

  // IPC::MessageFilter overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // media::DemuxerAndroidClient "implementation".
  //
  // TODO(scherkus): Consider having RendererDemuxerAndroid actually implement
  // media::DemuxerAndroidClient and MediaSourceDelegate actually implement
  // media::DemuxerAndroid.
  void DemuxerReady(int demuxer_client_id,
                    const media::DemuxerConfigs& configs);
  void ReadFromDemuxerAck(int demuxer_client_id,
                          const media::DemuxerData& data);
  void DemuxerSeekDone(int demuxer_client_id,
                       const base::TimeDelta& actual_browser_seek_time);
  void DurationChanged(int demuxer_client_id, const base::TimeDelta& duration);

 protected:
  friend class base::RefCountedThreadSafe<RendererDemuxerAndroid>;
  virtual ~RendererDemuxerAndroid();

 private:
  void DispatchMessage(const IPC::Message& message);
  void OnReadFromDemuxer(int demuxer_client_id,
                         media::DemuxerStream::Type type);
  void OnDemuxerSeekRequest(int demuxer_client_id,
                            const base::TimeDelta& time_to_seek,
                            bool is_browser_seek);

  base::AtomicSequenceNumber next_demuxer_client_id_;

  IDMap<MediaSourceDelegate> delegates_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<base::MessageLoopProxy> media_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(RendererDemuxerAndroid);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_DEMUXER_ANDROID_H_

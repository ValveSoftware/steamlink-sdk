// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEDIA_MEDIA_CHANNEL_PROXY_H_
#define CHROMECAST_RENDERER_MEDIA_MEDIA_CHANNEL_PROXY_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromecast/media/cma/pipeline/load_type.h"
#include "chromecast/renderer/media/cma_message_filter_proxy.h"

namespace IPC {
class Message;
}

namespace chromecast {
namespace media {

// MediaChannelProxy - Manage the lifetime of a CMA ipc channel.
// Must be invoked from the IO thread of the renderer process.
class MediaChannelProxy
    : public base::RefCountedThreadSafe<MediaChannelProxy> {
 public:
  MediaChannelProxy();

  // Opens a CMA ipc channel.
  void Open(LoadType load_type);

  // Closes the ipc channel.
  void Close();

  // Returns the ID of the CMA ipc channel.
  // Returns 0 or negative ID if no channel has been opened.
  int GetId() { return id_; }

  // Manages delegates.
  bool SetMediaDelegate(
      const CmaMessageFilterProxy::MediaDelegate& media_delegate);
  bool SetAudioDelegate(
      const CmaMessageFilterProxy::AudioDelegate& audio_delegate);
  bool SetVideoDelegate(
      const CmaMessageFilterProxy::VideoDelegate& video_delegate);

  // Sends an IPC message over this CMA ipc channel.
  bool Send(std::unique_ptr<IPC::Message> message);

 private:
  friend class base::RefCountedThreadSafe<MediaChannelProxy>;
  virtual ~MediaChannelProxy();

  // Message filter running on the renderer side.
  scoped_refptr<CmaMessageFilterProxy> filter_;

  // Indicates whether the CMA channel is open.
  bool is_open_;

  // Unique identifier per media pipeline.
  int id_;

  DISALLOW_COPY_AND_ASSIGN(MediaChannelProxy);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEDIA_MEDIA_CHANNEL_PROXY_H_

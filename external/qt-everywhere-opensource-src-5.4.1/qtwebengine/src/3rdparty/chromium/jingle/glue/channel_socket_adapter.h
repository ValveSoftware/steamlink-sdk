// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_GLUE_CHANNEL_SOCKET_ADAPTER_H_
#define JINGLE_GLUE_CHANNEL_SOCKET_ADAPTER_H_

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "net/socket/socket.h"
#include "third_party/libjingle/source/talk/base/asyncpacketsocket.h"
#include "third_party/libjingle/source/talk/base/socketaddress.h"
#include "third_party/libjingle/source/talk/base/sigslot.h"

namespace base {
class MessageLoop;
}

namespace cricket {
class TransportChannel;
}  // namespace cricket

namespace jingle_glue {

// TransportChannelSocketAdapter implements net::Socket interface on
// top of libjingle's TransportChannel. It is used by
// JingleChromotocolConnection to provide net::Socket interface for channels.
class TransportChannelSocketAdapter : public net::Socket,
                                      public sigslot::has_slots<> {
 public:
  // TransportChannel object is always owned by the corresponding session.
  explicit TransportChannelSocketAdapter(cricket::TransportChannel* channel);
  virtual ~TransportChannelSocketAdapter();

  // Sets callback that should be called when the adapter is being
  // destroyed. The callback is not allowed to touch the adapter, but
  // can do anything else, e.g. destroy the TransportChannel.
  void SetOnDestroyedCallback(const base::Closure& callback);

  // Closes the stream. |error_code| specifies error code that will
  // be returned by Read() and Write() after the stream is closed.
  // Must be called before the session and the channel are destroyed.
  void Close(int error_code);

  // Socket implementation.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) OVERRIDE;

  virtual int SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual int SetSendBufferSize(int32 size) OVERRIDE;

 private:
  void OnNewPacket(cricket::TransportChannel* channel,
                   const char* data,
                   size_t data_size,
                   const talk_base::PacketTime& packet_time,
                   int flags);
  void OnWritableState(cricket::TransportChannel* channel);
  void OnChannelDestroyed(cricket::TransportChannel* channel);

  base::MessageLoop* message_loop_;

  cricket::TransportChannel* channel_;

  base::Closure destruction_callback_;

  net::CompletionCallback read_callback_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;

  net::CompletionCallback write_callback_;
  scoped_refptr<net::IOBuffer> write_buffer_;
  int write_buffer_size_;

  int closed_error_code_;

  DISALLOW_COPY_AND_ASSIGN(TransportChannelSocketAdapter);
};

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_CHANNEL_SOCKET_ADAPTER_H_

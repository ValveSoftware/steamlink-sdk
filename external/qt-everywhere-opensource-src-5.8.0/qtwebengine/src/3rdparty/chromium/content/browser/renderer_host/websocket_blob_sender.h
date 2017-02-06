// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_WEBSOCKET_BLOB_SENDER_H_
#define CONTENT_BROWSER_RENDERER_HOST_WEBSOCKET_BLOB_SENDER_H_

#include <stddef.h>
#include <stdint.h>

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "net/base/completion_callback.h"
#include "net/websockets/websocket_event_interface.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class IOBuffer;
}

namespace storage {
class BlobReader;
class BlobStorageContext;
class FileSystemContext;
}

namespace content {

class WebSocketHost;

// Read the contents of a Blob and write it to a WebSocket. Single-use: a new
// object must be created each time a Blob is sent. Destroying the object
// cancels all pending operations.
class CONTENT_EXPORT WebSocketBlobSender final {
 public:
  // An abstraction of the WebSocketChannel this object will send frames to.
  class Channel {
   public:
    using ChannelState = net::WebSocketEventInterface::ChannelState;

    Channel() {}
    virtual ~Channel() {}

    // The currently available quota for sending. It must not decrease without
    // SendFrame() being called.
    virtual size_t GetSendQuota() const = 0;

    // Send a binary frame. |fin| is true for the final frame of the message.
    // |data| is the contents of the frame. data.size() must be less than
    // GetSendQuota(). If this call returns CHANNEL_DELETED, WebSocketBlobSender
    // will assume that it has been deleted and return without calling any
    // callbacks or accessing any other member data.
    virtual ChannelState SendFrame(bool fin, const std::vector<char>& data) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Channel);
  };

  // |channel| will be destroyed when this object is.
  explicit WebSocketBlobSender(std::unique_ptr<Channel> channel);
  ~WebSocketBlobSender();

  // Checks that the blob identified by |uuid| exists, has the size
  // |expected_size| and then starts sending it via |channel_|. Returns
  // ERR_IO_PENDING to indicate that |callback| will be called later with the
  // result. net::OK indicates synchronous success. Any other net error code
  // indicates synchronous failure. This method may result in the destruction of
  // the channel, in which case |*channel_state| will be set to CHANNEL_DELETED.
  int Start(const std::string& uuid,
            uint64_t expected_size,
            storage::BlobStorageContext* context,
            storage::FileSystemContext* file_system_context,
            base::SingleThreadTaskRunner* file_task_runner,
            net::WebSocketEventInterface::ChannelState* channel_state,
            const net::CompletionCallback& callback);

  // Sends more data if the object was waiting for quota and the new value of
  // GetSendQuota() is large enough.
  void OnNewSendQuota();

  uint64_t expected_size() const { return expected_size_; }

  // ActualSize() should only be called after completion: ie. Start() returned a
  // value other than ERR_IO_PENDING or |callback_| has been called.
  uint64_t ActualSize() const;

 private:
  // State proceeds through READ_SIZE and READ_SIZE_COMPLETE, then
  // loops WAIT_FOR_QUOTA -> WAIT_FOR_QUOTA_COMPLETE -> READ
  // -> READ_COMPLETE -> WAIT_FOR_QUOTA until the Blob is completely
  // sent.
  enum class State {
    NONE = 0,
    READ_SIZE,
    READ_SIZE_COMPLETE,
    WAIT_FOR_QUOTA,
    WAIT_FOR_QUOTA_COMPLETE,
    READ,
    READ_COMPLETE,
  };

  // This is needed to make DCHECK_EQ(), etc. compile.
  friend std::ostream& operator<<(std::ostream& os, State state);

  void OnReadComplete(int rv);
  void OnSizeCalculated(int rv);
  // |channel_state| should point to CHANNEL_ALIVE when called. If it is
  // CHANNEL_DELETED on return, the object has been deleted.
  int DoLoop(int result, Channel::ChannelState* channel_state);
  void DoLoopAsync(int result);
  int DoReadSize();
  int DoReadSizeComplete(int result);
  int DoWaitForQuota();
  int DoWaitForQuotaComplete();
  int DoRead();
  int DoReadComplete(int result, Channel::ChannelState* channel_state);

  State next_state_ = State::NONE;
  uint64_t expected_size_ = 0;
  uint64_t bytes_left_ = 0;
  net::CompletionCallback callback_;
  scoped_refptr<net::IOBuffer> buffer_;
  std::unique_ptr<storage::BlobReader> reader_;
  const std::unique_ptr<Channel> channel_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketBlobSender);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_WEBSOCKET_BLOB_SENDER_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_READER_H_
#define IPC_IPC_CHANNEL_READER_H_

#include "base/basictypes.h"
#include "ipc/ipc_channel.h"

namespace IPC {
namespace internal {

// This class provides common pipe reading functionality for the
// platform-specific IPC channel implementations.
//
// It does the common input buffer management and message dispatch, while the
// platform-specific parts provide the pipe management through a virtual
// interface implemented on a per-platform basis.
//
// Note that there is no "writer" corresponding to this because the code for
// writing to the channel is much simpler and has very little common
// functionality that would benefit from being factored out. If we add
// something like that in the future, it would be more appropriate to add it
// here (and rename appropriately) rather than writing a different class.
class ChannelReader {
 public:
  explicit ChannelReader(Listener* listener);
  virtual ~ChannelReader();

  void set_listener(Listener* listener) { listener_ = listener; }

  // Call to process messages received from the IPC connection and dispatch
  // them. Returns false on channel error. True indicates that everything
  // succeeded, although there may not have been any messages processed.
  bool ProcessIncomingMessages();

  // Handles asynchronously read data.
  //
  // Optionally call this after returning READ_PENDING from ReadData to
  // indicate that buffer was filled with the given number of bytes of
  // data. See ReadData for more.
  bool AsyncReadComplete(int bytes_read);

  // Returns true if the given message is internal to the IPC implementation,
  // like the "hello" message sent on channel set-up.
  bool IsInternalMessage(const Message& m) const;

  // Returns true if the given message is an Hello message
  // sent on channel set-up.
  bool IsHelloMessage(const Message& m) const;

 protected:
  enum ReadState { READ_SUCCEEDED, READ_FAILED, READ_PENDING };

  Listener* listener() const { return listener_; }

  // Populates the given buffer with data from the pipe.
  //
  // Returns the state of the read. On READ_SUCCESS, the number of bytes
  // read will be placed into |*bytes_read| (which can be less than the
  // buffer size). On READ_FAILED, the channel will be closed.
  //
  // If the return value is READ_PENDING, it means that there was no data
  // ready for reading. The implementation is then responsible for either
  // calling AsyncReadComplete with the number of bytes read into the
  // buffer, or ProcessIncomingMessages to try the read again (depending
  // on whether the platform's async I/O is "try again" or "write
  // asynchronously into your buffer").
  virtual ReadState ReadData(char* buffer, int buffer_len, int* bytes_read) = 0;

  // Loads the required file desciptors into the given message. Returns true
  // on success. False means a fatal channel error.
  //
  // This will read from the input_fds_ and read more handles from the FD
  // pipe if necessary.
  virtual bool WillDispatchInputMessage(Message* msg) = 0;

  // Performs post-dispatch checks. Called when all input buffers are empty,
  // though there could be more data ready to be read from the OS.
  virtual bool DidEmptyInputBuffers() = 0;

  // Handles internal messages, like the hello message sent on channel startup.
  virtual void HandleInternalMessage(const Message& msg) = 0;

 private:
  // Takes the given data received from the IPC channel and dispatches any
  // fully completed messages.
  //
  // Returns true on success. False means channel error.
  bool DispatchInputData(const char* input_data, int input_data_len);

  Listener* listener_;

  // We read from the pipe into this buffer. Managed by DispatchInputData, do
  // not access directly outside that function.
  char input_buf_[Channel::kReadBufferSize];

  // Large messages that span multiple pipe buffers, get built-up using
  // this buffer.
  std::string input_overflow_buf_;

  DISALLOW_COPY_AND_ASSIGN(ChannelReader);
};

}  // namespace internal
}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_READER_H_

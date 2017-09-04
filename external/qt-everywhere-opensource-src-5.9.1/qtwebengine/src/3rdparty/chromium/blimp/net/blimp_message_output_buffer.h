// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_MESSAGE_OUTPUT_BUFFER_H_
#define BLIMP_NET_BLIMP_MESSAGE_OUTPUT_BUFFER_H_

#include <stdint.h>

#include <list>
#include <queue>
#include <utility>

#include "base/macros.h"
#include "blimp/net/blimp_message_checkpoint_observer.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_net_export.h"
#include "net/base/completion_callback.h"

namespace blimp {

class BlimpConnection;

// Provides a FIFO buffer for reliable, ordered message delivery.
// Messages are retained for redelivery until they are acknowledged by the
// receiving end (via BlimpMessageCheckpointObserver).
// Messages can be paired with callbacks that are invoked on successful
// message acknowledgment.
// (Redelivery will be used in a future CL to implement Fast Recovery
// of dropped connections.)
// BlimpMessageOutputBuffer is created on the UI thread, and then used and
// destroyed on the IO thread.
class BLIMP_NET_EXPORT BlimpMessageOutputBuffer
    : public BlimpMessageProcessor,
      public BlimpMessageCheckpointObserver {
 public:
  explicit BlimpMessageOutputBuffer(int max_buffer_size_bytes);
  ~BlimpMessageOutputBuffer() override;

  // Sets the processor that will be used for writing buffered messages.
  void SetOutputProcessor(BlimpMessageProcessor* processor);

  // Marks all messages in buffer for retransmission.
  void RetransmitBufferedMessages();

  // BlimpMessageProcessor implementation.
  // |callback|, if set, will be called once the remote end has acknowledged the
  // receipt of |message|.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

  // MessageCheckpointObserver implementation.
  void OnMessageCheckpoint(int64_t message_id) override;

  int GetBufferByteSizeForTest() const;
  int GetUnacknowledgedMessageCountForTest() const;

 private:
  struct BufferEntry {
    BufferEntry(std::unique_ptr<BlimpMessage> message,
                net::CompletionCallback callback);
    ~BufferEntry();

    const std::unique_ptr<BlimpMessage> message;
    const net::CompletionCallback callback;
  };

  typedef std::list<std::unique_ptr<BufferEntry>> MessageBuffer;

  // Writes the next message in the buffer if an output processor is attached
  // and the buffer contains a message.
  void WriteNextMessageIfReady();

  // Receives the completion status of a write operation.
  void OnWriteComplete(int result);

  BlimpMessageProcessor* output_processor_ = nullptr;
  net::CancelableCompletionCallback write_complete_cb_;

  // Maximum serialized footprint of buffered messages.
  int max_buffer_size_bytes_;

  // Serialized footprint of the messages contained in the write and ack
  // buffers.
  int current_buffer_size_bytes_ = 0;

  // The ID used by the last outgoing message.
  int64_t prev_message_id_ = 0;

  // List of unsent messages.
  MessageBuffer write_buffer_;

  // List of messages that are sent and awaiting acknowledgment.
  // The messages in |ack_buffer_| are contiguous with the messages in
  // |write_buffer_|.
  MessageBuffer ack_buffer_;

  DISALLOW_COPY_AND_ASSIGN(BlimpMessageOutputBuffer);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_MESSAGE_OUTPUT_BUFFER_H_

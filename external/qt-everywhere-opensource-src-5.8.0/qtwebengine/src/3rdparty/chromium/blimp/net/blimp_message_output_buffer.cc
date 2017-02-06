// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_message_output_buffer.h"

#include <algorithm>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/common/logging.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "net/base/net_errors.h"

namespace blimp {

BlimpMessageOutputBuffer::BlimpMessageOutputBuffer(int max_buffer_size_bytes)
    : max_buffer_size_bytes_(max_buffer_size_bytes) {}

BlimpMessageOutputBuffer::~BlimpMessageOutputBuffer() {}

void BlimpMessageOutputBuffer::SetOutputProcessor(
    BlimpMessageProcessor* processor) {
  DVLOG(1) << "SetOutputProcessor " << processor;
  // Check that we are setting or removing the processor, not replacing it.
  if (processor) {
    DCHECK(!output_processor_);
    output_processor_ = processor;
    write_complete_cb_.Reset(base::Bind(
        &BlimpMessageOutputBuffer::OnWriteComplete, base::Unretained(this)));
    WriteNextMessageIfReady();
  } else {
    DCHECK(output_processor_);
    output_processor_ = nullptr;
    write_complete_cb_.Cancel();
  }
}

void BlimpMessageOutputBuffer::RetransmitBufferedMessages() {
  DCHECK(output_processor_);
  DVLOG(1) << "RetransmitBufferedMessages()";

  // Prepend the entirety of |ack_buffer_| to |write_buffer_|.
  write_buffer_.insert(write_buffer_.begin(),
                       std::make_move_iterator(ack_buffer_.begin()),
                       std::make_move_iterator(ack_buffer_.end()));
  ack_buffer_.clear();

  WriteNextMessageIfReady();
}

int BlimpMessageOutputBuffer::GetBufferByteSizeForTest() const {
  return write_buffer_.size() + ack_buffer_.size();
}

int BlimpMessageOutputBuffer::GetUnacknowledgedMessageCountForTest() const {
  return ack_buffer_.size();
}

void BlimpMessageOutputBuffer::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  DVLOG(2) << "OutputBuffer::ProcessMessage " << *message;

  message->set_message_id(++prev_message_id_);

  current_buffer_size_bytes_ += message->ByteSize();
  if (max_buffer_size_bytes_ < current_buffer_size_bytes_)
    DLOG(WARNING) << "Output Buffer Size exceeds " << max_buffer_size_bytes_
                  << "bytes. Current size: " << current_buffer_size_bytes_
                  << " bytes.";

  write_buffer_.push_back(
      base::WrapUnique(new BufferEntry(std::move(message), callback)));

  // Write the message
  if (write_buffer_.size() == 1 && output_processor_) {
    WriteNextMessageIfReady();
  }
}

// Flushes acknowledged messages from the buffer and invokes their
// |callbacks|, if any.
void BlimpMessageOutputBuffer::OnMessageCheckpoint(int64_t message_id) {
  VLOG(2) << "OnMessageCheckpoint (message_id=" << message_id << ")";
  if (ack_buffer_.empty()) {
    LOG(WARNING) << "Checkpoint called while buffer is empty.";
    return;
  }
  if (message_id > prev_message_id_) {
    LOG(WARNING) << "Illegal checkpoint response: " << message_id;
    return;
  }

  // Remove all acknowledged messages through |message_id| and invoke their
  // write callbacks, if set.
  while (!ack_buffer_.empty() &&
         ack_buffer_.front()->message->message_id() <= message_id) {
    const BufferEntry& ack_entry = *ack_buffer_.front();
    current_buffer_size_bytes_ -= ack_entry.message->GetCachedSize();
    DCHECK_GE(current_buffer_size_bytes_, 0);
    VLOG(3) << "Buffer size: " << current_buffer_size_bytes_
            << " (max=" << current_buffer_size_bytes_ << ")";

    if (!ack_entry.callback.is_null()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(ack_entry.callback, net::OK));
    }

    ack_buffer_.pop_front();
  }

  // An empty buffer should have a zero-byte footprint.
  DCHECK(current_buffer_size_bytes_ > 0 ||
         (ack_buffer_.empty() && write_buffer_.empty()))
      << "Expected zero-length buffer size, was " << current_buffer_size_bytes_
      << " bytes instead.";
}

BlimpMessageOutputBuffer::BufferEntry::BufferEntry(
    std::unique_ptr<BlimpMessage> message,
    net::CompletionCallback callback)
    : message(std::move(message)), callback(callback) {}

BlimpMessageOutputBuffer::BufferEntry::~BufferEntry() {}

void BlimpMessageOutputBuffer::WriteNextMessageIfReady() {
  DVLOG(3) << "WriteNextMessageIfReady";
  if (write_buffer_.empty()) {
    DVLOG(3) << "Nothing to write.";
    return;
  }

  std::unique_ptr<BlimpMessage> message_to_write(
      new BlimpMessage(*write_buffer_.front()->message));
  DVLOG(3) << "Writing message (id="
           << write_buffer_.front()->message->message_id() << ", "
           << *message_to_write << ")";

  output_processor_->ProcessMessage(std::move(message_to_write),
                                    write_complete_cb_.callback());
  DVLOG(3) << "Queue size: " << write_buffer_.size();
}

void BlimpMessageOutputBuffer::OnWriteComplete(int result) {
  DCHECK_LE(result, 0);

  VLOG(2) << "Write result=" << net::ErrorToString(result);
  if (result == net::OK) {
    ack_buffer_.push_back(std::move(write_buffer_.front()));
    write_buffer_.pop_front();
    WriteNextMessageIfReady();
  }
}

}  // namespace blimp

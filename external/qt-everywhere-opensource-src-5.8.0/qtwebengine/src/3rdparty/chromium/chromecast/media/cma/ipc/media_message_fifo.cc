// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/ipc/media_message_fifo.h"

#include <utility>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/media/cma/base/cma_logging.h"
#include "chromecast/media/cma/ipc/media_memory_chunk.h"
#include "chromecast/media/cma/ipc/media_message.h"
#include "chromecast/media/cma/ipc/media_message_type.h"

namespace chromecast {
namespace media {

class MediaMessageFlag
    : public base::RefCountedThreadSafe<MediaMessageFlag> {
 public:
  // |offset| is the offset in the fifo of the media message.
  explicit MediaMessageFlag(size_t offset);

  bool IsValid() const;

  void Invalidate();

  size_t offset() const { return offset_; }

 private:
  friend class base::RefCountedThreadSafe<MediaMessageFlag>;
  virtual ~MediaMessageFlag();

  const size_t offset_;
  bool flag_;

  DISALLOW_COPY_AND_ASSIGN(MediaMessageFlag);
};

MediaMessageFlag::MediaMessageFlag(size_t offset)
  : offset_(offset),
    flag_(true) {
}

MediaMessageFlag::~MediaMessageFlag() {
}

bool MediaMessageFlag::IsValid() const {
  return flag_;
}

void MediaMessageFlag::Invalidate() {
  flag_ = false;
}

class FifoOwnedMemory : public MediaMemoryChunk {
 public:
  FifoOwnedMemory(void* data, size_t size,
                  const scoped_refptr<MediaMessageFlag>& flag,
                  const base::Closure& release_msg_cb);
  ~FifoOwnedMemory() override;

  // MediaMemoryChunk implementation.
  void* data() const override { return data_; }
  size_t size() const override { return size_; }
  bool valid() const override { return flag_->IsValid(); }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::Closure release_msg_cb_;

  void* const data_;
  const size_t size_;
  scoped_refptr<MediaMessageFlag> flag_;

  DISALLOW_COPY_AND_ASSIGN(FifoOwnedMemory);
};

FifoOwnedMemory::FifoOwnedMemory(void* data,
                                 size_t size,
                                 const scoped_refptr<MediaMessageFlag>& flag,
                                 const base::Closure& release_msg_cb)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      release_msg_cb_(release_msg_cb),
      data_(data),
      size_(size),
      flag_(flag) {
}

FifoOwnedMemory::~FifoOwnedMemory() {
  // Release the flag before notifying that the message has been released.
  flag_ = scoped_refptr<MediaMessageFlag>();
  if (!release_msg_cb_.is_null()) {
    if (task_runner_->BelongsToCurrentThread()) {
      release_msg_cb_.Run();
    } else {
      task_runner_->PostTask(FROM_HERE, release_msg_cb_);
    }
  }
}

MediaMessageFifo::MediaMessageFifo(std::unique_ptr<MediaMemoryChunk> mem,
                                   bool init)
    : mem_(std::move(mem)), weak_factory_(this) {
  CHECK_EQ(reinterpret_cast<uintptr_t>(mem_->data()) % ALIGNOF(Descriptor),
           0u);
  CHECK_GE(mem_->size(), sizeof(Descriptor));
  Descriptor* desc = static_cast<Descriptor*>(mem_->data());
  base_ = static_cast<void*>(&desc->first_item);

  // TODO(damienv): remove cast when atomic size_t is defined in Chrome.
  // Currently, the sign differs.
  rd_offset_ = reinterpret_cast<AtomicSize*>(&(desc->rd_offset));
  wr_offset_ = reinterpret_cast<AtomicSize*>(&(desc->wr_offset));

  size_t max_size = mem_->size() -
      (static_cast<char*>(base_) - static_cast<char*>(mem_->data()));
  if (init) {
    size_ = max_size;
    desc->size = size_;
    internal_rd_offset_ = 0;
    internal_wr_offset_ = 0;
    base::subtle::Release_Store(rd_offset_, 0);
    base::subtle::Release_Store(wr_offset_, 0);
  } else {
    size_ = desc->size;
    CHECK_LE(size_, max_size);
    internal_rd_offset_ = current_rd_offset();
    internal_wr_offset_ = current_wr_offset();
  }
  CMALOG(kLogControl)
      << "MediaMessageFifo:" << " init=" << init << " size=" << size_;
  CHECK_GT(size_, 0u) << size_;

  weak_this_ = weak_factory_.GetWeakPtr();
  thread_checker_.DetachFromThread();
}

MediaMessageFifo::~MediaMessageFifo() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MediaMessageFifo::ObserveReadActivity(
    const base::Closure& read_event_cb) {
  read_event_cb_ = read_event_cb;
}

void MediaMessageFifo::ObserveWriteActivity(
    const base::Closure& write_event_cb) {
  write_event_cb_ = write_event_cb;
}

std::unique_ptr<MediaMemoryChunk> MediaMessageFifo::ReserveMemory(
    size_t size_to_reserve) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Capture first both the read and write offsets.
  // and exit right away if not enough free space.
  size_t wr_offset = internal_wr_offset();
  size_t rd_offset = current_rd_offset();
  size_t allocated_size = (size_ + wr_offset - rd_offset) % size_;
  size_t free_size = size_ - 1 - allocated_size;
  if (free_size < size_to_reserve)
    return std::unique_ptr<MediaMemoryChunk>();
  CHECK_LE(MediaMessage::minimum_msg_size(), size_to_reserve);

  // Note: in the next 2 conditions, we have:
  // trailing_byte_count < size_to_reserve
  // and since at this stage: size_to_reserve <= free_size
  // we also have trailing_byte_count <= free_size
  // which means that all the trailing bytes are free space in the fifo.
  size_t trailing_byte_count = size_ - wr_offset;
  if (trailing_byte_count < MediaMessage::minimum_msg_size()) {
    // If there is no space to even write the smallest message,
    // skip the trailing bytes and come back to the beginning of the fifo.
    // (no way to insert a padding message).
    if (free_size < trailing_byte_count)
      return std::unique_ptr<MediaMemoryChunk>();
    wr_offset = 0;
    CommitInternalWrite(wr_offset);

  } else if (trailing_byte_count < size_to_reserve) {
    // At this point, we know we have at least the space to write a message.
    // However, to avoid splitting a message, a padding message is needed.
    std::unique_ptr<MediaMemoryChunk> mem(
        ReserveMemoryNoCheck(trailing_byte_count));
    std::unique_ptr<MediaMessage> padding_message(
        MediaMessage::CreateMessage(PaddingMediaMsg, std::move(mem)));
  }

  // Recalculate the free size and exit if not enough free space.
  wr_offset = internal_wr_offset();
  allocated_size = (size_ + wr_offset - rd_offset) % size_;
  free_size = size_ - 1 - allocated_size;
  if (free_size < size_to_reserve)
    return std::unique_ptr<MediaMemoryChunk>();

  return ReserveMemoryNoCheck(size_to_reserve);
}

std::unique_ptr<MediaMessage> MediaMessageFifo::Pop() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Capture the read and write offsets.
  size_t rd_offset = internal_rd_offset();
  size_t wr_offset = current_wr_offset();
  size_t allocated_size = (size_ + wr_offset - rd_offset) % size_;

  if (allocated_size < MediaMessage::minimum_msg_size())
    return std::unique_ptr<MediaMessage>();

  size_t trailing_byte_count = size_ - rd_offset;
  if (trailing_byte_count < MediaMessage::minimum_msg_size()) {
    // If there is no space to even have the smallest message,
    // skip the trailing bytes and come back to the beginning of the fifo.
    // Note: all the trailing bytes correspond to allocated bytes since:
    // trailing_byte_count < MediaMessage::minimum_msg_size() <= allocated_size
    rd_offset = 0;
    allocated_size -= trailing_byte_count;
    trailing_byte_count = size_;
    CommitInternalRead(rd_offset);
  }

  // The message should not be longer than the allocated size
  // but since a message is a contiguous area of memory, it should also be
  // smaller than |trailing_byte_count|.
  size_t max_msg_size = std::min(allocated_size, trailing_byte_count);
  if (max_msg_size < MediaMessage::minimum_msg_size())
    return std::unique_ptr<MediaMessage>();
  void* msg_src = static_cast<uint8_t*>(base_) + rd_offset;

  // Create a flag to protect the serialized structure of the message
  // from being overwritten.
  // The serialized structure starts at offset |rd_offset|.
  scoped_refptr<MediaMessageFlag> rd_flag(new MediaMessageFlag(rd_offset));
  rd_flags_.push_back(rd_flag);
  std::unique_ptr<MediaMemoryChunk> mem(new FifoOwnedMemory(
      msg_src, max_msg_size, rd_flag,
      base::Bind(&MediaMessageFifo::OnRdMemoryReleased, weak_this_)));

  // Create the message which wraps its the serialized structure.
  std::unique_ptr<MediaMessage> message(
      MediaMessage::MapMessage(std::move(mem)));
  CHECK(message);

  // Update the internal read pointer.
  rd_offset = (rd_offset + message->size()) % size_;
  CommitInternalRead(rd_offset);

  return message;
}

void MediaMessageFifo::Flush() {
  DCHECK(thread_checker_.CalledOnValidThread());

  size_t wr_offset = current_wr_offset();

  // Invalidate every memory region before flushing.
  while (!rd_flags_.empty()) {
    CMALOG(kLogControl) << "Invalidate flag";
    rd_flags_.front()->Invalidate();
    rd_flags_.pop_front();
  }

  // Flush by setting the read pointer to the value of the write pointer.
  // Update first the internal read pointer then the public one.
  CommitInternalRead(wr_offset);
  CommitRead(wr_offset);
}

std::unique_ptr<MediaMemoryChunk> MediaMessageFifo::ReserveMemoryNoCheck(
    size_t size_to_reserve) {
  size_t wr_offset = internal_wr_offset();

  // Memory block corresponding to the serialized structure of the message.
  void* msg_start = static_cast<uint8_t*>(base_) + wr_offset;
  scoped_refptr<MediaMessageFlag> wr_flag(new MediaMessageFlag(wr_offset));
  wr_flags_.push_back(wr_flag);
  std::unique_ptr<MediaMemoryChunk> mem(new FifoOwnedMemory(
      msg_start, size_to_reserve, wr_flag,
      base::Bind(&MediaMessageFifo::OnWrMemoryReleased, weak_this_)));

  // Update the internal write pointer.
  wr_offset = (wr_offset + size_to_reserve) % size_;
  CommitInternalWrite(wr_offset);

  return mem;
}

void MediaMessageFifo::OnWrMemoryReleased() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (wr_flags_.empty()) {
    // Sanity check: when there is no protected memory area,
    // the external write offset has no reason to be different from
    // the internal write offset.
    DCHECK_EQ(current_wr_offset(), internal_wr_offset());
    return;
  }

  // Update the external write offset.
  while (!wr_flags_.empty() &&
         (!wr_flags_.front()->IsValid() || wr_flags_.front()->HasOneRef())) {
    // TODO(damienv): Could add a sanity check to make sure the offset is
    // between the external write offset and the read offset (not included).
    wr_flags_.pop_front();
  }

  // Update the read offset to the first locked memory area
  // or to the internal read pointer if nothing prevents it.
  size_t external_wr_offset = internal_wr_offset();
  if (!wr_flags_.empty())
    external_wr_offset = wr_flags_.front()->offset();
  CommitWrite(external_wr_offset);
}

void MediaMessageFifo::OnRdMemoryReleased() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (rd_flags_.empty()) {
    // Sanity check: when there is no protected memory area,
    // the external read offset has no reason to be different from
    // the internal read offset.
    DCHECK_EQ(current_rd_offset(), internal_rd_offset());
    return;
  }

  // Update the external read offset.
  while (!rd_flags_.empty() &&
         (!rd_flags_.front()->IsValid() || rd_flags_.front()->HasOneRef())) {
    // TODO(damienv): Could add a sanity check to make sure the offset is
    // between the external read offset and the write offset.
    rd_flags_.pop_front();
  }

  // Update the read offset to the first locked memory area
  // or to the internal read pointer if nothing prevents it.
  size_t external_rd_offset = internal_rd_offset();
  if (!rd_flags_.empty())
    external_rd_offset = rd_flags_.front()->offset();
  CommitRead(external_rd_offset);
}

size_t MediaMessageFifo::current_rd_offset() const {
  DCHECK_EQ(sizeof(size_t), sizeof(AtomicSize));
  size_t rd_offset = base::subtle::Acquire_Load(rd_offset_);
  CHECK_LT(rd_offset, size_);
  return rd_offset;
}

size_t MediaMessageFifo::current_wr_offset() const {
  DCHECK_EQ(sizeof(size_t), sizeof(AtomicSize));

  // When the fifo consumer acquires the write offset,
  // we have to make sure that any possible following reads are actually
  // returning results at least inline with the memory snapshot taken
  // when the write offset was sampled.
  // That's why an Acquire_Load is used here.
  size_t wr_offset = base::subtle::Acquire_Load(wr_offset_);
  CHECK_LT(wr_offset, size_);
  return wr_offset;
}

void MediaMessageFifo::CommitRead(size_t new_rd_offset) {
  // Add a memory fence to ensure the message content is completely read
  // before updating the read offset.
  base::subtle::Release_Store(rd_offset_, new_rd_offset);

  // Since rd_offset_ is updated by a release_store above, any thread that
  // does acquire_load is guaranteed to see the new rd_offset_ set above.
  // So it is safe to send the notification.
  if (!read_event_cb_.is_null()) {
    read_event_cb_.Run();
  }
}

void MediaMessageFifo::CommitWrite(size_t new_wr_offset) {
  // Add a memory fence to ensure the message content is written
  // before updating the write offset.
  base::subtle::Release_Store(wr_offset_, new_wr_offset);

  // Since wr_offset_ is updated by a release_store above, any thread that
  // does acquire_load is guaranteed to see the new wr_offset_ set above.
  // So it is safe to send the notification.
  if (!write_event_cb_.is_null()) {
    write_event_cb_.Run();
  }
}

void MediaMessageFifo::CommitInternalRead(size_t new_rd_offset) {
  internal_rd_offset_ = new_rd_offset;
}

void MediaMessageFifo::CommitInternalWrite(size_t new_wr_offset) {
  internal_wr_offset_ = new_wr_offset;
}

}  // namespace media
}  // namespace chromecast

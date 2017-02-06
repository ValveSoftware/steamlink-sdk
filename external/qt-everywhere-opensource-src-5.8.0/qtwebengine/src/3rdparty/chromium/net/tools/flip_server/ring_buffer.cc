// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/ring_buffer.h"
#include "base/logging.h"

namespace net {

RingBuffer::RingBuffer(int buffer_size)
    : buffer_(new char[buffer_size]),
      buffer_size_(buffer_size),
      bytes_used_(0),
      read_idx_(0),
      write_idx_(0) {}

RingBuffer::~RingBuffer() {}

int RingBuffer::ReadableBytes() const { return bytes_used_; }

int RingBuffer::BufferSize() const { return buffer_size_; }

int RingBuffer::BytesFree() const { return BufferSize() - ReadableBytes(); }

bool RingBuffer::Empty() const { return ReadableBytes() == 0; }

bool RingBuffer::Full() const { return ReadableBytes() == BufferSize(); }

// Returns the number of characters written.
// Appends up-to-'size' bytes to the ringbuffer.
int RingBuffer::Write(const char* bytes, int size) {
  CHECK_GE(size, 0);
#if 1
  char* wptr;
  int wsize;
  GetWritablePtr(&wptr, &wsize);
  int bytes_remaining = size;
  int bytes_written = 0;

  while (wsize && bytes_remaining) {
    if (wsize > bytes_remaining) {
      wsize = bytes_remaining;
    }
    memcpy(wptr, bytes + bytes_written, wsize);
    bytes_written += wsize;
    bytes_remaining -= wsize;
    AdvanceWritablePtr(wsize);
    GetWritablePtr(&wptr, &wsize);
  }
  return bytes_written;
#else
  const char* p = bytes;

  int bytes_to_write = size;
  int bytes_available = BytesFree();
  if (bytes_available < bytes_to_write) {
    bytes_to_write = bytes_available;
  }
  const char* end = bytes + bytes_to_write;

  while (p != end) {
    this->buffer_[this->write_idx_] = *p;
    ++p;
    ++this->write_idx_;
    if (this->write_idx_ >= this->buffer_size_) {
      this->write_idx_ = 0;
    }
  }
  bytes_used_ += bytes_to_write;
  return bytes_to_write;
#endif
}

// Sets *ptr to the beginning of writable memory, and sets *size to the size
// available for writing using this pointer.
void RingBuffer::GetWritablePtr(char** ptr, int* size) const {
  *ptr = buffer_.get() + write_idx_;

  if (bytes_used_ == buffer_size_) {
    *size = 0;
  } else if (read_idx_ > write_idx_) {
    *size = read_idx_ - write_idx_;
  } else {
    *size = buffer_size_ - write_idx_;
  }
}

// Sets *ptr to the beginning of readable memory, and sets *size to the size
// available for reading using this pointer.
void RingBuffer::GetReadablePtr(char** ptr, int* size) const {
  *ptr = buffer_.get() + read_idx_;

  if (bytes_used_ == 0) {
    *size = 0;
  } else if (write_idx_ > read_idx_) {
    *size = write_idx_ - read_idx_;
  } else {
    *size = buffer_size_ - read_idx_;
  }
}

// returns the number of bytes read into
int RingBuffer::Read(char* bytes, int size) {
  CHECK_GE(size, 0);
#if 1
  char* rptr;
  int rsize;
  GetReadablePtr(&rptr, &rsize);
  int bytes_remaining = size;
  int bytes_read = 0;

  while (rsize && bytes_remaining) {
    if (rsize > bytes_remaining) {
      rsize = bytes_remaining;
    }
    memcpy(bytes + bytes_read, rptr, rsize);
    bytes_read += rsize;
    bytes_remaining -= rsize;
    AdvanceReadablePtr(rsize);
    GetReadablePtr(&rptr, &rsize);
  }
  return bytes_read;
#else
  char* p = bytes;
  int bytes_to_read = size;
  int bytes_used = ReadableBytes();
  if (bytes_used < bytes_to_read) {
    bytes_to_read = bytes_used;
  }
  char* end = bytes + bytes_to_read;

  while (p != end) {
    *p = this->buffer_[this->read_idx_];
    ++p;
    ++this->read_idx_;
    if (this->read_idx_ >= this->buffer_size_) {
      this->read_idx_ = 0;
    }
  }
  this->bytes_used_ -= bytes_to_read;
  return bytes_to_read;
#endif
}

void RingBuffer::Clear() {
  bytes_used_ = 0;
  write_idx_ = 0;
  read_idx_ = 0;
}

bool RingBuffer::Reserve(int size) {
  DCHECK_GT(size, 0);
  char* write_ptr = NULL;
  int write_size = 0;
  GetWritablePtr(&write_ptr, &write_size);

  if (write_size < size) {
    char* read_ptr = NULL;
    int read_size = 0;
    GetReadablePtr(&read_ptr, &read_size);
    if (size <= BytesFree()) {
      // The fact that the total Free size is big enough but writable size is
      // not means that the writeable region is broken into two pieces: only
      // possible if the read_idx < write_idx. If write_idx < read_idx, then
      // the writeable region must be contiguous: [write_idx, read_idx). There
      // is no work to be done for the latter.
      DCHECK_LE(read_idx_, write_idx_);
      DCHECK_EQ(read_size, ReadableBytes());
      if (read_idx_ < write_idx_) {
        // Writeable area fragmented, consolidate it.
        memmove(buffer_.get(), read_ptr, read_size);
        read_idx_ = 0;
        write_idx_ = read_size;
      } else if (read_idx_ == write_idx_) {
        // No unconsumed data in the buffer, simply reset the indexes.
        DCHECK_EQ(ReadableBytes(), 0);
        read_idx_ = 0;
        write_idx_ = 0;
      }
    } else {
      Resize(ReadableBytes() + size);
    }
  }
  DCHECK_LE(size, buffer_size_ - write_idx_);
  return true;
}

void RingBuffer::AdvanceReadablePtr(int amount_to_consume) {
  CHECK_GE(amount_to_consume, 0);
  if (amount_to_consume >= bytes_used_) {
    Clear();
    return;
  }
  read_idx_ += amount_to_consume;
  read_idx_ %= buffer_size_;
  bytes_used_ -= amount_to_consume;
}

void RingBuffer::AdvanceWritablePtr(int amount_to_produce) {
  CHECK_GE(amount_to_produce, 0);
  CHECK_LE(amount_to_produce, BytesFree());
  write_idx_ += amount_to_produce;
  write_idx_ %= buffer_size_;
  bytes_used_ += amount_to_produce;
}

void RingBuffer::Resize(int buffer_size) {
  CHECK_GE(buffer_size, 0);
  if (buffer_size == buffer_size_)
    return;

  char* new_buffer = new char[buffer_size];
  if (buffer_size < bytes_used_) {
    // consume the oldest data.
    AdvanceReadablePtr(bytes_used_ - buffer_size);
  }

  int bytes_written = 0;
  int bytes_used = bytes_used_;
  while (true) {
    int size;
    char* ptr;
    GetReadablePtr(&ptr, &size);
    if (size == 0)
      break;
    if (size > buffer_size) {
      size = buffer_size;
    }
    memcpy(new_buffer + bytes_written, ptr, size);
    bytes_written += size;
    AdvanceReadablePtr(size);
  }
  buffer_.reset(new_buffer);

  buffer_size_ = buffer_size;
  bytes_used_ = bytes_used;
  read_idx_ = 0;
  write_idx_ = bytes_used_ % buffer_size_;
}

}  // namespace net

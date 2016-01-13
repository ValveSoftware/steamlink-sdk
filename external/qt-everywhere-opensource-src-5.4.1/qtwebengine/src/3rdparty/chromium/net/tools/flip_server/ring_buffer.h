// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_RING_BUFFER_H__
#define NET_TOOLS_FLIP_SERVER_RING_BUFFER_H__

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/tools/balsa/buffer_interface.h"

namespace net {

// The ring buffer is a circular buffer, that is, reads or writes may wrap
// around the end of the linear memory contained by the class (and back to
// the beginning). This is a good choice when you want to use a fixed amount
// of buffering and don't want to be moving memory around a lot.
//
// What is the penalty for using this over a normal, linear buffer?
// Reading all the data may take two operations, and
// writing all the data may take two operations.
//
// In the proxy, this class is used as a fixed size buffer between
// clients and servers (so that the memory size is constrained).

class RingBuffer : public BufferInterface {
 public:
  explicit RingBuffer(int buffer_size);
  virtual ~RingBuffer();

  // Resize the buffer to the size specified here.  If the buffer_size passed
  // in here is smaller than the amount of data in the buffer, then the oldest
  // data will be dropped, but all other data will be saved.
  // This means: If the buffer size is increasing, all data  that was resident
  // in the buffer prior to this call will be resident after this call.
  void Resize(int buffer_size);

  // The following functions all override pure virtual functions
  // in BufferInterface. See buffer_interface.h for a description
  // of what they do if the function isn't documented here.
  virtual int ReadableBytes() const OVERRIDE;
  virtual int BufferSize() const OVERRIDE;
  virtual int BytesFree() const OVERRIDE;

  virtual bool Empty() const OVERRIDE;
  virtual bool Full() const OVERRIDE;

  // returns the number of characters written.
  // appends up-to-'size' bytes to the ringbuffer.
  virtual int Write(const char* bytes, int size) OVERRIDE;

  // Stores a pointer into the ring buffer in *ptr,  and stores the number of
  // characters which are allowed to be written in *size.
  // If there are no writable bytes available, then *size will contain 0.
  virtual void GetWritablePtr(char** ptr, int* size) const OVERRIDE;

  // Stores a pointer into the ring buffer in *ptr,  and stores the number of
  // characters which are allowed to be read in *size.
  // If there are no readable bytes available, then *size will contain 0.
  virtual void GetReadablePtr(char** ptr, int* size) const OVERRIDE;

  // Returns the number of bytes read into 'bytes'.
  virtual int Read(char* bytes, int size) OVERRIDE;

  // Removes all data from the ring buffer.
  virtual void Clear() OVERRIDE;

  // Reserves contiguous writable empty space in the buffer of size bytes.
  // Since the point of this class is to have a fixed size buffer, be careful
  // not to inadvertently resize the buffer using Reserve(). If the reserve
  // size is <= BytesFree(), it is guaranteed that the buffer size will not
  // change.
  // This can be an expensive operation, it may new a buffer copy all existing
  // data and delete the old data. Even if the existing buffer does not need
  // to be resized, unread data may still need to be non-destructively copied
  // to consolidate fragmented free space. If the size requested is less than
  // or equal to BytesFree(), it is guaranteed that the buffer size will not
  // change.
  virtual bool Reserve(int size) OVERRIDE;

  // Removes the oldest 'amount_to_advance' characters.
  // If amount_to_consume > ReadableBytes(), this performs a Clear() instead.
  virtual void AdvanceReadablePtr(int amount_to_advance) OVERRIDE;

  // Moves the internal pointers around such that the  amount of data specified
  // here is expected to already be resident (as if it was Written).
  virtual void AdvanceWritablePtr(int amount_to_advance) OVERRIDE;

 protected:
  int read_idx() const { return read_idx_; }
  int write_idx() const { return write_idx_; }
  int bytes_used() const { return bytes_used_; }
  int buffer_size() const { return buffer_size_; }
  const char* buffer() const { return buffer_.get(); }

  int set_read_idx(int idx) { return read_idx_ = idx; }
  int set_write_idx(int idx) { return write_idx_ = idx; }

 private:
  scoped_ptr<char[]> buffer_;
  int buffer_size_;
  int bytes_used_;
  int read_idx_;
  int write_idx_;

  RingBuffer(const RingBuffer&);
  void operator=(const RingBuffer&);
};

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_RING_BUFFER_H__

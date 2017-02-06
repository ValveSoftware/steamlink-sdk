// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_IPC_MEDIA_MESSAGE_FIFO_H_
#define CHROMECAST_MEDIA_CMA_IPC_MEDIA_MESSAGE_FIFO_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>

#include "base/atomicops.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"

namespace chromecast {
namespace media {
class MediaMemoryChunk;
class MediaMessage;
class MediaMessageFlag;

// MediaMessageFifo is a lock free fifo implementation
// to pass audio/video data from one thread to another or from one process
// to another one (in that case using shared memory).
//
// Assuming the feeder and the consumer have a common block of shared memory
// (representing the serialized structure of the fifo),
// the feeder (which must be running on a single thread) instantiates its own
// instance of MediaMessageFifo, same applies to the consumer.
//
// Example: assuming the block of shared memory is given by |mem|, a typical
// feeder (using MediaMessageFifo instance fifo_feeder) will push messages
// in the following way:
//   // Create a dummy message to calculate the size of the serialized message.
//   std::unique_ptr<MediaMessage> dummy_msg(
//     MediaMessage::CreateDummyMessage(msg_type));
//   // ...
//   // Write all the fields to the dummy message.
//   // ...
//
//   // Create the real message, once the size of the serialized message
//   // is known.
//   std::unique_ptr<MediaMessage> msg(
//     MediaMessage::CreateMessage(
//       msg_type,
//       base::Bind(&MediaMessageFifo::ReserveMemory,
//                  base::Unretained(fifo_feeder.get())),
//       dummy_msg->content_size()));
//   if (!msg) {
//     // Not enough space for the message:
//     // retry later (e.g. when receiving a read activity event, meaning
//     // some enough space might have been release).
//     return;
//   }
//   // ...
//   // Write all the fields to the real message
//   // in exactly the same way it was done for the dummy message.
//   // ...
//   // Once message |msg| is going out of scope, then MediaMessageFifo
//   // fifo_feeder is informed that the message is not needed anymore
//   // (i.e. it was fully written), and fifo_feeder can then update
//   // the external write pointer of the fifo so that the consumer
//   // can start consuming this message.
//
// A typical consumer (using MediaMessageFifo instance fifo_consumer)
// will retrive messages in the following way:
//   std::unique_ptr<MediaMessage> msg(fifo_consumer->Pop());
//   if (!msg) {
//     // The fifo is empty, i.e. no message left.
//     // Try reading again later (e.g. after receiving a write activity event.
//     return;
//   }
//   // Parse the message using Read functions of MediaMessage:
//   // ...
//   // Once the message is going out of scope, MediaMessageFifo will receive
//   // a notification that the underlying memory can be released
//   // (i.e. the external read pointer can be updated).
//
//
class MediaMessageFifo {
 private:
  struct Descriptor {
    size_t size;
    size_t rd_offset;
    size_t wr_offset;

    // Ensure the first item has the same alignment as an int64_t.
    int64_t first_item;
  };

 public:
  static const int kDescriptorSize = sizeof(Descriptor);

  // Creates a media message fifo using |mem| as the underlying serialized
  // structure.
  // If |init| is true, the underlying fifo structure is initialized.
  MediaMessageFifo(std::unique_ptr<MediaMemoryChunk> mem, bool init);
  ~MediaMessageFifo();

  // When the consumer and the feeder are living in two different processes,
  // we might want to convey some messages between these two processes to notify
  // about some fifo activity.
  void ObserveReadActivity(const base::Closure& read_event_cb);
  void ObserveWriteActivity(const base::Closure& write_event_cb);

  // Reserves a writeable block of memory at the back of the fifo,
  // corresponding to the serialized structure of the message.
  // Returns NULL if the required size cannot be allocated.
  std::unique_ptr<MediaMemoryChunk> ReserveMemory(size_t size);

  // Pop a message from the queue.
  // Returns a null pointer if there is no message left.
  std::unique_ptr<MediaMessage> Pop();

  // Flush the fifo.
  void Flush();

 private:
  // Add some accessors to ensure security on the browser process side.
  size_t current_rd_offset() const;
  size_t current_wr_offset() const;
  size_t internal_rd_offset() const {
    DCHECK_LT(internal_rd_offset_, size_);
    return internal_rd_offset_;
  }
  size_t internal_wr_offset() const {
    DCHECK_LT(internal_wr_offset_, size_);
    return internal_wr_offset_;
  }

  // Reserve a block of free memory without doing any check on the available
  // space. Invoke this function only when all the checks have been done.
  std::unique_ptr<MediaMemoryChunk> ReserveMemoryNoCheck(size_t size);

  // Invoked each time there is a memory region in the free space of the fifo
  // that has possibly been written.
  void OnWrMemoryReleased();

  // Invoked each time there is a memory region in the allocated space
  // of the fifo that has possibly been released.
  void OnRdMemoryReleased();

  // Functions to modify the internal/external read/write pointers.
  void CommitRead(size_t new_rd_offset);
  void CommitWrite(size_t new_wr_offset);
  void CommitInternalRead(size_t new_rd_offset);
  void CommitInternalWrite(size_t new_wr_offset);

  // An instance of MediaMessageFifo must be running on a single thread.
  // If the fifo feeder and consumer are living on 2 different threads
  // or 2 different processes, they must create their own instance
  // of MediaMessageFifo using the same underlying block of (shared) memory
  // in the constructor.
  base::ThreadChecker thread_checker_;

  // Callbacks invoked to notify either of some read or write activity on the
  // fifo. This is especially useful when the feeder and consumer are living in
  // two different processes.
  base::Closure read_event_cb_;
  base::Closure write_event_cb_;

  // The serialized structure of the fifo.
  std::unique_ptr<MediaMemoryChunk> mem_;

  // The size in bytes of the fifo is cached locally for security purpose.
  // (the renderer process cannot modify the size and make the browser process
  // access out of range addresses).
  size_t size_;

  // TODO(damienv): This is a work-around since atomicops.h does not define
  // an atomic size_t type.
#if SIZE_MAX == UINT32_MAX
  typedef base::subtle::Atomic32 AtomicSize;
#elif SIZE_MAX == UINT64_MAX
  typedef base::subtle::Atomic64 AtomicSize;
#else
#error "Unsupported size_t"
#endif
  AtomicSize* rd_offset_;
  AtomicSize* wr_offset_;

  // Internal read offset: this is where data is actually read from.
  // The external offset |rd_offset_| is only used to protect data from being
  // overwritten by the feeder.
  // At any time, the internal read pointer must be between the external read
  // offset and the write offset (circular fifo definition of "between").
  size_t internal_rd_offset_;
  size_t internal_wr_offset_;

  // Note: all the memory read/write are followed by a memory fence before
  // updating the rd/wr pointer.
  void* base_;

  // Protects the messages that are either being read or written.
  std::list<scoped_refptr<MediaMessageFlag> > rd_flags_;
  std::list<scoped_refptr<MediaMessageFlag> > wr_flags_;

  base::WeakPtr<MediaMessageFifo> weak_this_;
  base::WeakPtrFactory<MediaMessageFifo> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaMessageFifo);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_IPC_MEDIA_MESSAGE_FIFO_H_

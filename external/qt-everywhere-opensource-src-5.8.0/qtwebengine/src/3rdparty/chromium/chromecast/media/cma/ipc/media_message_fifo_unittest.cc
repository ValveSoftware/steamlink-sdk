// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/ipc/media_message_fifo.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chromecast/media/cma/ipc/media_memory_chunk.h"
#include "chromecast/media/cma/ipc/media_message.h"
#include "chromecast/media/cma/ipc/media_message_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {

class FifoMemoryChunk : public MediaMemoryChunk {
 public:
  FifoMemoryChunk(void* mem, size_t size)
      : mem_(mem), size_(size) {}
  ~FifoMemoryChunk() override {}

  void* data() const override { return mem_; }
  size_t size() const override { return size_; }
  bool valid() const override { return true; }

 private:
  void* mem_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(FifoMemoryChunk);
};

void MsgProducer(std::unique_ptr<MediaMessageFifo> fifo,
                 int msg_count,
                 base::WaitableEvent* event) {
  for (int k = 0; k < msg_count; k++) {
    uint32_t msg_type = 0x2 + (k % 5);
    uint32_t max_msg_content_size = k % 64;
    do {
      std::unique_ptr<MediaMessage> msg1(MediaMessage::CreateMessage(
          msg_type, base::Bind(&MediaMessageFifo::ReserveMemory,
                               base::Unretained(fifo.get())),
          max_msg_content_size));
      if (msg1)
        break;
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));
    } while (true);
  }

  fifo.reset();

  event->Signal();
}

void MsgConsumer(std::unique_ptr<MediaMessageFifo> fifo,
                 int msg_count,
                 base::WaitableEvent* event) {
  int k = 0;
  while (k < msg_count) {
    uint32_t msg_type = 0x2 + (k % 5);
    do {
      std::unique_ptr<MediaMessage> msg2(fifo->Pop());
      if (msg2) {
        if (msg2->type() != PaddingMediaMsg) {
          EXPECT_EQ(msg2->type(), msg_type);
          k++;
        }
        break;
      }
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));
    } while (true);
  }

  fifo.reset();

  event->Signal();
}

void MsgProducerConsumer(std::unique_ptr<MediaMessageFifo> producer_fifo,
                         std::unique_ptr<MediaMessageFifo> consumer_fifo,
                         base::WaitableEvent* event) {
  for (int k = 0; k < 2048; k++) {
    // Should have enough space to create a message.
    uint32_t msg_type = 0x2 + (k % 5);
    uint32_t max_msg_content_size = k % 64;
    std::unique_ptr<MediaMessage> msg1(MediaMessage::CreateMessage(
        msg_type, base::Bind(&MediaMessageFifo::ReserveMemory,
                             base::Unretained(producer_fifo.get())),
        max_msg_content_size));
    EXPECT_TRUE(msg1);

    // Make sure the message is commited.
    msg1.reset();

    // At this point, we should have a message to read.
    std::unique_ptr<MediaMessage> msg2(consumer_fifo->Pop());
    EXPECT_TRUE(msg2);
  }

  producer_fifo.reset();
  consumer_fifo.reset();

  event->Signal();
}

}  // namespace

TEST(MediaMessageFifoTest, AlternateWriteRead) {
  size_t buffer_size = 64 * 1024;
  std::unique_ptr<uint64_t[]> buffer(
      new uint64_t[buffer_size / sizeof(uint64_t)]);

  std::unique_ptr<base::Thread> thread(
      new base::Thread("FeederConsumerThread"));
  thread->Start();

  std::unique_ptr<MediaMessageFifo> producer_fifo(
      new MediaMessageFifo(std::unique_ptr<MediaMemoryChunk>(
                               new FifoMemoryChunk(&buffer[0], buffer_size)),
                           true));
  std::unique_ptr<MediaMessageFifo> consumer_fifo(
      new MediaMessageFifo(std::unique_ptr<MediaMemoryChunk>(
                               new FifoMemoryChunk(&buffer[0], buffer_size)),
                           false));

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread->task_runner()->PostTask(
      FROM_HERE, base::Bind(&MsgProducerConsumer, base::Passed(&producer_fifo),
                            base::Passed(&consumer_fifo), &event));
  event.Wait();

  thread.reset();
}

TEST(MediaMessageFifoTest, MultiThreaded) {
  size_t buffer_size = 64 * 1024;
  std::unique_ptr<uint64_t[]> buffer(
      new uint64_t[buffer_size / sizeof(uint64_t)]);

  std::unique_ptr<base::Thread> producer_thread(
      new base::Thread("FeederThread"));
  std::unique_ptr<base::Thread> consumer_thread(
      new base::Thread("ConsumerThread"));
  producer_thread->Start();
  consumer_thread->Start();

  std::unique_ptr<MediaMessageFifo> producer_fifo(
      new MediaMessageFifo(std::unique_ptr<MediaMemoryChunk>(
                               new FifoMemoryChunk(&buffer[0], buffer_size)),
                           true));
  std::unique_ptr<MediaMessageFifo> consumer_fifo(
      new MediaMessageFifo(std::unique_ptr<MediaMemoryChunk>(
                               new FifoMemoryChunk(&buffer[0], buffer_size)),
                           false));

  base::WaitableEvent producer_event_done(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent consumer_event_done(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  const int msg_count = 2048;
  producer_thread->task_runner()->PostTask(
      FROM_HERE, base::Bind(&MsgProducer, base::Passed(&producer_fifo),
                            msg_count, &producer_event_done));
  consumer_thread->task_runner()->PostTask(
      FROM_HERE, base::Bind(&MsgConsumer, base::Passed(&consumer_fifo),
                            msg_count, &consumer_event_done));

  producer_event_done.Wait();
  consumer_event_done.Wait();

  producer_thread.reset();
  consumer_thread.reset();
}

}  // namespace media
}  // namespace chromecast


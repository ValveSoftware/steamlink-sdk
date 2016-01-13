// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_write_queue.h"

#include <cstddef>
#include <cstring>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/net_log.h"
#include "net/base/request_priority.h"
#include "net/spdy/spdy_buffer_producer.h"
#include "net/spdy/spdy_stream.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

using std::string;

const char kOriginal[] = "original";
const char kRequeued[] = "requeued";

class SpdyWriteQueueTest : public ::testing::Test {};

// Makes a SpdyFrameProducer producing a frame with the data in the
// given string.
scoped_ptr<SpdyBufferProducer> StringToProducer(const std::string& s) {
  scoped_ptr<char[]> data(new char[s.size()]);
  std::memcpy(data.get(), s.data(), s.size());
  return scoped_ptr<SpdyBufferProducer>(
      new SimpleBufferProducer(
          scoped_ptr<SpdyBuffer>(
              new SpdyBuffer(
                  scoped_ptr<SpdyFrame>(
                      new SpdyFrame(data.release(), s.size(), true))))));
}

// Makes a SpdyBufferProducer producing a frame with the data in the
// given int (converted to a string).
scoped_ptr<SpdyBufferProducer> IntToProducer(int i) {
  return StringToProducer(base::IntToString(i));
}

// Producer whose produced buffer will enqueue yet another buffer into the
// SpdyWriteQueue upon destruction.
class RequeingBufferProducer : public SpdyBufferProducer {
 public:
  RequeingBufferProducer(SpdyWriteQueue* queue) {
    buffer_.reset(new SpdyBuffer(kOriginal, arraysize(kOriginal)));
    buffer_->AddConsumeCallback(
        base::Bind(RequeingBufferProducer::ConsumeCallback, queue));
  }

  virtual scoped_ptr<SpdyBuffer> ProduceBuffer() OVERRIDE {
    return buffer_.Pass();
  }

  static void ConsumeCallback(SpdyWriteQueue* queue,
                              size_t size,
                              SpdyBuffer::ConsumeSource source) {
    scoped_ptr<SpdyBufferProducer> producer(
        new SimpleBufferProducer(scoped_ptr<SpdyBuffer>(
            new SpdyBuffer(kRequeued, arraysize(kRequeued)))));

    queue->Enqueue(
        MEDIUM, RST_STREAM, producer.Pass(), base::WeakPtr<SpdyStream>());
  }

 private:
  scoped_ptr<SpdyBuffer> buffer_;
};

// Produces a frame with the given producer and returns a copy of its
// data as a string.
std::string ProducerToString(scoped_ptr<SpdyBufferProducer> producer) {
  scoped_ptr<SpdyBuffer> buffer = producer->ProduceBuffer();
  return std::string(buffer->GetRemainingData(), buffer->GetRemainingSize());
}

// Produces a frame with the given producer and returns a copy of its
// data as an int (converted from a string).
int ProducerToInt(scoped_ptr<SpdyBufferProducer> producer) {
  int i = 0;
  EXPECT_TRUE(base::StringToInt(ProducerToString(producer.Pass()), &i));
  return i;
}

// Makes a SpdyStream with the given priority and a NULL SpdySession
// -- be careful to not call any functions that expect the session to
// be there.
SpdyStream* MakeTestStream(RequestPriority priority) {
  return new SpdyStream(
      SPDY_BIDIRECTIONAL_STREAM, base::WeakPtr<SpdySession>(),
      GURL(), priority, 0, 0, BoundNetLog());
}

// Add some frame producers of different priority. The producers
// should be dequeued in priority order with their associated stream.
TEST_F(SpdyWriteQueueTest, DequeuesByPriority) {
  SpdyWriteQueue write_queue;

  scoped_ptr<SpdyBufferProducer> producer_low = StringToProducer("LOW");
  scoped_ptr<SpdyBufferProducer> producer_medium = StringToProducer("MEDIUM");
  scoped_ptr<SpdyBufferProducer> producer_highest = StringToProducer("HIGHEST");

  scoped_ptr<SpdyStream> stream_medium(MakeTestStream(MEDIUM));
  scoped_ptr<SpdyStream> stream_highest(MakeTestStream(HIGHEST));

  // A NULL stream should still work.
  write_queue.Enqueue(
      LOW, SYN_STREAM, producer_low.Pass(), base::WeakPtr<SpdyStream>());
  write_queue.Enqueue(
      MEDIUM, SYN_REPLY, producer_medium.Pass(), stream_medium->GetWeakPtr());
  write_queue.Enqueue(
      HIGHEST, RST_STREAM, producer_highest.Pass(),
      stream_highest->GetWeakPtr());

  SpdyFrameType frame_type = DATA;
  scoped_ptr<SpdyBufferProducer> frame_producer;
  base::WeakPtr<SpdyStream> stream;
  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
  EXPECT_EQ(RST_STREAM, frame_type);
  EXPECT_EQ("HIGHEST", ProducerToString(frame_producer.Pass()));
  EXPECT_EQ(stream_highest, stream.get());

  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
  EXPECT_EQ(SYN_REPLY, frame_type);
  EXPECT_EQ("MEDIUM", ProducerToString(frame_producer.Pass()));
  EXPECT_EQ(stream_medium, stream.get());

  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
  EXPECT_EQ(SYN_STREAM, frame_type);
  EXPECT_EQ("LOW", ProducerToString(frame_producer.Pass()));
  EXPECT_EQ(NULL, stream.get());

  EXPECT_FALSE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
}

// Add some frame producers with the same priority. The producers
// should be dequeued in FIFO order with their associated stream.
TEST_F(SpdyWriteQueueTest, DequeuesFIFO) {
  SpdyWriteQueue write_queue;

  scoped_ptr<SpdyBufferProducer> producer1 = IntToProducer(1);
  scoped_ptr<SpdyBufferProducer> producer2 = IntToProducer(2);
  scoped_ptr<SpdyBufferProducer> producer3 = IntToProducer(3);

  scoped_ptr<SpdyStream> stream1(MakeTestStream(DEFAULT_PRIORITY));
  scoped_ptr<SpdyStream> stream2(MakeTestStream(DEFAULT_PRIORITY));
  scoped_ptr<SpdyStream> stream3(MakeTestStream(DEFAULT_PRIORITY));

  write_queue.Enqueue(DEFAULT_PRIORITY, SYN_STREAM, producer1.Pass(),
                      stream1->GetWeakPtr());
  write_queue.Enqueue(DEFAULT_PRIORITY, SYN_REPLY, producer2.Pass(),
                      stream2->GetWeakPtr());
  write_queue.Enqueue(DEFAULT_PRIORITY, RST_STREAM, producer3.Pass(),
                      stream3->GetWeakPtr());

  SpdyFrameType frame_type = DATA;
  scoped_ptr<SpdyBufferProducer> frame_producer;
  base::WeakPtr<SpdyStream> stream;
  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
  EXPECT_EQ(SYN_STREAM, frame_type);
  EXPECT_EQ(1, ProducerToInt(frame_producer.Pass()));
  EXPECT_EQ(stream1, stream.get());

  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
  EXPECT_EQ(SYN_REPLY, frame_type);
  EXPECT_EQ(2, ProducerToInt(frame_producer.Pass()));
  EXPECT_EQ(stream2, stream.get());

  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
  EXPECT_EQ(RST_STREAM, frame_type);
  EXPECT_EQ(3, ProducerToInt(frame_producer.Pass()));
  EXPECT_EQ(stream3, stream.get());

  EXPECT_FALSE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
}

// Enqueue a bunch of writes and then call
// RemovePendingWritesForStream() on one of the streams. No dequeued
// write should be for that stream.
TEST_F(SpdyWriteQueueTest, RemovePendingWritesForStream) {
  SpdyWriteQueue write_queue;

  scoped_ptr<SpdyStream> stream1(MakeTestStream(DEFAULT_PRIORITY));
  scoped_ptr<SpdyStream> stream2(MakeTestStream(DEFAULT_PRIORITY));

  for (int i = 0; i < 100; ++i) {
    base::WeakPtr<SpdyStream> stream =
        (((i % 3) == 0) ? stream1 : stream2)->GetWeakPtr();
    write_queue.Enqueue(DEFAULT_PRIORITY, SYN_STREAM, IntToProducer(i), stream);
  }

  write_queue.RemovePendingWritesForStream(stream2->GetWeakPtr());

  for (int i = 0; i < 100; i += 3) {
    SpdyFrameType frame_type = DATA;
    scoped_ptr<SpdyBufferProducer> frame_producer;
    base::WeakPtr<SpdyStream> stream;
    ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
    EXPECT_EQ(SYN_STREAM, frame_type);
    EXPECT_EQ(i, ProducerToInt(frame_producer.Pass()));
    EXPECT_EQ(stream1, stream.get());
  }

  SpdyFrameType frame_type = DATA;
  scoped_ptr<SpdyBufferProducer> frame_producer;
  base::WeakPtr<SpdyStream> stream;
  EXPECT_FALSE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
}

// Enqueue a bunch of writes and then call
// RemovePendingWritesForStreamsAfter(). No dequeued write should be for
// those streams without a stream id, or with a stream_id after that
// argument.
TEST_F(SpdyWriteQueueTest, RemovePendingWritesForStreamsAfter) {
  SpdyWriteQueue write_queue;

  scoped_ptr<SpdyStream> stream1(MakeTestStream(DEFAULT_PRIORITY));
  stream1->set_stream_id(1);
  scoped_ptr<SpdyStream> stream2(MakeTestStream(DEFAULT_PRIORITY));
  stream2->set_stream_id(3);
  scoped_ptr<SpdyStream> stream3(MakeTestStream(DEFAULT_PRIORITY));
  stream3->set_stream_id(5);
  // No stream id assigned.
  scoped_ptr<SpdyStream> stream4(MakeTestStream(DEFAULT_PRIORITY));
  base::WeakPtr<SpdyStream> streams[] = {
    stream1->GetWeakPtr(), stream2->GetWeakPtr(),
    stream3->GetWeakPtr(), stream4->GetWeakPtr()
  };

  for (int i = 0; i < 100; ++i) {
    write_queue.Enqueue(DEFAULT_PRIORITY, SYN_STREAM, IntToProducer(i),
                        streams[i % arraysize(streams)]);
  }

  write_queue.RemovePendingWritesForStreamsAfter(stream1->stream_id());

  for (int i = 0; i < 100; i += arraysize(streams)) {
    SpdyFrameType frame_type = DATA;
    scoped_ptr<SpdyBufferProducer> frame_producer;
    base::WeakPtr<SpdyStream> stream;
    ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream))
        << "Unable to Dequeue i: " << i;
    EXPECT_EQ(SYN_STREAM, frame_type);
    EXPECT_EQ(i, ProducerToInt(frame_producer.Pass()));
    EXPECT_EQ(stream1, stream.get());
  }

  SpdyFrameType frame_type = DATA;
  scoped_ptr<SpdyBufferProducer> frame_producer;
  base::WeakPtr<SpdyStream> stream;
  EXPECT_FALSE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
}

// Enqueue a bunch of writes and then call Clear(). The write queue
// should clean up the memory properly, and Dequeue() should return
// false.
TEST_F(SpdyWriteQueueTest, Clear) {
  SpdyWriteQueue write_queue;

  for (int i = 0; i < 100; ++i) {
    write_queue.Enqueue(DEFAULT_PRIORITY, SYN_STREAM, IntToProducer(i),
                        base::WeakPtr<SpdyStream>());
  }

  write_queue.Clear();

  SpdyFrameType frame_type = DATA;
  scoped_ptr<SpdyBufferProducer> frame_producer;
  base::WeakPtr<SpdyStream> stream;
  EXPECT_FALSE(write_queue.Dequeue(&frame_type, &frame_producer, &stream));
}

TEST_F(SpdyWriteQueueTest, RequeingProducerWithoutReentrance) {
  SpdyWriteQueue queue;
  queue.Enqueue(
      DEFAULT_PRIORITY,
      SYN_STREAM,
      scoped_ptr<SpdyBufferProducer>(new RequeingBufferProducer(&queue)),
      base::WeakPtr<SpdyStream>());
  {
    SpdyFrameType frame_type;
    scoped_ptr<SpdyBufferProducer> producer;
    base::WeakPtr<SpdyStream> stream;

    EXPECT_TRUE(queue.Dequeue(&frame_type, &producer, &stream));
    EXPECT_TRUE(queue.IsEmpty());
    EXPECT_EQ(string(kOriginal), producer->ProduceBuffer()->GetRemainingData());
  }
  // |producer| was destroyed, and a buffer is re-queued.
  EXPECT_FALSE(queue.IsEmpty());

  SpdyFrameType frame_type;
  scoped_ptr<SpdyBufferProducer> producer;
  base::WeakPtr<SpdyStream> stream;

  EXPECT_TRUE(queue.Dequeue(&frame_type, &producer, &stream));
  EXPECT_EQ(string(kRequeued), producer->ProduceBuffer()->GetRemainingData());
}

TEST_F(SpdyWriteQueueTest, ReentranceOnClear) {
  SpdyWriteQueue queue;
  queue.Enqueue(
      DEFAULT_PRIORITY,
      SYN_STREAM,
      scoped_ptr<SpdyBufferProducer>(new RequeingBufferProducer(&queue)),
      base::WeakPtr<SpdyStream>());

  queue.Clear();
  EXPECT_FALSE(queue.IsEmpty());

  SpdyFrameType frame_type;
  scoped_ptr<SpdyBufferProducer> producer;
  base::WeakPtr<SpdyStream> stream;

  EXPECT_TRUE(queue.Dequeue(&frame_type, &producer, &stream));
  EXPECT_EQ(string(kRequeued), producer->ProduceBuffer()->GetRemainingData());
}

TEST_F(SpdyWriteQueueTest, ReentranceOnRemovePendingWritesAfter) {
  scoped_ptr<SpdyStream> stream(MakeTestStream(DEFAULT_PRIORITY));
  stream->set_stream_id(2);

  SpdyWriteQueue queue;
  queue.Enqueue(
      DEFAULT_PRIORITY,
      SYN_STREAM,
      scoped_ptr<SpdyBufferProducer>(new RequeingBufferProducer(&queue)),
      stream->GetWeakPtr());

  queue.RemovePendingWritesForStreamsAfter(1);
  EXPECT_FALSE(queue.IsEmpty());

  SpdyFrameType frame_type;
  scoped_ptr<SpdyBufferProducer> producer;
  base::WeakPtr<SpdyStream> weak_stream;

  EXPECT_TRUE(queue.Dequeue(&frame_type, &producer, &weak_stream));
  EXPECT_EQ(string(kRequeued), producer->ProduceBuffer()->GetRemainingData());
}

TEST_F(SpdyWriteQueueTest, ReentranceOnRemovePendingWritesForStream) {
  scoped_ptr<SpdyStream> stream(MakeTestStream(DEFAULT_PRIORITY));
  stream->set_stream_id(2);

  SpdyWriteQueue queue;
  queue.Enqueue(
      DEFAULT_PRIORITY,
      SYN_STREAM,
      scoped_ptr<SpdyBufferProducer>(new RequeingBufferProducer(&queue)),
      stream->GetWeakPtr());

  queue.RemovePendingWritesForStream(stream->GetWeakPtr());
  EXPECT_FALSE(queue.IsEmpty());

  SpdyFrameType frame_type;
  scoped_ptr<SpdyBufferProducer> producer;
  base::WeakPtr<SpdyStream> weak_stream;

  EXPECT_TRUE(queue.Dequeue(&frame_type, &producer, &weak_stream));
  EXPECT_EQ(string(kRequeued), producer->ProduceBuffer()->GetRemainingData());
}

}  // namespace

}  // namespace net

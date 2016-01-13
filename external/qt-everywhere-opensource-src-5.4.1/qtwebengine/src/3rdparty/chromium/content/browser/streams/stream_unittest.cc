// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_read_observer.h"
#include "content/browser/streams/stream_registry.h"
#include "content/browser/streams/stream_write_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class StreamTest : public testing::Test {
 public:
  StreamTest() : producing_seed_key_(0) {}

  virtual void SetUp() OVERRIDE {
    registry_.reset(new StreamRegistry());
  }

  // Create a new IO buffer of the given |buffer_size| and fill it with random
  // data.
  scoped_refptr<net::IOBuffer> NewIOBuffer(size_t buffer_size) {
    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(buffer_size));
    char *bufferp = buffer->data();
    for (size_t i = 0; i < buffer_size; i++)
      bufferp[i] = (i + producing_seed_key_) % (1 << sizeof(char));
    ++producing_seed_key_;
    return buffer;
  }

 protected:
  base::MessageLoop message_loop_;
  scoped_ptr<StreamRegistry> registry_;

 private:
  int producing_seed_key_;
};

class TestStreamReader : public StreamReadObserver {
 public:
  TestStreamReader() : buffer_(new net::GrowableIOBuffer()), completed_(false) {
  }
  virtual ~TestStreamReader() {}

  void Read(Stream* stream) {
    const size_t kBufferSize = 32768;
    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));

    int bytes_read = 0;
    while (true) {
      Stream::StreamState state =
          stream->ReadRawData(buffer.get(), kBufferSize, &bytes_read);
      switch (state) {
        case Stream::STREAM_HAS_DATA:
          // TODO(tyoshino): Move these expectations to the beginning of Read()
          // method once Stream::Finalize() is fixed.
          EXPECT_FALSE(completed_);
          break;
        case Stream::STREAM_COMPLETE:
          completed_ = true;
          return;
        case Stream::STREAM_EMPTY:
          EXPECT_FALSE(completed_);
          return;
        case Stream::STREAM_ABORTED:
          EXPECT_FALSE(completed_);
          return;
      }
      size_t old_capacity = buffer_->capacity();
      buffer_->SetCapacity(old_capacity + bytes_read);
      memcpy(buffer_->StartOfBuffer() + old_capacity,
             buffer->data(), bytes_read);
    }
  }

  virtual void OnDataAvailable(Stream* stream) OVERRIDE {
    Read(stream);
  }

  scoped_refptr<net::GrowableIOBuffer> buffer() { return buffer_; }

  bool completed() const {
    return completed_;
  }

 private:
  scoped_refptr<net::GrowableIOBuffer> buffer_;
  bool completed_;
};

class TestStreamWriter : public StreamWriteObserver {
 public:
  TestStreamWriter() {}
  virtual ~TestStreamWriter() {}

  void Write(Stream* stream,
             scoped_refptr<net::IOBuffer> buffer,
             size_t buffer_size) {
    stream->AddData(buffer, buffer_size);
  }

  virtual void OnSpaceAvailable(Stream* stream) OVERRIDE {
  }

  virtual void OnClose(Stream* stream) OVERRIDE {
  }
};

TEST_F(StreamTest, SetReadObserver) {
  TestStreamReader reader;
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), &writer, url));
  EXPECT_TRUE(stream->SetReadObserver(&reader));
}

TEST_F(StreamTest, SetReadObserver_SecondFails) {
  TestStreamReader reader1;
  TestStreamReader reader2;
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), &writer, url));
  EXPECT_TRUE(stream->SetReadObserver(&reader1));
  EXPECT_FALSE(stream->SetReadObserver(&reader2));
}

TEST_F(StreamTest, SetReadObserver_TwoReaders) {
  TestStreamReader reader1;
  TestStreamReader reader2;
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), &writer, url));
  EXPECT_TRUE(stream->SetReadObserver(&reader1));

  // Once the first read observer is removed, a new one can be added.
  stream->RemoveReadObserver(&reader1);
  EXPECT_TRUE(stream->SetReadObserver(&reader2));
}

TEST_F(StreamTest, Stream) {
  TestStreamReader reader;
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), &writer, url));
  EXPECT_TRUE(stream->SetReadObserver(&reader));

  const int kBufferSize = 1000000;
  scoped_refptr<net::IOBuffer> buffer(NewIOBuffer(kBufferSize));
  writer.Write(stream.get(), buffer, kBufferSize);
  stream->Finalize();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(reader.completed());

  ASSERT_EQ(reader.buffer()->capacity(), kBufferSize);
  for (int i = 0; i < kBufferSize; i++)
    EXPECT_EQ(buffer->data()[i], reader.buffer()->data()[i]);
}

// Test that even if a reader receives an empty buffer, once TransferData()
// method is called on it with |source_complete| = true, following Read() calls
// on it never returns STREAM_EMPTY. Together with StreamTest.Stream above, this
// guarantees that Reader::Read() call returns only STREAM_HAS_DATA
// or STREAM_COMPLETE in |data_available_callback_| call corresponding to
// Writer::Close().
TEST_F(StreamTest, ClosedReaderDoesNotReturnStreamEmpty) {
  TestStreamReader reader;
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), &writer, url));
  EXPECT_TRUE(stream->SetReadObserver(&reader));

  const int kBufferSize = 0;
  scoped_refptr<net::IOBuffer> buffer(NewIOBuffer(kBufferSize));
  stream->AddData(buffer, kBufferSize);
  stream->Finalize();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(reader.completed());
  EXPECT_EQ(0, reader.buffer()->capacity());
}

TEST_F(StreamTest, GetStream) {
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream1(
      new Stream(registry_.get(), &writer, url));

  scoped_refptr<Stream> stream2 = registry_->GetStream(url);
  ASSERT_EQ(stream1, stream2);
}

TEST_F(StreamTest, GetStream_Missing) {
  TestStreamWriter writer;

  GURL url1("blob://stream");
  scoped_refptr<Stream> stream1(
      new Stream(registry_.get(), &writer, url1));

  GURL url2("blob://stream2");
  scoped_refptr<Stream> stream2 = registry_->GetStream(url2);
  ASSERT_FALSE(stream2.get());
}

TEST_F(StreamTest, CloneStream) {
  TestStreamWriter writer;

  GURL url1("blob://stream");
  scoped_refptr<Stream> stream1(
      new Stream(registry_.get(), &writer, url1));

  GURL url2("blob://stream2");
  ASSERT_TRUE(registry_->CloneStream(url2, url1));
  scoped_refptr<Stream> stream2 = registry_->GetStream(url2);
  ASSERT_EQ(stream1, stream2);
}

TEST_F(StreamTest, CloneStream_Missing) {
  TestStreamWriter writer;

  GURL url1("blob://stream");
  scoped_refptr<Stream> stream1(
      new Stream(registry_.get(), &writer, url1));

  GURL url2("blob://stream2");
  GURL url3("blob://stream3");
  ASSERT_FALSE(registry_->CloneStream(url2, url3));
  scoped_refptr<Stream> stream2 = registry_->GetStream(url2);
  ASSERT_FALSE(stream2.get());
}

TEST_F(StreamTest, UnregisterStream) {
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream1(
      new Stream(registry_.get(), &writer, url));

  registry_->UnregisterStream(url);
  scoped_refptr<Stream> stream2 = registry_->GetStream(url);
  ASSERT_FALSE(stream2.get());
}

TEST_F(StreamTest, MemoryExceedMemoryUsageLimit) {
  TestStreamWriter writer1;
  TestStreamWriter writer2;

  GURL url1("blob://stream");
  scoped_refptr<Stream> stream1(
      new Stream(registry_.get(), &writer1, url1));

  GURL url2("blob://stream2");
  scoped_refptr<Stream> stream2(
      new Stream(registry_.get(), &writer2, url2));

  const int kMaxMemoryUsage = 1500000;
  registry_->set_max_memory_usage_for_testing(kMaxMemoryUsage);

  const int kBufferSize = 1000000;
  scoped_refptr<net::IOBuffer> buffer(NewIOBuffer(kBufferSize));
  writer1.Write(stream1.get(), buffer, kBufferSize);
  // Make transfer happen.
  base::MessageLoop::current()->RunUntilIdle();

  writer2.Write(stream2.get(), buffer, kBufferSize);

  // Written data (1000000 * 2) exceeded limit (1500000). |stream2| should be
  // unregistered with |registry_|.
  EXPECT_EQ(NULL, registry_->GetStream(url2).get());

  writer1.Write(stream1.get(), buffer, kMaxMemoryUsage - kBufferSize);
  // Should be accepted since stream2 is unregistered and the new data is not
  // so big to exceed the limit.
  EXPECT_FALSE(registry_->GetStream(url1).get() == NULL);
}

TEST_F(StreamTest, UnderMemoryUsageLimit) {
  TestStreamWriter writer;
  TestStreamReader reader;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(new Stream(registry_.get(), &writer, url));
  EXPECT_TRUE(stream->SetReadObserver(&reader));

  registry_->set_max_memory_usage_for_testing(1500000);

  const int kBufferSize = 1000000;
  scoped_refptr<net::IOBuffer> buffer(NewIOBuffer(kBufferSize));
  writer.Write(stream.get(), buffer, kBufferSize);

  // Run loop to make |reader| consume the data.
  base::MessageLoop::current()->RunUntilIdle();

  writer.Write(stream.get(), buffer, kBufferSize);

  EXPECT_EQ(stream.get(), registry_->GetStream(url).get());
}

}  // namespace content

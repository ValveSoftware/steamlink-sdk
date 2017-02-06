// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/media/cma/ipc/media_memory_chunk.h"
#include "chromecast/media/cma/ipc/media_message_fifo.h"
#include "chromecast/media/cma/ipc_streamer/av_streamer_proxy.h"
#include "chromecast/media/cma/ipc_streamer/coded_frame_provider_host.h"
#include "chromecast/media/cma/test/frame_generator_for_test.h"
#include "chromecast/media/cma/test/mock_frame_consumer.h"
#include "chromecast/media/cma/test/mock_frame_provider.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
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

}  // namespace

class AvStreamerTest : public testing::Test {
 public:
  AvStreamerTest();
  ~AvStreamerTest() override;

  // Setups the test.
  void Configure(size_t frame_count,
                 const std::vector<bool>& provider_delayed_pattern,
                 const std::vector<bool>& consumer_delayed_pattern,
                 bool delay_flush);

  // Starts the test.
  void Start();

  // Back to back flush
  void FlushThenStop();

  // Timeout indicates test failure
  void OnTestTimeout();

 protected:
  std::unique_ptr<uint64_t[]> fifo_mem_;

  std::unique_ptr<AvStreamerProxy> av_buffer_proxy_;
  std::unique_ptr<CodedFrameProviderHost> coded_frame_provider_host_;
  std::unique_ptr<MockFrameConsumer> frame_consumer_;

  // number of pending cb in StopAndFlush
  int stop_and_flush_cb_count_;

 private:
  void OnTestCompleted();

  void OnFifoRead();
  void OnFifoWrite();

  void OnStopAndFlush();

  DISALLOW_COPY_AND_ASSIGN(AvStreamerTest);
};

AvStreamerTest::AvStreamerTest() {
}

AvStreamerTest::~AvStreamerTest() {
}

void AvStreamerTest::Configure(
    size_t frame_count,
    const std::vector<bool>& provider_delayed_pattern,
    const std::vector<bool>& consumer_delayed_pattern,
    bool delay_flush) {
  // Frame generation on the producer and consumer side.
  std::vector<FrameGeneratorForTest::FrameSpec> frame_specs;
  frame_specs.resize(frame_count);
  for (size_t k = 0; k < frame_specs.size() - 1; k++) {
    frame_specs[k].has_config = (k == 0);
    frame_specs[k].timestamp = base::TimeDelta::FromMilliseconds(40) * k;
    frame_specs[k].size = 512;
    frame_specs[k].has_decrypt_config = ((k % 3) == 0);
  }
  frame_specs[frame_specs.size() - 1].is_eos = true;

  std::unique_ptr<FrameGeneratorForTest> frame_generator_provider(
      new FrameGeneratorForTest(frame_specs));
  std::unique_ptr<FrameGeneratorForTest> frame_generator_consumer(
      new FrameGeneratorForTest(frame_specs));

  std::unique_ptr<MockFrameProvider> frame_provider(new MockFrameProvider());
  frame_provider->Configure(provider_delayed_pattern,
                            std::move(frame_generator_provider));
  frame_provider->SetDelayFlush(delay_flush);

  size_t fifo_size_div_8 = 512;
  fifo_mem_.reset(new uint64_t[fifo_size_div_8]);
  std::unique_ptr<MediaMessageFifo> producer_fifo(new MediaMessageFifo(
      std::unique_ptr<MediaMemoryChunk>(
          new FifoMemoryChunk(&fifo_mem_[0], fifo_size_div_8 * 8)),
      true));
  std::unique_ptr<MediaMessageFifo> consumer_fifo(new MediaMessageFifo(
      std::unique_ptr<MediaMemoryChunk>(
          new FifoMemoryChunk(&fifo_mem_[0], fifo_size_div_8 * 8)),
      false));
  producer_fifo->ObserveWriteActivity(
      base::Bind(&AvStreamerTest::OnFifoWrite, base::Unretained(this)));
  consumer_fifo->ObserveReadActivity(
      base::Bind(&AvStreamerTest::OnFifoRead, base::Unretained(this)));

  av_buffer_proxy_.reset(
      new AvStreamerProxy());
  av_buffer_proxy_->SetCodedFrameProvider(
      std::unique_ptr<CodedFrameProvider>(frame_provider.release()));
  av_buffer_proxy_->SetMediaMessageFifo(std::move(producer_fifo));

  coded_frame_provider_host_.reset(
      new CodedFrameProviderHost(std::move(consumer_fifo)));

  frame_consumer_.reset(
      new MockFrameConsumer(coded_frame_provider_host_.get()));
  frame_consumer_->Configure(consumer_delayed_pattern, false,
                             std::move(frame_generator_consumer));

  stop_and_flush_cb_count_ = 0;
}

void AvStreamerTest::Start() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&AvStreamerProxy::Start,
                            base::Unretained(av_buffer_proxy_.get())));

  frame_consumer_->Start(
      base::Bind(&AvStreamerTest::OnTestCompleted,
                 base::Unretained(this)));
}

void AvStreamerTest::FlushThenStop() {
  base::Closure cb =
      base::Bind(&AvStreamerTest::OnStopAndFlush, base::Unretained(this));

  stop_and_flush_cb_count_++;
  av_buffer_proxy_->StopAndFlush(cb);

  ASSERT_EQ(stop_and_flush_cb_count_, 1);

  stop_and_flush_cb_count_++;
  av_buffer_proxy_->StopAndFlush(cb);
}

void AvStreamerTest::OnTestTimeout() {
  ADD_FAILURE() << "Test timed out";
  if (base::MessageLoop::current())
    base::MessageLoop::current()->QuitWhenIdle();
}

void AvStreamerTest::OnTestCompleted() {
  base::MessageLoop::current()->QuitWhenIdle();
}

void AvStreamerTest::OnFifoWrite() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&CodedFrameProviderHost::OnFifoWriteEvent,
                 base::Unretained(coded_frame_provider_host_.get())));
}

void AvStreamerTest::OnFifoRead() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&AvStreamerProxy::OnFifoReadEvent,
                            base::Unretained(av_buffer_proxy_.get())));
}

void AvStreamerTest::OnStopAndFlush() {
  stop_and_flush_cb_count_--;
  if (stop_and_flush_cb_count_ == 0) {
    OnTestCompleted();
  }
}

TEST_F(AvStreamerTest, FastProviderSlowConsumer) {
  bool provider_delayed_pattern[] = { false };
  bool consumer_delayed_pattern[] = { true };

  const size_t frame_count = 100u;
  Configure(frame_count,
            std::vector<bool>(
                provider_delayed_pattern,
                provider_delayed_pattern + arraysize(provider_delayed_pattern)),
            std::vector<bool>(
                consumer_delayed_pattern,
                consumer_delayed_pattern + arraysize(consumer_delayed_pattern)),
            false);

  std::unique_ptr<base::MessageLoop> message_loop(new base::MessageLoop());
  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&AvStreamerTest::Start, base::Unretained(this)));
  message_loop->Run();
};

TEST_F(AvStreamerTest, SlowProviderFastConsumer) {
  bool provider_delayed_pattern[] = { true };
  bool consumer_delayed_pattern[] = { false };

  const size_t frame_count = 100u;
  Configure(frame_count,
            std::vector<bool>(
                provider_delayed_pattern,
                provider_delayed_pattern + arraysize(provider_delayed_pattern)),
            std::vector<bool>(
                consumer_delayed_pattern,
                consumer_delayed_pattern + arraysize(consumer_delayed_pattern)),
            false);

  std::unique_ptr<base::MessageLoop> message_loop(new base::MessageLoop());
  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&AvStreamerTest::Start, base::Unretained(this)));
  message_loop->Run();
};

TEST_F(AvStreamerTest, SlowFastProducerConsumer) {
  // Pattern lengths are prime between each other
  // so that a lot of combinations can be tested.
  bool provider_delayed_pattern[] = {
    true, true, true, true, true,
    false, false, false, false
  };
  bool consumer_delayed_pattern[] = {
    true, true, true, true, true, true, true,
    false, false, false, false, false, false, false
  };

  const size_t frame_count = 100u;
  Configure(frame_count,
            std::vector<bool>(
                provider_delayed_pattern,
                provider_delayed_pattern + arraysize(provider_delayed_pattern)),
            std::vector<bool>(
                consumer_delayed_pattern,
                consumer_delayed_pattern + arraysize(consumer_delayed_pattern)),
            false);

  std::unique_ptr<base::MessageLoop> message_loop(new base::MessageLoop());
  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&AvStreamerTest::Start, base::Unretained(this)));
  message_loop->Run();
};

// Test case for when AvStreamerProxy::StopAndFlush is invoked while a previous
// flush is pending. This can happen when pipeline is stopped while a seek/flush
// is pending.
TEST_F(AvStreamerTest, StopInFlush) {
  // We don't care about the delayed pattern. Setting to true to make sure the
  // test won't exit before flush is called.
  bool dummy_delayed_pattern[] = {true};
  std::vector<bool> dummy_delayed_pattern_vector(
      dummy_delayed_pattern,
      dummy_delayed_pattern + arraysize(dummy_delayed_pattern));
  const size_t frame_count = 100u;

  // Delay flush callback in frame provider
  Configure(frame_count, dummy_delayed_pattern_vector,
            dummy_delayed_pattern_vector, true);

  std::unique_ptr<base::MessageLoop> message_loop(new base::MessageLoop());

  // Flush takes 10ms to finish. 1s timeout is enough for this test.
  message_loop->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AvStreamerTest::OnTestTimeout, base::Unretained(this)),
      base::TimeDelta::FromSeconds(1));

  message_loop->PostTask(
      FROM_HERE, base::Bind(&AvStreamerTest::Start, base::Unretained(this)));

  // Let AvStreamerProxy run for a while. Fire flush and stop later
  message_loop->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AvStreamerTest::FlushThenStop, base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(10));

  message_loop->Run();
}

}  // namespace media
}  // namespace chromecast

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "content/renderer/media/audio_renderer_sink_cache_impl.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "media/base/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {
const char* const kDefaultDeviceId =
    media::AudioDeviceDescription::kDefaultDeviceId;
const char kAnotherDeviceId[] = "another-device-id";
const char kUnhealthyDeviceId[] = "i-am-sick";
const int kRenderFrameId = 124;
const int kDeleteTimeoutMs = 500;
}  // namespace

class AudioRendererSinkCacheTest : public testing::Test {
 public:
  AudioRendererSinkCacheTest()
      : cache_(new AudioRendererSinkCacheImpl(
            message_loop_.task_runner(),
            base::Bind(&AudioRendererSinkCacheTest::CreateSink,
                       base::Unretained(this)),
            base::TimeDelta::FromMilliseconds(kDeleteTimeoutMs))) {}

  void GetSink(int render_frame_id,
               const std::string& device_id,
               const url::Origin& security_origin,
               media::AudioRendererSink** sink) {
    *sink = cache_->GetSink(render_frame_id, device_id, security_origin).get();
  }

  void GetRandomSinkInfo(int frame) {
    // Get info and check if memory is not corrupted.
    EXPECT_EQ(kDefaultDeviceId,
              cache_->GetSinkInfo(frame, 0, kDefaultDeviceId, url::Origin())
                  .device_id());
  }

  void GetRandomSink(int frame, base::TimeDelta sleep_timeout) {
    scoped_refptr<media::AudioRendererSink> sink =
        cache_->GetSink(frame, kDefaultDeviceId, url::Origin()).get();
    ExpectToStop(sink.get());
    base::PlatformThread::Sleep(sleep_timeout);
    cache_->ReleaseSink(sink.get());
    sink->Stop();  // Call a method to make the object is not corrupted.
  }

 protected:
  int sink_count() {
    DCHECK(message_loop_.task_runner()->BelongsToCurrentThread());
    return cache_->GetCacheSizeForTesting();
  }

  scoped_refptr<media::AudioRendererSink> CreateSink(
      int render_frame_id,
      int session_id,
      const std::string& device_id,
      const url::Origin& security_origin) {
    return new media::MockAudioRendererSink(
        device_id, (device_id == kUnhealthyDeviceId)
                       ? media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL
                       : media::OUTPUT_DEVICE_STATUS_OK);
  }

  void ExpectToStop(media::AudioRendererSink* sink) {
    // Sink must be stoped before deletion.
    EXPECT_CALL(*static_cast<media::MockAudioRendererSink*>(sink), Stop())
        .Times(1);
  }

  void ExpectNotToStop(media::AudioRendererSink* sink) {
    // The sink must be stoped before deletion.
    EXPECT_CALL(*static_cast<media::MockAudioRendererSink*>(sink), Stop())
        .Times(0);
  }

  // Posts the task to the specified thread and runs current message loop until
  // the task is completed.
  void PostAndRunUntilDone(const base::Thread& thread,
                           const base::Closure& task) {
    media::WaitableMessageLoopEvent event;
    thread.task_runner()->PostTaskAndReply(FROM_HERE, task, event.GetClosure());
    // Runs the loop and waits for the thread to call event's closure.
    event.RunAndWait();
  }

  void WaitOnAnotherThread(const base::Thread& thread, int timeout_ms) {
    PostAndRunUntilDone(
        thread, base::Bind(base::IgnoreResult(&base::PlatformThread::Sleep),
                           base::TimeDelta::FromMilliseconds(timeout_ms)));
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<AudioRendererSinkCacheImpl> cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioRendererSinkCacheTest);
};

// Verify that normal get/release sink sequence works.
TEST_F(AudioRendererSinkCacheTest, GetReleaseSink) {
  // Verify that a new sink is successfully created.
  EXPECT_EQ(0, sink_count());
  scoped_refptr<media::AudioRendererSink> sink =
      cache_->GetSink(kRenderFrameId, kDefaultDeviceId, url::Origin()).get();
  ExpectNotToStop(sink.get());  // Cache should not stop sinks marked as used.
  EXPECT_EQ(kDefaultDeviceId, sink->GetOutputDeviceInfo().device_id());
  EXPECT_EQ(1, sink_count());

  // Verify that another sink with the same key is successfully created
  scoped_refptr<media::AudioRendererSink> another_sink =
      cache_->GetSink(kRenderFrameId, kDefaultDeviceId, url::Origin()).get();
  ExpectNotToStop(another_sink.get());
  EXPECT_EQ(kDefaultDeviceId, another_sink->GetOutputDeviceInfo().device_id());
  EXPECT_EQ(2, sink_count());
  EXPECT_NE(sink, another_sink);

  // Verify that another sink with a different kay is successfully created.
  scoped_refptr<media::AudioRendererSink> yet_another_sink =
      cache_->GetSink(kRenderFrameId, kAnotherDeviceId, url::Origin()).get();
  ExpectNotToStop(yet_another_sink.get());
  EXPECT_EQ(kAnotherDeviceId,
            yet_another_sink->GetOutputDeviceInfo().device_id());
  EXPECT_EQ(3, sink_count());
  EXPECT_NE(sink, yet_another_sink);
  EXPECT_NE(another_sink, yet_another_sink);

  // Verify that the first sink is successfully deleted.
  cache_->ReleaseSink(sink.get());
  EXPECT_EQ(2, sink_count());
  sink = nullptr;

  // Make sure we deleted the right sink, and the memory for the rest is not
  // corrupted.
  EXPECT_EQ(kDefaultDeviceId, another_sink->GetOutputDeviceInfo().device_id());
  EXPECT_EQ(kAnotherDeviceId,
            yet_another_sink->GetOutputDeviceInfo().device_id());

  // Verify that the second sink is successfully deleted.
  cache_->ReleaseSink(another_sink.get());
  EXPECT_EQ(1, sink_count());
  EXPECT_EQ(kAnotherDeviceId,
            yet_another_sink->GetOutputDeviceInfo().device_id());

  cache_->ReleaseSink(yet_another_sink.get());
  EXPECT_EQ(0, sink_count());
}

// Verify that the sink created with GetSinkInfo() is reused when possible.
TEST_F(AudioRendererSinkCacheTest, GetDeviceInfo) {
  EXPECT_EQ(0, sink_count());
  media::OutputDeviceInfo device_info =
      cache_->GetSinkInfo(kRenderFrameId, 0, kDefaultDeviceId, url::Origin());
  EXPECT_EQ(1, sink_count());

  // The info on the same device is requested, so no new sink is created.
  media::OutputDeviceInfo one_more_device_info =
      cache_->GetSinkInfo(kRenderFrameId, 0, kDefaultDeviceId, url::Origin());
  EXPECT_EQ(1, sink_count());
  EXPECT_EQ(device_info.device_id(), one_more_device_info.device_id());

  // Aquire the sink that was created on GetSinkInfo().
  scoped_refptr<media::AudioRendererSink> sink =
      cache_->GetSink(kRenderFrameId, kDefaultDeviceId, url::Origin()).get();
  EXPECT_EQ(1, sink_count());
  EXPECT_EQ(device_info.device_id(), sink->GetOutputDeviceInfo().device_id());

  // Now the sink is in used, but we can still get the device info out of it, no
  // new sink is created.
  one_more_device_info =
      cache_->GetSinkInfo(kRenderFrameId, 0, kDefaultDeviceId, url::Origin());
  EXPECT_EQ(1, sink_count());
  EXPECT_EQ(device_info.device_id(), one_more_device_info.device_id());

  // Request sink for the same device. The first sink is in use, so a new one
  // should be created.
  scoped_refptr<media::AudioRendererSink> another_sink =
      cache_->GetSink(kRenderFrameId, kDefaultDeviceId, url::Origin()).get();
  EXPECT_EQ(2, sink_count());
  EXPECT_EQ(device_info.device_id(),
            another_sink->GetOutputDeviceInfo().device_id());
}

// Verify that the sink created with GetSinkInfo() is deleted if unused.
// The test produces 2 "Uninteresting mock" warnings for
// MockAudioRendererSink::Stop().
TEST_F(AudioRendererSinkCacheTest, GarbageCollection) {
  EXPECT_EQ(0, sink_count());
  media::OutputDeviceInfo device_info =
      cache_->GetSinkInfo(kRenderFrameId, 0, kDefaultDeviceId, url::Origin());
  EXPECT_EQ(1, sink_count());

  media::OutputDeviceInfo another_device_info =
      cache_->GetSinkInfo(kRenderFrameId, 0, kAnotherDeviceId, url::Origin());
  EXPECT_EQ(2, sink_count());

  base::Thread thread("timeout_thread");
  thread.Start();

  // 100 ms more than garbage collection timeout.
  WaitOnAnotherThread(thread, kDeleteTimeoutMs + 100);

  // All the sinks should be garbage-collected by now.
  EXPECT_EQ(0, sink_count());
}

// Verify that the sink created with GetSinkInfo() is not deleted if used within
// the timeout.
TEST_F(AudioRendererSinkCacheTest, NoGarbageCollectionForUsedSink) {
  EXPECT_EQ(0, sink_count());
  media::OutputDeviceInfo device_info =
      cache_->GetSinkInfo(kRenderFrameId, 0, kDefaultDeviceId, url::Origin());
  EXPECT_EQ(1, sink_count());

  base::Thread thread("timeout_thread");
  thread.Start();

  // Wait significantly less than grabage collection timeout.
  int wait_a_bit = 100;
  DCHECK_GT(kDeleteTimeoutMs, wait_a_bit * 2);
  WaitOnAnotherThread(thread, wait_a_bit);

  // Sink is not deleted yet.
  EXPECT_EQ(1, sink_count());

  // Request it:
  scoped_refptr<media::AudioRendererSink> sink =
      cache_->GetSink(kRenderFrameId, kDefaultDeviceId, url::Origin()).get();
  EXPECT_EQ(kDefaultDeviceId, sink->GetOutputDeviceInfo().device_id());
  EXPECT_EQ(1, sink_count());

  // Wait more to hit garbage collection timeout.
  WaitOnAnotherThread(thread, kDeleteTimeoutMs);

  // The sink is still in place.
  EXPECT_EQ(1, sink_count());
}

// Verify that the sink created with GetSinkInfo() is not cached if it is
// unhealthy.
TEST_F(AudioRendererSinkCacheTest, UnhealthySinkIsNotCached) {
  EXPECT_EQ(0, sink_count());
  media::OutputDeviceInfo device_info =
      cache_->GetSinkInfo(kRenderFrameId, 0, kUnhealthyDeviceId, url::Origin());
  EXPECT_EQ(0, sink_count());
  scoped_refptr<media::AudioRendererSink> sink =
      cache_->GetSink(kRenderFrameId, kUnhealthyDeviceId, url::Origin()).get();
  EXPECT_EQ(0, sink_count());
}

// Verify that cache works fine if a sink scheduled for delettion is aquired and
// released before deletion timeout elapses.
// The test produces one "Uninteresting mock" warning for
// MockAudioRendererSink::Stop().
TEST_F(AudioRendererSinkCacheTest, ReleaseSinkBeforeScheduledDeletion) {
  EXPECT_EQ(0, sink_count());

  media::OutputDeviceInfo device_info =
      cache_->GetSinkInfo(kRenderFrameId, 0, kDefaultDeviceId, url::Origin());
  EXPECT_EQ(1, sink_count());  // This sink is scheduled for deletion now.

  // Request it:
  scoped_refptr<media::AudioRendererSink> sink =
      cache_->GetSink(kRenderFrameId, kDefaultDeviceId, url::Origin()).get();
  ExpectNotToStop(sink.get());
  EXPECT_EQ(1, sink_count());

  // Release it:
  cache_->ReleaseSink(sink.get());
  EXPECT_EQ(0, sink_count());

  media::OutputDeviceInfo another_device_info =
      cache_->GetSinkInfo(kRenderFrameId, 0, kAnotherDeviceId, url::Origin());
  EXPECT_EQ(1, sink_count());  // This sink is scheduled for deletion now.

  base::Thread thread("timeout_thread");
  thread.Start();

  // 100 ms more than garbage collection timeout.
  WaitOnAnotherThread(thread, kDeleteTimeoutMs + 100);

  // Nothing crashed and the second sink deleted on schedule.
  EXPECT_EQ(0, sink_count());
}

// Check that a sink created on one thread in response to GetSinkInfo can be
// used on another thread.
TEST_F(AudioRendererSinkCacheTest, MultithreadedAccess) {
  EXPECT_EQ(0, sink_count());

  base::Thread thread1("thread1");
  thread1.Start();

  base::Thread thread2("thread2");
  thread2.Start();

  // Request device information on the first thread.
  PostAndRunUntilDone(
      thread1,
      base::Bind(base::IgnoreResult(&AudioRendererSinkCacheImpl::GetSinkInfo),
                 base::Unretained(cache_.get()), kRenderFrameId, 0,
                 kDefaultDeviceId, url::Origin()));

  EXPECT_EQ(1, sink_count());

  // Request the sink on the second thread.
  media::AudioRendererSink* sink;

  PostAndRunUntilDone(
      thread2,
      base::Bind(&AudioRendererSinkCacheTest::GetSink, base::Unretained(this),
                 kRenderFrameId, kDefaultDeviceId, url::Origin(), &sink));

  EXPECT_EQ(kDefaultDeviceId, sink->GetOutputDeviceInfo().device_id());
  EXPECT_EQ(1, sink_count());

  // Request device information on the first thread again.
  PostAndRunUntilDone(
      thread1,
      base::Bind(base::IgnoreResult(&AudioRendererSinkCacheImpl::GetSinkInfo),
                 base::Unretained(cache_.get()), kRenderFrameId, 0,
                 kDefaultDeviceId, url::Origin()));
  EXPECT_EQ(1, sink_count());

  // Release the sink on the second thread.
  PostAndRunUntilDone(thread2,
                      base::Bind(&AudioRendererSinkCache::ReleaseSink,
                                 base::Unretained(cache_.get()), sink));

  EXPECT_EQ(0, sink_count());
}

// Intensive parallell access to the cache. Produces a ton of "Uninteresting
// mock" warnings for Stop() calls - this is fine.
TEST_F(AudioRendererSinkCacheTest, SmokeTest) {
  const int kExperimentSize = 1000;
  const int kSinkCount = 10;
  const int kThreadCount = 3;

  // Sleep no more than (kDeleteTimeoutMs * 3) in total per thread.
  const base::TimeDelta kSleepTimeout =
      base::TimeDelta::FromMilliseconds(kDeleteTimeoutMs * 3 / kExperimentSize);

  srand(42);  // Does not matter.

  std::array<std::unique_ptr<base::Thread>, kThreadCount> threads;
  for (int i = 0; i < kThreadCount; ++i) {
    threads[i].reset(new base::Thread(std::to_string(i)));
    threads[i]->Start();
  }

  for (int i = 0; i < kExperimentSize; ++i) {
    for (auto& thread : threads) {
      thread->task_runner()->PostTask(
          FROM_HERE, base::Bind(&AudioRendererSinkCacheTest::GetRandomSinkInfo,
                                base::Unretained(this), rand() % kSinkCount));
      thread->task_runner()->PostTask(
          FROM_HERE, base::Bind(&AudioRendererSinkCacheTest::GetRandomSink,
                                base::Unretained(this), rand() % kSinkCount,
                                kSleepTimeout));
    }
  }

  // Wait for completion of all the tasks posted to at least one thread.
  media::WaitableMessageLoopEvent loop_event(
      TestTimeouts::action_max_timeout());
  threads[kThreadCount - 1]->task_runner()->PostTaskAndReply(
      FROM_HERE, base::Bind(&base::DoNothing), loop_event.GetClosure());
  // Runs the loop and waits for the thread to call event's closure.
  loop_event.RunAndWait();
}

}  // namespace content

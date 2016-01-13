// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gamepad_seqlock.h"

#include <stdlib.h>

#include "base/atomic_ref_count.h"
#include "base/threading/platform_thread.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Basic test to make sure that basic operation works correctly.

struct TestData {
  unsigned a, b, c;
};

class BasicSeqLockTestThread : public PlatformThread::Delegate {
 public:
  BasicSeqLockTestThread() {}

  void Init(
      content::GamepadSeqLock* seqlock,
      TestData* data,
      base::subtle::Atomic32* ready) {
    seqlock_ = seqlock;
    data_ = data;
    ready_ = ready;
  }
  virtual void ThreadMain() {
    while (AtomicRefCountIsZero(ready_)) {
      PlatformThread::YieldCurrentThread();
    }

    for (unsigned i = 0; i < 1000; ++i) {
      TestData copy;
      base::subtle::Atomic32 version;
      do {
        version = seqlock_->ReadBegin();
        copy = *data_;
      } while (seqlock_->ReadRetry(version));

      EXPECT_EQ(copy.a + 100, copy.b);
      EXPECT_EQ(copy.c, copy.b + copy.a);
    }

    AtomicRefCountDec(ready_);
  }

 private:
  content::GamepadSeqLock* seqlock_;
  TestData* data_;
  base::AtomicRefCount* ready_;

  DISALLOW_COPY_AND_ASSIGN(BasicSeqLockTestThread);
};

TEST(GamepadSeqLockTest, ManyThreads) {
  content::GamepadSeqLock seqlock;
  TestData data = { 0, 0, 0 };
  base::AtomicRefCount ready = 0;

  ANNOTATE_BENIGN_RACE_SIZED(&data, sizeof(data), "Racey reads are discarded");

  static const unsigned kNumReaderThreads = 10;
  BasicSeqLockTestThread threads[kNumReaderThreads];
  PlatformThreadHandle handles[kNumReaderThreads];

  for (unsigned i = 0; i < kNumReaderThreads; ++i)
    threads[i].Init(&seqlock, &data, &ready);
  for (unsigned i = 0; i < kNumReaderThreads; ++i)
    ASSERT_TRUE(PlatformThread::Create(0, &threads[i], &handles[i]));

  // The main thread is the writer, and the spawned are readers.
  unsigned counter = 0;
  for (;;) {
    seqlock.WriteBegin();
    data.a = counter++;
    data.b = data.a + 100;
    data.c = data.b + data.a;
    seqlock.WriteEnd();

    if (counter == 1)
      base::AtomicRefCountIncN(&ready, kNumReaderThreads);

    if (AtomicRefCountIsZero(&ready))
      break;
  }

  for (unsigned i = 0; i < kNumReaderThreads; ++i)
    PlatformThread::Join(handles[i]);
}

}  // namespace base

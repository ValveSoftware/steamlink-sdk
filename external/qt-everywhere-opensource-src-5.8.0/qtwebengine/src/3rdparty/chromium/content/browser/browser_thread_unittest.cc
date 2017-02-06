// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/single_thread_task_runner.h"
#include "content/browser/browser_thread_impl.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace content {

class BrowserThreadTest : public testing::Test {
 public:
  void Release() const {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    loop_.task_runner()->PostTask(FROM_HERE,
                                  base::MessageLoop::QuitWhenIdleClosure());
  }

 protected:
  void SetUp() override {
    ui_thread_.reset(new BrowserThreadImpl(BrowserThread::UI));
    file_thread_.reset(new BrowserThreadImpl(BrowserThread::FILE));
    ui_thread_->Start();
    file_thread_->Start();
  }

  void TearDown() override {
    ui_thread_->Stop();
    file_thread_->Stop();
  }

  static void BasicFunction(base::MessageLoop* message_loop) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    message_loop->task_runner()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  }

  class DeletedOnFile
      : public base::RefCountedThreadSafe<
            DeletedOnFile, BrowserThread::DeleteOnFileThread> {
   public:
    explicit DeletedOnFile(base::MessageLoop* message_loop)
        : message_loop_(message_loop) {}

   private:
    friend struct BrowserThread::DeleteOnThread<BrowserThread::FILE>;
    friend class base::DeleteHelper<DeletedOnFile>;

    ~DeletedOnFile() {
      CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
      message_loop_->task_runner()->PostTask(
          FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
    }

    base::MessageLoop* message_loop_;
  };

 private:
  std::unique_ptr<BrowserThreadImpl> ui_thread_;
  std::unique_ptr<BrowserThreadImpl> file_thread_;
  // It's kind of ugly to make this mutable - solely so we can post the Quit
  // Task from Release(). This should be fixed.
  mutable base::MessageLoop loop_;
};

TEST_F(BrowserThreadTest, PostTask) {
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&BasicFunction, base::MessageLoop::current()));
  base::MessageLoop::current()->Run();
}

TEST_F(BrowserThreadTest, Release) {
  BrowserThread::ReleaseSoon(BrowserThread::UI, FROM_HERE, this);
  base::MessageLoop::current()->Run();
}

TEST_F(BrowserThreadTest, ReleasedOnCorrectThread) {
  {
    scoped_refptr<DeletedOnFile> test(
        new DeletedOnFile(base::MessageLoop::current()));
  }
  base::MessageLoop::current()->Run();
}

TEST_F(BrowserThreadTest, PostTaskViaTaskRunner) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE);
  task_runner->PostTask(
      FROM_HERE, base::Bind(&BasicFunction, base::MessageLoop::current()));
  base::MessageLoop::current()->Run();
}

TEST_F(BrowserThreadTest, ReleaseViaTaskRunner) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
  task_runner->ReleaseSoon(FROM_HERE, this);
  base::MessageLoop::current()->Run();
}

TEST_F(BrowserThreadTest, PostTaskAndReply) {
  // Most of the heavy testing for PostTaskAndReply() is done inside the
  // task runner test.  This just makes sure we get piped through at all.
  ASSERT_TRUE(BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE, base::Bind(&base::DoNothing),
      base::Bind(&base::MessageLoop::QuitWhenIdle,
                 base::Unretained(base::MessageLoop::current()->current()))));
  base::MessageLoop::current()->Run();
}

}  // namespace content

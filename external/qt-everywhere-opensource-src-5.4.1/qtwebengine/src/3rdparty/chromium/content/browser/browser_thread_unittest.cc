// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/browser/browser_thread_impl.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace content {

class BrowserThreadTest : public testing::Test {
 public:
  void Release() const {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    loop_.PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
  }

 protected:
  virtual void SetUp() {
    ui_thread_.reset(new BrowserThreadImpl(BrowserThread::UI));
    file_thread_.reset(new BrowserThreadImpl(BrowserThread::FILE));
    ui_thread_->Start();
    file_thread_->Start();
  }

  virtual void TearDown() {
    ui_thread_->Stop();
    file_thread_->Stop();
  }

  static void BasicFunction(base::MessageLoop* message_loop) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    message_loop->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
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
      message_loop_->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
    }

    base::MessageLoop* message_loop_;
  };

 private:
  scoped_ptr<BrowserThreadImpl> ui_thread_;
  scoped_ptr<BrowserThreadImpl> file_thread_;
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

TEST_F(BrowserThreadTest, PostTaskViaMessageLoopProxy) {
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE);
  message_loop_proxy->PostTask(
      FROM_HERE, base::Bind(&BasicFunction, base::MessageLoop::current()));
  base::MessageLoop::current()->Run();
}

TEST_F(BrowserThreadTest, ReleaseViaMessageLoopProxy) {
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
  message_loop_proxy->ReleaseSoon(FROM_HERE, this);
  base::MessageLoop::current()->Run();
}

TEST_F(BrowserThreadTest, PostTaskAndReply) {
  // Most of the heavy testing for PostTaskAndReply() is done inside the
  // MessageLoopProxy test.  This just makes sure we get piped through at all.
  ASSERT_TRUE(BrowserThread::PostTaskAndReply(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&base::DoNothing),
      base::Bind(&base::MessageLoop::Quit,
                 base::Unretained(base::MessageLoop::current()->current()))));
  base::MessageLoop::current()->Run();
}

}  // namespace content

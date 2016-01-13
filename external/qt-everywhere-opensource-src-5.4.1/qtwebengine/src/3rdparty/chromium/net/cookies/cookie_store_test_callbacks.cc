// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_store_test_callbacks.h"

#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

CookieCallback::CookieCallback(base::Thread* run_in_thread)
    : did_run_(false),
      run_in_thread_(run_in_thread),
      run_in_loop_(NULL),
      parent_loop_(base::MessageLoop::current()),
      loop_to_quit_(base::MessageLoop::current()) {}

CookieCallback::CookieCallback()
    : did_run_(false),
      run_in_thread_(NULL),
      run_in_loop_(base::MessageLoop::current()),
      parent_loop_(NULL),
      loop_to_quit_(base::MessageLoop::current()) {}

void CookieCallback::CallbackEpilogue() {
  base::MessageLoop* expected_loop = NULL;
  if (run_in_thread_) {
    DCHECK(!run_in_loop_);
    expected_loop = run_in_thread_->message_loop();
  } else if (run_in_loop_) {
    expected_loop = run_in_loop_;
  }
  ASSERT_TRUE(expected_loop != NULL);

  did_run_ = true;
  EXPECT_EQ(expected_loop, base::MessageLoop::current());
  loop_to_quit_->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
}

StringResultCookieCallback::StringResultCookieCallback() {}
StringResultCookieCallback::StringResultCookieCallback(
    base::Thread* run_in_thread)
    : CookieCallback(run_in_thread) {}

NoResultCookieCallback::NoResultCookieCallback() {}
NoResultCookieCallback::NoResultCookieCallback(base::Thread* run_in_thread)
    : CookieCallback(run_in_thread) {}

}  // namespace net

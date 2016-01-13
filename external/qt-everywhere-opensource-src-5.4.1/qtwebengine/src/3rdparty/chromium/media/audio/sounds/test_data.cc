// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/sounds/test_data.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"

namespace media {

TestObserver::TestObserver(const base::Closure& quit)
    : loop_(base::MessageLoop::current()),
      quit_(quit),
      num_play_requests_(0),
      num_stop_requests_(0),
      cursor_(0) {
  DCHECK(loop_);
}

TestObserver::~TestObserver() {
}

void TestObserver::OnPlay() {
  ++num_play_requests_;
}

void TestObserver::OnStop(size_t cursor) {
  ++num_stop_requests_;
  cursor_ = cursor;
  loop_->PostTask(FROM_HERE, quit_);
}

}  // namespace media

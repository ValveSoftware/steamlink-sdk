// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/async_api_function.h"

#include "base/bind.h"
#include "extensions/browser/extension_system.h"

using content::BrowserThread;

namespace extensions {

// AsyncApiFunction
AsyncApiFunction::AsyncApiFunction() : work_thread_id_(BrowserThread::IO) {}

AsyncApiFunction::~AsyncApiFunction() {}

bool AsyncApiFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!PrePrepare() || !Prepare()) {
    return false;
  }
  bool rv = BrowserThread::PostTask(
      work_thread_id_,
      FROM_HERE,
      base::Bind(&AsyncApiFunction::WorkOnWorkThread, this));
  DCHECK(rv);
  return true;
}

bool AsyncApiFunction::PrePrepare() { return true; }

void AsyncApiFunction::Work() {}

void AsyncApiFunction::AsyncWorkStart() {
  Work();
  AsyncWorkCompleted();
}

void AsyncApiFunction::AsyncWorkCompleted() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    bool rv = BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&AsyncApiFunction::RespondOnUIThread, this));
    DCHECK(rv);
  } else {
    SendResponse(Respond());
  }
}

void AsyncApiFunction::WorkOnWorkThread() {
  DCHECK_CURRENTLY_ON(work_thread_id_);
  DLOG_IF(ERROR, (work_thread_id_ == BrowserThread::UI))
      << "You have specified that AsyncApiFunction::Work() should happen on "
         "the UI thread. This nullifies the point of this class. Either "
         "specify a different thread or derive from a different class.";
  AsyncWorkStart();
}

void AsyncApiFunction::RespondOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SendResponse(Respond());
}

}  // namespace extensions

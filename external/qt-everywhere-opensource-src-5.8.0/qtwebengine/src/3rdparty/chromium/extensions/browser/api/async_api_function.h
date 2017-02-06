// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_ASYNC_API_FUCTION_H_
#define EXTENSIONS_BROWSER_API_ASYNC_API_FUCTION_H_

#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// AsyncApiFunction provides convenient thread management for APIs that need to
// do essentially all their work on a thread other than the UI thread.
class AsyncApiFunction : public AsyncExtensionFunction {
 protected:
  AsyncApiFunction();
  ~AsyncApiFunction() override;

  // Like Prepare(). A useful place to put common work in an ApiFunction
  // superclass that multiple API functions want to share.
  virtual bool PrePrepare();

  // Set up for work (e.g., validate arguments). Guaranteed to happen on UI
  // thread.
  virtual bool Prepare() = 0;

  // Do actual work. Guaranteed to happen on the thread specified in
  // work_thread_id_.
  virtual void Work();

  // Start the asynchronous work. Guraranteed to happen on requested thread.
  virtual void AsyncWorkStart();

  // Notify AsyncIOApiFunction that the work is completed
  void AsyncWorkCompleted();

  // Respond. Guaranteed to happen on UI thread.
  virtual bool Respond() = 0;

  // ExtensionFunction::RunAsync()
  bool RunAsync() override;

 protected:
  content::BrowserThread::ID work_thread_id() const { return work_thread_id_; }
  void set_work_thread_id(content::BrowserThread::ID work_thread_id) {
    work_thread_id_ = work_thread_id;
  }

 private:
  void WorkOnWorkThread();
  void RespondOnUIThread();

  // If you don't want your Work() method to happen on the IO thread, then set
  // this to the thread that you do want, preferably in Prepare().
  content::BrowserThread::ID work_thread_id_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_ASYNC_API_FUCTION_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBTHREAD_IMPL_H_
#define CONTENT_CHILD_WEBTHREAD_IMPL_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebThread.h"

namespace content {

class CONTENT_EXPORT WebThreadBase : public blink::WebThread {
 public:
  virtual ~WebThreadBase();

  virtual void addTaskObserver(TaskObserver* observer);
  virtual void removeTaskObserver(TaskObserver* observer);

  virtual bool isCurrentThread() const = 0;

 protected:
  WebThreadBase();

 private:
  class TaskObserverAdapter;

  typedef std::map<TaskObserver*, TaskObserverAdapter*> TaskObserverMap;
  TaskObserverMap task_observer_map_;
};

class CONTENT_EXPORT WebThreadImpl : public WebThreadBase {
 public:
  explicit WebThreadImpl(const char* name);
  virtual ~WebThreadImpl();

  virtual void postTask(Task* task);
  virtual void postDelayedTask(Task* task, long long delay_ms);

  virtual void enterRunLoop();
  virtual void exitRunLoop();

  base::MessageLoop* message_loop() const { return thread_->message_loop(); }

  virtual bool isCurrentThread() const OVERRIDE;

 private:
  scoped_ptr<base::Thread> thread_;
};

class WebThreadImplForMessageLoop : public WebThreadBase {
 public:
  CONTENT_EXPORT explicit WebThreadImplForMessageLoop(
      base::MessageLoopProxy* message_loop);
  CONTENT_EXPORT virtual ~WebThreadImplForMessageLoop();

  virtual void postTask(Task* task) OVERRIDE;
  virtual void postDelayedTask(Task* task, long long delay_ms) OVERRIDE;

  virtual void enterRunLoop() OVERRIDE;
  virtual void exitRunLoop() OVERRIDE;

 private:
  virtual bool isCurrentThread() const OVERRIDE;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
};

} // namespace content

#endif  // CONTENT_CHILD_WEBTHREAD_IMPL_H_

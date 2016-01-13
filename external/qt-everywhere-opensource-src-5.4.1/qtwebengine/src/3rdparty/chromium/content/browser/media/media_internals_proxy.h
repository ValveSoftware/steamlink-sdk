// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_PROXY_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_PROXY_H_

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/browser/media/media_internals.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/base/net_log.h"

namespace base {
class ListValue;
class Value;
}

namespace content {
class MediaInternalsMessageHandler;

// This class is a proxy between MediaInternals (on the IO thread) and
// MediaInternalsMessageHandler (on the UI thread).
// It is ref_counted to ensure that it completes all pending Tasks on both
// threads before destruction.
class MediaInternalsProxy
    : public base::RefCountedThreadSafe<
          MediaInternalsProxy,
          BrowserThread::DeleteOnUIThread>,
      public net::NetLog::ThreadSafeObserver,
      public NotificationObserver {
 public:
  MediaInternalsProxy();

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Register a Handler and start receiving callbacks from MediaInternals.
  void Attach(MediaInternalsMessageHandler* handler);

  // Unregister the same and stop receiving callbacks.
  void Detach();

  // Have MediaInternals send all the data it has.
  void GetEverything();

  // MediaInternals callback. Called on the IO thread.
  void OnUpdate(const base::string16& update);

  // net::NetLog::ThreadSafeObserver implementation. Callable from any thread:
  virtual void OnAddEntry(const net::NetLog::Entry& entry) OVERRIDE;

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<MediaInternalsProxy>;
  virtual ~MediaInternalsProxy();

  // Build a dictionary mapping constant names to values.
  base::Value* GetConstants();

  void ObserveMediaInternalsOnIOThread();
  void StopObservingMediaInternalsOnIOThread();
  void GetEverythingOnIOThread();
  void UpdateUIOnUIThread(const base::string16& update);

  // Put |entry| on a list of events to be sent to the page.
  void AddNetEventOnUIThread(base::Value* entry);

  // Send all pending events to the page.
  void SendNetEventsOnUIThread();

  // Call a JavaScript function on the page. Takes ownership of |args|.
  void CallJavaScriptFunctionOnUIThread(const std::string& function,
                                        base::Value* args);

  MediaInternalsMessageHandler* handler_;
  scoped_ptr<base::ListValue> pending_net_updates_;
  NotificationRegistrar registrar_;
  MediaInternals::UpdateCallback update_callback_;

  DISALLOW_COPY_AND_ASSIGN(MediaInternalsProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_PROXY_H_

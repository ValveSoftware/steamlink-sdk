// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_process_sub_thread.h"

#include "base/debug/leak_tracker.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/notification_service_impl.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace content {

BrowserProcessSubThread::BrowserProcessSubThread(BrowserThread::ID identifier)
    : BrowserThreadImpl(identifier) {
}

BrowserProcessSubThread::~BrowserProcessSubThread() {
  Stop();
}

void BrowserProcessSubThread::Init() {
#if defined(OS_WIN)
  com_initializer_.reset(new base::win::ScopedCOMInitializer());
#endif

  notification_service_.reset(new NotificationServiceImpl());

  BrowserThreadImpl::Init();

  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    // Though this thread is called the "IO" thread, it actually just routes
    // messages around; it shouldn't be allowed to perform any blocking disk
    // I/O.
    base::ThreadRestrictions::SetIOAllowed(false);
    base::ThreadRestrictions::DisallowWaiting();
  }
}

void BrowserProcessSubThread::CleanUp() {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO))
    IOThreadPreCleanUp();

  BrowserThreadImpl::CleanUp();

  notification_service_.reset();

#if defined(OS_WIN)
  com_initializer_.reset();
#endif
}

void BrowserProcessSubThread::IOThreadPreCleanUp() {
  // Kill all things that might be holding onto
  // net::URLRequest/net::URLRequestContexts.

  // Destroy all URLRequests started by URLFetchers.
  net::URLFetcher::CancelAll();

  // If any child processes are still running, terminate them and
  // and delete the BrowserChildProcessHost instances to release whatever
  // IO thread only resources they are referencing.
  BrowserChildProcessHostImpl::TerminateAll();

  // Unregister GpuMemoryBuffer dump provider before IO thread is shut down.
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      BrowserGpuMemoryBufferManager::current());
}

}  // namespace content

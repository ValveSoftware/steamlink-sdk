// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NETLOG_OBSERVER_H_
#define CONTENT_BROWSER_LOADER_NETLOG_OBSERVER_H_

#include <stdint.h>

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "content/public/common/resource_devtools_info.h"
#include "net/log/net_log.h"

namespace net {
class URLRequest;
class NetLogEntry;
}  // namespace net

namespace content {
struct ResourceResponse;

// NetLogObserver watches the NetLog event stream and collects the stuff that
// may be of interest to the client. Currently, this only includes actual
// HTTP/SPDY headers sent and received over the network.
//
// As NetLogObserver shares live data with objects that live on the IO Thread,
// it must also reside on the IO Thread.  Only OnAddEntry can be called from
// other threads.
class NetLogObserver : public net::NetLog::ThreadSafeObserver {
  typedef ResourceDevToolsInfo ResourceInfo;

 public:
  // net::NetLog::ThreadSafeObserver implementation:
  void OnAddEntry(const net::NetLogEntry& entry) override;

  // The NetLog instance is passed in via the |net_log| parameter.
  static void Attach(net::NetLog* net_log);

  static void Detach();

  // Must be called on the IO thread. May return NULL if no observers
  // are active.
  static NetLogObserver* GetInstance();
  static void PopulateResponseInfo(net::URLRequest*, ResourceResponse*);

 private:
  static NetLogObserver* instance_;

  NetLogObserver();
  ~NetLogObserver() override;

  ResourceInfo* GetResourceInfo(uint32_t id);

  void OnAddURLRequestEntry(const net::NetLogEntry& entry);

  typedef base::hash_map<uint32_t, scoped_refptr<ResourceInfo>>
      RequestToInfoMap;
  RequestToInfoMap request_to_info_;

  // Used to validate that calls to the NetLogObserver methods occur on the
  // thread it was instantiated on. Typically the IO thread.
  static base::LazyInstance<std::unique_ptr<base::ThreadChecker>>::Leaky
      io_thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(NetLogObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NETLOG_OBSERVER_H_

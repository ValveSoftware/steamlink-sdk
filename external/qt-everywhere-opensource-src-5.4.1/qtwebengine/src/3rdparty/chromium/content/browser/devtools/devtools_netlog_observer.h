// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_NETLOG_OBSERVER_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_NETLOG_OBSERVER_H_

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "content/public/common/resource_devtools_info.h"
#include "net/base/net_log.h"

namespace net {
class URLRequest;
}  // namespace net

namespace content {
struct ResourceResponse;

// DevToolsNetLogObserver watches the NetLog event stream and collects the
// stuff that may be of interest to DevTools. Currently, this only includes
// actual HTTP/SPDY headers sent and received over the network.
//
// As DevToolsNetLogObserver shares live data with objects that live on the
// IO Thread, it must also reside on the IO Thread.  Only OnAddEntry can be
// called from other threads.
class DevToolsNetLogObserver : public net::NetLog::ThreadSafeObserver {
  typedef ResourceDevToolsInfo ResourceInfo;

 public:
  // net::NetLog::ThreadSafeObserver implementation:
  virtual void OnAddEntry(const net::NetLog::Entry& entry) OVERRIDE;

  void OnAddURLRequestEntry(const net::NetLog::Entry& entry);

  static void Attach();
  static void Detach();

  // Must be called on the IO thread. May return NULL if no observers
  // are active.
  static DevToolsNetLogObserver* GetInstance();
  static void PopulateResponseInfo(net::URLRequest*,
                                   ResourceResponse*);

 private:
  static DevToolsNetLogObserver* instance_;

  DevToolsNetLogObserver();
  virtual ~DevToolsNetLogObserver();

  ResourceInfo* GetResourceInfo(uint32 id);

  typedef base::hash_map<uint32, scoped_refptr<ResourceInfo> > RequestToInfoMap;
  RequestToInfoMap request_to_info_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetLogObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_NETLOG_OBSERVER_H_

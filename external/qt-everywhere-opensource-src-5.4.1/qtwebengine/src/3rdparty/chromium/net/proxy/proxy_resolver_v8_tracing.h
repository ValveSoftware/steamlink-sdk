// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_V8_TRACING_H_
#define NET_PROXY_PROXY_RESOLVER_V8_TRACING_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/net_export.h"
#include "net/proxy/proxy_resolver.h"

namespace base {
class Thread;
class MessageLoopProxy;
}  // namespace base

namespace net {

class HostResolver;
class NetLog;
class ProxyResolverErrorObserver;
class ProxyResolverV8;

// ProxyResolverV8Tracing is a non-blocking ProxyResolver. It executes
// ProxyResolverV8 on a single helper thread, and does some magic to avoid
// blocking in DNS. For more details see the design document:
// https://docs.google.com/a/google.com/document/d/16Ij5OcVnR3s0MH4Z5XkhI9VTPoMJdaBn9rKreAmGOdE/edit?pli=1
class NET_EXPORT_PRIVATE ProxyResolverV8Tracing
    : public ProxyResolver,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  // Constructs a ProxyResolver that will issue DNS requests through
  // |host_resolver|, forward Javascript errors through |error_observer|, and
  // log Javascript errors and alerts to |net_log|.
  //
  // Note that the constructor takes ownership of |error_observer|, whereas
  // |host_resolver| and |net_log| are expected to outlive |this|.
  ProxyResolverV8Tracing(HostResolver* host_resolver,
                         ProxyResolverErrorObserver* error_observer,
                         NetLog* net_log);

  virtual ~ProxyResolverV8Tracing();

  // ProxyResolver implementation:
  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             const CompletionCallback& callback,
                             RequestHandle* request,
                             const BoundNetLog& net_log) OVERRIDE;
  virtual void CancelRequest(RequestHandle request) OVERRIDE;
  virtual LoadState GetLoadState(RequestHandle request) const OVERRIDE;
  virtual void CancelSetPacScript() OVERRIDE;
  virtual int SetPacScript(
      const scoped_refptr<ProxyResolverScriptData>& script_data,
      const CompletionCallback& callback) OVERRIDE;

 private:
  class Job;

  // The worker thread on which the ProxyResolverV8 will be run.
  scoped_ptr<base::Thread> thread_;
  scoped_ptr<ProxyResolverV8> v8_resolver_;

  // Non-owned host resolver, which is to be operated on the origin thread.
  HostResolver* host_resolver_;

  scoped_ptr<ProxyResolverErrorObserver> error_observer_;
  NetLog* net_log_;

  // The outstanding SetPacScript operation, or NULL.
  scoped_refptr<Job> set_pac_script_job_;

  // The number of outstanding (non-cancelled) jobs.
  int num_outstanding_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverV8Tracing);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_V8_TRACING_H_

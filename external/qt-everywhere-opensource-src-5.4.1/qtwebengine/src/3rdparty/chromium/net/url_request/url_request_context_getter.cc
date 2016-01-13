// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context_getter.h"

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "net/url_request/url_request_context.h"

namespace net {

URLRequestContextGetter::URLRequestContextGetter() {}

URLRequestContextGetter::~URLRequestContextGetter() {}

void URLRequestContextGetter::OnDestruct() const {
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner =
      GetNetworkTaskRunner();
  DCHECK(network_task_runner.get());
  if (network_task_runner.get()) {
    if (network_task_runner->BelongsToCurrentThread()) {
      delete this;
    } else {
      if (!network_task_runner->DeleteSoon(FROM_HERE, this)) {
        // Can't force-delete the object here, because some derived classes
        // can only be deleted on the owning thread, so just emit a warning to
        // aid in debugging.
        DLOG(WARNING) << "URLRequestContextGetter leaking due to no owning"
                      << " thread.";
      }
    }
  }
  // If no IO message loop proxy was available, we will just leak memory.
  // This is also true if the IO thread is gone.
}

TrivialURLRequestContextGetter::TrivialURLRequestContextGetter(
    net::URLRequestContext* context,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
    : context_(context), main_task_runner_(main_task_runner) {}

TrivialURLRequestContextGetter::~TrivialURLRequestContextGetter() {}

net::URLRequestContext*
TrivialURLRequestContextGetter::GetURLRequestContext() {
  return context_;
}

scoped_refptr<base::SingleThreadTaskRunner>
TrivialURLRequestContextGetter::GetNetworkTaskRunner() const {
  return main_task_runner_;
}


}  // namespace net

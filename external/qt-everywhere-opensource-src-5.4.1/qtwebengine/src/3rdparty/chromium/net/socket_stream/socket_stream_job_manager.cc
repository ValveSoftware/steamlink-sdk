// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket_stream/socket_stream_job_manager.h"

#include "base/memory/singleton.h"

namespace net {

SocketStreamJobManager::SocketStreamJobManager() {
}

SocketStreamJobManager::~SocketStreamJobManager() {
}

// static
SocketStreamJobManager* SocketStreamJobManager::GetInstance() {
  return Singleton<SocketStreamJobManager>::get();
}

SocketStreamJob* SocketStreamJobManager::CreateJob(
    const GURL& url, SocketStream::Delegate* delegate,
    URLRequestContext* context, CookieStore* cookie_store) const {
  // If url is invalid, create plain SocketStreamJob, which will close
  // the socket immediately.
  if (!url.is_valid()) {
    SocketStreamJob* job = new SocketStreamJob();
    job->InitSocketStream(new SocketStream(url, delegate, context,
                                           cookie_store));
    return job;
  }

  const std::string& scheme = url.scheme();  // already lowercase

  base::AutoLock locked(lock_);
  FactoryMap::const_iterator found = factories_.find(scheme);
  if (found != factories_.end()) {
    SocketStreamJob* job = found->second(url, delegate, context, cookie_store);
    if (job)
      return job;
  }
  SocketStreamJob* job = new SocketStreamJob();
  job->InitSocketStream(new SocketStream(url, delegate, context, cookie_store));
  return job;
}

SocketStreamJob::ProtocolFactory*
SocketStreamJobManager::RegisterProtocolFactory(
    const std::string& scheme, SocketStreamJob::ProtocolFactory* factory) {
  base::AutoLock locked(lock_);

  SocketStreamJob::ProtocolFactory* old_factory;
  FactoryMap::iterator found = factories_.find(scheme);
  if (found != factories_.end()) {
    old_factory = found->second;
  } else {
    old_factory = NULL;
  }
  if (factory) {
    factories_[scheme] = factory;
  } else if (found != factories_.end()) {
    factories_.erase(found);
  }
  return old_factory;
}

}  // namespace net

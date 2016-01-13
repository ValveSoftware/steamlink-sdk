// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_STREAM_SOCKET_STREAM_JOB_MANAGER_H_
#define NET_SOCKET_STREAM_SOCKET_STREAM_JOB_MANAGER_H_

#include <map>
#include <string>

#include "net/socket_stream/socket_stream.h"
#include "net/socket_stream/socket_stream_job.h"

template <typename T> struct DefaultSingletonTraits;
class GURL;

namespace net {

class SocketStreamJobManager {
 public:
  // Returns the singleton instance.
  static SocketStreamJobManager* GetInstance();

  SocketStreamJob* CreateJob(
      const GURL& url, SocketStream::Delegate* delegate,
      URLRequestContext* context, CookieStore* cookie_store) const;

  SocketStreamJob::ProtocolFactory* RegisterProtocolFactory(
      const std::string& scheme, SocketStreamJob::ProtocolFactory* factory);

 private:
  friend struct DefaultSingletonTraits<SocketStreamJobManager>;
  typedef std::map<std::string, SocketStreamJob::ProtocolFactory*> FactoryMap;

  SocketStreamJobManager();
  ~SocketStreamJobManager();

  mutable base::Lock lock_;
  FactoryMap factories_;

  DISALLOW_COPY_AND_ASSIGN(SocketStreamJobManager);
};

}  // namespace net

#endif  // NET_SOCKET_STREAM_SOCKET_STREAM_JOB_MANAGER_H_

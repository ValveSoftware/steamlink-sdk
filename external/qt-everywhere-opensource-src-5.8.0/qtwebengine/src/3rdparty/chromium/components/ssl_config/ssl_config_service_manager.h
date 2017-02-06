// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SSL_CONFIG_SSL_CONFIG_SERVICE_MANAGER_H_
#define COMPONENTS_SSL_CONFIG_SSL_CONFIG_SERVICE_MANAGER_H_

#include "base/memory/ref_counted.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class SSLConfigService;
}  // namespace net

class PrefService;
class PrefRegistrySimple;

namespace ssl_config {

// An interface for creating SSLConfigService objects.
class SSLConfigServiceManager {
 public:
  // Create an instance of the SSLConfigServiceManager. The lifetime of the
  // PrefService objects must be longer than that of the manager. Get SSL
  // preferences from local_state object.
  static SSLConfigServiceManager* CreateDefaultManager(
      PrefService* local_state,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);

  static void RegisterPrefs(PrefRegistrySimple* registry);

  virtual ~SSLConfigServiceManager() {}

  // Get an SSLConfigService instance.  It may be a new instance or the manager
  // may return the same instance multiple times.
  // The caller should hold a reference as long as it needs the instance (eg,
  // using scoped_refptr.)
  virtual net::SSLConfigService* Get() = 0;
};

}  // namespace ssl_config
#endif  // COMPONENTS_SSL_CONFIG_SSL_CONFIG_SERVICE_MANAGER_H_

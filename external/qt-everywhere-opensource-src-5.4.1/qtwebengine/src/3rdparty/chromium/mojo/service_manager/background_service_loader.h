// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICE_MANAGER_BACKGROUND_SERVICE_LOADER_H_
#define MOJO_SERVICE_MANAGER_BACKGROUND_SERVICE_LOADER_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "mojo/service_manager/service_loader.h"

namespace mojo {

class ServiceManager;

// ServiceLoader implementation that creates a background thread and issues load
// requests there.
class MOJO_SERVICE_MANAGER_EXPORT BackgroundServiceLoader
    : public ServiceLoader {
 public:
  BackgroundServiceLoader(scoped_ptr<ServiceLoader> real_loader,
                          const char* thread_name,
                          base::MessageLoop::Type message_loop_type);
  virtual ~BackgroundServiceLoader();

  // ServiceLoader overrides:
  virtual void LoadService(ServiceManager* manager,
                           const GURL& url,
                           ScopedMessagePipeHandle service_handle) OVERRIDE;
  virtual void OnServiceError(ServiceManager* manager,
                              const GURL& url) OVERRIDE;

 private:
  class BackgroundLoader;

  // These functions are exected on the background thread. They call through
  // to |background_loader_| to do the actual loading.
  // TODO: having this code take a |manager| is fragile (as ServiceManager isn't
  // thread safe).
  void LoadServiceOnBackgroundThread(
      ServiceManager* manager,
      const GURL& url,
      ScopedMessagePipeHandle* service_provider_handle);
  void OnServiceErrorOnBackgroundThread(ServiceManager* manager,
                                        const GURL& url);
  void ShutdownOnBackgroundThread();

  scoped_ptr<ServiceLoader> loader_;
  base::Thread thread_;
  base::MessageLoop::Type message_loop_type_;

  // Lives on |thread_|. Trivial interface that calls through to |loader_|.
  BackgroundLoader* background_loader_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundServiceLoader);
};

}  // namespace mojo

#endif  // MOJO_SERVICE_MANAGER_BACKGROUND_SERVICE_LOADER_H_

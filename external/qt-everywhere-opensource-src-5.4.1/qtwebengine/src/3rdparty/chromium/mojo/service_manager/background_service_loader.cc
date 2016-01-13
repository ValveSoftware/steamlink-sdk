// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/service_manager/background_service_loader.h"

#include "base/bind.h"
#include "mojo/service_manager/service_manager.h"

namespace mojo {

class BackgroundServiceLoader::BackgroundLoader {
 public:
  explicit BackgroundLoader(ServiceLoader* loader) : loader_(loader) {}
  ~BackgroundLoader() {}

  void LoadService(ServiceManager* manager,
                   const GURL& url,
                   ScopedMessagePipeHandle service_provider_handle) {
    loader_->LoadService(manager, url, service_provider_handle.Pass());
  }

  void OnServiceError(ServiceManager* manager, const GURL& url) {
    loader_->OnServiceError(manager, url);
  }

 private:
  base::MessageLoop::Type message_loop_type_;
  ServiceLoader* loader_;  // Owned by BackgroundServiceLoader

  DISALLOW_COPY_AND_ASSIGN(BackgroundLoader);
};

BackgroundServiceLoader::BackgroundServiceLoader(
    scoped_ptr<ServiceLoader> real_loader,
    const char* thread_name,
    base::MessageLoop::Type message_loop_type)
    : loader_(real_loader.Pass()),
      thread_(thread_name),
      message_loop_type_(message_loop_type),
      background_loader_(NULL) {
}

BackgroundServiceLoader::~BackgroundServiceLoader() {
  if (thread_.IsRunning()) {
    thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&BackgroundServiceLoader::ShutdownOnBackgroundThread,
                   base::Unretained(this)));
  }
  thread_.Stop();
}

void BackgroundServiceLoader::LoadService(
    ServiceManager* manager,
    const GURL& url,
    ScopedMessagePipeHandle service_handle) {
  const int kDefaultStackSize = 0;
  if (!thread_.IsRunning())
    thread_.StartWithOptions(
        base::Thread::Options(message_loop_type_, kDefaultStackSize));
  thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&BackgroundServiceLoader::LoadServiceOnBackgroundThread,
                 base::Unretained(this), manager, url,
                 base::Owned(
                    new ScopedMessagePipeHandle(service_handle.Pass()))));
}

void BackgroundServiceLoader::OnServiceError(ServiceManager* manager,
                                             const GURL& url) {
  if (!thread_.IsRunning())
    thread_.Start();
  thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&BackgroundServiceLoader::OnServiceErrorOnBackgroundThread,
                 base::Unretained(this), manager, url));
}

void BackgroundServiceLoader::LoadServiceOnBackgroundThread(
    ServiceManager* manager,
    const GURL& url,
    ScopedMessagePipeHandle* service_provider_handle) {
  if (!background_loader_)
    background_loader_ = new BackgroundLoader(loader_.get());
  background_loader_->LoadService(
      manager, url, service_provider_handle->Pass());
}

void BackgroundServiceLoader::OnServiceErrorOnBackgroundThread(
    ServiceManager* manager,
    const GURL& url) {
  if (!background_loader_)
    background_loader_ = new BackgroundLoader(loader_.get());
  background_loader_->OnServiceError(manager, url);
}

void BackgroundServiceLoader::ShutdownOnBackgroundThread() {
  delete background_loader_;
  // Destroy |loader_| on the thread it's actually used on.
  loader_.reset();
}

}  // namespace mojo

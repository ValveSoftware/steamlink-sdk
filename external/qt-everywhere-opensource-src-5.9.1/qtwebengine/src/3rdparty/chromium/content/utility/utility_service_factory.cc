// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_service_factory.h"

#include "base/bind.h"
#include "content/public/common/content_client.h"
#include "content/public/utility/content_utility_client.h"
#include "content/public/utility/utility_thread.h"
#include "content/utility/utility_thread_impl.h"

#if defined(ENABLE_MOJO_MEDIA_IN_UTILITY_PROCESS)
#include "media/mojo/services/media_service_factory.h"  // nogncheck
#endif

namespace content {

UtilityServiceFactory::UtilityServiceFactory() {}

UtilityServiceFactory::~UtilityServiceFactory() {}

void UtilityServiceFactory::RegisterServices(ServiceMap* services) {
  GetContentClient()->utility()->RegisterServices(services);

#if defined(ENABLE_MOJO_MEDIA_IN_UTILITY_PROCESS)
  ServiceInfo info;
  info.factory = base::Bind(&media::CreateMediaService);
  services->insert(std::make_pair("media", info));
#endif
}

void UtilityServiceFactory::OnServiceQuit() {
  UtilityThread::Get()->ReleaseProcessIfNeeded();
}

void UtilityServiceFactory::OnLoadFailed() {
  UtilityThreadImpl* utility_thread =
      static_cast<UtilityThreadImpl*>(UtilityThread::Get());
  utility_thread->Shutdown();
  utility_thread->ReleaseProcessIfNeeded();
}

}  // namespace content

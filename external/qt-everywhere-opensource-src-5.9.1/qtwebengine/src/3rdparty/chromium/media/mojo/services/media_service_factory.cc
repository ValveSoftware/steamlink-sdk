// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_service_factory.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "media/mojo/services/media_service.h"
#include "media/mojo/services/test_mojo_media_client.h"

#if defined(OS_ANDROID)
#include "media/mojo/services/android_mojo_media_client.h"  // nogncheck
#endif

namespace media {

std::unique_ptr<service_manager::Service> CreateMediaService() {
#if defined(ENABLE_TEST_MOJO_MEDIA_CLIENT)
  return CreateMediaServiceForTesting();
#elif defined(OS_ANDROID)
  return std::unique_ptr<service_manager::Service>(
      new MediaService(base::MakeUnique<AndroidMojoMediaClient>()));
#else
  NOTREACHED() << "No MediaService implementation available.";
  return nullptr;
#endif
}

std::unique_ptr<service_manager::Service> CreateMediaServiceForTesting() {
  return std::unique_ptr<service_manager::Service>(
      new MediaService(base::MakeUnique<TestMojoMediaClient>()));
}

}  // namespace media

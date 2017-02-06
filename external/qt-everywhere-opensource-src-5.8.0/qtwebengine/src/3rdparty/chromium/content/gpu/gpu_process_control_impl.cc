// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_process_control_impl.h"

#if defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "media/mojo/services/mojo_media_application_factory.h"  // nogncheck
#endif

namespace content {

GpuProcessControlImpl::GpuProcessControlImpl() {}

GpuProcessControlImpl::~GpuProcessControlImpl() {}

void GpuProcessControlImpl::RegisterApplications(ApplicationMap* apps) {
#if defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  MojoApplicationInfo app_info;
  app_info.application_factory = base::Bind(&media::CreateMojoMediaApplication);
  app_info.use_own_thread = true;
  apps->insert(std::make_pair("mojo:media", app_info));
#endif
}

}  // namespace content

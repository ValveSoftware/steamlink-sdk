// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/video_encode_accelerator.h"

#include "base/task_runner_util.h"
#include "content/common/gpu/client/gpu_video_encode_accelerator_host.h"
#include "content/renderer/render_thread_impl.h"
#include "media/filters/gpu_video_accelerator_factories.h"

namespace content {

void CreateVideoEncodeAccelerator(
    const OnCreateVideoEncodeAcceleratorCallback& callback) {
  DCHECK(!callback.is_null());

  scoped_refptr<media::GpuVideoAcceleratorFactories> gpu_factories =
        RenderThreadImpl::current()->GetGpuFactories();
  if (!gpu_factories.get()) {
    callback.Run(NULL, scoped_ptr<media::VideoEncodeAccelerator>());
    return;
  }

  scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner =
      gpu_factories->GetTaskRunner();
  base::PostTaskAndReplyWithResult(
      encode_task_runner,
      FROM_HERE,
      base::Bind(
          &media::GpuVideoAcceleratorFactories::CreateVideoEncodeAccelerator,
          gpu_factories),
      base::Bind(callback, encode_task_runner));
}

std::vector<media::VideoEncodeAccelerator::SupportedProfile>
GetSupportedVideoEncodeAcceleratorProfiles() {
  return GpuVideoEncodeAcceleratorHost::GetSupportedProfiles();
}

}  // namespace content

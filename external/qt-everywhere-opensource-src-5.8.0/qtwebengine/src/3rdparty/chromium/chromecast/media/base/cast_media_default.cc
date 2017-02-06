// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_default.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/media_codec_support_shlib.h"
#include "chromecast/public/video_plane.h"

namespace chromecast {
namespace media {
namespace {

class DefaultVideoPlane : public VideoPlane {
 public:
  ~DefaultVideoPlane() override {}

  void SetGeometry(const RectF& display_rect,
                   Transform transform) override {}
};

DefaultVideoPlane* g_video_plane = nullptr;
base::ThreadTaskRunnerHandle* g_thread_task_runner_handle = nullptr;

}  // namespace

void CastMediaShlib::Initialize(const std::vector<std::string>& argv) {
  g_video_plane = new DefaultVideoPlane();
}

void CastMediaShlib::Finalize() {
  delete g_video_plane;
  g_video_plane = nullptr;
  delete g_thread_task_runner_handle;
  g_thread_task_runner_handle = nullptr;
}

VideoPlane* CastMediaShlib::GetVideoPlane() {
  return g_video_plane;
}

MediaPipelineBackend* CastMediaShlib::CreateMediaPipelineBackend(
    const MediaPipelineDeviceParams& params) {
  // Set up the static reference in base::ThreadTaskRunnerHandle::Get
  // for the media thread in this shared library.  We can extract the
  // SingleThreadTaskRunner passed in from cast_shell for this.
  if (!base::ThreadTaskRunnerHandle::IsSet()) {
    DCHECK(!g_thread_task_runner_handle);
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        static_cast<TaskRunnerImpl*>(params.task_runner)->runner();
    DCHECK(task_runner->BelongsToCurrentThread());
    g_thread_task_runner_handle = new base::ThreadTaskRunnerHandle(task_runner);
  }

  return new MediaPipelineBackendDefault();
}

MediaCodecSupportShlib::CodecSupport MediaCodecSupportShlib::IsSupported(
    const std::string& codec) {
#if defined(OS_ANDROID)
  // TODO(servolk): Find a way to reuse IsCodecSupportedOnAndroid.

  // Theora is not supported
  if (codec == "theora")
    return kNotSupported;

  // MPEG-2 variants of AAC are not supported on Android.
  // MPEG2_AAC_MAIN / MPEG2_AAC_LC / MPEG2_AAC_SSR
  if (codec == "mp4a.66" || codec == "mp4a.67" || codec == "mp4a.68")
    return kNotSupported;

  // VP9 is guaranteed supported but is often software-decode only.
  // TODO(gunsch/servolk): look into querying for hardware decode support.
  if (codec == "vp9" || codec == "vp9.0")
    return kNotSupported;
#endif

  return kDefault;
}

double CastMediaShlib::GetMediaClockRate() {
  return 0.0;
}

double CastMediaShlib::MediaClockRatePrecision() {
  return 0.0;
}

void CastMediaShlib::MediaClockRateRange(double* minimum_rate,
                                         double* maximum_rate) {}

bool CastMediaShlib::SetMediaClockRate(double new_rate) {
  return false;
}

bool CastMediaShlib::SupportsMediaClockRateChange() {
  return false;
}

}  // namespace media
}  // namespace chromecast

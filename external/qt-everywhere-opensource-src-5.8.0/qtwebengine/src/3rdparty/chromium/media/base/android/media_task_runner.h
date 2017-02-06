// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_TASK_RUNNER_H_
#define MEDIA_BASE_ANDROID_MEDIA_TASK_RUNNER_H_

#include "base/single_thread_task_runner.h"
#include "media/base/media_export.h"

namespace media {

// Returns the task runner for the media thread.
MEDIA_EXPORT scoped_refptr<base::SingleThreadTaskRunner> GetMediaTaskRunner();

// Returns true if the MediaCodecPlayer (which works on Media thread) should be
// used. This behavior is controlled by a finch flag.
MEDIA_EXPORT bool UseMediaThreadForMediaPlayback();

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_TASK_RUNNER_H_

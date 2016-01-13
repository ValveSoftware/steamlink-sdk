// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "media/base/yuv_convert.h"

namespace media {

namespace internal {
// Platform specific initialization method.
extern bool InitializeMediaLibraryInternal(const base::FilePath& module_dir);
}  // namespace internal

// Media must only be initialized once, so use a LazyInstance to ensure this.
class MediaInitializer {
 public:
  bool Initialize(const base::FilePath& module_dir) {
    base::AutoLock auto_lock(lock_);
    if (!tried_initialize_) {
      tried_initialize_ = true;
      initialized_ = internal::InitializeMediaLibraryInternal(module_dir);
    }
    return initialized_;
  }

  bool IsInitialized() {
    base::AutoLock auto_lock(lock_);
    return initialized_;
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<MediaInitializer>;

  MediaInitializer()
      : initialized_(false),
        tried_initialize_(false) {
    // Perform initialization of libraries which require runtime CPU detection.
    InitializeCPUSpecificYUVConversions();
  }

  ~MediaInitializer() {
    NOTREACHED() << "MediaInitializer should be leaky!";
  }

  base::Lock lock_;
  bool initialized_;
  bool tried_initialize_;

  DISALLOW_COPY_AND_ASSIGN(MediaInitializer);
};

static base::LazyInstance<MediaInitializer>::Leaky g_media_library =
    LAZY_INSTANCE_INITIALIZER;

bool InitializeMediaLibrary(const base::FilePath& module_dir) {
  return g_media_library.Get().Initialize(module_dir);
}

void InitializeMediaLibraryForTesting() {
  base::FilePath module_dir;
  CHECK(PathService::Get(base::DIR_EXE, &module_dir));
  CHECK(g_media_library.Get().Initialize(module_dir));
}

bool IsMediaLibraryInitialized() {
  return g_media_library.Get().IsInitialized();
}

void InitializeCPUSpecificMediaFeatures() {
  // Force initialization of the media initializer, but don't call Initialize().
  g_media_library.Get();
}

}  // namespace media

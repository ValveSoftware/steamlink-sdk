// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_OPENSLES_UTIL_H_
#define MEDIA_AUDIO_ANDROID_OPENSLES_UTIL_H_

#include <SLES/OpenSLES.h>

#include "base/logging.h"

namespace media {

template <typename SLType, typename SLDerefType>
class ScopedSLObject {
 public:
  ScopedSLObject() : obj_(NULL) {}

  ~ScopedSLObject() { Reset(); }

  SLType* Receive() {
    DCHECK(!obj_);
    return &obj_;
  }

  SLDerefType operator->() { return *obj_; }

  SLType Get() const { return obj_; }

  void Reset() {
    if (obj_) {
      (*obj_)->Destroy(obj_);
      obj_ = NULL;
    }
  }

 private:
  SLType obj_;
};

typedef ScopedSLObject<SLObjectItf, const SLObjectItf_*> ScopedSLObjectItf;

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_OPENSLES_UTIL_H_

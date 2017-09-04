// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_TEXTURE_HANDLE_H_
#define SKIA_EXT_TEXTURE_HANDLE_H_

#include "third_party/skia/include/gpu/gl/GrGLTypes.h"

namespace skia {

// TODO(bsalomon): Remove both of these conversions when Skia bug 5019 is fixed.
inline GrBackendObject GrGLTextureInfoToGrBackendObject(
    const GrGLTextureInfo& info) {
  return reinterpret_cast<GrBackendObject>(&info);
}

inline const GrGLTextureInfo* GrBackendObjectToGrGLTextureInfo(
    GrBackendObject object) {
  return reinterpret_cast<const GrGLTextureInfo*>(object);
}

}  // namespace skia

#endif  // SKIA_EXT_TEXTURE_HANDLE_H_

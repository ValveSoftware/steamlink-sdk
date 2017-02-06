// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_PICTURE_DATA_H_
#define CC_BLIMP_PICTURE_DATA_H_

#include <stdint.h>

#include "cc/base/cc_export.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {

// PictureData is a holder object for a serialized SkPicture and its unique ID.
struct CC_EXPORT PictureData {
  PictureData(uint32_t unique_id, sk_sp<SkData> data);
  PictureData(const PictureData& other);
  ~PictureData();

  uint32_t unique_id;
  sk_sp<SkData> data;
};

}  // namespace cc

#endif  // CC_BLIMP_PICTURE_DATA_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_PICTURE_DATA_CONVERSIONS_H_
#define CC_BLIMP_PICTURE_DATA_CONVERSIONS_H_

#include <memory>
#include <vector>

#include "cc/base/cc_export.h"
#include "cc/blimp/picture_data.h"

namespace cc {

namespace proto {
class SkPictures;

CC_EXPORT void PictureDataVectorToSkPicturesProto(
    const std::vector<PictureData>& cache_update,
    SkPictures* proto_pictures);

CC_EXPORT std::vector<PictureData> SkPicturesProtoToPictureDataVector(
    const SkPictures& proto_pictures);

}  // namespace proto
}  // namespace cc

#endif  // CC_BLIMP_PICTURE_DATA_CONVERSIONS_H_

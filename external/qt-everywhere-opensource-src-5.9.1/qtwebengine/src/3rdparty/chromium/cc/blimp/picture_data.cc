// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blimp/picture_data.h"

namespace cc {

PictureData::PictureData(uint32_t unique_id, sk_sp<SkData> data)
    : unique_id(unique_id), data(data) {}

PictureData::PictureData(const PictureData& other) = default;

PictureData::~PictureData() = default;

}  // namespace cc

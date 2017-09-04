// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blimp/picture_data_conversions.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "cc/proto/display_item.pb.h"
#include "cc/proto/layer_tree_host.pb.h"

namespace cc {
namespace proto {

void PictureDataVectorToSkPicturesProto(
    const std::vector<PictureData>& cache_update,
    SkPictures* proto_pictures) {
  for (const PictureData& picture : cache_update) {
    proto::SkPictureData* picture_data = proto_pictures->add_pictures();
    proto::SkPictureID* picture_id = picture_data->mutable_id();
    picture_id->set_unique_id(picture.unique_id);
    picture_data->set_payload(picture.data->data(), picture.data->size());
  }
}

std::vector<PictureData> SkPicturesProtoToPictureDataVector(
    const SkPictures& proto_pictures) {
  std::vector<PictureData> result;
  for (int i = 0; i < proto_pictures.pictures_size(); ++i) {
    SkPictureData proto_picture = proto_pictures.pictures(i);
    DCHECK(proto_picture.has_id());
    DCHECK(proto_picture.id().has_unique_id());
    DCHECK(proto_picture.has_payload());
    PictureData picture_data(
        proto_picture.id().unique_id(),
        SkData::MakeWithCopy(proto_picture.payload().data(),
                             proto_picture.payload().size()));
    result.push_back(picture_data);
  }
  return result;
}

}  // namespace proto
}  // namespace cc

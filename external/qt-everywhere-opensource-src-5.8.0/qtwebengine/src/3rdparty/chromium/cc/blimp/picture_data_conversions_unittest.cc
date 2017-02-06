// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blimp/picture_data_conversions.h"

#include <memory>
#include <set>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "cc/blimp/picture_data.h"
#include "cc/proto/layer_tree_host.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkStream.h"

namespace cc {
namespace {
sk_sp<const SkPicture> CreateSkPicture(SkColor color) {
  SkPictureRecorder recorder;
  sk_sp<SkCanvas> canvas =
      sk_ref_sp(recorder.beginRecording(SkRect::MakeWH(1, 1)));
  canvas->drawColor(color);
  return recorder.finishRecordingAsPicture();
}

sk_sp<SkData> SerializePicture(sk_sp<const SkPicture> picture) {
  SkDynamicMemoryWStream stream;
  picture->serialize(&stream, nullptr);
  DCHECK(stream.bytesWritten());
  return sk_sp<SkData>(stream.copyToData());
}

bool SamePicture(sk_sp<const SkPicture> picture,
                 const PictureData& picture_data) {
  if (picture->uniqueID() != picture_data.unique_id)
    return false;

  sk_sp<const SkData> serialized_picture = SerializePicture(picture);
  return picture_data.data->equals(serialized_picture);
}

PictureData CreatePictureData(sk_sp<const SkPicture> picture) {
  return PictureData(picture->uniqueID(), SerializePicture(picture));
}

TEST(PictureCacheConversionsTest, SerializePictureCaheUpdate) {
  sk_sp<const SkPicture> picture1 = CreateSkPicture(SK_ColorRED);
  sk_sp<const SkPicture> picture2 = CreateSkPicture(SK_ColorBLUE);
  std::vector<PictureData> update;
  update.push_back(CreatePictureData(picture1));
  update.push_back(CreatePictureData(picture2));

  proto::SkPictures proto_pictures;
  PictureDataVectorToSkPicturesProto(update, &proto_pictures);
  ASSERT_EQ(2, proto_pictures.pictures_size());

  std::vector<PictureData> deserialized =
      SkPicturesProtoToPictureDataVector(proto_pictures);

  ASSERT_EQ(2U, deserialized.size());
  PictureData picture_data_1 = deserialized.at(0);
  PictureData picture_data_2 = deserialized.at(1);
  EXPECT_TRUE(SamePicture(picture1, picture_data_1));
  EXPECT_TRUE(SamePicture(picture2, picture_data_2));
}

}  // namespace
}  // namespace cc

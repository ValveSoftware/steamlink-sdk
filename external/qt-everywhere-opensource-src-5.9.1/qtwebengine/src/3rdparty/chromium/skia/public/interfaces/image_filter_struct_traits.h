// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_PUBLIC_INTERFACES_IMAGE_FILTER_STRUCT_TRAITS_H_
#define SKIA_PUBLIC_INTERFACES_IMAGE_FILTER_STRUCT_TRAITS_H_

#include "skia/public/interfaces/image_filter.mojom-shared.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkFlattenableSerialization.h"

namespace mojo {

struct ImageFilterBuffer {
  ImageFilterBuffer();
  ImageFilterBuffer(const ImageFilterBuffer& other);
  ~ImageFilterBuffer();
  sk_sp<SkData> data;
};

template <>
struct ArrayTraits<ImageFilterBuffer> {
  using Element = uint8_t;
  static size_t GetSize(const ImageFilterBuffer& b);
  static uint8_t* GetData(ImageFilterBuffer& b);
  static const uint8_t* GetData(const ImageFilterBuffer& b);
  static uint8_t& GetAt(ImageFilterBuffer& b, size_t i);
  static const uint8_t& GetAt(const ImageFilterBuffer& b, size_t i);
  static bool Resize(ImageFilterBuffer& b, size_t size);
};

template <>
struct StructTraits<skia::mojom::ImageFilterDataView, sk_sp<SkImageFilter>> {
  static ImageFilterBuffer data(const sk_sp<SkImageFilter>& filter) {
    ImageFilterBuffer buffer;
    buffer.data = sk_sp<SkData>(SkValidatingSerializeFlattenable(filter.get()));
    return buffer;
  }

  static bool Read(skia::mojom::ImageFilterDataView data,
                   sk_sp<SkImageFilter>* out) {
    ImageFilterBuffer buffer;
    if (!data.ReadData(&buffer))
      return false;
    SkFlattenable* flattenable = SkValidatingDeserializeFlattenable(
        buffer.data->bytes(), buffer.data->size(),
        SkImageFilter::GetFlattenableType());
    *out = sk_sp<SkImageFilter>(static_cast<SkImageFilter*>(flattenable));
    return true;
  }
};

}  // namespace mojo

#endif  // SKIA_PUBLIC_INTERFACES_IMAGE_FILTER_STRUCT_TRAITS_H_

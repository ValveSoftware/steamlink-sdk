// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

#include <string>

#include "base/pickle.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/transform.h"

namespace {

struct SkBitmap_Data {
  // The color type for the bitmap (bits per pixel, etc).
  SkColorType color_type;

  // The alpha type for the bitmap (opaque, premul, unpremul).
  SkAlphaType alpha_type;

  // The width of the bitmap in pixels.
  uint32_t width;

  // The height of the bitmap in pixels.
  uint32_t height;

  void InitSkBitmapDataForTransfer(const SkBitmap& bitmap) {
    const SkImageInfo& info = bitmap.info();
    color_type = info.colorType();
    alpha_type = info.alphaType();
    width = info.width();
    height = info.height();
  }

  // Returns whether |bitmap| successfully initialized.
  bool InitSkBitmapFromData(SkBitmap* bitmap,
                            const char* pixels,
                            size_t pixels_size) const {
    if (!bitmap->tryAllocPixels(
            SkImageInfo::Make(width, height, color_type, alpha_type)))
      return false;
    if (pixels_size != bitmap->getSize())
      return false;
    memcpy(bitmap->getPixels(), pixels, pixels_size);
    return true;
  }
};

}  // namespace

namespace IPC {

void ParamTraits<SkBitmap>::GetSize(base::PickleSizer* s, const param_type& p) {
  s->AddData(sizeof(SkBitmap_Data));
  s->AddData(static_cast<int>(p.getSize()));
}

void ParamTraits<SkBitmap>::Write(base::Pickle* m, const SkBitmap& p) {
  size_t fixed_size = sizeof(SkBitmap_Data);
  SkBitmap_Data bmp_data;
  bmp_data.InitSkBitmapDataForTransfer(p);
  m->WriteData(reinterpret_cast<const char*>(&bmp_data),
               static_cast<int>(fixed_size));
  size_t pixel_size = p.getSize();
  SkAutoLockPixels p_lock(p);
  m->WriteData(reinterpret_cast<const char*>(p.getPixels()),
               static_cast<int>(pixel_size));
}

bool ParamTraits<SkBitmap>::Read(const base::Pickle* m,
                                 base::PickleIterator* iter,
                                 SkBitmap* r) {
  const char* fixed_data;
  int fixed_data_size = 0;
  if (!iter->ReadData(&fixed_data, &fixed_data_size) ||
     (fixed_data_size <= 0)) {
    return false;
  }
  if (fixed_data_size != sizeof(SkBitmap_Data))
    return false;  // Message is malformed.

  const char* variable_data;
  int variable_data_size = 0;
  if (!iter->ReadData(&variable_data, &variable_data_size) ||
     (variable_data_size < 0)) {
    return false;
  }
  const SkBitmap_Data* bmp_data =
      reinterpret_cast<const SkBitmap_Data*>(fixed_data);
  return bmp_data->InitSkBitmapFromData(r, variable_data, variable_data_size);
}

void ParamTraits<SkBitmap>::Log(const SkBitmap& p, std::string* l) {
  l->append("<SkBitmap>");
}

void ParamTraits<gfx::Transform>::GetSize(base::PickleSizer* s,
                                          const param_type& p) {
  s->AddBytes(sizeof(SkMScalar) * 16);
}

void ParamTraits<gfx::Transform>::Write(base::Pickle* m, const param_type& p) {
#ifdef SK_MSCALAR_IS_FLOAT
  float column_major_data[16];
  p.matrix().asColMajorf(column_major_data);
#else
  double column_major_data[16];
  p.matrix().asColMajord(column_major_data);
#endif
  // We do this in a single write for performance reasons.
  m->WriteBytes(&column_major_data, sizeof(SkMScalar) * 16);
}

bool ParamTraits<gfx::Transform>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* r) {
  const char* column_major_data;
  if (!iter->ReadBytes(&column_major_data, sizeof(SkMScalar) * 16))
    return false;
  r->matrix().setColMajor(
      reinterpret_cast<const SkMScalar*>(column_major_data));
  return true;
}

void ParamTraits<gfx::Transform>::Log(
    const param_type& p, std::string* l) {
#ifdef SK_MSCALAR_IS_FLOAT
  float row_major_data[16];
  p.matrix().asRowMajorf(row_major_data);
#else
  double row_major_data[16];
  p.matrix().asRowMajord(row_major_data);
#endif
  l->append("(");
  for (int i = 0; i < 16; ++i) {
    if (i > 0)
      l->append(", ");
    LogParam(row_major_data[i], l);
  }
  l->append(") ");
}

}  // namespace IPC

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/metafile_skia_wrapper.h"
#include "skia/ext/platform_device.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkMetaData.h"

namespace printing {

namespace {

const char* kMetafileKey = "CrMetafile";
const char* kCustomScaleKey = "CrCustomScale";

}  // namespace

// static
void MetafileSkiaWrapper::SetMetafileOnCanvas(const SkCanvas& canvas,
                                              Metafile* metafile) {
  skia::RefPtr<MetafileSkiaWrapper> wrapper;
  if (metafile)
    wrapper = skia::AdoptRef(new MetafileSkiaWrapper(metafile));

  SkMetaData& meta = skia::getMetaData(canvas);
  meta.setRefCnt(kMetafileKey, wrapper.get());
}

// static
Metafile* MetafileSkiaWrapper::GetMetafileFromCanvas(const SkCanvas& canvas) {
  SkMetaData& meta = skia::getMetaData(canvas);
  SkRefCnt* value;
  if (!meta.findRefCnt(kMetafileKey, &value) || !value)
    return NULL;

  return static_cast<MetafileSkiaWrapper*>(value)->metafile_;
}

// static
void MetafileSkiaWrapper::SetCustomScaleOnCanvas(const SkCanvas& canvas,
                                                 double scale) {
  SkMetaData& meta = skia::getMetaData(canvas);
  meta.setScalar(kCustomScaleKey, SkFloatToScalar(scale));
}

// static
bool MetafileSkiaWrapper::GetCustomScaleOnCanvas(const SkCanvas& canvas,
                                                 double* scale) {
  SkMetaData& meta = skia::getMetaData(canvas);
  SkScalar value;
  if (!meta.findScalar(kCustomScaleKey, &value))
    return false;

  *scale = SkScalarToFloat(value);
  return true;
}

MetafileSkiaWrapper::MetafileSkiaWrapper(Metafile* metafile)
    : metafile_(metafile) {
}

}  // namespace printing

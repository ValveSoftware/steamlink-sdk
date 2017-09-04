// Copyright 2016 The PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "core/fxcodec/codec/ccodec_iccmodule.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  CCodec_IccModule icc_module;
  uint32_t nComponent = 0;
  void* transform = icc_module.CreateTransform_sRGB(data, size, nComponent);

  if (transform) {
    FX_FLOAT src[4];
    FX_FLOAT dst[4];
    for (int i = 0; i < 4; i++)
      src[i] = 0.5f;
    icc_module.SetComponents(nComponent);
    icc_module.Translate(transform, src, dst);
    icc_module.DestroyTransform(transform);
  }

  return 0;
}

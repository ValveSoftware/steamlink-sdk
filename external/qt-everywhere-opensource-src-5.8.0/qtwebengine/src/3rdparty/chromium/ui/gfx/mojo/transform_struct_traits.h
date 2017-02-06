// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJO_TRANSFORM_STRUCT_TRAITS_H_
#define UI_GFX_MOJO_TRANSFORM_STRUCT_TRAITS_H_

#include "ui/gfx/mojo/transform.mojom.h"
#include "ui/gfx/transform.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::Transform, gfx::Transform> {
  static mojo::Array<float> matrix(const gfx::Transform& transform) {
    std::vector<float> storage(16);
    transform.matrix().asRowMajorf(&storage[0]);
    mojo::Array<float> matrix;
    matrix.Swap(&storage);
    return matrix;
  }

  static bool Read(gfx::mojom::TransformDataView data, gfx::Transform* out) {
    mojo::Array<float> matrix;
    if (!data.ReadMatrix(&matrix))
      return false;
    out->matrix().setRowMajorf(&matrix.storage()[0]);
    return true;
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJO_TRANSFORM_STRUCT_TRAITS_H_

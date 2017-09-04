// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_FILTER_OPERATION_STRUCT_TRAITS_H_
#define CC_IPC_FILTER_OPERATION_STRUCT_TRAITS_H_

#include "cc/ipc/filter_operation.mojom-shared.h"
#include "cc/output/filter_operation.h"
#include "skia/public/interfaces/image_filter_struct_traits.h"

namespace mojo {

namespace {
cc::mojom::FilterType CCFilterTypeToMojo(
    const cc::FilterOperation::FilterType& type) {
  switch (type) {
    case cc::FilterOperation::GRAYSCALE:
      return cc::mojom::FilterType::GRAYSCALE;
    case cc::FilterOperation::SEPIA:
      return cc::mojom::FilterType::SEPIA;
    case cc::FilterOperation::SATURATE:
      return cc::mojom::FilterType::SATURATE;
    case cc::FilterOperation::HUE_ROTATE:
      return cc::mojom::FilterType::HUE_ROTATE;
    case cc::FilterOperation::INVERT:
      return cc::mojom::FilterType::INVERT;
    case cc::FilterOperation::BRIGHTNESS:
      return cc::mojom::FilterType::BRIGHTNESS;
    case cc::FilterOperation::CONTRAST:
      return cc::mojom::FilterType::CONTRAST;
    case cc::FilterOperation::OPACITY:
      return cc::mojom::FilterType::OPACITY;
    case cc::FilterOperation::BLUR:
      return cc::mojom::FilterType::BLUR;
    case cc::FilterOperation::DROP_SHADOW:
      return cc::mojom::FilterType::DROP_SHADOW;
    case cc::FilterOperation::COLOR_MATRIX:
      return cc::mojom::FilterType::COLOR_MATRIX;
    case cc::FilterOperation::ZOOM:
      return cc::mojom::FilterType::ZOOM;
    case cc::FilterOperation::REFERENCE:
      return cc::mojom::FilterType::REFERENCE;
    case cc::FilterOperation::SATURATING_BRIGHTNESS:
      return cc::mojom::FilterType::SATURATING_BRIGHTNESS;
    case cc::FilterOperation::ALPHA_THRESHOLD:
      NOTREACHED();
      return cc::mojom::FilterType::ALPHA_THRESHOLD;
  }
  NOTREACHED();
  return cc::mojom::FilterType::FILTER_TYPE_LAST;
}

cc::FilterOperation::FilterType MojoFilterTypeToCC(
    const cc::mojom::FilterType& type) {
  switch (type) {
    case cc::mojom::FilterType::GRAYSCALE:
      return cc::FilterOperation::GRAYSCALE;
    case cc::mojom::FilterType::SEPIA:
      return cc::FilterOperation::SEPIA;
    case cc::mojom::FilterType::SATURATE:
      return cc::FilterOperation::SATURATE;
    case cc::mojom::FilterType::HUE_ROTATE:
      return cc::FilterOperation::HUE_ROTATE;
    case cc::mojom::FilterType::INVERT:
      return cc::FilterOperation::INVERT;
    case cc::mojom::FilterType::BRIGHTNESS:
      return cc::FilterOperation::BRIGHTNESS;
    case cc::mojom::FilterType::CONTRAST:
      return cc::FilterOperation::CONTRAST;
    case cc::mojom::FilterType::OPACITY:
      return cc::FilterOperation::OPACITY;
    case cc::mojom::FilterType::BLUR:
      return cc::FilterOperation::BLUR;
    case cc::mojom::FilterType::DROP_SHADOW:
      return cc::FilterOperation::DROP_SHADOW;
    case cc::mojom::FilterType::COLOR_MATRIX:
      return cc::FilterOperation::COLOR_MATRIX;
    case cc::mojom::FilterType::ZOOM:
      return cc::FilterOperation::ZOOM;
    case cc::mojom::FilterType::REFERENCE:
      return cc::FilterOperation::REFERENCE;
    case cc::mojom::FilterType::SATURATING_BRIGHTNESS:
      return cc::FilterOperation::SATURATING_BRIGHTNESS;
    case cc::mojom::FilterType::ALPHA_THRESHOLD:
      NOTREACHED();
      return cc::FilterOperation::ALPHA_THRESHOLD;
  }
  NOTREACHED();
  return cc::FilterOperation::FILTER_TYPE_LAST;
}

}  // namespace

using FilterOperationMatrix = CArray<float>;

template <>
struct StructTraits<cc::mojom::FilterOperationDataView, cc::FilterOperation> {
  static cc::mojom::FilterType type(const cc::FilterOperation& op) {
    return CCFilterTypeToMojo(op.type());
  }

  static float amount(const cc::FilterOperation& operation) {
    if (operation.type() == cc::FilterOperation::COLOR_MATRIX ||
        operation.type() == cc::FilterOperation::REFERENCE) {
      return 0.f;
    }
    return operation.amount();
  }

  static gfx::Point drop_shadow_offset(const cc::FilterOperation& operation) {
    if (operation.type() != cc::FilterOperation::DROP_SHADOW)
      return gfx::Point();
    return operation.drop_shadow_offset();
  }

  static uint32_t drop_shadow_color(const cc::FilterOperation& operation) {
    if (operation.type() != cc::FilterOperation::DROP_SHADOW)
      return 0;
    return operation.drop_shadow_color();
  }

  static sk_sp<SkImageFilter> image_filter(
      const cc::FilterOperation& operation) {
    if (operation.type() != cc::FilterOperation::REFERENCE)
      return sk_sp<SkImageFilter>();
    return operation.image_filter();
  }

  static FilterOperationMatrix matrix(const cc::FilterOperation& operation) {
    if (operation.type() != cc::FilterOperation::COLOR_MATRIX)
      return FilterOperationMatrix();
    constexpr size_t MATRIX_SIZE = 20;
    return {MATRIX_SIZE, MATRIX_SIZE,
            const_cast<float*>(&operation.matrix()[0])};
  }

  static int32_t zoom_inset(const cc::FilterOperation& operation) {
    if (operation.type() != cc::FilterOperation::ZOOM)
      return 0;
    return operation.zoom_inset();
  }

  static bool Read(cc::mojom::FilterOperationDataView data,
                   cc::FilterOperation* out) {
    out->set_type(MojoFilterTypeToCC(data.type()));
    switch (out->type()) {
      case cc::FilterOperation::GRAYSCALE:
      case cc::FilterOperation::SEPIA:
      case cc::FilterOperation::SATURATE:
      case cc::FilterOperation::HUE_ROTATE:
      case cc::FilterOperation::INVERT:
      case cc::FilterOperation::BRIGHTNESS:
      case cc::FilterOperation::SATURATING_BRIGHTNESS:
      case cc::FilterOperation::CONTRAST:
      case cc::FilterOperation::OPACITY:
      case cc::FilterOperation::BLUR:
        out->set_amount(data.amount());
        return true;
      case cc::FilterOperation::DROP_SHADOW: {
        out->set_amount(data.amount());
        gfx::Point offset;
        if (!data.ReadDropShadowOffset(&offset))
          return false;
        out->set_drop_shadow_offset(offset);
        out->set_drop_shadow_color(data.drop_shadow_color());
        return true;
      }
      case cc::FilterOperation::COLOR_MATRIX: {
        // TODO(fsamuel): It would be nice to modify cc::FilterOperation to
        // avoid this extra copy.
        constexpr size_t MATRIX_SIZE = 20;
        float matrix_buffer[MATRIX_SIZE];
        memset(&matrix_buffer[0], 0, sizeof(matrix_buffer));
        FilterOperationMatrix matrix = {MATRIX_SIZE, MATRIX_SIZE,
                                        &matrix_buffer[0]};
        if (!data.ReadMatrix(&matrix))
          return false;
        out->set_matrix(matrix_buffer);
        return true;
      }
      case cc::FilterOperation::ZOOM: {
        if (data.amount() < 0.f || data.zoom_inset() < 0)
          return false;
        out->set_amount(data.amount());
        out->set_zoom_inset(data.zoom_inset());
        return true;
      }
      case cc::FilterOperation::REFERENCE: {
        sk_sp<SkImageFilter> filter;
        if (!data.ReadImageFilter(&filter))
          return false;
        out->set_image_filter(filter);
        return true;
      }
      case cc::FilterOperation::ALPHA_THRESHOLD:
        // TODO(fsamuel): We cannot serialize this type.
        return false;
    }
    return false;
  }
};

}  // namespace mojo

#endif  // CC_IPC_FILTER_OPERATION_STRUCT_TRAITS_H_

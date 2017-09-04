// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_SHARED_QUAD_STATE_STRUCT_TRAITS_H_
#define CC_IPC_SHARED_QUAD_STATE_STRUCT_TRAITS_H_

#include "cc/ipc/shared_quad_state.mojom-shared.h"
#include "cc/quads/shared_quad_state.h"

namespace mojo {

struct OptSharedQuadState {
  const cc::SharedQuadState* sqs;
};

template <>
struct StructTraits<cc::mojom::SharedQuadStateDataView, OptSharedQuadState> {
  static bool IsNull(const OptSharedQuadState& input) { return !input.sqs; }

  static void SetToNull(OptSharedQuadState* output) { output->sqs = nullptr; }

  static const gfx::Transform& quad_to_target_transform(
      const OptSharedQuadState& input) {
    return input.sqs->quad_to_target_transform;
  }

  static const gfx::Size& quad_layer_bounds(const OptSharedQuadState& input) {
    return input.sqs->quad_layer_bounds;
  }

  static const gfx::Rect& visible_quad_layer_rect(
      const OptSharedQuadState& input) {
    return input.sqs->visible_quad_layer_rect;
  }

  static const gfx::Rect& clip_rect(const OptSharedQuadState& input) {
    return input.sqs->clip_rect;
  }

  static bool is_clipped(const OptSharedQuadState& input) {
    return input.sqs->is_clipped;
  }

  static float opacity(const OptSharedQuadState& input) {
    return input.sqs->opacity;
  }

  static uint32_t blend_mode(const OptSharedQuadState& input) {
    return input.sqs->blend_mode;
  }

  static int32_t sorting_context_id(const OptSharedQuadState& input) {
    return input.sqs->sorting_context_id;
  }
};

template <>
struct StructTraits<cc::mojom::SharedQuadStateDataView, cc::SharedQuadState> {
  static const gfx::Transform& quad_to_target_transform(
      const cc::SharedQuadState& sqs) {
    return sqs.quad_to_target_transform;
  }

  static const gfx::Size& quad_layer_bounds(const cc::SharedQuadState& sqs) {
    return sqs.quad_layer_bounds;
  }

  static const gfx::Rect& visible_quad_layer_rect(
      const cc::SharedQuadState& sqs) {
    return sqs.visible_quad_layer_rect;
  }

  static const gfx::Rect& clip_rect(const cc::SharedQuadState& sqs) {
    return sqs.clip_rect;
  }

  static bool is_clipped(const cc::SharedQuadState& sqs) {
    return sqs.is_clipped;
  }

  static float opacity(const cc::SharedQuadState& sqs) { return sqs.opacity; }

  static uint32_t blend_mode(const cc::SharedQuadState& sqs) {
    return sqs.blend_mode;
  }

  static int32_t sorting_context_id(const cc::SharedQuadState& sqs) {
    return sqs.sorting_context_id;
  }

  static bool Read(cc::mojom::SharedQuadStateDataView data,
                   cc::SharedQuadState* out) {
    if (!data.ReadQuadToTargetTransform(&out->quad_to_target_transform) ||
        !data.ReadQuadLayerBounds(&out->quad_layer_bounds) ||
        !data.ReadVisibleQuadLayerRect(&out->visible_quad_layer_rect) ||
        !data.ReadClipRect(&out->clip_rect)) {
      return false;
    }

    out->is_clipped = data.is_clipped();
    out->opacity = data.opacity();
    if (data.blend_mode() > SkXfermode::kLastMode)
      return false;
    out->blend_mode = static_cast<SkXfermode::Mode>(data.blend_mode());
    out->sorting_context_id = data.sorting_context_id();
    return true;
  }
};

}  // namespace mojo

#endif  // CC_IPC_SHARED_QUAD_STATE_STRUCT_TRAITS_H_

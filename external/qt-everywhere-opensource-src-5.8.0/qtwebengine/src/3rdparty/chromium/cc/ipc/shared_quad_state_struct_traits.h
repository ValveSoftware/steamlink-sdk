// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_SHARED_QUAD_STATE_STRUCT_TRAITS_H_
#define CC_IPC_SHARED_QUAD_STATE_STRUCT_TRAITS_H_

#include "cc/ipc/shared_quad_state.mojom.h"
#include "cc/quads/shared_quad_state.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::SharedQuadState, cc::SharedQuadState> {
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

struct SharedQuadStateListArray {
  cc::SharedQuadStateList* list;
};

template <>
struct ArrayTraits<SharedQuadStateListArray> {
  using Element = cc::SharedQuadState;
  using Iterator = cc::SharedQuadStateList::Iterator;
  using ConstIterator = cc::SharedQuadStateList::ConstIterator;

  static ConstIterator GetBegin(const SharedQuadStateListArray& input) {
    return input.list->begin();
  }
  static Iterator GetBegin(SharedQuadStateListArray& input) {
    return input.list->begin();
  }
  static void AdvanceIterator(ConstIterator& iterator) { ++iterator; }
  static void AdvanceIterator(Iterator& iterator) { ++iterator; }
  static const Element& GetValue(ConstIterator& iterator) { return **iterator; }
  static Element& GetValue(Iterator& iterator) { return **iterator; }
  static size_t GetSize(const SharedQuadStateListArray& input) {
    return input.list->size();
  }
  static bool Resize(SharedQuadStateListArray& input, size_t size) {
    if (input.list->size() == size)
      return true;
    input.list->clear();
    for (size_t i = 0; i < size; ++i)
      input.list->AllocateAndConstruct<cc::SharedQuadState>();
    return true;
  }
};

template <>
struct StructTraits<cc::mojom::SharedQuadStateList, cc::SharedQuadStateList> {
  static SharedQuadStateListArray shared_quad_states(
      const cc::SharedQuadStateList& list) {
    return {const_cast<cc::SharedQuadStateList*>(&list)};
  }

  static bool Read(cc::mojom::SharedQuadStateListDataView data,
                   cc::SharedQuadStateList* out) {
    SharedQuadStateListArray sqs_array = {out};
    return data.ReadSharedQuadStates(&sqs_array);
  }
};

}  // namespace mojo

#endif  // CC_IPC_SHARED_QUAD_STATE_STRUCT_TRAITS_H_

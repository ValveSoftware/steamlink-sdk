// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_RENDER_PASS_STRUCT_TRAITS_H_
#define CC_IPC_RENDER_PASS_STRUCT_TRAITS_H_

#include "cc/ipc/quads_struct_traits.h"
#include "cc/ipc/render_pass.mojom.h"
#include "cc/ipc/render_pass_id_struct_traits.h"
#include "cc/quads/render_pass.h"
#include "ui/gfx/mojo/transform_struct_traits.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::RenderPass, std::unique_ptr<cc::RenderPass>> {
  static void* SetUpContext(const std::unique_ptr<cc::RenderPass>& input);
  static void TearDownContext(const std::unique_ptr<cc::RenderPass>& input,
                              void* context);

  static const cc::RenderPassId& id(
      const std::unique_ptr<cc::RenderPass>& input) {
    return input->id;
  }

  static const gfx::Rect& output_rect(
      const std::unique_ptr<cc::RenderPass>& input) {
    return input->output_rect;
  }

  static const gfx::Rect& damage_rect(
      const std::unique_ptr<cc::RenderPass>& input) {
    return input->damage_rect;
  }

  static const gfx::Transform& transform_to_root_target(
      const std::unique_ptr<cc::RenderPass>& input) {
    return input->transform_to_root_target;
  }

  static bool has_transparent_background(
      const std::unique_ptr<cc::RenderPass>& input) {
    return input->has_transparent_background;
  }

  static const cc::QuadList& quad_list(
      const std::unique_ptr<cc::RenderPass>& input) {
    return input->quad_list;
  }

  static const mojo::Array<uint32_t>& shared_quad_state_references(
      const std::unique_ptr<cc::RenderPass>& input,
      void* context) {
    return *static_cast<mojo::Array<uint32_t>*>(context);
  }

  static const cc::SharedQuadStateList& shared_quad_state_list(
      const std::unique_ptr<cc::RenderPass>& input) {
    return input->shared_quad_state_list;
  }

  static bool Read(cc::mojom::RenderPassDataView data,
                   std::unique_ptr<cc::RenderPass>* out);
};

}  // namespace mojo

#endif  // CC_IPC_RENDER_PASS_STRUCT_TRAITS_H_

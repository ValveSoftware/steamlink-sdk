// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/ipc/render_pass_struct_traits.h"

#include "base/numerics/safe_conversions.h"
#include "cc/ipc/shared_quad_state_struct_traits.h"

namespace mojo {

// static
bool StructTraits<cc::mojom::RenderPassDataView,
                  std::unique_ptr<cc::RenderPass>>::
    Read(cc::mojom::RenderPassDataView data,
         std::unique_ptr<cc::RenderPass>* out) {
  *out = cc::RenderPass::Create();
  if (!data.ReadId(&(*out)->id) || !data.ReadOutputRect(&(*out)->output_rect) ||
      !data.ReadDamageRect(&(*out)->damage_rect) ||
      !data.ReadTransformToRootTarget(&(*out)->transform_to_root_target)) {
    return false;
  }
  (*out)->has_transparent_background = data.has_transparent_background();

  mojo::ArrayDataView<cc::mojom::DrawQuadDataView> quads;
  data.GetQuadListDataView(&quads);
  cc::SharedQuadState* last_sqs = nullptr;
  for (size_t i = 0; i < quads.size(); ++i) {
    cc::mojom::DrawQuadDataView quad_data_view;
    quads.GetDataView(i, &quad_data_view);
    cc::mojom::DrawQuadStateDataView quad_state_data_view;
    quad_data_view.GetDrawQuadStateDataView(&quad_state_data_view);

    cc::DrawQuad* quad =
        AllocateAndConstruct(quad_state_data_view.tag(), &(*out)->quad_list);
    if (!quad)
      return false;
    if (!quads.Read(i, quad))
      return false;

    // Read the SharedQuadState.
    cc::mojom::SharedQuadStateDataView sqs_data_view;
    quad_data_view.GetSqsDataView(&sqs_data_view);
    // If there is no seralized SharedQuadState then used the last deseriaized
    // one.
    if (!sqs_data_view.is_null()) {
      last_sqs = (*out)->CreateAndAppendSharedQuadState();
      if (!quad_data_view.ReadSqs(last_sqs))
        return false;
    }
    quad->shared_quad_state = last_sqs;
    if (!quad->shared_quad_state)
      return false;
  }
  return true;
}

}  // namespace mojo

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/ipc/render_pass_struct_traits.h"

#include "base/numerics/safe_conversions.h"
#include "cc/ipc/shared_quad_state_struct_traits.h"

namespace mojo {

// static
void* StructTraits<cc::mojom::RenderPass, std::unique_ptr<cc::RenderPass>>::
    SetUpContext(const std::unique_ptr<cc::RenderPass>& input) {
  DCHECK_GT(input->quad_list.size(), 0u);
  std::unique_ptr<mojo::Array<uint32_t>> sqs_references(
      new mojo::Array<uint32_t>(input->quad_list.size()));
  cc::SharedQuadStateList::ConstIterator sqs_iter =
      input->shared_quad_state_list.begin();
  for (auto it = input->quad_list.begin(); it != input->quad_list.end(); ++it) {
    if ((*it)->shared_quad_state != *sqs_iter)
      ++sqs_iter;
    (*sqs_references)[it.index()] =
        base::checked_cast<uint32_t>(sqs_iter.index());
    DCHECK_NE(nullptr, (*it)->shared_quad_state);
    DCHECK_EQ(*sqs_iter, (*it)->shared_quad_state);
  }
  DCHECK_EQ(input->shared_quad_state_list.size() - 1, sqs_iter.index());
  return sqs_references.release();
}

// static
void StructTraits<cc::mojom::RenderPass, std::unique_ptr<cc::RenderPass>>::
    TearDownContext(const std::unique_ptr<cc::RenderPass>& input,
                    void* context) {
  // static_cast to ensure the destructor is called.
  delete static_cast<mojo::Array<uint32_t>*>(context);
}

// static
bool StructTraits<cc::mojom::RenderPass, std::unique_ptr<cc::RenderPass>>::Read(
    cc::mojom::RenderPassDataView data,
    std::unique_ptr<cc::RenderPass>* out) {
  *out = cc::RenderPass::Create();
  if (!data.ReadId(&(*out)->id) || !data.ReadOutputRect(&(*out)->output_rect) ||
      !data.ReadDamageRect(&(*out)->damage_rect) ||
      !data.ReadTransformToRootTarget(&(*out)->transform_to_root_target)) {
    return false;
  }
  (*out)->has_transparent_background = data.has_transparent_background();
  if (!data.ReadQuadList(&(*out)->quad_list) ||
      !data.ReadSharedQuadStateList(&(*out)->shared_quad_state_list))
    return false;

  mojo::Array<uint32_t> shared_quad_state_references;
  if (!data.ReadSharedQuadStateReferences(&shared_quad_state_references))
    return false;

  if ((*out)->quad_list.size() != shared_quad_state_references.size())
    return false;

  cc::SharedQuadStateList::ConstIterator sqs_iter =
      (*out)->shared_quad_state_list.begin();
  for (auto it = (*out)->quad_list.begin(); it != (*out)->quad_list.end();
       ++it) {
    if (sqs_iter.index() != shared_quad_state_references[it.index()])
      ++sqs_iter;
    if (shared_quad_state_references[it.index()] != sqs_iter.index())
      return false;
    (*it)->shared_quad_state = *sqs_iter;
    if (!(*it)->shared_quad_state)
      return false;
  }
  return sqs_iter.index() == (*out)->shared_quad_state_list.size() - 1;
}

}  // namespace mojo

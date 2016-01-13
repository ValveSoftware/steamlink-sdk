// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/render_pass.h"

#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/debug/traced_value.h"
#include "cc/output/copy_output_request.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"

namespace {
const size_t kDefaultNumSharedQuadStatesToReserve = 32;
const size_t kDefaultNumQuadsToReserve = 128;
}

namespace cc {

void* RenderPass::Id::AsTracingId() const {
  COMPILE_ASSERT(sizeof(size_t) <= sizeof(void*),  // NOLINT
                 size_t_bigger_than_pointer);
  return reinterpret_cast<void*>(base::HashPair(layer_id, index));
}

scoped_ptr<RenderPass> RenderPass::Create() {
  return make_scoped_ptr(new RenderPass());
}

scoped_ptr<RenderPass> RenderPass::Create(size_t num_layers) {
  return make_scoped_ptr(new RenderPass(num_layers));
}

RenderPass::RenderPass() : id(Id(-1, -1)), has_transparent_background(true) {
  shared_quad_state_list.reserve(kDefaultNumSharedQuadStatesToReserve);
  quad_list.reserve(kDefaultNumQuadsToReserve);
}

RenderPass::RenderPass(size_t num_layers)
    : id(Id(-1, -1)), has_transparent_background(true) {
  // Each layer usually produces one shared quad state, so the number of layers
  // is a good hint for what to reserve here.
  shared_quad_state_list.reserve(num_layers);
  quad_list.reserve(kDefaultNumQuadsToReserve);
}

RenderPass::~RenderPass() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.quads"),
      "cc::RenderPass", id.AsTracingId());
}

scoped_ptr<RenderPass> RenderPass::Copy(Id new_id) const {
  scoped_ptr<RenderPass> copy_pass(Create());
  copy_pass->SetAll(new_id,
                    output_rect,
                    damage_rect,
                    transform_to_root_target,
                    has_transparent_background);
  return copy_pass.Pass();
}

// static
void RenderPass::CopyAll(const ScopedPtrVector<RenderPass>& in,
                         ScopedPtrVector<RenderPass>* out) {
  for (size_t i = 0; i < in.size(); ++i) {
    RenderPass* source = in[i];

    // Since we can't copy these, it's wrong to use CopyAll in a situation where
    // you may have copy_requests present.
    DCHECK_EQ(source->copy_requests.size(), 0u);

    scoped_ptr<RenderPass> copy_pass(Create());
    copy_pass->SetAll(source->id,
                      source->output_rect,
                      source->damage_rect,
                      source->transform_to_root_target,
                      source->has_transparent_background);
    for (size_t i = 0; i < source->shared_quad_state_list.size(); ++i) {
      SharedQuadState* copy_shared_quad_state =
          copy_pass->CreateAndAppendSharedQuadState();
      copy_shared_quad_state->CopyFrom(source->shared_quad_state_list[i]);
    }
    for (size_t i = 0, sqs_i = 0; i < source->quad_list.size(); ++i) {
      while (source->quad_list[i]->shared_quad_state !=
             source->shared_quad_state_list[sqs_i]) {
        ++sqs_i;
        DCHECK_LT(sqs_i, source->shared_quad_state_list.size());
      }
      DCHECK(source->quad_list[i]->shared_quad_state ==
             source->shared_quad_state_list[sqs_i]);

      DrawQuad* quad = source->quad_list[i];

      if (quad->material == DrawQuad::RENDER_PASS) {
        const RenderPassDrawQuad* pass_quad =
            RenderPassDrawQuad::MaterialCast(quad);
        copy_pass->quad_list.push_back(
            pass_quad->Copy(copy_pass->shared_quad_state_list[sqs_i],
                            pass_quad->render_pass_id).PassAs<DrawQuad>());
      } else {
        copy_pass->quad_list.push_back(source->quad_list[i]->Copy(
            copy_pass->shared_quad_state_list[sqs_i]));
      }
    }
    out->push_back(copy_pass.Pass());
  }
}

void RenderPass::SetNew(Id id,
                        const gfx::Rect& output_rect,
                        const gfx::Rect& damage_rect,
                        const gfx::Transform& transform_to_root_target) {
  DCHECK_GT(id.layer_id, 0);
  DCHECK_GE(id.index, 0);
  DCHECK(damage_rect.IsEmpty() || output_rect.Contains(damage_rect))
      << "damage_rect: " << damage_rect.ToString()
      << " output_rect: " << output_rect.ToString();

  this->id = id;
  this->output_rect = output_rect;
  this->damage_rect = damage_rect;
  this->transform_to_root_target = transform_to_root_target;

  DCHECK(quad_list.empty());
  DCHECK(shared_quad_state_list.empty());
}

void RenderPass::SetAll(Id id,
                        const gfx::Rect& output_rect,
                        const gfx::Rect& damage_rect,
                        const gfx::Transform& transform_to_root_target,
                        bool has_transparent_background) {
  DCHECK_GT(id.layer_id, 0);
  DCHECK_GE(id.index, 0);

  this->id = id;
  this->output_rect = output_rect;
  this->damage_rect = damage_rect;
  this->transform_to_root_target = transform_to_root_target;
  this->has_transparent_background = has_transparent_background;

  DCHECK(quad_list.empty());
  DCHECK(shared_quad_state_list.empty());
}

scoped_ptr<base::Value> RenderPass::AsValue() const {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->Set("output_rect", MathUtil::AsValue(output_rect).release());
  value->Set("damage_rect", MathUtil::AsValue(damage_rect).release());
  value->SetBoolean("has_transparent_background", has_transparent_background);
  value->SetInteger("copy_requests", copy_requests.size());
  scoped_ptr<base::ListValue> shared_states_value(new base::ListValue());
  for (size_t i = 0; i < shared_quad_state_list.size(); ++i) {
    shared_states_value->Append(shared_quad_state_list[i]->AsValue().release());
  }
  value->Set("shared_quad_state_list", shared_states_value.release());
  scoped_ptr<base::ListValue> quad_list_value(new base::ListValue());
  for (size_t i = 0; i < quad_list.size(); ++i) {
    quad_list_value->Append(quad_list[i]->AsValue().release());
  }
  value->Set("quad_list", quad_list_value.release());

  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.quads"),
      value.get(), "cc::RenderPass", id.AsTracingId());
  return value.PassAs<base::Value>();
}

SharedQuadState* RenderPass::CreateAndAppendSharedQuadState() {
  shared_quad_state_list.push_back(make_scoped_ptr(new SharedQuadState));
  return shared_quad_state_list.back();
}

void RenderPass::AppendDrawQuad(scoped_ptr<DrawQuad> draw_quad) {
  quad_list.push_back(draw_quad.Pass());
}

}  // namespace cc

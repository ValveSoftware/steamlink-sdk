// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_AGGREGATOR_H_
#define CC_SURFACES_SURFACE_AGGREGATOR_H_

#include <set>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "cc/quads/render_pass.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {

class CompositorFrame;
class DelegatedFrameData;
class SurfaceDrawQuad;
class SurfaceManager;

class CC_SURFACES_EXPORT SurfaceAggregator {
 public:
  explicit SurfaceAggregator(SurfaceManager* manager);
  ~SurfaceAggregator();

  scoped_ptr<CompositorFrame> Aggregate(SurfaceId surface_id);

 private:
  DelegatedFrameData* GetReferencedDataForSurfaceId(SurfaceId surface_id);
  RenderPass::Id RemapPassId(RenderPass::Id surface_local_pass_id,
                             int surface_id);

  void HandleSurfaceQuad(const SurfaceDrawQuad* surface_quad,
                         RenderPass* dest_pass);
  void CopySharedQuadState(const SharedQuadState* source_sqs,
                           const gfx::Transform& content_to_target_transform,
                           RenderPass* dest_render_pass);
  void CopyQuadsToPass(const QuadList& source_quad_list,
                       const SharedQuadStateList& source_shared_quad_state_list,
                       const gfx::Transform& content_to_target_transform,
                       RenderPass* dest_pass,
                       int surface_id);
  void CopyPasses(const RenderPassList& source_pass_list, int surface_id);

  SurfaceManager* manager_;

  class RenderPassIdAllocator;
  typedef base::ScopedPtrHashMap<int, RenderPassIdAllocator>
      RenderPassIdAllocatorMap;
  RenderPassIdAllocatorMap render_pass_allocator_map_;

  // The following state is only valid for the duration of one Aggregate call
  // and is only stored on the class to avoid having to pass through every
  // function call.

  // This is the set of surfaces referenced in the aggregation so far, used to
  // detect cycles.
  std::set<int> referenced_surfaces_;

  // This is the pass list for the aggregated frame.
  RenderPassList* dest_pass_list_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceAggregator);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_AGGREGATOR_H_

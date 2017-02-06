// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_MUTATOR_HOST_CLIENT_H_
#define CC_TREES_MUTATOR_HOST_CLIENT_H_

#include "cc/animation/element_id.h"

namespace gfx {
class Transform;
class ScrollOffset;
}

namespace cc {

class FilterOperations;

enum class ElementListType { ACTIVE, PENDING };

enum class AnimationChangeType { POTENTIAL, RUNNING, BOTH };

class MutatorHostClient {
 public:
  virtual bool IsElementInList(ElementId element_id,
                               ElementListType list_type) const = 0;

  virtual void SetMutatorsNeedCommit() = 0;
  virtual void SetMutatorsNeedRebuildPropertyTrees() = 0;

  virtual void SetElementFilterMutated(ElementId element_id,
                                       ElementListType list_type,
                                       const FilterOperations& filters) = 0;
  virtual void SetElementOpacityMutated(ElementId element_id,
                                        ElementListType list_type,
                                        float opacity) = 0;
  virtual void SetElementTransformMutated(ElementId element_id,
                                          ElementListType list_type,
                                          const gfx::Transform& transform) = 0;
  virtual void SetElementScrollOffsetMutated(
      ElementId element_id,
      ElementListType list_type,
      const gfx::ScrollOffset& scroll_offset) = 0;

  virtual void ElementTransformIsAnimatingChanged(
      ElementId element_id,
      ElementListType list_type,
      AnimationChangeType change_type,
      bool is_animating) = 0;

  virtual void ElementOpacityIsAnimatingChanged(ElementId element_id,
                                                ElementListType list_type,
                                                AnimationChangeType change_type,
                                                bool is_animating) = 0;

  virtual void ScrollOffsetAnimationFinished() = 0;
  virtual gfx::ScrollOffset GetScrollOffsetForAnimation(
      ElementId element_id) const = 0;
};

}  // namespace cc

#endif  // CC_TREES_MUTATOR_HOST_CLIENT_H_

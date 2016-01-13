// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/priority_calculator.h"

#include <algorithm>

#include "ui/gfx/rect.h"

namespace cc {

static const int kNothingPriorityCutoff = -3;

static const int kMostHighPriority = -2;

static const int kUIDrawsToRootSurfacePriority = -1;
static const int kVisibleDrawsToRootSurfacePriority = 0;
static const int kRenderSurfacesPriority = 1;
static const int kUIDoesNotDrawToRootSurfacePriority = 2;
static const int kVisibleDoesNotDrawToRootSurfacePriority = 3;

static const int kVisibleOnlyPriorityCutoff = 4;

// The lower digits are how far from being visible the texture is,
// in pixels.
static const int kNotVisibleBasePriority = 1000000;
static const int kNotVisibleLimitPriority = 1900000;

// Arbitrarily define "nearby" to be 2000 pixels. A better estimate
// would be percent-of-viewport or percent-of-screen.
static const int kVisibleAndNearbyPriorityCutoff =
    kNotVisibleBasePriority + 2000;

// Small animated layers are treated as though they are 512 pixels
// from being visible.
static const int kSmallAnimatedLayerPriority = kNotVisibleBasePriority + 512;

static const int kLingeringBasePriority = 2000000;
static const int kLingeringLimitPriority = 2900000;

static const int kMostLowPriority = 3000000;

static const int kEverythingPriorityCutoff = 3000001;

// static
int PriorityCalculator::UIPriority(bool draws_to_root_surface) {
  return draws_to_root_surface ? kUIDrawsToRootSurfacePriority
                               : kUIDoesNotDrawToRootSurfacePriority;
}

// static
int PriorityCalculator::VisiblePriority(bool draws_to_root_surface) {
  return draws_to_root_surface ? kVisibleDrawsToRootSurfacePriority
                               : kVisibleDoesNotDrawToRootSurfacePriority;
}

// static
int PriorityCalculator::RenderSurfacePriority() {
  return kRenderSurfacesPriority;
}

// static
int PriorityCalculator::LingeringPriority(int previous_priority) {
  // TODO(reveman): We should remove this once we have priorities for all
  // textures (we can't currently calculate distances for off-screen textures).
  return std::min(kLingeringLimitPriority,
                  std::max(kLingeringBasePriority, previous_priority + 1));
}

// static
int PriorityCalculator::PriorityFromDistance(const gfx::Rect& visible_rect,
                                             const gfx::Rect& texture_rect,
                                             bool draws_to_root_surface) {
  int distance = visible_rect.ManhattanInternalDistance(texture_rect);
  if (!distance)
    return VisiblePriority(draws_to_root_surface);
  return std::min(kNotVisibleLimitPriority, kNotVisibleBasePriority + distance);
}

// static
int PriorityCalculator::SmallAnimatedLayerMinPriority() {
  return kSmallAnimatedLayerPriority;
}

// static
int PriorityCalculator::HighestPriority() {
  return kMostHighPriority;
}

// static
int PriorityCalculator::LowestPriority() {
  return kMostLowPriority;
}

// static
int PriorityCalculator::AllowNothingCutoff() {
  return kNothingPriorityCutoff;
}

// static
int PriorityCalculator::AllowVisibleOnlyCutoff() {
  return kVisibleOnlyPriorityCutoff;
}

// static
int PriorityCalculator::AllowVisibleAndNearbyCutoff() {
  return kVisibleAndNearbyPriorityCutoff;
}

// static
int PriorityCalculator::AllowEverythingCutoff() {
  return kEverythingPriorityCutoff;
}

}  // namespace cc

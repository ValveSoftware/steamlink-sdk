// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_SCHEDULED_ANIMATION_GROUP_H_
#define COMPONENTS_MUS_WS_SCHEDULED_ANIMATION_GROUP_H_

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "components/mus/public/interfaces/animations.mojom.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/transform.h"

namespace mus {
namespace ws {

class ServerWindow;

struct ScheduledAnimationValue {
  ScheduledAnimationValue();
  ~ScheduledAnimationValue();

  float float_value;
  gfx::Transform transform;
};

struct ScheduledAnimationElement {
  ScheduledAnimationElement();
  // Needed because we call resize() vector of elements.
  ScheduledAnimationElement(const ScheduledAnimationElement& other);
  ~ScheduledAnimationElement();

  mojom::AnimationProperty property;
  base::TimeDelta duration;
  gfx::Tween::Type tween_type;
  bool is_start_valid;
  ScheduledAnimationValue start_value;
  ScheduledAnimationValue target_value;
  // Start time is based on scheduled time and relative to any other elements
  // in the sequence.
  base::TimeTicks start_time;
};

struct ScheduledAnimationSequence {
  ScheduledAnimationSequence();
  // Needed because we call resize() and erase() on vector of sequences.
  ScheduledAnimationSequence(const ScheduledAnimationSequence& other);
  ~ScheduledAnimationSequence();

  bool run_until_stopped;
  std::vector<ScheduledAnimationElement> elements;

  // Sum of the duration of all elements. This does not take into account
  // |cycle_count|.
  base::TimeDelta duration;

  // The following values are updated as the animation progresses.

  // Number of cycles remaining. This is only used if |run_until_stopped| is
  // false.
  uint32_t cycle_count;

  // Index into |elements| of the element currently animating.
  size_t current_index;
};

// Corresponds to a mojom::AnimationGroup and is responsible for running the
// actual animation.
class ScheduledAnimationGroup {
 public:
  ~ScheduledAnimationGroup();

  // Returns a new ScheduledAnimationGroup from the supplied parameters, or
  // null if |transport_group| isn't valid.
  static std::unique_ptr<ScheduledAnimationGroup> Create(
      ServerWindow* window,
      base::TimeTicks now,
      uint32_t id,
      const mojom::AnimationGroup& transport_group);

  uint32_t id() const { return id_; }

  // Gets the start value for any elements that don't have an explicit start.
  // value.
  void ObtainStartValues();

  // Sets the values of any properties that are not in |other| to their final
  // value.
  void SetValuesToTargetValuesForPropertiesNotIn(
      const ScheduledAnimationGroup& other);

  // Advances the group. |time| is the current time. Returns true if the group
  // is done (nothing left to animate).
  bool Tick(base::TimeTicks time);

  ServerWindow* window() { return window_; }

 private:
  ScheduledAnimationGroup(ServerWindow* window,
                          uint32_t id,
                          base::TimeTicks time_scheduled);

  ServerWindow* window_;
  const uint32_t id_;
  base::TimeTicks time_scheduled_;
  std::vector<ScheduledAnimationSequence> sequences_;

  DISALLOW_COPY_AND_ASSIGN(ScheduledAnimationGroup);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_SCHEDULED_ANIMATION_GROUP_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_ANIMATION_RUNNER_H_
#define COMPONENTS_MUS_WS_ANIMATION_RUNNER_H_

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "base/observer_list.h"
#include "base/time/time.h"

namespace mus {
namespace mojom {
class AnimationGroup;
}

namespace ws {

class AnimationRunnerObserver;
class ScheduledAnimationGroup;
class ServerWindow;

// AnimationRunner is responsible for maintaining and running a set of
// animations. The animations are represented as a set of AnimationGroups. New
// animations are scheduled by way of Schedule(). A |window| may only have one
// animation running at a time. Schedule()ing a new animation for a window
// already animating implicitly cancels the current animation for the window.
// Animations progress by way of the Tick() function.
class AnimationRunner {
 public:
  using AnimationId = uint32_t;
  using WindowAndAnimationPair =
      std::pair<ServerWindow*, const mojom::AnimationGroup*>;

  explicit AnimationRunner(base::TimeTicks now);
  ~AnimationRunner();

  void AddObserver(AnimationRunnerObserver* observer);
  void RemoveObserver(AnimationRunnerObserver* observer);

  // Schedules animations. If any of the groups are not valid no animations are
  // scheuled and 0 is returned. If there is an existing animation in progress
  // for any of the windows it is canceled and any properties that were
  // animating but are no longer animating are set to their target value.
  AnimationId Schedule(const std::vector<WindowAndAnimationPair>& groups,
                       base::TimeTicks now);

  // Cancels an animation scheduled by an id that was previously returned from
  // Schedule().
  void CancelAnimation(AnimationId id);

  // Cancels the animation scheduled for |window|. Does nothing if there is no
  // animation scheduled for |window|. This does not change |window|. That is,
  // any in progress animations are stopped.
  void CancelAnimationForWindow(ServerWindow* window);

  // Advance the animations updating values appropriately.
  void Tick(base::TimeTicks time);

  // Returns true if there are animations currently scheduled.
  bool HasAnimations() const { return !window_to_animation_map_.empty(); }

  // Returns true if the animation identified by |id| is valid and animating.
  bool IsAnimating(AnimationId id) const {
    return id_to_windows_map_.count(id) > 0;
  }

  // Returns the windows that are currently animating for |id|. Returns an empty
  // set if |id| does not identify a valid animation.
  std::set<ServerWindow*> GetWindowsAnimating(AnimationId id) {
    return IsAnimating(id) ? id_to_windows_map_.find(id)->second
                           : std::set<ServerWindow*>();
  }

 private:
  enum CancelSource {
    // Cancel is the result of scheduling another animation for the window.
    CANCEL_SOURCE_SCHEDULE,

    // Cancel originates from an explicit call to cancel.
    CANCEL_SOURCE_CANCEL,
  };

  using WindowToAnimationMap =
      std::unordered_map<ServerWindow*,
                         std::unique_ptr<ScheduledAnimationGroup>>;
  using IdToWindowsMap = std::map<AnimationId, std::set<ServerWindow*>>;

  void CancelAnimationForWindowImpl(ServerWindow* window, CancelSource source);

  // Removes |window| from both |window_to_animation_map_| and
  // |id_to_windows_map_|.
  // Returns true if there are no more windows animating with the animation id
  // the window is associated with.
  bool RemoveWindowFromMaps(ServerWindow* window);

  AnimationId next_id_;

  base::TimeTicks last_tick_time_;

  base::ObserverList<AnimationRunnerObserver> observers_;

  WindowToAnimationMap window_to_animation_map_;

  IdToWindowsMap id_to_windows_map_;

  DISALLOW_COPY_AND_ASSIGN(AnimationRunner);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_ANIMATION_RUNNER_H_

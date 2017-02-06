// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/animation_runner.h"

#include "base/memory/scoped_vector.h"
#include "components/mus/ws/animation_runner_observer.h"
#include "components/mus/ws/scheduled_animation_group.h"
#include "components/mus/ws/server_window.h"

namespace mus {
namespace ws {
namespace {

bool ConvertWindowAndAnimationPairsToScheduledAnimationGroups(
    const std::vector<AnimationRunner::WindowAndAnimationPair>& pairs,
    AnimationRunner::AnimationId id,
    base::TimeTicks now,
    std::vector<std::unique_ptr<ScheduledAnimationGroup>>* groups) {
  for (const auto& window_animation_pair : pairs) {
    DCHECK(window_animation_pair.first);
    DCHECK(window_animation_pair.second);
    std::unique_ptr<ScheduledAnimationGroup> group(
        ScheduledAnimationGroup::Create(window_animation_pair.first, now, id,
                                        *(window_animation_pair.second)));
    if (!group.get())
      return false;
    groups->push_back(std::move(group));
  }
  return true;
}

}  // namespace

AnimationRunner::AnimationRunner(base::TimeTicks now)
    : next_id_(1), last_tick_time_(now) {}

AnimationRunner::~AnimationRunner() {}

void AnimationRunner::AddObserver(AnimationRunnerObserver* observer) {
  observers_.AddObserver(observer);
}

void AnimationRunner::RemoveObserver(AnimationRunnerObserver* observer) {
  observers_.RemoveObserver(observer);
}

AnimationRunner::AnimationId AnimationRunner::Schedule(
    const std::vector<WindowAndAnimationPair>& pairs,
    base::TimeTicks now) {
  DCHECK_GE(now, last_tick_time_);

  const AnimationId animation_id = next_id_++;
  std::vector<std::unique_ptr<ScheduledAnimationGroup>> groups;
  if (!ConvertWindowAndAnimationPairsToScheduledAnimationGroups(
          pairs, animation_id, now, &groups)) {
    return 0;
  }

  // Cancel any animations for the affected windows. If the cancelled animations
  // also animated properties that are not animated by the new group - instantly
  // set those to the target value.
  for (auto& group : groups) {
    ScheduledAnimationGroup* current_group =
        window_to_animation_map_[group->window()].get();
    if (current_group)
      current_group->SetValuesToTargetValuesForPropertiesNotIn(*group.get());

    CancelAnimationForWindowImpl(group->window(), CANCEL_SOURCE_SCHEDULE);
  }

  // Update internal maps
  for (auto& group : groups) {
    group->ObtainStartValues();
    ServerWindow* window = group->window();
    window_to_animation_map_[window] = std::move(group);
    DCHECK(!id_to_windows_map_[animation_id].count(window));
    id_to_windows_map_[animation_id].insert(window);
  }

  FOR_EACH_OBSERVER(AnimationRunnerObserver, observers_,
                    OnAnimationScheduled(animation_id));
  return animation_id;
}

void AnimationRunner::CancelAnimation(AnimationId id) {
  if (id_to_windows_map_.count(id) == 0)
    return;

  std::set<ServerWindow*> windows(id_to_windows_map_[id]);
  for (ServerWindow* window : windows)
    CancelAnimationForWindow(window);
}

void AnimationRunner::CancelAnimationForWindow(ServerWindow* window) {
  CancelAnimationForWindowImpl(window, CANCEL_SOURCE_CANCEL);
}

void AnimationRunner::Tick(base::TimeTicks time) {
  DCHECK(time >= last_tick_time_);
  last_tick_time_ = time;
  if (window_to_animation_map_.empty())
    return;

  // The animation ids of any windows whose animation completes are added here.
  // We notify after processing all windows so that if an observer mutates us in
  // some way we're aren't left in a weird state.
  std::set<AnimationId> animations_completed;
  for (WindowToAnimationMap::iterator i = window_to_animation_map_.begin();
       i != window_to_animation_map_.end();) {
    bool animation_done = i->second->Tick(time);
    if (animation_done) {
      const AnimationId animation_id = i->second->id();
      ServerWindow* window = i->first;
      ++i;
      bool animation_completed = RemoveWindowFromMaps(window);
      if (animation_completed)
        animations_completed.insert(animation_id);
    } else {
      ++i;
    }
  }
  for (const AnimationId& id : animations_completed) {
    FOR_EACH_OBSERVER(AnimationRunnerObserver, observers_, OnAnimationDone(id));
  }
}

void AnimationRunner::CancelAnimationForWindowImpl(ServerWindow* window,
                                                   CancelSource source) {
  if (!window_to_animation_map_[window])
    return;

  const AnimationId animation_id = window_to_animation_map_[window]->id();
  if (RemoveWindowFromMaps(window)) {
    // This was the last window in the group.
    if (source == CANCEL_SOURCE_CANCEL) {
      FOR_EACH_OBSERVER(AnimationRunnerObserver, observers_,
                        OnAnimationCanceled(animation_id));
    } else {
      FOR_EACH_OBSERVER(AnimationRunnerObserver, observers_,
                        OnAnimationInterrupted(animation_id));
    }
  }
}

bool AnimationRunner::RemoveWindowFromMaps(ServerWindow* window) {
  DCHECK(window_to_animation_map_[window]);

  const AnimationId animation_id = window_to_animation_map_[window]->id();
  window_to_animation_map_.erase(window);

  DCHECK(id_to_windows_map_.count(animation_id));
  id_to_windows_map_[animation_id].erase(window);
  if (!id_to_windows_map_[animation_id].empty())
    return false;

  id_to_windows_map_.erase(animation_id);
  return true;
}

}  // namespace ws
}  // namespace mus

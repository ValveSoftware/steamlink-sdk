// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_timeline.h"

#include <algorithm>

#include "cc/animation/animation_player.h"

namespace cc {

scoped_refptr<AnimationTimeline> AnimationTimeline::Create(int id) {
  return make_scoped_refptr(new AnimationTimeline(id));
}

AnimationTimeline::AnimationTimeline(int id)
    : id_(id), animation_host_(), is_impl_only_(false) {
}

AnimationTimeline::~AnimationTimeline() {
  for (auto& kv : id_to_player_map_)
    kv.second->SetAnimationTimeline(nullptr);
}

scoped_refptr<AnimationTimeline> AnimationTimeline::CreateImplInstance() const {
  scoped_refptr<AnimationTimeline> timeline = AnimationTimeline::Create(id());
  return timeline;
}

void AnimationTimeline::SetAnimationHost(AnimationHost* animation_host) {
  animation_host_ = animation_host;
  for (auto& kv : id_to_player_map_)
    kv.second->SetAnimationHost(animation_host);
}

void AnimationTimeline::AttachPlayer(scoped_refptr<AnimationPlayer> player) {
  DCHECK(player->id());
  player->SetAnimationHost(animation_host_);
  player->SetAnimationTimeline(this);
  id_to_player_map_.insert(std::make_pair(player->id(), std::move(player)));
}

void AnimationTimeline::DetachPlayer(scoped_refptr<AnimationPlayer> player) {
  DCHECK(player->id());
  ErasePlayer(player);
  id_to_player_map_.erase(player->id());
}

AnimationPlayer* AnimationTimeline::GetPlayerById(int player_id) const {
  auto f = id_to_player_map_.find(player_id);
  return f == id_to_player_map_.end() ? nullptr : f->second.get();
}

void AnimationTimeline::ClearPlayers() {
  for (auto& kv : id_to_player_map_)
    ErasePlayer(kv.second);
  id_to_player_map_.clear();
}

void AnimationTimeline::PushPropertiesTo(AnimationTimeline* timeline_impl) {
  PushAttachedPlayersToImplThread(timeline_impl);
  RemoveDetachedPlayersFromImplThread(timeline_impl);
  PushPropertiesToImplThread(timeline_impl);
}

void AnimationTimeline::PushAttachedPlayersToImplThread(
    AnimationTimeline* timeline_impl) const {
  for (auto& kv : id_to_player_map_) {
    auto& player = kv.second;
    AnimationPlayer* player_impl = timeline_impl->GetPlayerById(player->id());
    if (player_impl)
      continue;

    scoped_refptr<AnimationPlayer> to_add = player->CreateImplInstance();
    timeline_impl->AttachPlayer(to_add.get());
  }
}

void AnimationTimeline::RemoveDetachedPlayersFromImplThread(
    AnimationTimeline* timeline_impl) const {
  IdToPlayerMap& players_impl = timeline_impl->id_to_player_map_;

  // Erase all the impl players which |this| doesn't have.
  for (auto it = players_impl.begin(); it != players_impl.end();) {
    if (GetPlayerById(it->second->id())) {
      ++it;
    } else {
      timeline_impl->ErasePlayer(it->second);
      it = players_impl.erase(it);
    }
  }
}

void AnimationTimeline::ErasePlayer(scoped_refptr<AnimationPlayer> player) {
  if (player->element_animations())
    player->DetachElement();
  player->SetAnimationTimeline(nullptr);
  player->SetAnimationHost(nullptr);
}

void AnimationTimeline::PushPropertiesToImplThread(
    AnimationTimeline* timeline_impl) {
  for (auto& kv : id_to_player_map_) {
    AnimationPlayer* player = kv.second.get();
    AnimationPlayer* player_impl = timeline_impl->GetPlayerById(player->id());
    if (player_impl)
      player->PushPropertiesTo(player_impl);
  }
}

}  // namespace cc

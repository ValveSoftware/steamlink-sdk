// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_registrar.h"

#include "cc/animation/layer_animation_controller.h"

namespace cc {

AnimationRegistrar::AnimationRegistrar() {}

AnimationRegistrar::~AnimationRegistrar() {
  AnimationControllerMap copy = all_animation_controllers_;
  for (AnimationControllerMap::iterator iter = copy.begin();
       iter != copy.end();
       ++iter)
    (*iter).second->SetAnimationRegistrar(NULL);
}

scoped_refptr<LayerAnimationController>
AnimationRegistrar::GetAnimationControllerForId(int id) {
  scoped_refptr<LayerAnimationController> to_return;
  if (!ContainsKey(all_animation_controllers_, id)) {
    to_return = LayerAnimationController::Create(id);
    to_return->SetAnimationRegistrar(this);
    all_animation_controllers_[id] = to_return.get();
  } else {
    to_return = all_animation_controllers_[id];
  }
  return to_return;
}

void AnimationRegistrar::DidActivateAnimationController(
    LayerAnimationController* controller) {
  active_animation_controllers_[controller->id()] = controller;
}

void AnimationRegistrar::DidDeactivateAnimationController(
    LayerAnimationController* controller) {
  if (ContainsKey(active_animation_controllers_, controller->id()))
    active_animation_controllers_.erase(controller->id());
}

void AnimationRegistrar::RegisterAnimationController(
    LayerAnimationController* controller) {
  all_animation_controllers_[controller->id()] = controller;
}

void AnimationRegistrar::UnregisterAnimationController(
    LayerAnimationController* controller) {
  if (ContainsKey(all_animation_controllers_, controller->id()))
    all_animation_controllers_.erase(controller->id());
  DidDeactivateAnimationController(controller);
}

}  // namespace cc

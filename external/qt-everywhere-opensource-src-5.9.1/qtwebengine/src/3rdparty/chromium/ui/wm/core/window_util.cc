// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/window_util.h"

#include "base/memory/ptr_util.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/public/activation_client.h"

namespace {

// Invokes RecreateLayer() on all the children of |to_clone|, adding the newly
// cloned children to |parent|.
//
// WARNING: It is assumed that |parent| is ultimately owned by a LayerTreeOwner.
void CloneChildren(ui::Layer* to_clone, ui::Layer* parent) {
  typedef std::vector<ui::Layer*> Layers;
  // Make a copy of the children since RecreateLayer() mutates it.
  Layers children(to_clone->children());
  for (Layers::const_iterator i = children.begin(); i != children.end(); ++i) {
    ui::LayerOwner* owner = (*i)->owner();
    ui::Layer* old_layer = owner ? owner->RecreateLayer().release() : NULL;
    if (old_layer) {
      parent->Add(old_layer);
      // RecreateLayer() moves the existing children to the new layer. Create a
      // copy of those.
      CloneChildren(owner->layer(), old_layer);
    }
  }
}

// Invokes Mirror() on all the children of |to_mirror|, adding the newly cloned
// children to |parent|.
//
// WARNING: It is assumed that |parent| is ultimately owned by a LayerTreeOwner.
void MirrorChildren(ui::Layer* to_mirror,
                    ui::Layer* parent,
                    bool sync_bounds) {
  for (auto* child : to_mirror->children()) {
    child->set_sync_bounds(sync_bounds);
    ui::Layer* mirror = child->Mirror().release();
    parent->Add(mirror);
    MirrorChildren(child, mirror, sync_bounds);
  }
}

}  // namespace

namespace wm {

void ActivateWindow(aura::Window* window) {
  DCHECK(window);
  DCHECK(window->GetRootWindow());
  aura::client::GetActivationClient(window->GetRootWindow())->ActivateWindow(
      window);
}

void DeactivateWindow(aura::Window* window) {
  DCHECK(window);
  DCHECK(window->GetRootWindow());
  aura::client::GetActivationClient(window->GetRootWindow())->DeactivateWindow(
      window);
}

bool IsActiveWindow(aura::Window* window) {
  DCHECK(window);
  if (!window->GetRootWindow())
    return false;
  aura::client::ActivationClient* client =
      aura::client::GetActivationClient(window->GetRootWindow());
  return client && client->GetActiveWindow() == window;
}

bool CanActivateWindow(aura::Window* window) {
  DCHECK(window);
  if (!window->GetRootWindow())
    return false;
  aura::client::ActivationClient* client =
      aura::client::GetActivationClient(window->GetRootWindow());
  return client && client->CanActivateWindow(window);
}

aura::Window* GetActivatableWindow(aura::Window* window) {
  aura::client::ActivationClient* client =
      aura::client::GetActivationClient(window->GetRootWindow());
  return client ? client->GetActivatableWindow(window) : NULL;
}

aura::Window* GetToplevelWindow(aura::Window* window) {
  aura::client::ActivationClient* client =
      aura::client::GetActivationClient(window->GetRootWindow());
  return client ? client->GetToplevelWindow(window) : NULL;
}

std::unique_ptr<ui::LayerTreeOwner> RecreateLayers(ui::LayerOwner* root) {
  DCHECK(root->OwnsLayer());
  auto old_layer = base::MakeUnique<ui::LayerTreeOwner>(root->RecreateLayer());
  CloneChildren(root->layer(), old_layer->root());
  return old_layer;
}

std::unique_ptr<ui::LayerTreeOwner> MirrorLayers(
    ui::LayerOwner* root, bool sync_bounds) {
  auto mirror = base::MakeUnique<ui::LayerTreeOwner>(root->layer()->Mirror());
  MirrorChildren(root->layer(), mirror->root(), sync_bounds);
  return mirror;
}

aura::Window* GetTransientParent(aura::Window* window) {
  return const_cast<aura::Window*>(GetTransientParent(
                                 const_cast<const aura::Window*>(window)));
}

const aura::Window* GetTransientParent(const aura::Window* window) {
  const TransientWindowManager* manager = TransientWindowManager::Get(window);
  return manager ? manager->transient_parent() : NULL;
}

const std::vector<aura::Window*>& GetTransientChildren(
    const aura::Window* window) {
  const TransientWindowManager* manager = TransientWindowManager::Get(window);
  if (manager)
    return manager->transient_children();

  static std::vector<aura::Window*>* shared = new std::vector<aura::Window*>;
  return *shared;
}

void AddTransientChild(aura::Window* parent, aura::Window* child) {
  TransientWindowManager::Get(parent)->AddTransientChild(child);
}

void RemoveTransientChild(aura::Window* parent, aura::Window* child) {
  TransientWindowManager::Get(parent)->RemoveTransientChild(child);
}

bool HasTransientAncestor(const aura::Window* window,
                          const aura::Window* ancestor) {
  const aura::Window* transient_parent = GetTransientParent(window);
  if (transient_parent == ancestor)
    return true;
  return transient_parent ?
      HasTransientAncestor(transient_parent, ancestor) : false;
}

}  // namespace wm

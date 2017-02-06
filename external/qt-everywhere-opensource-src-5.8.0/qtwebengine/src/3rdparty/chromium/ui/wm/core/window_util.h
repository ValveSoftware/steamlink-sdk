// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_WINDOW_UTIL_H_
#define UI_WM_CORE_WINDOW_UTIL_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "ui/wm/wm_export.h"

namespace aura {
class Window;
}

namespace ui {
class Layer;
class LayerDelegate;
class LayerOwner;
class LayerTreeOwner;
}

namespace wm {

WM_EXPORT void ActivateWindow(aura::Window* window);
WM_EXPORT void DeactivateWindow(aura::Window* window);
WM_EXPORT bool IsActiveWindow(aura::Window* window);
WM_EXPORT bool CanActivateWindow(aura::Window* window);

// Retrieves the activatable window for |window|. The ActivationClient makes
// this determination.
WM_EXPORT aura::Window* GetActivatableWindow(aura::Window* window);

// Retrieves the toplevel window for |window|. The ActivationClient makes this
// determination.
WM_EXPORT aura::Window* GetToplevelWindow(aura::Window* window);

// A factory method to create a delegate for recreated layers.
class WM_EXPORT LayerDelegateFactory {
 public:
  virtual ~LayerDelegateFactory() = default;
  // |original_delegate| may already be deleted by the time the new
  // delegate is created, so if the new delegate has to access it
  // later, it is the new delegate's responsibility to make sure the
  // original delegate is alive.
  virtual ui::LayerDelegate* CreateDelegate(
      ui::LayerDelegate* original_delegate) = 0;
};

// Returns the existing Layer for |root| (and all its descendants) and creates
// a new layer for |root| and all its descendants. This is intended for
// animations that want to animate between the existing visuals and a new state.
//
// As a result of this |root| has freshly created layers, meaning the layers
// have not yet been painted to.
//
// When a non null |delegate_factory| is passed, it will be used to
// create a delegate for an old layer which had its own delegate.
WM_EXPORT std::unique_ptr<ui::LayerTreeOwner> RecreateLayers(
    ui::LayerOwner* root,
    LayerDelegateFactory* delegate_factory);

// Convenience functions that get the TransientWindowManager for the window and
// redirect appropriately. These are preferable to calling functions on
// TransientWindowManager as they handle the appropriate NULL checks.
WM_EXPORT aura::Window* GetTransientParent(aura::Window* window);
WM_EXPORT const aura::Window* GetTransientParent(
    const aura::Window* window);
WM_EXPORT const std::vector<aura::Window*>& GetTransientChildren(
    const aura::Window* window);
WM_EXPORT void AddTransientChild(aura::Window* parent, aura::Window* child);
WM_EXPORT void RemoveTransientChild(aura::Window* parent, aura::Window* child);

// Returns true if |window| has |ancestor| as a transient ancestor. A transient
// ancestor is found by following the transient parent chain of the window.
WM_EXPORT bool HasTransientAncestor(const aura::Window* window,
                                    const aura::Window* ancestor);

}  // namespace wm

#endif  // UI_WM_CORE_WINDOW_UTIL_H_

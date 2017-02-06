// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_AURA_OBJ_CACHE_H_
#define UI_VIEWS_ACCESSIBILITY_AX_AURA_OBJ_CACHE_H_

#include <stdint.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/views/views_export.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

namespace aura {
namespace client {
class FocusClient;
}
class Window;
}  // namespace aura

namespace views {
class AXAuraObjWrapper;
class View;
class Widget;

// A cache responsible for assigning id's to a set of interesting Aura views.
class VIEWS_EXPORT AXAuraObjCache
    : public aura::client::FocusChangeObserver,
      public aura::WindowObserver {
 public:
  // Get the single instance of this class.
  static AXAuraObjCache* GetInstance();

  // Get or create an entry in the cache based on an Aura view.
  AXAuraObjWrapper* GetOrCreate(View* view);
  AXAuraObjWrapper* GetOrCreate(Widget* widget);
  AXAuraObjWrapper* GetOrCreate(aura::Window* window);

  // Gets an id given an Aura view.
  int32_t GetID(View* view) const;
  int32_t GetID(Widget* widget) const;
  int32_t GetID(aura::Window* window) const;

  // Gets the next unique id for this cache. Useful for non-Aura view backed
  // views.
  int32_t GetNextID() { return current_id_++; }

  // Removes an entry from this cache based on an Aura view.
  void Remove(View* view);
  void Remove(Widget* widget);
  void Remove(aura::Window* window);

  // Removes a view and all of its descendants from the cache.
  void RemoveViewSubtree(View* view);

  // Lookup a cached entry based on an id.
  AXAuraObjWrapper* Get(int32_t id);

  // Remove a cached entry based on an id.
  void Remove(int32_t id);

  // Get all top level windows this cache knows about.
  void GetTopLevelWindows(std::vector<AXAuraObjWrapper*>* children);

  // Get the object that has focus.
  AXAuraObjWrapper* GetFocus();

  // Indicates if this object's currently being destroyed.
  bool is_destroying() { return is_destroying_; }

 private:
  friend struct base::DefaultSingletonTraits<AXAuraObjCache>;

  AXAuraObjCache();
  ~AXAuraObjCache() override;

  View* GetFocusedView();

  // aura::client::FocusChangeObserver override.
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // aura::WindowObserver override.
  void OnWindowDestroying(aura::Window* window) override;

  template <typename AuraViewWrapper, typename AuraView>
  AXAuraObjWrapper* CreateInternal(
      AuraView* aura_view,
      std::map<AuraView*, int32_t>& aura_view_to_id_map);

  template <typename AuraView>
  int32_t GetIDInternal(
      AuraView* aura_view,
      const std::map<AuraView*, int32_t>& aura_view_to_id_map) const;

  template <typename AuraView>
  void RemoveInternal(AuraView* aura_view,
                      std::map<AuraView*, int32_t>& aura_view_to_id_map);

  std::map<views::View*, int32_t> view_to_id_map_;
  std::map<views::Widget*, int32_t> widget_to_id_map_;
  std::map<aura::Window*, int32_t> window_to_id_map_;

  std::map<int32_t, AXAuraObjWrapper*> cache_;
  int32_t current_id_;

  aura::client::FocusClient* focus_client_;

  // True immediately when entering this object's destructor.
  bool is_destroying_;

  DISALLOW_COPY_AND_ASSIGN(AXAuraObjCache);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_AURA_OBJ_CACHE_H_

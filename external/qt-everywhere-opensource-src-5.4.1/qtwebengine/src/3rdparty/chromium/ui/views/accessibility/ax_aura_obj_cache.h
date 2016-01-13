// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_AURA_OBJ_CACHE_H_
#define UI_VIEWS_ACCESSIBILITY_AX_AURA_OBJ_CACHE_H_

#include <map>

#include "base/basictypes.h"
#include "ui/views/views_export.h"

template <typename T> struct DefaultSingletonTraits;

namespace aura {
class Window;
}  // namespace aura

namespace views {
class AXAuraObjWrapper;
class View;
class Widget;

// A cache responsible for assigning id's to a set of interesting Aura views.
class VIEWS_EXPORT AXAuraObjCache {
 public:
  // Get the single instance of this class.
  static AXAuraObjCache* GetInstance();

  // Get or create an entry in the cache based on an Aura view.
  AXAuraObjWrapper* GetOrCreate(View* view);
  AXAuraObjWrapper* GetOrCreate(Widget* widget);
  AXAuraObjWrapper* GetOrCreate(aura::Window* window);

  // Gets an id given an Aura view.
  int32 GetID(View* view);
  int32 GetID(Widget* widget);
  int32 GetID(aura::Window* window);

  // Gets the next unique id for this cache. Useful for non-Aura view backed
  // views.
  int32 GetNextID() { return current_id_++; }

  // Removes an entry from this cache based on an Aura view.
  void Remove(View* view);
  void Remove(Widget* widget);
  void Remove(aura::Window* window);

  // Lookup a cached entry based on an id.
  AXAuraObjWrapper* Get(int32 id);

  // Remove a cached entry based on an id.
  void Remove(int32 id);

 private:
  friend struct DefaultSingletonTraits<AXAuraObjCache>;

  AXAuraObjCache();
  virtual ~AXAuraObjCache();

  template <typename AuraViewWrapper, typename AuraView>
      AXAuraObjWrapper* CreateInternal(
          AuraView* aura_view, std::map<AuraView*, int32>& aura_view_to_id_map);

  template<typename AuraView> int32 GetIDInternal(
      AuraView* aura_view, std::map<AuraView*, int32>& aura_view_to_id_map);

  template<typename AuraView> void RemoveInternal(
      AuraView* aura_view, std::map<AuraView*, int32>& aura_view_to_id_map);

  std::map<views::View*, int32> view_to_id_map_;
  std::map<views::Widget*, int32> widget_to_id_map_;
  std::map<aura::Window*, int32> window_to_id_map_;

  std::map<int32, AXAuraObjWrapper*> cache_;
  int32 current_id_;

  DISALLOW_COPY_AND_ASSIGN(AXAuraObjCache);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_AURA_OBJ_CACHE_H_

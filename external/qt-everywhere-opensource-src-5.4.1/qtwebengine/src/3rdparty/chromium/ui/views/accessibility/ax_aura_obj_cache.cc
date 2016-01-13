// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_aura_obj_cache.h"

#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "ui/aura/window.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_view_obj_wrapper.h"
#include "ui/views/accessibility/ax_widget_obj_wrapper.h"
#include "ui/views/accessibility/ax_window_obj_wrapper.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

// static
AXAuraObjCache* AXAuraObjCache::GetInstance() {
  return Singleton<AXAuraObjCache>::get();
}

AXAuraObjWrapper* AXAuraObjCache::GetOrCreate(View* view) {
  return CreateInternal<AXViewObjWrapper>(view, view_to_id_map_);
}

AXAuraObjWrapper* AXAuraObjCache::GetOrCreate(Widget* widget) {
  return CreateInternal<AXWidgetObjWrapper>(widget, widget_to_id_map_);
}

AXAuraObjWrapper* AXAuraObjCache::GetOrCreate(aura::Window* window) {
  return CreateInternal<AXWindowObjWrapper>(window, window_to_id_map_);
}

int32 AXAuraObjCache::GetID(View* view) {
  return GetIDInternal(view, view_to_id_map_);
}

int32 AXAuraObjCache::GetID(Widget* widget) {
  return GetIDInternal(widget, widget_to_id_map_);
}

int32 AXAuraObjCache::GetID(aura::Window* window) {
  return GetIDInternal(window, window_to_id_map_);
}

void AXAuraObjCache::Remove(View* view) {
  RemoveInternal(view, view_to_id_map_);
}

void AXAuraObjCache::Remove(Widget* widget) {
  RemoveInternal(widget, widget_to_id_map_);
}

void AXAuraObjCache::Remove(aura::Window* window) {
  RemoveInternal(window, window_to_id_map_);
}

AXAuraObjWrapper* AXAuraObjCache::Get(int32 id) {
  std::map<int32, AXAuraObjWrapper*>::iterator it = cache_.find(id);

  if (it == cache_.end())
    return NULL;

  return it->second;
}

void AXAuraObjCache::Remove(int32 id) {
  AXAuraObjWrapper* obj = Get(id);

  if (id == -1 || !obj)
    return;

  cache_.erase(id);
  delete obj;
}

AXAuraObjCache::AXAuraObjCache() : current_id_(1) {
}

AXAuraObjCache::~AXAuraObjCache() {
  STLDeleteContainerPairSecondPointers(cache_.begin(), cache_.end());
  cache_.clear();
}

template <typename AuraViewWrapper, typename AuraView>
AXAuraObjWrapper* AXAuraObjCache::CreateInternal(
    AuraView* aura_view, std::map<AuraView*, int32>& aura_view_to_id_map) {
  if (!aura_view)
    return NULL;

  typename std::map<AuraView*, int32>::iterator it =
      aura_view_to_id_map.find(aura_view);

  if (it != aura_view_to_id_map.end())
    return Get(it->second);

  AXAuraObjWrapper* wrapper = new AuraViewWrapper(aura_view);
  aura_view_to_id_map[aura_view] = current_id_;
  cache_[current_id_] = wrapper;
  current_id_++;
  return wrapper;
}

template<typename AuraView> int32 AXAuraObjCache::GetIDInternal(
    AuraView* aura_view, std::map<AuraView*, int32>& aura_view_to_id_map) {
  if (!aura_view)
    return -1;

  typename std::map<AuraView*, int32>::iterator it =
      aura_view_to_id_map.find(aura_view);

  if (it != aura_view_to_id_map.end())
    return it->second;

  return -1;
}

template<typename AuraView>
void AXAuraObjCache::RemoveInternal(
    AuraView* aura_view, std::map<AuraView*, int32>& aura_view_to_id_map) {
  int32 id = GetID(aura_view);
  if (id == -1)
    return;
  aura_view_to_id_map.erase(aura_view);
  Remove(id);
}

}  // namespace views

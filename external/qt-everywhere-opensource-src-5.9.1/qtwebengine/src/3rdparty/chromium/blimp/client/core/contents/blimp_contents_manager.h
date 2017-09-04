// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_MANAGER_H_
#define BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_MANAGER_H_

#include <map>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "ui/gfx/native_widget_types.h"

namespace blimp {
namespace client {

class BlimpCompositorDependencies;
class BlimpContents;
class BlimpContentsImpl;
class ImeFeature;
class NavigationFeature;
class RenderWidgetFeature;
class TabControlFeature;

// BlimpContentsManager does the real work of creating BlimpContentsImpl, and
// then passes the ownership to the caller. It also owns the observers to
// monitor the life time of the contents it creates.
class BlimpContentsManager {
 public:
  explicit BlimpContentsManager(
      BlimpCompositorDependencies* blimp_compositor_dependencies,
      ImeFeature* ime_feature,
      NavigationFeature* nav_feature,
      RenderWidgetFeature* render_widget_feature,
      TabControlFeature* tab_control_feature);
  ~BlimpContentsManager();

  // Builds a BlimpContentsImpl and notifies the engine.
  // TODO(mlliu): Currently we want to have a single BlimpContents. If there is
  // an existing contents, return nullptr (http://crbug.com/642558).
  std::unique_ptr<BlimpContentsImpl> CreateBlimpContents(
      gfx::NativeWindow window);

  // The caller can query the contents through its id.
  BlimpContentsImpl* GetBlimpContents(int id);

  // Returns a vector of the currently active BlimpContentsImpls. There is no
  // guarantee for the lifetime of these pointers after this stack frame.
  std::vector<BlimpContentsImpl*> GetAllActiveBlimpContents();

 private:
  class BlimpContentsDeletionObserver;
  friend class BlimpContentsDeletionObserver;

  // When creating the BlimpContentsImpl, an id is created for the content.
  int CreateBlimpContentsId();

  // When a BlimpContentsImpl is destroyed, its observer will pass the contents
  // pointer to the manager. The contents pointer is used to retrieve its id,
  // which in turn is used to destroy the observer entry from the observer_map_
  // and close the tab.
  void OnContentsDestroyed(BlimpContents* contents);

  // Destroy the observer entry from the observer_map_ when a BlimpContentsImpl
  // is destroyed.
  void EraseObserverFromMap(int id);
  base::WeakPtr<BlimpContentsManager> GetWeakPtr();

  // The ID to use whenever a new BlimpContentsImpl is created. Incremented
  // after each use.
  int next_blimp_contents_id_ = 0;

  // BlimpContentsManager owns the BlimpContentsDeletionObserver for the
  // contents it creates, with the content id being the key to help manage the
  // lifetime of the observers.
  std::map<int, std::unique_ptr<BlimpContentsDeletionObserver>> observer_map_;

  BlimpCompositorDependencies* blimp_compositor_dependencies_;
  ImeFeature* ime_feature_;
  NavigationFeature* navigation_feature_;
  RenderWidgetFeature* render_widget_feature_;
  TabControlFeature* tab_control_feature_;

  // TODO(mlliu): Currently we want to have a single BlimpContents. Remove this
  // when it supports multiple tabs (http://crbug.com/642558).
  bool tab_exists_ = false;

  base::WeakPtrFactory<BlimpContentsManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentsManager);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_MANAGER_H_

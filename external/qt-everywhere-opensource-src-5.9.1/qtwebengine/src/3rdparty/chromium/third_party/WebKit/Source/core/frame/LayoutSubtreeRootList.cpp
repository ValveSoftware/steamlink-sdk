// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/LayoutSubtreeRootList.h"

#include "core/layout/LayoutObject.h"

namespace blink {

void LayoutSubtreeRootList::clearAndMarkContainingBlocksForLayout() {
  for (auto& iter : unordered())
    iter->markContainerChainForLayout(false);
  clear();
}

LayoutObject* LayoutSubtreeRootList::randomRoot() {
  ASSERT(!isEmpty());
  return *unordered().begin();
}

void LayoutSubtreeRootList::countObjectsNeedingLayoutInRoot(
    const LayoutObject* object,
    unsigned& needsLayoutObjects,
    unsigned& totalObjects) {
  for (const LayoutObject* o = object; o; o = o->nextInPreOrder(object)) {
    ++totalObjects;
    if (o->needsLayout())
      ++needsLayoutObjects;
  }
}

void LayoutSubtreeRootList::countObjectsNeedingLayout(
    unsigned& needsLayoutObjects,
    unsigned& totalObjects) {
  // TODO(leviw): This will double-count nested roots crbug.com/509141
  for (auto& root : unordered())
    countObjectsNeedingLayoutInRoot(root, needsLayoutObjects, totalObjects);
}

}  // namespace blink

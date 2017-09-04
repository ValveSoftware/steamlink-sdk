// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/DepthOrderedLayoutObjectList.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutObject.h"
#include <algorithm>

namespace blink {

struct DepthOrderedLayoutObjectListData {
  // LayoutObjects sorted by depth (deepest first). This structure is only
  // populated at the beginning of enumerations. See ordered().
  Vector<DepthOrderedLayoutObjectList::LayoutObjectWithDepth> m_orderedObjects;

  // Outside of layout, LayoutObjects can be added and removed as needed such
  // as when style was changed or destroyed. They're kept in this hashset to
  // keep those operations fast.
  HashSet<LayoutObject*> m_objects;
};

DepthOrderedLayoutObjectList::DepthOrderedLayoutObjectList()
    : m_data(new DepthOrderedLayoutObjectListData) {}

DepthOrderedLayoutObjectList::~DepthOrderedLayoutObjectList() {
  delete m_data;
}

int DepthOrderedLayoutObjectList::size() const {
  return m_data->m_objects.size();
}

bool DepthOrderedLayoutObjectList::isEmpty() const {
  return m_data->m_objects.isEmpty();
}

void DepthOrderedLayoutObjectList::add(LayoutObject& object) {
  ASSERT(!object.frameView()->isInPerformLayout());
  m_data->m_objects.add(&object);
  m_data->m_orderedObjects.clear();
}

void DepthOrderedLayoutObjectList::remove(LayoutObject& object) {
  auto it = m_data->m_objects.find(&object);
  if (it == m_data->m_objects.end())
    return;
  ASSERT(!object.frameView()->isInPerformLayout());
  m_data->m_objects.remove(it);
  m_data->m_orderedObjects.clear();
}

void DepthOrderedLayoutObjectList::clear() {
  m_data->m_objects.clear();
  m_data->m_orderedObjects.clear();
}

unsigned DepthOrderedLayoutObjectList::LayoutObjectWithDepth::determineDepth(
    LayoutObject* object) {
  unsigned depth = 1;
  for (LayoutObject* parent = object->parent(); parent;
       parent = parent->parent())
    ++depth;
  return depth;
}

const HashSet<LayoutObject*>& DepthOrderedLayoutObjectList::unordered() const {
  return m_data->m_objects;
}

const Vector<DepthOrderedLayoutObjectList::LayoutObjectWithDepth>&
DepthOrderedLayoutObjectList::ordered() {
  if (m_data->m_objects.isEmpty() || !m_data->m_orderedObjects.isEmpty())
    return m_data->m_orderedObjects;

  copyToVector(m_data->m_objects, m_data->m_orderedObjects);
  std::sort(m_data->m_orderedObjects.begin(), m_data->m_orderedObjects.end());
  return m_data->m_orderedObjects;
}

}  // namespace blink

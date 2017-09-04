// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DepthOrderedLayoutObjectList_h
#define DepthOrderedLayoutObjectList_h

#include "wtf/Allocator.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"

namespace blink {

class LayoutObject;

// Put data inside a forward-declared struct, to avoid including LayoutObject.h.
struct DepthOrderedLayoutObjectListData;

class DepthOrderedLayoutObjectList {
 public:
  DepthOrderedLayoutObjectList();
  ~DepthOrderedLayoutObjectList();

  void add(LayoutObject&);
  void remove(LayoutObject&);
  void clear();

  int size() const;
  bool isEmpty() const;

  struct LayoutObjectWithDepth {
    LayoutObjectWithDepth(LayoutObject* inObject)
        : object(inObject), depth(determineDepth(inObject)) {}

    LayoutObjectWithDepth() : object(nullptr), depth(0) {}

    LayoutObject* object;
    unsigned depth;

    LayoutObject& operator*() const { return *object; }
    LayoutObject* operator->() const { return object; }

    bool operator<(const DepthOrderedLayoutObjectList::LayoutObjectWithDepth&
                       other) const {
      return depth > other.depth;
    }

   private:
    static unsigned determineDepth(LayoutObject*);
  };

  const HashSet<LayoutObject*>& unordered() const;
  const Vector<LayoutObjectWithDepth>& ordered();

 private:
  DepthOrderedLayoutObjectListData* m_data;
};

}  // namespace blink

#endif  // DepthOrderedLayoutObjectList_h

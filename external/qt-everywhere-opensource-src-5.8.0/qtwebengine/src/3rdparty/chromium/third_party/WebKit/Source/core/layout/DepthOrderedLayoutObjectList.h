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

class DepthOrderedLayoutObjectList {
public:
    DepthOrderedLayoutObjectList()
    {
    }

    void add(LayoutObject&);
    void remove(LayoutObject&);
    void clear();

    int size() const { return m_objects.size(); }
    bool isEmpty() const { return m_objects.isEmpty(); }

    struct LayoutObjectWithDepth {
        LayoutObjectWithDepth(LayoutObject* inObject)
            : object(inObject)
            , depth(determineDepth(inObject))
        {
        }

        LayoutObjectWithDepth()
            : object(nullptr)
            , depth(0)
        {
        }

        LayoutObject* object;
        unsigned depth;

        LayoutObject& operator*() const { return *object; }
        LayoutObject* operator->() const { return object; }

        bool operator<(const DepthOrderedLayoutObjectList::LayoutObjectWithDepth& other) const
        {
            return depth > other.depth;
        }

    private:
        static unsigned determineDepth(LayoutObject*);
    };

    const HashSet<LayoutObject*>& unordered() const { return m_objects; }
    const Vector<LayoutObjectWithDepth>& ordered();

private:
    // LayoutObjects sorted by depth (deepest first). This structure is only
    // populated at the beginning of enumerations. See ordered().
    Vector<LayoutObjectWithDepth> m_orderedObjects;

    // Outside of layout, LayoutObjects can be added and removed as needed such
    // as when style was changed or destroyed. They're kept in this hashset to
    // keep those operations fast.
    HashSet<LayoutObject*> m_objects;
};

} // namespace blink

#endif // DepthOrderedLayoutObjectList_h

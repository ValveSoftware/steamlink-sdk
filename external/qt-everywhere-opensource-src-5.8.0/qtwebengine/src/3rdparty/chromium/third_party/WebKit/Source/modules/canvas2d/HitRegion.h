// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HitRegion_h
#define HitRegion_h

#include "core/dom/Element.h"
#include "modules/canvas2d/HitRegionOptions.h"
#include "platform/graphics/Path.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class HitRegion final : public GarbageCollectedFinalized<HitRegion> {
public:
    static HitRegion* create(const Path& path, const HitRegionOptions& options)
    {
        return new HitRegion(path, options);
    }

    virtual ~HitRegion() { }

    void removePixels(const Path&);

    bool contains(const FloatPoint&) const;

    const String& id() const { return m_id; }
    const Path& path() const { return m_path; }
    Element* control() const { return m_control.get(); }

    DECLARE_TRACE();

private:
    HitRegion(const Path&, const HitRegionOptions&);

    String m_id;
    Member<Element> m_control;
    Path m_path;
    WindRule m_fillRule;
};

class HitRegionManager final : public GarbageCollected<HitRegionManager> {
    WTF_MAKE_NONCOPYABLE(HitRegionManager);
public:
    static HitRegionManager* create() { return new HitRegionManager; }

    void addHitRegion(HitRegion*);

    void removeHitRegion(HitRegion*);
    void removeHitRegionById(const String& id);
    void removeHitRegionByControl(const Element*);
    void removeHitRegionsInRect(const FloatRect&, const AffineTransform&);
    void removeAllHitRegions();

    HitRegion* getHitRegionById(const String& id) const;
    HitRegion* getHitRegionByControl(const Element*) const;
    HitRegion* getHitRegionAtPoint(const FloatPoint&) const;

    unsigned getHitRegionsCount() const;

    DECLARE_TRACE();

private:
    HitRegionManager() { }

    typedef HeapListHashSet<Member<HitRegion>> HitRegionList;
    typedef HitRegionList::const_reverse_iterator HitRegionIterator;
    typedef HeapHashMap<String, Member<HitRegion>> HitRegionIdMap;
    typedef HeapHashMap<Member<const Element>, Member<HitRegion>> HitRegionControlMap;

    HitRegionList m_hitRegionList;
    HitRegionIdMap m_hitRegionIdMap;
    HitRegionControlMap m_hitRegionControlMap;
};

} // namespace blink

#endif

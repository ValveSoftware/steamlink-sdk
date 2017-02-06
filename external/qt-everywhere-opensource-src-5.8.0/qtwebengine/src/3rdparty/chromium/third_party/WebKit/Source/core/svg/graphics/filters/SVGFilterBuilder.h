/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SVGFilterBuilder_h
#define SVGFilterBuilder_h

#include "platform/graphics/filters/FilterEffect.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/WTFString.h"

class SkPaint;

namespace blink {

class FloatRect;
class LayoutObject;
class SVGFilterElement;

// A map from LayoutObject -> FilterEffect and FilterEffect -> dependent (downstream) FilterEffects ("reverse DAG").
// Used during invalidations from changes to the primitives (graph nodes).
class SVGFilterGraphNodeMap final : public GarbageCollected<SVGFilterGraphNodeMap> {
public:
    static SVGFilterGraphNodeMap* create()
    {
        return new SVGFilterGraphNodeMap;
    }

    typedef HeapHashSet<Member<FilterEffect>> FilterEffectSet;

    void addBuiltinEffect(FilterEffect*);
    void addPrimitive(LayoutObject*, FilterEffect*);

    inline FilterEffectSet& effectReferences(FilterEffect* effect)
    {
        // Only allowed for effects belongs to this builder.
        ASSERT(m_effectReferences.contains(effect));
        return m_effectReferences.find(effect)->value;
    }

    // Required to change the attributes of a filter during an svgAttributeChanged.
    inline FilterEffect* effectByRenderer(LayoutObject* object) { return m_effectRenderer.get(object); }

    void invalidateDependentEffects(FilterEffect*);

    DECLARE_TRACE();

private:
    SVGFilterGraphNodeMap();

    // The value is a list, which contains those filter effects,
    // which depends on the key filter effect.
    HeapHashMap<Member<FilterEffect>, FilterEffectSet> m_effectReferences;
    HeapHashMap<LayoutObject*, Member<FilterEffect>> m_effectRenderer;
};

class SVGFilterBuilder {
    STACK_ALLOCATED();
public:
    SVGFilterBuilder(
        FilterEffect* sourceGraphic,
        SVGFilterGraphNodeMap* = nullptr,
        const SkPaint* fillPaint = nullptr,
        const SkPaint* strokePaint = nullptr);

    void buildGraph(Filter*, SVGFilterElement&, const FloatRect&);

    FilterEffect* getEffectById(const AtomicString& id) const;
    FilterEffect* lastEffect() const { return m_lastEffect.get(); }

private:
    void add(const AtomicString& id, FilterEffect*);
    void addBuiltinEffects();

    typedef HeapHashMap<AtomicString, Member<FilterEffect>> NamedFilterEffectMap;

    NamedFilterEffectMap m_builtinEffects;
    NamedFilterEffectMap m_namedEffects;

    Member<FilterEffect> m_lastEffect;
    Member<SVGFilterGraphNodeMap> m_nodeMap;
};

} // namespace blink

#endif // SVGFilterBuilder_h

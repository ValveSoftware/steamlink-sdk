/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 *
 */

#ifndef TreeBoundaryCrossingRules_h
#define TreeBoundaryCrossingRules_h

#include "core/css/RuleSet.h"
#include "core/dom/DocumentOrderedList.h"

#include "wtf/OwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class CSSStyleSheet;
class ContainerNode;
class ElementRuleCollector;
class RuleFeatureSet;

class TreeBoundaryCrossingRules {
    DISALLOW_ALLOCATION();
public:
    void addTreeBoundaryCrossingRules(const RuleSet&, ContainerNode& scope, CSSStyleSheet*);

    void reset(const ContainerNode* scopingNode);
    void collectFeaturesTo(RuleFeatureSet&);
    void collectTreeBoundaryCrossingRules(Element*, ElementRuleCollector&, bool includeEmptyRules);

    void trace(Visitor* visitor) { visitor->trace(m_treeBoundaryCrossingRuleSetMap); }

private:
    size_t size() const { return m_scopingNodes.size(); }
    typedef WillBeHeapVector<std::pair<CSSStyleSheet*, OwnPtrWillBeMember<RuleSet> > > CSSStyleSheetRuleSubSet;
    void collectFeaturesFromRuleSubSet(CSSStyleSheetRuleSubSet*, RuleFeatureSet&);

    DocumentOrderedList m_scopingNodes;
    typedef WillBeHeapHashMap<const ContainerNode*, OwnPtrWillBeMember<CSSStyleSheetRuleSubSet> > TreeBoundaryCrossingRuleSetMap;
    TreeBoundaryCrossingRuleSetMap m_treeBoundaryCrossingRuleSetMap;
};

} // namespace WebCore

#endif // TreeBoundaryCrossingRules_h

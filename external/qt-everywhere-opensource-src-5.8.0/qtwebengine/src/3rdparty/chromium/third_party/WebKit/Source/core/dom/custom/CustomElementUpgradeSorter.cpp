// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementUpgradeSorter.h"

#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/Node.h"
#include "core/dom/shadow/ShadowRoot.h"

namespace blink {

CustomElementUpgradeSorter::CustomElementUpgradeSorter()
    : m_elements(new HeapHashSet<Member<Element>>())
    , m_parentChildMap(new ParentChildMap())
{
}

void CustomElementUpgradeSorter::add(Element* element)
{
    m_elements->add(element);

    for (Node* n = element, *parent = n->parentOrShadowHostNode();
        parent;
        n = parent, parent = parent->parentOrShadowHostNode()) {

        ParentChildMap::iterator it = m_parentChildMap->find(parent);
        if (it == m_parentChildMap->end()) {
            ParentChildMap::AddResult result =
                m_parentChildMap->add(parent, HeapHashSet<Member<Node>>());
            result.storedValue->value.add(n);
        } else {
            it->value.add(n);
            // The entry for the parent exists; so must its parents.
            break;
        }
    }
}

void CustomElementUpgradeSorter::visit(
    HeapVector<Member<Element>>* result,
    ChildSet& children,
    const ChildSet::iterator& it)
{
    if (it == children.end())
        return;
    if (it->get()->isElementNode() && m_elements->contains(toElement(*it)))
        result->append(toElement(*it));
    sorted(result, *it);
    children.remove(it);
}

void CustomElementUpgradeSorter::sorted(
    HeapVector<Member<Element>>* result,
    Node* parent)
{
    ParentChildMap::iterator childrenIterator = m_parentChildMap->find(parent);
    if (childrenIterator == m_parentChildMap->end())
        return;

    ChildSet& children = childrenIterator->value;

    if (children.size() == 1) {
        visit(result, children, children.begin());
        return;
    }

    // TODO(dominicc): When custom elements are used in UA shadow
    // roots, expand this to include UA shadow roots.
    ShadowRoot* shadowRoot = parent->isElementNode()
        ? toElement(parent)->authorShadowRoot()
        : nullptr;
    if (shadowRoot)
        visit(result, children, children.find(shadowRoot));

    for (Element* e = ElementTraversal::firstChild(*parent);
        e && children.size() > 1;
        e = ElementTraversal::nextSibling(*e)) {

        visit(result, children, children.find(e));
    }

    if (children.size() == 1)
        visit(result, children, children.begin());

    DCHECK(children.isEmpty());
}

} // namespace blink

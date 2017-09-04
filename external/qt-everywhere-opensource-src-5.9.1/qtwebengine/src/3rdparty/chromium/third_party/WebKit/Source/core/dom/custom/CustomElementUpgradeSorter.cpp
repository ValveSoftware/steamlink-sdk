// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementUpgradeSorter.h"

#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/Node.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/imports/HTMLImportChild.h"
#include "core/html/imports/HTMLImportLoader.h"

namespace blink {

CustomElementUpgradeSorter::CustomElementUpgradeSorter()
    : m_elements(new HeapHashSet<Member<Element>>()),
      m_parentChildMap(new ParentChildMap()) {}

static HTMLLinkElement* getLinkElementForImport(const Document& import) {
  if (HTMLImportLoader* loader = import.importLoader())
    return loader->firstImport()->link();
  return nullptr;
}

CustomElementUpgradeSorter::AddResult
CustomElementUpgradeSorter::addToParentChildMap(Node* parent, Node* child) {
  ParentChildMap::AddResult result = m_parentChildMap->add(parent, nullptr);
  if (!result.isNewEntry) {
    result.storedValue->value->add(child);
    // The entry for the parent exists; so must its parents.
    return kParentAlreadyExistsInMap;
  }

  ChildSet* childSet = new ChildSet();
  childSet->add(child);
  result.storedValue->value = childSet;
  return kParentAddedToMap;
}

void CustomElementUpgradeSorter::add(Element* element) {
  m_elements->add(element);

  for (Node *n = element, *parent = n->parentOrShadowHostNode(); parent;
       n = parent, parent = parent->parentOrShadowHostNode()) {
    if (addToParentChildMap(parent, n) == kParentAlreadyExistsInMap)
      break;

    // Create parent-child link between <link rel="import"> and its imported
    // document so that the content of the imported document be visited as if
    // the imported document were inserted in the link element.
    if (parent->isDocumentNode()) {
      Element* link = getLinkElementForImport(*toDocument(parent));
      if (!link ||
          addToParentChildMap(link, parent) == kParentAlreadyExistsInMap)
        break;
      parent = link;
    }
  }
}

void CustomElementUpgradeSorter::visit(HeapVector<Member<Element>>* result,
                                       ChildSet& children,
                                       const ChildSet::iterator& it) {
  if (it == children.end())
    return;
  if (it->get()->isElementNode() && m_elements->contains(toElement(*it)))
    result->append(toElement(*it));
  sorted(result, *it);
  children.remove(it);
}

void CustomElementUpgradeSorter::sorted(HeapVector<Member<Element>>* result,
                                        Node* parent) {
  ParentChildMap::iterator childrenIterator = m_parentChildMap->find(parent);
  if (childrenIterator == m_parentChildMap->end())
    return;

  ChildSet* children = childrenIterator->value.get();

  if (children->size() == 1) {
    visit(result, *children, children->begin());
    return;
  }

  // TODO(dominicc): When custom elements are used in UA shadow
  // roots, expand this to include UA shadow roots.
  ShadowRoot* shadowRoot =
      parent->isElementNode() ? toElement(parent)->authorShadowRoot() : nullptr;
  if (shadowRoot)
    visit(result, *children, children->find(shadowRoot));

  for (Element* e = ElementTraversal::firstChild(*parent);
       e && children->size() > 1; e = ElementTraversal::nextSibling(*e)) {
    visit(result, *children, children->find(e));
  }

  if (children->size() == 1)
    visit(result, *children, children->begin());

  DCHECK(children->isEmpty());
}

}  // namespace blink

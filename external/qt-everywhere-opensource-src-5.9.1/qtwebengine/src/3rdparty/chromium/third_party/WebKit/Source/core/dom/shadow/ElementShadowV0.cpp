/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/shadow/ElementShadowV0.h"

#include "core/dom/ElementTraversal.h"
#include "core/dom/shadow/DistributedNodes.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLShadowElement.h"
#include "core/inspector/InspectorInstrumentation.h"

namespace blink {

class DistributionPool final {
  STACK_ALLOCATED();

 public:
  explicit DistributionPool(const ContainerNode&);
  void clear();
  ~DistributionPool();
  void distributeTo(InsertionPoint*, ElementShadowV0*);
  void populateChildren(const ContainerNode&);

 private:
  void detachNonDistributedNodes();
  HeapVector<Member<Node>, 32> m_nodes;
  Vector<bool, 32> m_distributed;
};

inline DistributionPool::DistributionPool(const ContainerNode& parent) {
  populateChildren(parent);
}

inline void DistributionPool::clear() {
  detachNonDistributedNodes();
  m_nodes.clear();
  m_distributed.clear();
}

inline void DistributionPool::populateChildren(const ContainerNode& parent) {
  clear();
  for (Node* child = parent.firstChild(); child; child = child->nextSibling()) {
    if (isHTMLSlotElement(child)) {
      // TODO(hayato): Support re-distribution across v0 and v1 shadow trees
      continue;
    }
    if (isActiveInsertionPoint(*child)) {
      InsertionPoint* insertionPoint = toInsertionPoint(child);
      for (size_t i = 0; i < insertionPoint->distributedNodesSize(); ++i)
        m_nodes.append(insertionPoint->distributedNodeAt(i));
    } else {
      m_nodes.append(child);
    }
  }
  m_distributed.resize(m_nodes.size());
  m_distributed.fill(false);
}

void DistributionPool::distributeTo(InsertionPoint* insertionPoint,
                                    ElementShadowV0* elementShadow) {
  DistributedNodes distributedNodes;

  for (size_t i = 0; i < m_nodes.size(); ++i) {
    if (m_distributed[i])
      continue;

    if (isHTMLContentElement(*insertionPoint) &&
        !toHTMLContentElement(insertionPoint)->canSelectNode(m_nodes, i))
      continue;

    Node* node = m_nodes[i];
    distributedNodes.append(node);
    elementShadow->didDistributeNode(node, insertionPoint);
    m_distributed[i] = true;
  }

  // Distributes fallback elements
  if (insertionPoint->isContentInsertionPoint() && distributedNodes.isEmpty()) {
    for (Node* fallbackNode = insertionPoint->firstChild(); fallbackNode;
         fallbackNode = fallbackNode->nextSibling()) {
      distributedNodes.append(fallbackNode);
      elementShadow->didDistributeNode(fallbackNode, insertionPoint);
    }
  }
  insertionPoint->setDistributedNodes(distributedNodes);
}

inline DistributionPool::~DistributionPool() {
  detachNonDistributedNodes();
}

inline void DistributionPool::detachNonDistributedNodes() {
  for (size_t i = 0; i < m_nodes.size(); ++i) {
    if (m_distributed[i])
      continue;
    if (m_nodes[i]->layoutObject())
      m_nodes[i]->lazyReattachIfAttached();
  }
}

ElementShadowV0* ElementShadowV0::create(ElementShadow& elementShadow) {
  return new ElementShadowV0(elementShadow);
}

ElementShadowV0::ElementShadowV0(ElementShadow& elementShadow)
    : m_elementShadow(&elementShadow), m_needsSelectFeatureSet(false) {}

ElementShadowV0::~ElementShadowV0() {}

ShadowRoot& ElementShadowV0::youngestShadowRoot() const {
  return m_elementShadow->youngestShadowRoot();
}

ShadowRoot& ElementShadowV0::oldestShadowRoot() const {
  return m_elementShadow->oldestShadowRoot();
}

const InsertionPoint* ElementShadowV0::finalDestinationInsertionPointFor(
    const Node* key) const {
  DCHECK(key);
  DCHECK(!key->needsDistributionRecalc());
  NodeToDestinationInsertionPoints::const_iterator it =
      m_nodeToInsertionPoints.find(key);
  return it == m_nodeToInsertionPoints.end() ? nullptr : it->value->last();
}

const DestinationInsertionPoints*
ElementShadowV0::destinationInsertionPointsFor(const Node* key) const {
  DCHECK(key);
  DCHECK(!key->needsDistributionRecalc());
  NodeToDestinationInsertionPoints::const_iterator it =
      m_nodeToInsertionPoints.find(key);
  return it == m_nodeToInsertionPoints.end() ? nullptr : it->value;
}

void ElementShadowV0::distribute() {
  HeapVector<Member<HTMLShadowElement>, 32> shadowInsertionPoints;
  DistributionPool pool(m_elementShadow->host());

  for (ShadowRoot* root = &youngestShadowRoot(); root;
       root = root->olderShadowRoot()) {
    HTMLShadowElement* shadowInsertionPoint = 0;
    const HeapVector<Member<InsertionPoint>>& insertionPoints =
        root->descendantInsertionPoints();
    for (size_t i = 0; i < insertionPoints.size(); ++i) {
      InsertionPoint* point = insertionPoints[i];
      if (!point->isActive())
        continue;
      if (isHTMLShadowElement(*point)) {
        DCHECK(!shadowInsertionPoint);
        shadowInsertionPoint = toHTMLShadowElement(point);
        shadowInsertionPoints.append(shadowInsertionPoint);
      } else {
        pool.distributeTo(point, this);
        if (ElementShadow* shadow =
                shadowWhereNodeCanBeDistributedForV0(*point))
          shadow->setNeedsDistributionRecalc();
      }
    }
  }

  for (size_t i = shadowInsertionPoints.size(); i > 0; --i) {
    HTMLShadowElement* shadowInsertionPoint = shadowInsertionPoints[i - 1];
    ShadowRoot* root = shadowInsertionPoint->containingShadowRoot();
    DCHECK(root);
    if (root->isOldest()) {
      pool.distributeTo(shadowInsertionPoint, this);
    } else if (root->olderShadowRoot()->type() == root->type()) {
      // Only allow reprojecting older shadow roots between the same type to
      // disallow reprojecting UA elements into author shadows.
      DistributionPool olderShadowRootPool(*root->olderShadowRoot());
      olderShadowRootPool.distributeTo(shadowInsertionPoint, this);
      root->olderShadowRoot()->setShadowInsertionPointOfYoungerShadowRoot(
          shadowInsertionPoint);
    }
    if (ElementShadow* shadow =
            shadowWhereNodeCanBeDistributedForV0(*shadowInsertionPoint))
      shadow->setNeedsDistributionRecalc();
  }
  InspectorInstrumentation::didPerformElementShadowDistribution(
      &m_elementShadow->host());
}

void ElementShadowV0::didDistributeNode(const Node* node,
                                        InsertionPoint* insertionPoint) {
  NodeToDestinationInsertionPoints::AddResult result =
      m_nodeToInsertionPoints.add(node, nullptr);
  if (result.isNewEntry)
    result.storedValue->value = new DestinationInsertionPoints;
  result.storedValue->value->append(insertionPoint);
}

const SelectRuleFeatureSet& ElementShadowV0::ensureSelectFeatureSet() {
  if (!m_needsSelectFeatureSet)
    return m_selectFeatures;

  m_selectFeatures.clear();
  for (const ShadowRoot* root = &oldestShadowRoot(); root;
       root = root->youngerShadowRoot())
    collectSelectFeatureSetFrom(*root);
  m_needsSelectFeatureSet = false;
  return m_selectFeatures;
}

void ElementShadowV0::collectSelectFeatureSetFrom(const ShadowRoot& root) {
  if (!root.containsShadowRoots() && !root.containsContentElements())
    return;

  for (Element& element : ElementTraversal::descendantsOf(root)) {
    if (ElementShadow* shadow = element.shadow()) {
      if (!shadow->isV1())
        m_selectFeatures.add(shadow->v0().ensureSelectFeatureSet());
    }
    if (!isHTMLContentElement(element))
      continue;
    const CSSSelectorList& list = toHTMLContentElement(element).selectorList();
    m_selectFeatures.collectFeaturesFromSelectorList(list);
  }
}

void ElementShadowV0::willAffectSelector() {
  for (ElementShadow* shadow = m_elementShadow; shadow;
       shadow = shadow->containingShadow()) {
    if (shadow->isV1() || shadow->v0().needsSelectFeatureSet())
      break;
    shadow->v0().setNeedsSelectFeatureSet();
  }
  m_elementShadow->setNeedsDistributionRecalc();
}

void ElementShadowV0::clearDistribution() {
  m_nodeToInsertionPoints.clear();

  for (ShadowRoot* root = &m_elementShadow->youngestShadowRoot(); root;
       root = root->olderShadowRoot())
    root->setShadowInsertionPointOfYoungerShadowRoot(nullptr);
}

DEFINE_TRACE(ElementShadowV0) {
  visitor->trace(m_elementShadow);
  visitor->trace(m_nodeToInsertionPoints);
  visitor->trace(m_selectFeatures);
}

DEFINE_TRACE_WRAPPERS(ElementShadowV0) {}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/invalidation/StyleInvalidator.h"

#include "core/css/invalidation/InvalidationSet.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLSlotElement.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/layout/LayoutObject.h"
#include "wtf/PtrUtil.h"

namespace blink {

// StyleInvalidator methods are super sensitive to performance benchmarks.
// We easily get 1% regression per additional if statement on recursive
// invalidate methods.
// To minimize performance impact, we wrap trace events with a lookup of
// cached flag. The cached flag is made "static const" and is not shared
// with InvalidationSet to avoid additional GOT lookup cost.
static const unsigned char* s_tracingEnabled = nullptr;

#define TRACE_STYLE_INVALIDATOR_INVALIDATION_IF_ENABLED(element, reason) \
  if (UNLIKELY(*s_tracingEnabled))                                       \
    TRACE_STYLE_INVALIDATOR_INVALIDATION(element, reason);

void StyleInvalidator::invalidate(Document& document) {
  RecursionData recursionData;
  SiblingData siblingData;
  if (Element* documentElement = document.documentElement())
    invalidate(*documentElement, recursionData, siblingData);
  document.clearChildNeedsStyleInvalidation();
  document.clearNeedsStyleInvalidation();
  m_pendingInvalidationMap.clear();
}

void StyleInvalidator::scheduleInvalidationSetsForNode(
    const InvalidationLists& invalidationLists,
    ContainerNode& node) {
  DCHECK(node.inActiveDocument());
  bool requiresDescendantInvalidation = false;

  if (node.getStyleChangeType() < SubtreeStyleChange) {
    for (auto& invalidationSet : invalidationLists.descendants) {
      if (invalidationSet->wholeSubtreeInvalid()) {
        node.setNeedsStyleRecalc(SubtreeStyleChange,
                                 StyleChangeReasonForTracing::create(
                                     StyleChangeReason::StyleInvalidator));
        requiresDescendantInvalidation = false;
        break;
      }

      if (invalidationSet->invalidatesSelf())
        node.setNeedsStyleRecalc(LocalStyleChange,
                                 StyleChangeReasonForTracing::create(
                                     StyleChangeReason::StyleInvalidator));

      if (!invalidationSet->isEmpty())
        requiresDescendantInvalidation = true;
    }
  }

  if (!requiresDescendantInvalidation &&
      (invalidationLists.siblings.isEmpty() || !node.nextSibling()))
    return;

  node.setNeedsStyleInvalidation();

  PendingInvalidations& pendingInvalidations = ensurePendingInvalidations(node);
  if (node.nextSibling()) {
    for (auto& invalidationSet : invalidationLists.siblings) {
      if (pendingInvalidations.siblings().contains(invalidationSet))
        continue;
      pendingInvalidations.siblings().append(invalidationSet);
    }
  }

  if (!requiresDescendantInvalidation)
    return;

  for (auto& invalidationSet : invalidationLists.descendants) {
    DCHECK(!invalidationSet->wholeSubtreeInvalid());
    if (invalidationSet->isEmpty())
      continue;
    if (pendingInvalidations.descendants().contains(invalidationSet))
      continue;
    pendingInvalidations.descendants().append(invalidationSet);
  }
}

void StyleInvalidator::scheduleSiblingInvalidationsAsDescendants(
    const InvalidationLists& invalidationLists,
    ContainerNode& schedulingParent) {
  DCHECK(invalidationLists.descendants.isEmpty());

  if (invalidationLists.siblings.isEmpty())
    return;

  PendingInvalidations& pendingInvalidations =
      ensurePendingInvalidations(schedulingParent);

  schedulingParent.setNeedsStyleInvalidation();

  for (auto& invalidationSet : invalidationLists.siblings) {
    if (invalidationSet->wholeSubtreeInvalid()) {
      schedulingParent.setNeedsStyleRecalc(
          SubtreeStyleChange, StyleChangeReasonForTracing::create(
                                  StyleChangeReason::StyleInvalidator));
      return;
    }
    if (invalidationSet->invalidatesSelf() &&
        !pendingInvalidations.descendants().contains(invalidationSet))
      pendingInvalidations.descendants().append(invalidationSet);

    if (DescendantInvalidationSet* descendants =
            toSiblingInvalidationSet(*invalidationSet).siblingDescendants()) {
      if (descendants->wholeSubtreeInvalid()) {
        schedulingParent.setNeedsStyleRecalc(
            SubtreeStyleChange, StyleChangeReasonForTracing::create(
                                    StyleChangeReason::StyleInvalidator));
        return;
      }
      if (!pendingInvalidations.descendants().contains(descendants))
        pendingInvalidations.descendants().append(descendants);
    }
  }
}

void StyleInvalidator::clearInvalidation(ContainerNode& node) {
  if (!node.needsStyleInvalidation())
    return;
  m_pendingInvalidationMap.remove(&node);
  node.clearNeedsStyleInvalidation();
}

PendingInvalidations& StyleInvalidator::ensurePendingInvalidations(
    ContainerNode& node) {
  PendingInvalidationMap::AddResult addResult =
      m_pendingInvalidationMap.add(&node, nullptr);
  if (addResult.isNewEntry)
    addResult.storedValue->value = makeUnique<PendingInvalidations>();
  return *addResult.storedValue->value;
}

StyleInvalidator::StyleInvalidator() {
  s_tracingEnabled = TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"));
  InvalidationSet::cacheTracingFlag();
}

StyleInvalidator::~StyleInvalidator() {}

void StyleInvalidator::RecursionData::pushInvalidationSet(
    const InvalidationSet& invalidationSet) {
  DCHECK(!m_wholeSubtreeInvalid);
  DCHECK(!invalidationSet.wholeSubtreeInvalid());
  DCHECK(!invalidationSet.isEmpty());
  if (invalidationSet.customPseudoInvalid())
    m_invalidateCustomPseudo = true;
  if (invalidationSet.treeBoundaryCrossing())
    m_treeBoundaryCrossing = true;
  if (invalidationSet.insertionPointCrossing())
    m_insertionPointCrossing = true;
  if (invalidationSet.invalidatesSlotted())
    m_invalidatesSlotted = true;
  m_invalidationSets.append(&invalidationSet);
}

ALWAYS_INLINE bool
StyleInvalidator::RecursionData::matchesCurrentInvalidationSets(
    Element& element) const {
  if (m_invalidateCustomPseudo && element.shadowPseudoId() != nullAtom) {
    TRACE_STYLE_INVALIDATOR_INVALIDATION_IF_ENABLED(element,
                                                    InvalidateCustomPseudo);
    return true;
  }

  if (m_insertionPointCrossing && element.isInsertionPoint())
    return true;

  for (const auto& invalidationSet : m_invalidationSets) {
    if (invalidationSet->invalidatesElement(element))
      return true;
  }

  return false;
}

bool StyleInvalidator::RecursionData::matchesCurrentInvalidationSetsAsSlotted(
    Element& element) const {
  DCHECK(m_invalidatesSlotted);

  for (const auto& invalidationSet : m_invalidationSets) {
    if (!invalidationSet->invalidatesSlotted())
      continue;
    if (invalidationSet->invalidatesElement(element))
      return true;
  }
  return false;
}

void StyleInvalidator::SiblingData::pushInvalidationSet(
    const SiblingInvalidationSet& invalidationSet) {
  unsigned invalidationLimit;
  if (invalidationSet.maxDirectAdjacentSelectors() == UINT_MAX)
    invalidationLimit = UINT_MAX;
  else
    invalidationLimit =
        m_elementIndex + invalidationSet.maxDirectAdjacentSelectors();
  m_invalidationEntries.append(Entry(&invalidationSet, invalidationLimit));
}

bool StyleInvalidator::SiblingData::matchCurrentInvalidationSets(
    Element& element,
    RecursionData& recursionData) {
  bool thisElementNeedsStyleRecalc = false;
  DCHECK(!recursionData.wholeSubtreeInvalid());

  unsigned index = 0;
  while (index < m_invalidationEntries.size()) {
    if (m_elementIndex > m_invalidationEntries[index].m_invalidationLimit) {
      // m_invalidationEntries[index] only applies to earlier siblings. Remove
      // it.
      m_invalidationEntries[index] = m_invalidationEntries.last();
      m_invalidationEntries.pop_back();
      continue;
    }

    const SiblingInvalidationSet& invalidationSet =
        *m_invalidationEntries[index].m_invalidationSet;
    ++index;
    if (!invalidationSet.invalidatesElement(element))
      continue;

    if (invalidationSet.invalidatesSelf())
      thisElementNeedsStyleRecalc = true;

    if (const DescendantInvalidationSet* descendants =
            invalidationSet.siblingDescendants()) {
      if (descendants->wholeSubtreeInvalid()) {
        element.setNeedsStyleRecalc(SubtreeStyleChange,
                                    StyleChangeReasonForTracing::create(
                                        StyleChangeReason::StyleInvalidator));
        return true;
      }

      if (!descendants->isEmpty())
        recursionData.pushInvalidationSet(*descendants);
    }
  }
  return thisElementNeedsStyleRecalc;
}

void StyleInvalidator::pushInvalidationSetsForContainerNode(
    ContainerNode& node,
    RecursionData& recursionData,
    SiblingData& siblingData) {
  PendingInvalidations* pendingInvalidations =
      m_pendingInvalidationMap.get(&node);
  DCHECK(pendingInvalidations);

  for (const auto& invalidationSet : pendingInvalidations->siblings()) {
    RELEASE_ASSERT(invalidationSet->isAlive());
    siblingData.pushInvalidationSet(toSiblingInvalidationSet(*invalidationSet));
  }

  if (node.getStyleChangeType() >= SubtreeStyleChange)
    return;

  if (!pendingInvalidations->descendants().isEmpty()) {
    for (const auto& invalidationSet : pendingInvalidations->descendants()) {
      RELEASE_ASSERT(invalidationSet->isAlive());
      recursionData.pushInvalidationSet(*invalidationSet);
    }
    if (UNLIKELY(*s_tracingEnabled)) {
      TRACE_EVENT_INSTANT1(
          TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),
          "StyleInvalidatorInvalidationTracking", TRACE_EVENT_SCOPE_THREAD,
          "data", InspectorStyleInvalidatorInvalidateEvent::invalidationList(
                      node, pendingInvalidations->descendants()));
    }
  }
}

ALWAYS_INLINE bool StyleInvalidator::checkInvalidationSetsAgainstElement(
    Element& element,
    RecursionData& recursionData,
    SiblingData& siblingData) {
  if (recursionData.wholeSubtreeInvalid())
    return false;

  bool thisElementNeedsStyleRecalc = false;
  if (element.getStyleChangeType() >= SubtreeStyleChange) {
    recursionData.setWholeSubtreeInvalid();
  } else {
    thisElementNeedsStyleRecalc =
        recursionData.matchesCurrentInvalidationSets(element);
    if (UNLIKELY(!siblingData.isEmpty()))
      thisElementNeedsStyleRecalc |=
          siblingData.matchCurrentInvalidationSets(element, recursionData);
  }

  if (UNLIKELY(element.needsStyleInvalidation()))
    pushInvalidationSetsForContainerNode(element, recursionData, siblingData);

  return thisElementNeedsStyleRecalc;
}

bool StyleInvalidator::invalidateShadowRootChildren(
    Element& element,
    RecursionData& recursionData) {
  bool someChildrenNeedStyleRecalc = false;
  for (ShadowRoot* root = element.youngestShadowRoot(); root;
       root = root->olderShadowRoot()) {
    if (!recursionData.treeBoundaryCrossing() &&
        !root->childNeedsStyleInvalidation() && !root->needsStyleInvalidation())
      continue;
    RecursionCheckpoint checkpoint(&recursionData);
    SiblingData siblingData;
    if (UNLIKELY(root->needsStyleInvalidation()))
      pushInvalidationSetsForContainerNode(*root, recursionData, siblingData);
    for (Element* child = ElementTraversal::firstChild(*root); child;
         child = ElementTraversal::nextSibling(*child)) {
      bool childRecalced = invalidate(*child, recursionData, siblingData);
      someChildrenNeedStyleRecalc =
          someChildrenNeedStyleRecalc || childRecalced;
    }
    root->clearChildNeedsStyleInvalidation();
    root->clearNeedsStyleInvalidation();
  }
  return someChildrenNeedStyleRecalc;
}

bool StyleInvalidator::invalidateChildren(Element& element,
                                          RecursionData& recursionData) {
  SiblingData siblingData;
  bool someChildrenNeedStyleRecalc = false;
  if (UNLIKELY(!!element.youngestShadowRoot())) {
    someChildrenNeedStyleRecalc =
        invalidateShadowRootChildren(element, recursionData);
  }

  for (Element* child = ElementTraversal::firstChild(element); child;
       child = ElementTraversal::nextSibling(*child)) {
    bool childRecalced = invalidate(*child, recursionData, siblingData);
    someChildrenNeedStyleRecalc = someChildrenNeedStyleRecalc || childRecalced;
  }
  return someChildrenNeedStyleRecalc;
}

bool StyleInvalidator::invalidate(Element& element,
                                  RecursionData& recursionData,
                                  SiblingData& siblingData) {
  siblingData.advance();
  RecursionCheckpoint checkpoint(&recursionData);

  bool thisElementNeedsStyleRecalc =
      checkInvalidationSetsAgainstElement(element, recursionData, siblingData);

  bool someChildrenNeedStyleRecalc = false;
  if (recursionData.hasInvalidationSets() ||
      element.childNeedsStyleInvalidation())
    someChildrenNeedStyleRecalc = invalidateChildren(element, recursionData);

  if (thisElementNeedsStyleRecalc) {
    DCHECK(!recursionData.wholeSubtreeInvalid());
    element.setNeedsStyleRecalc(LocalStyleChange,
                                StyleChangeReasonForTracing::create(
                                    StyleChangeReason::StyleInvalidator));
  } else if (recursionData.hasInvalidationSets() &&
             someChildrenNeedStyleRecalc) {
    // Clone the ComputedStyle in order to preserve correct style sharing, if
    // possible. Otherwise recalc style.
    if (LayoutObject* layoutObject = element.layoutObject()) {
      layoutObject->setStyleInternal(
          ComputedStyle::clone(layoutObject->styleRef()));
    } else {
      TRACE_STYLE_INVALIDATOR_INVALIDATION_IF_ENABLED(
          element, PreventStyleSharingForParent);
      element.setNeedsStyleRecalc(LocalStyleChange,
                                  StyleChangeReasonForTracing::create(
                                      StyleChangeReason::StyleInvalidator));
    }
  }

  if (recursionData.insertionPointCrossing() && element.isInsertionPoint())
    element.setNeedsStyleRecalc(SubtreeStyleChange,
                                StyleChangeReasonForTracing::create(
                                    StyleChangeReason::StyleInvalidator));
  if (recursionData.invalidatesSlotted() && isHTMLSlotElement(element))
    invalidateSlotDistributedElements(toHTMLSlotElement(element),
                                      recursionData);

  element.clearChildNeedsStyleInvalidation();
  element.clearNeedsStyleInvalidation();

  return thisElementNeedsStyleRecalc;
}

void StyleInvalidator::invalidateSlotDistributedElements(
    HTMLSlotElement& slot,
    const RecursionData& recursionData) const {
  for (auto& distributedNode : slot.getDistributedNodes()) {
    if (distributedNode->needsStyleRecalc())
      continue;
    if (!distributedNode->isElementNode())
      continue;
    if (recursionData.matchesCurrentInvalidationSetsAsSlotted(
            toElement(*distributedNode)))
      distributedNode->setNeedsStyleRecalc(
          LocalStyleChange, StyleChangeReasonForTracing::create(
                                StyleChangeReason::StyleInvalidator));
  }
}

}  // namespace blink

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

#include "core/dom/shadow/ElementShadow.h"

#include "core/css/StyleSheetList.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/shadow/ElementShadowV0.h"
#include "core/frame/Deprecation.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/ScriptForbiddenScope.h"

namespace blink {

ElementShadow* ElementShadow::create() {
  return new ElementShadow();
}

ElementShadow::ElementShadow()
    : m_elementShadowV0(this, nullptr),
      m_shadowRoot(this, nullptr),
      m_needsDistributionRecalc(false) {}

ElementShadow::~ElementShadow() {}

ShadowRoot& ElementShadow::youngestShadowRoot() const {
  ShadowRoot* current = m_shadowRoot;
  DCHECK(current);
  while (current->youngerShadowRoot())
    current = current->youngerShadowRoot();
  return *current;
}

ShadowRoot& ElementShadow::addShadowRoot(Element& shadowHost,
                                         ShadowRootType type) {
  EventDispatchForbiddenScope assertNoEventDispatch;
  ScriptForbiddenScope forbidScript;

  if (type == ShadowRootType::V0 && m_shadowRoot) {
    DCHECK_EQ(m_shadowRoot->type(), ShadowRootType::V0);
    Deprecation::countDeprecation(shadowHost.document(),
                                  UseCounter::ElementCreateShadowRootMultiple);
  }

  if (m_shadowRoot) {
    // TODO(hayato): Is the order, from the youngest to the oldest, important?
    for (ShadowRoot* root = &youngestShadowRoot(); root;
         root = root->olderShadowRoot())
      root->lazyReattachIfAttached();
  } else if (type == ShadowRootType::V0 || type == ShadowRootType::UserAgent) {
    DCHECK(!m_elementShadowV0);
    m_elementShadowV0 = ElementShadowV0::create(*this);
  }

  ShadowRoot* shadowRoot = ShadowRoot::create(shadowHost.document(), type);
  shadowRoot->setParentOrShadowHostNode(&shadowHost);
  shadowRoot->setParentTreeScope(shadowHost.treeScope());
  appendShadowRoot(*shadowRoot);
  setNeedsDistributionRecalc();

  shadowRoot->insertedInto(&shadowHost);
  shadowHost.setChildNeedsStyleRecalc();
  shadowHost.setNeedsStyleRecalc(
      SubtreeStyleChange,
      StyleChangeReasonForTracing::create(StyleChangeReason::Shadow));

  InspectorInstrumentation::didPushShadowRoot(&shadowHost, shadowRoot);

  return *shadowRoot;
}

void ElementShadow::appendShadowRoot(ShadowRoot& shadowRoot) {
  if (!m_shadowRoot) {
    m_shadowRoot = &shadowRoot;
    return;
  }
  ShadowRoot& youngest = youngestShadowRoot();
  DCHECK(shadowRoot.type() == ShadowRootType::V0);
  DCHECK(youngest.type() == ShadowRootType::V0);
  youngest.setYoungerShadowRoot(shadowRoot);
  shadowRoot.setOlderShadowRoot(youngest);
}

void ElementShadow::attach(const Node::AttachContext& context) {
  Node::AttachContext childrenContext(context);
  childrenContext.resolvedStyle = 0;

  for (ShadowRoot* root = &youngestShadowRoot(); root;
       root = root->olderShadowRoot()) {
    if (root->needsAttach())
      root->attachLayoutTree(childrenContext);
  }
}

void ElementShadow::detach(const Node::AttachContext& context) {
  Node::AttachContext childrenContext(context);
  childrenContext.resolvedStyle = 0;

  for (ShadowRoot* root = &youngestShadowRoot(); root;
       root = root->olderShadowRoot())
    root->detachLayoutTree(childrenContext);
}

void ElementShadow::setNeedsDistributionRecalc() {
  if (m_needsDistributionRecalc)
    return;
  m_needsDistributionRecalc = true;
  host().markAncestorsWithChildNeedsDistributionRecalc();
  if (!isV1())
    v0().clearDistribution();
}

bool ElementShadow::hasSameStyles(const ElementShadow& other) const {
  ShadowRoot* root = &youngestShadowRoot();
  ShadowRoot* otherRoot = &other.youngestShadowRoot();
  while (root || otherRoot) {
    if (!root || !otherRoot)
      return false;

    StyleSheetList& list = root->styleSheets();
    StyleSheetList& otherList = otherRoot->styleSheets();

    if (list.length() != otherList.length())
      return false;

    for (size_t i = 0; i < list.length(); i++) {
      if (toCSSStyleSheet(list.item(i))->contents() !=
          toCSSStyleSheet(otherList.item(i))->contents())
        return false;
    }
    root = root->olderShadowRoot();
    otherRoot = otherRoot->olderShadowRoot();
  }

  return true;
}

void ElementShadow::distribute() {
  if (isV1())
    youngestShadowRoot().distributeV1();
  else
    v0().distribute();
}

DEFINE_TRACE(ElementShadow) {
  visitor->trace(m_elementShadowV0);
  visitor->trace(m_shadowRoot);
}

DEFINE_TRACE_WRAPPERS(ElementShadow) {
  visitor->traceWrappers(m_elementShadowV0);
  visitor->traceWrappers(m_shadowRoot);
}

}  // namespace blink

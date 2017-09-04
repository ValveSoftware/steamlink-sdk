// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/SVGElementProxy.h"

#include "core/dom/IdTargetObserver.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGResourceClient.h"

namespace blink {

class SVGElementProxy::IdObserver : public IdTargetObserver {
 public:
  IdObserver(TreeScope& treeScope, SVGElementProxy& proxy)
      : IdTargetObserver(treeScope.idTargetObserverRegistry(), proxy.id()),
        m_treeScope(&treeScope) {}

  void addClient(SVGResourceClient* client) { m_clients.add(client); }
  bool removeClient(SVGResourceClient* client) {
    return m_clients.remove(client);
  }
  bool hasClients() const { return !m_clients.isEmpty(); }

  TreeScope* treeScope() const { return m_treeScope; }
  void transferClients(IdObserver& observer) {
    for (const auto& client : m_clients)
      observer.m_clients.add(client.key, client.value);
    m_clients.clear();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_clients);
    visitor->trace(m_treeScope);
    IdTargetObserver::trace(visitor);
  }

  void contentChanged() {
    DCHECK(lifecycle().state() <= DocumentLifecycle::CompositingClean ||
           lifecycle().state() >= DocumentLifecycle::PaintClean);
    HeapVector<Member<SVGResourceClient>> clients;
    copyToVector(m_clients, clients);
    for (SVGResourceClient* client : clients)
      client->resourceContentChanged();
  }

 private:
  const DocumentLifecycle& lifecycle() const {
    return m_treeScope->document().lifecycle();
  }
  void idTargetChanged() override {
    DCHECK(lifecycle().stateAllowsTreeMutations());
    HeapVector<Member<SVGResourceClient>> clients;
    copyToVector(m_clients, clients);
    for (SVGResourceClient* client : clients)
      client->resourceElementChanged();
  }
  HeapHashCountedSet<Member<SVGResourceClient>> m_clients;
  Member<TreeScope> m_treeScope;
};

SVGElementProxy::SVGElementProxy(const AtomicString& id)
    : m_id(id), m_isLocal(true) {}

SVGElementProxy::SVGElementProxy(const String& url, const AtomicString& id)
    : m_id(id), m_url(url), m_isLocal(false) {}

SVGElementProxy::~SVGElementProxy() {}

void SVGElementProxy::addClient(SVGResourceClient* client) {
  // An empty id will never be a valid element reference.
  if (m_id.isEmpty())
    return;
  if (!m_isLocal) {
    if (m_document)
      m_document->addClient(client);
    return;
  }
  TreeScope* clientScope = client->treeScope();
  if (!clientScope)
    return;
  // Ensure sure we have an observer registered for this tree scope.
  auto& scopeObserver =
      m_observers.add(clientScope, nullptr).storedValue->value;
  if (!scopeObserver)
    scopeObserver = new IdObserver(*clientScope, *this);

  auto& observer = m_clients.add(client, nullptr).storedValue->value;
  if (!observer)
    observer = scopeObserver;

  DCHECK(observer && scopeObserver);

  // If the client moved to a different scope, we need to unregister the old
  // observer and transfer any clients from it before replacing it. Thus any
  // clients that remain to be removed will be transferred to the new observer,
  // and hence removed from it instead.
  if (observer != scopeObserver) {
    observer->unregister();
    observer->transferClients(*scopeObserver);
    observer = scopeObserver;
  }
  observer->addClient(client);
}

void SVGElementProxy::removeClient(SVGResourceClient* client) {
  // An empty id will never be a valid element reference.
  if (m_id.isEmpty())
    return;
  if (!m_isLocal) {
    if (m_document)
      m_document->removeClient(client);
    return;
  }
  auto entry = m_clients.find(client);
  if (entry == m_clients.end())
    return;
  IdObserver* observer = entry->value;
  DCHECK(observer);
  // If the client is not the last client in the scope, then no further action
  // needs to be taken.
  if (!observer->removeClient(client))
    return;
  // Unregister and drop the scope association, then drop the client.
  if (!observer->hasClients()) {
    observer->unregister();
    m_observers.remove(observer->treeScope());
  }
  m_clients.remove(entry);
}

void SVGElementProxy::resolve(Document& document) {
  if (m_isLocal || m_id.isEmpty() || m_url.isEmpty())
    return;
  FetchRequest request(ResourceRequest(m_url), FetchInitiatorTypeNames::css);
  m_document = DocumentResource::fetchSVGDocument(request, document.fetcher());
  m_url = String();
}

TreeScope* SVGElementProxy::treeScopeForLookup(TreeScope& treeScope) const {
  if (m_isLocal)
    return &treeScope;
  if (!m_document)
    return nullptr;
  return m_document->document();
}

SVGElement* SVGElementProxy::findElement(TreeScope& treeScope) {
  // An empty id will never be a valid element reference.
  if (m_id.isEmpty())
    return nullptr;
  TreeScope* lookupScope = treeScopeForLookup(treeScope);
  if (!lookupScope)
    return nullptr;
  if (Element* targetElement = lookupScope->getElementById(m_id)) {
    SVGElementProxySet* proxySet =
        targetElement->isSVGElement()
            ? toSVGElement(targetElement)->elementProxySet()
            : nullptr;
    if (proxySet) {
      proxySet->add(*this);
      return toSVGElement(targetElement);
    }
  }
  return nullptr;
}

void SVGElementProxy::contentChanged(TreeScope& treeScope) {
  if (auto* observer = m_observers.get(&treeScope))
    observer->contentChanged();
}

DEFINE_TRACE(SVGElementProxy) {
  visitor->trace(m_clients);
  visitor->trace(m_observers);
  visitor->trace(m_document);
}

void SVGElementProxySet::add(SVGElementProxy& elementProxy) {
  m_elementProxies.add(&elementProxy);
}

bool SVGElementProxySet::isEmpty() const {
  return m_elementProxies.isEmpty();
}

void SVGElementProxySet::notifyContentChanged(TreeScope& treeScope) {
  for (SVGElementProxy* proxy : m_elementProxies)
    proxy->contentChanged(treeScope);
}

DEFINE_TRACE(SVGElementProxySet) {
  visitor->trace(m_elementProxies);
}

}  // namespace blink

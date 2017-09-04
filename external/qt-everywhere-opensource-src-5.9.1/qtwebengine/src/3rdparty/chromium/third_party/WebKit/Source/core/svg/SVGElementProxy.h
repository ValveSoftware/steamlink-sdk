// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGElementProxy_h
#define SVGElementProxy_h

#include "core/loader/resource/DocumentResource.h"
#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;
class SVGElement;
class SVGResourceClient;
class TreeScope;

// A proxy to an SVGElement. Allows access to an element with a certain 'id',
// and provides its clients with notifications when the proxied element
// changes.
//
// SVGElements can be referenced using CSS, for example like:
//
//   filter: url(#baz);
//
//    or
//
//   filter: url(foo.com/bar.svg#baz);
//
// SVGElementProxy provide a mechanism to persistently reference an SVGElement
// in such cases - regardless of if the proxied element reside in the same
// document (read: tree scope) or in an external (resource) document. Loading
// events related to external documents case needs be handled by the clients
// (SVGResourceClient's default implementation does this.)
//
// The proxy uses an 'id' (c.f getElementById) to determine which element to
// proxy. The id is looked up in the TreeScope provided by the client for a
// same-document reference and in the TreeScope of the Document for a external
// document reference.
//
// For same document references, the proxy will track changes that could affect
// the 'id' lookup, to handle elements being added, removed or having their
// 'id' mutated. (This does not apply for the external document case because
// it's assumed they will not mutate after load, due to scripts not being run
// etc.)
//
// SVGElementProxys are created by CSSURIValue, and have SVGResourceClients as
// a means to deliver change notifications. Clients that are interested in
// change notifications hence need to register a SVGResourceClient with the
// proxy. Most commonly this registration would take place when the
// ComputedStyle changes.
//
// Registration of a client does not establish a link to the proxied element
// right away. That is instead deferred until findElement(...) is called on the
// SVGElementProxy. At this point, the proxy will be registered with the
// element if the lookup is successful. (For external references lookups will
// not be registered, per the same assumption as mentioned above.)
//
// As content is mutated, clients will get notified via the proxied element
// which in turn will notify all registered proxies, which will notify all
// registered clients.
//
// <event> -> SVG...Element -> SVGElementProxy(0..N) -> SVGResourceClient(0..N)
//
class SVGElementProxy : public GarbageCollectedFinalized<SVGElementProxy> {
 public:
  // Create a proxy to an element in the same document. (See also
  // SVGURLReferenceResolver and the definition of 'local url'.)
  static SVGElementProxy* create(const AtomicString& id) {
    return new SVGElementProxy(id);
  }

  // Create a proxy to an element in a different document (indicated by |url|.)
  static SVGElementProxy* create(const String& url, const AtomicString& id) {
    return new SVGElementProxy(url, id);
  }
  virtual ~SVGElementProxy();

  void addClient(SVGResourceClient*);
  void removeClient(SVGResourceClient*);

  // Resolve a potentially external document reference.
  void resolve(Document&);

  // Returns the proxied element, or null if the proxy is invalid.
  SVGElement* findElement(TreeScope&);

  // Notify the proxy that the structure of the subtree rooted at the proxied
  // element has mutated. This should generally only be called via a proxy set.
  void contentChanged(TreeScope&);

  const AtomicString& id() const { return m_id; }

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit SVGElementProxy(const AtomicString&);
  SVGElementProxy(const String&, const AtomicString&);

  TreeScope* treeScopeForLookup(TreeScope&) const;

  class IdObserver;

  HeapHashMap<Member<SVGResourceClient>, Member<IdObserver>> m_clients;
  HeapHashMap<Member<TreeScope>, Member<IdObserver>> m_observers;
  Member<DocumentResource> m_document;
  AtomicString m_id;
  // URL for resolving references to external resource documents. Contains an
  // absolute URL to the resource to load. Cleared when a load has been
  // initiated. Ignored if m_isLocal is true.
  String m_url;
  bool m_isLocal;
};

// Collection of SVGElementProxys. This is hosted by elements that can be
// subject to proxies (see SVGElement::elementProxySet), and is mainly a helper
// for dealing with the many-to-one structure of SVGElementProxy.
class SVGElementProxySet : public GarbageCollected<SVGElementProxySet> {
 public:
  void add(SVGElementProxy&);
  bool isEmpty() const;

  // Notify proxy clients that the (content of the) proxied element has
  // changed.
  void notifyContentChanged(TreeScope&);

  DECLARE_TRACE();

 private:
  using ProxySet = HeapHashSet<WeakMember<SVGElementProxy>>;
  ProxySet m_elementProxies;
};

}  // namespace blink

#endif  // SVGElementProxy_h

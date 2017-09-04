/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef HTMLLinkElement_h
#define HTMLLinkElement_h

#include "bindings/core/v8/TraceWrapperMember.h"
#include "core/CoreExport.h"
#include "core/dom/DOMTokenList.h"
#include "core/dom/IncrementLoadEventDelayCount.h"
#include "core/html/HTMLElement.h"
#include "core/html/LinkRelAttribute.h"
#include "core/html/LinkResource.h"
#include "core/html/LinkStyle.h"
#include "core/html/RelList.h"
#include "core/loader/LinkLoader.h"
#include "core/loader/LinkLoaderClient.h"
#include <memory>

namespace blink {

class KURL;
class LinkImport;

class CORE_EXPORT HTMLLinkElement final : public HTMLElement,
                                          public LinkLoaderClient,
                                          private DOMTokenListObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(HTMLLinkElement);

 public:
  static HTMLLinkElement* create(Document&, bool createdByParser);
  ~HTMLLinkElement() override;

  KURL href() const;
  const AtomicString& rel() const;
  String media() const { return m_media; }
  String typeValue() const { return m_type; }
  String asValue() const { return m_as; }
  ReferrerPolicy referrerPolicy() const { return m_referrerPolicy; }
  const LinkRelAttribute& relAttribute() const { return m_relAttribute; }
  DOMTokenList& relList() const {
    return static_cast<DOMTokenList&>(*m_relList);
  }
  String scope() const { return m_scope; }

  const AtomicString& type() const;

  IconType getIconType() const;

  // the icon sizes as parsed from the HTML attribute
  const Vector<IntSize>& iconSizes() const;

  bool async() const;

  CSSStyleSheet* sheet() const {
    return linkStyle() ? linkStyle()->sheet() : 0;
  }
  Document* import() const;

  bool styleSheetIsLoading() const;

  bool isImport() const { return linkImport(); }
  bool isDisabled() const { return linkStyle() && linkStyle()->isDisabled(); }
  bool isEnabledViaScript() const {
    return linkStyle() && linkStyle()->isEnabledViaScript();
  }

  DOMTokenList* sizes() const;

  void dispatchPendingEvent(std::unique_ptr<IncrementLoadEventDelayCount>);
  void scheduleEvent();

  // From LinkLoaderClient
  bool shouldLoadLink() override;

  // For LinkStyle
  bool loadLink(const String& type,
                const String& as,
                const String& media,
                ReferrerPolicy,
                const KURL&);
  bool isAlternate() const {
    return linkStyle()->isUnset() && m_relAttribute.isAlternate();
  }
  bool shouldProcessStyle() { return linkResourceToProcess() && linkStyle(); }
  bool isCreatedByParser() const { return m_createdByParser; }

  DECLARE_VIRTUAL_TRACE();

  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 private:
  HTMLLinkElement(Document&, bool createdByParser);

  LinkStyle* linkStyle() const;
  LinkImport* linkImport() const;
  LinkResource* linkResourceToProcess();

  void process();
  static void processCallback(Node*);

  // From Node and subclassses
  void parseAttribute(const QualifiedName&,
                      const AtomicString&,
                      const AtomicString&) override;
  InsertionNotificationRequest insertedInto(ContainerNode*) override;
  void removedFrom(ContainerNode*) override;
  bool isURLAttribute(const Attribute&) const override;
  bool hasLegalLinkAttribute(const QualifiedName&) const override;
  const QualifiedName& subResourceAttributeName() const override;
  bool sheetLoaded() override;
  void notifyLoadedSheetAndAllCriticalSubresources(
      LoadedSheetErrorStatus) override;
  void startLoadingDynamicSheet() override;
  void finishParsingChildren() override;

  // From LinkLoaderClient
  void linkLoaded() override;
  void linkLoadingErrored() override;
  void didStartLinkPrerender() override;
  void didStopLinkPrerender() override;
  void didSendLoadForLinkPrerender() override;
  void didSendDOMContentLoadedForLinkPrerender() override;

  // From DOMTokenListObserver
  void valueWasSet() final;

  Member<LinkResource> m_link;
  Member<LinkLoader> m_linkLoader;

  String m_type;
  String m_as;
  String m_media;
  ReferrerPolicy m_referrerPolicy;
  Member<DOMTokenList> m_sizes;
  Vector<IntSize> m_iconSizes;
  TraceWrapperMember<RelList> m_relList;
  LinkRelAttribute m_relAttribute;
  String m_scope;

  bool m_createdByParser;
};

}  // namespace blink

#endif  // HTMLLinkElement_h

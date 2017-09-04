/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2009 Rob Buis (rwlbuis@gmail.com)
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
 */

#include "core/html/HTMLLinkElement.h"

#include "bindings/core/v8/ScriptEventListener.h"
#include "core/HTMLNames.h"
#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/Event.h"
#include "core/frame/UseCounter.h"
#include "core/html/CrossOriginAttribute.h"
#include "core/html/LinkManifest.h"
#include "core/html/imports/LinkImport.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/NetworkHintsInterface.h"
#include "core/origin_trials/OriginTrials.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/WebIconSizesParser.h"
#include "public/platform/WebSize.h"

namespace blink {

using namespace HTMLNames;

inline HTMLLinkElement::HTMLLinkElement(Document& document,
                                        bool createdByParser)
    : HTMLElement(linkTag, document),
      m_linkLoader(LinkLoader::create(this)),
      m_sizes(DOMTokenList::create(this)),
      m_relList(this, RelList::create(this)),
      m_createdByParser(createdByParser) {}

HTMLLinkElement* HTMLLinkElement::create(Document& document,
                                         bool createdByParser) {
  return new HTMLLinkElement(document, createdByParser);
}

HTMLLinkElement::~HTMLLinkElement() {}

void HTMLLinkElement::parseAttribute(const QualifiedName& name,
                                     const AtomicString& oldValue,
                                     const AtomicString& value) {
  if (name == relAttr) {
    m_relAttribute = LinkRelAttribute(value);
    m_relList->setRelValues(value);
    process();
  } else if (name == hrefAttr) {
    // Log href attribute before logging resource fetching in process().
    logUpdateAttributeIfIsolatedWorldAndInDocument("link", hrefAttr, oldValue,
                                                   value);
    process();
  } else if (name == typeAttr) {
    m_type = value;
    process();
  } else if (name == asAttr) {
    m_as = value;
    process();
  } else if (name == referrerpolicyAttr) {
    m_referrerPolicy = ReferrerPolicyDefault;
    if (!value.isNull())
      SecurityPolicy::referrerPolicyFromString(value, &m_referrerPolicy);
  } else if (name == sizesAttr) {
    m_sizes->setValue(value);
  } else if (name == mediaAttr) {
    m_media = value.lower();
    process();
  } else if (name == scopeAttr) {
    m_scope = value;
    process();
  } else if (name == disabledAttr) {
    UseCounter::count(document(), UseCounter::HTMLLinkElementDisabled);
    if (LinkStyle* link = linkStyle())
      link->setDisabledState(!value.isNull());
  } else {
    if (name == titleAttr) {
      if (LinkStyle* link = linkStyle())
        link->setSheetTitle(value, StyleEngine::UpdateActiveSheets);
    }

    HTMLElement::parseAttribute(name, oldValue, value);
  }
}

bool HTMLLinkElement::shouldLoadLink() {
  return isInDocumentTree() || (isConnected() && m_relAttribute.isStyleSheet());
}

bool HTMLLinkElement::loadLink(const String& type,
                               const String& as,
                               const String& media,
                               ReferrerPolicy referrerPolicy,
                               const KURL& url) {
  return m_linkLoader->loadLink(
      m_relAttribute,
      crossOriginAttributeValue(fastGetAttribute(HTMLNames::crossoriginAttr)),
      type, as, media, referrerPolicy, url, document(),
      NetworkHintsInterfaceImpl());
}

LinkResource* HTMLLinkElement::linkResourceToProcess() {
  if (!shouldLoadLink()) {
    DCHECK(!linkStyle() || !linkStyle()->hasSheet());
    return nullptr;
  }

  if (!m_link) {
    if (m_relAttribute.isImport()) {
      m_link = LinkImport::create(this);
    } else if (m_relAttribute.isManifest()) {
      m_link = LinkManifest::create(this);
    } else if (m_relAttribute.isServiceWorker() &&
               OriginTrials::linkServiceWorkerEnabled(getExecutionContext())) {
      if (document().frame())
        m_link = document()
                     .frame()
                     ->loader()
                     .client()
                     ->createServiceWorkerLinkResource(this);
    } else {
      LinkStyle* link = LinkStyle::create(this);
      if (fastHasAttribute(disabledAttr)) {
        UseCounter::count(document(), UseCounter::HTMLLinkElementDisabled);
        link->setDisabledState(true);
      }
      m_link = link;
    }
  }

  return m_link.get();
}

LinkStyle* HTMLLinkElement::linkStyle() const {
  if (!m_link || m_link->type() != LinkResource::Style)
    return nullptr;
  return static_cast<LinkStyle*>(m_link.get());
}

LinkImport* HTMLLinkElement::linkImport() const {
  if (!m_link || m_link->type() != LinkResource::Import)
    return nullptr;
  return static_cast<LinkImport*>(m_link.get());
}

Document* HTMLLinkElement::import() const {
  if (LinkImport* link = linkImport())
    return link->importedDocument();
  return nullptr;
}

void HTMLLinkElement::process() {
  if (LinkResource* link = linkResourceToProcess())
    link->process();
}

Node::InsertionNotificationRequest HTMLLinkElement::insertedInto(
    ContainerNode* insertionPoint) {
  HTMLElement::insertedInto(insertionPoint);
  logAddElementIfIsolatedWorldAndInDocument("link", relAttr, hrefAttr);
  if (!insertionPoint->isConnected())
    return InsertionDone;
  DCHECK(isConnected());
  if (!shouldLoadLink()) {
    DCHECK(isInShadowTree());
    String message = "HTML element <link> is ignored in shadow tree.";
    document().addConsoleMessage(
        ConsoleMessage::create(JSMessageSource, WarningMessageLevel, message));
    return InsertionDone;
  }

  document().styleEngine().addStyleSheetCandidateNode(*this);

  process();

  if (m_link)
    m_link->ownerInserted();

  return InsertionDone;
}

void HTMLLinkElement::removedFrom(ContainerNode* insertionPoint) {
  // Store the result of isConnected() here before Node::removedFrom(..) clears
  // the flags.
  bool wasConnected = isConnected();
  HTMLElement::removedFrom(insertionPoint);
  if (!insertionPoint->isConnected())
    return;

  m_linkLoader->released();

  if (!wasConnected) {
    DCHECK(!linkStyle() || !linkStyle()->hasSheet());
    return;
  }
  document().styleEngine().removeStyleSheetCandidateNode(*this);

  StyleSheet* removedSheet = sheet();

  if (m_link)
    m_link->ownerRemoved();

  document().styleEngine().setNeedsActiveStyleUpdate(removedSheet,
                                                     FullStyleUpdate);
}

void HTMLLinkElement::finishParsingChildren() {
  m_createdByParser = false;
  HTMLElement::finishParsingChildren();
}

bool HTMLLinkElement::styleSheetIsLoading() const {
  return linkStyle() && linkStyle()->styleSheetIsLoading();
}

void HTMLLinkElement::linkLoaded() {
  dispatchEvent(Event::create(EventTypeNames::load));
}

void HTMLLinkElement::linkLoadingErrored() {
  dispatchEvent(Event::create(EventTypeNames::error));
}

void HTMLLinkElement::didStartLinkPrerender() {
  dispatchEvent(Event::create(EventTypeNames::webkitprerenderstart));
}

void HTMLLinkElement::didStopLinkPrerender() {
  dispatchEvent(Event::create(EventTypeNames::webkitprerenderstop));
}

void HTMLLinkElement::didSendLoadForLinkPrerender() {
  dispatchEvent(Event::create(EventTypeNames::webkitprerenderload));
}

void HTMLLinkElement::didSendDOMContentLoadedForLinkPrerender() {
  dispatchEvent(Event::create(EventTypeNames::webkitprerenderdomcontentloaded));
}

void HTMLLinkElement::valueWasSet() {
  setSynchronizedLazyAttribute(HTMLNames::sizesAttr, m_sizes->value());
  WebVector<WebSize> webIconSizes =
      WebIconSizesParser::parseIconSizes(m_sizes->value());
  m_iconSizes.resize(webIconSizes.size());
  for (size_t i = 0; i < webIconSizes.size(); ++i)
    m_iconSizes[i] = webIconSizes[i];
  process();
}

bool HTMLLinkElement::sheetLoaded() {
  DCHECK(linkStyle());
  return linkStyle()->sheetLoaded();
}

void HTMLLinkElement::notifyLoadedSheetAndAllCriticalSubresources(
    LoadedSheetErrorStatus errorStatus) {
  DCHECK(linkStyle());
  linkStyle()->notifyLoadedSheetAndAllCriticalSubresources(errorStatus);
}

void HTMLLinkElement::dispatchPendingEvent(
    std::unique_ptr<IncrementLoadEventDelayCount>) {
  DCHECK(m_link);
  if (m_link->hasLoaded())
    linkLoaded();
  else
    linkLoadingErrored();
}

void HTMLLinkElement::scheduleEvent() {
  // TODO(hiroshige): Use DOMManipulation task runner. Unthrottled
  // is temporarily used for fixing https://crbug.com/649942 only on M-56.
  TaskRunnerHelper::get(TaskType::Unthrottled, &document())
      ->postTask(
          BLINK_FROM_HERE,
          WTF::bind(&HTMLLinkElement::dispatchPendingEvent,
                    wrapPersistent(this),
                    passed(IncrementLoadEventDelayCount::create(document()))));
}

void HTMLLinkElement::startLoadingDynamicSheet() {
  DCHECK(linkStyle());
  linkStyle()->startLoadingDynamicSheet();
}

bool HTMLLinkElement::isURLAttribute(const Attribute& attribute) const {
  return attribute.name().localName() == hrefAttr ||
         HTMLElement::isURLAttribute(attribute);
}

bool HTMLLinkElement::hasLegalLinkAttribute(const QualifiedName& name) const {
  return name == hrefAttr || HTMLElement::hasLegalLinkAttribute(name);
}

const QualifiedName& HTMLLinkElement::subResourceAttributeName() const {
  // If the link element is not css, ignore it.
  if (equalIgnoringCase(getAttribute(typeAttr), "text/css")) {
    // FIXME: Add support for extracting links of sub-resources which
    // are inside style-sheet such as @import, @font-face, url(), etc.
    return hrefAttr;
  }
  return HTMLElement::subResourceAttributeName();
}

KURL HTMLLinkElement::href() const {
  return document().completeURL(getAttribute(hrefAttr));
}

const AtomicString& HTMLLinkElement::rel() const {
  return getAttribute(relAttr);
}

const AtomicString& HTMLLinkElement::type() const {
  return getAttribute(typeAttr);
}

bool HTMLLinkElement::async() const {
  return fastHasAttribute(HTMLNames::asyncAttr);
}

IconType HTMLLinkElement::getIconType() const {
  return m_relAttribute.getIconType();
}

const Vector<IntSize>& HTMLLinkElement::iconSizes() const {
  return m_iconSizes;
}

DOMTokenList* HTMLLinkElement::sizes() const {
  return m_sizes.get();
}

DEFINE_TRACE(HTMLLinkElement) {
  visitor->trace(m_link);
  visitor->trace(m_sizes);
  visitor->trace(m_linkLoader);
  visitor->trace(m_relList);
  HTMLElement::trace(visitor);
  LinkLoaderClient::trace(visitor);
  DOMTokenListObserver::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(HTMLLinkElement) {
  visitor->traceWrappers(m_relList);
  HTMLElement::traceWrappers(visitor);
}

}  // namespace blink

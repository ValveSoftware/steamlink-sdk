/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Ericsson AB. All rights reserved.
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

#include "core/html/HTMLIFrameElement.h"

#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLDocument.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/LayoutIFrame.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

using namespace HTMLNames;

inline HTMLIFrameElement::HTMLIFrameElement(Document& document)
    : HTMLFrameElementBase(iframeTag, document),
      m_didLoadNonEmptyDocument(false),
      m_sandbox(HTMLIFrameElementSandbox::create(this)),
      m_referrerPolicy(ReferrerPolicyDefault) {}

DEFINE_NODE_FACTORY(HTMLIFrameElement)

DEFINE_TRACE(HTMLIFrameElement) {
  visitor->trace(m_sandbox);
  visitor->trace(m_permissions);
  HTMLFrameElementBase::trace(visitor);
  Supplementable<HTMLIFrameElement>::trace(visitor);
}

HTMLIFrameElement::~HTMLIFrameElement() {}

DOMTokenList* HTMLIFrameElement::sandbox() const {
  return m_sandbox.get();
}

DOMTokenList* HTMLIFrameElement::permissions() const {
  if (!const_cast<HTMLIFrameElement*>(this)->initializePermissionsAttribute())
    return nullptr;
  return m_permissions.get();
}

bool HTMLIFrameElement::isPresentationAttribute(
    const QualifiedName& name) const {
  if (name == widthAttr || name == heightAttr || name == alignAttr ||
      name == frameborderAttr)
    return true;
  return HTMLFrameElementBase::isPresentationAttribute(name);
}

void HTMLIFrameElement::collectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (name == widthAttr) {
    addHTMLLengthToStyle(style, CSSPropertyWidth, value);
  } else if (name == heightAttr) {
    addHTMLLengthToStyle(style, CSSPropertyHeight, value);
  } else if (name == alignAttr) {
    applyAlignmentAttributeToStyle(value, style);
  } else if (name == frameborderAttr) {
    // LocalFrame border doesn't really match the HTML4 spec definition for
    // iframes. It simply adds a presentational hint that the border should be
    // off if set to zero.
    if (!value.toInt()) {
      // Add a rule that nulls out our border width.
      addPropertyToPresentationAttributeStyle(
          style, CSSPropertyBorderWidth, 0,
          CSSPrimitiveValue::UnitType::Pixels);
    }
  } else {
    HTMLFrameElementBase::collectStyleForPresentationAttribute(name, value,
                                                               style);
  }
}

void HTMLIFrameElement::parseAttribute(const QualifiedName& name,
                                       const AtomicString& oldValue,
                                       const AtomicString& value) {
  if (name == nameAttr) {
    if (isInDocumentTree() && document().isHTMLDocument()) {
      HTMLDocument& document = toHTMLDocument(this->document());
      document.removeExtraNamedItem(m_name);
      document.addExtraNamedItem(value);
    }
    m_name = value;
  } else if (name == sandboxAttr) {
    m_sandbox->setValue(value);
    UseCounter::count(document(), UseCounter::SandboxViaIFrame);
  } else if (name == referrerpolicyAttr) {
    m_referrerPolicy = ReferrerPolicyDefault;
    if (!value.isNull())
      SecurityPolicy::referrerPolicyFromStringWithLegacyKeywords(
          value, &m_referrerPolicy);
  } else if (name == allowfullscreenAttr) {
    bool oldAllowFullscreen = m_allowFullscreen;
    m_allowFullscreen = !value.isNull();
    if (m_allowFullscreen != oldAllowFullscreen)
      frameOwnerPropertiesChanged();
  } else if (name == allowpaymentrequestAttr) {
    bool oldAllowPaymentRequest = m_allowPaymentRequest;
    m_allowPaymentRequest = !value.isNull();
    if (m_allowPaymentRequest != oldAllowPaymentRequest)
      frameOwnerPropertiesChanged();
  } else if (name == permissionsAttr) {
    if (initializePermissionsAttribute())
      m_permissions->setValue(value);
  } else if (RuntimeEnabledFeatures::embedderCSPEnforcementEnabled() &&
             name == cspAttr) {
    // TODO(amalika): add more robust validation of the value
    if (!value.getString().containsOnlyASCII()) {
      m_csp = nullAtom;
      document().addConsoleMessage(ConsoleMessage::create(
          OtherMessageSource, ErrorMessageLevel,
          "'csp' attribute contains non-ASCII characters: " + value));
      return;
    }
    AtomicString oldCSP = m_csp;
    m_csp = value;
    if (m_csp != oldCSP)
      frameOwnerPropertiesChanged();
  } else {
    if (name == srcAttr)
      logUpdateAttributeIfIsolatedWorldAndInDocument("iframe", srcAttr,
                                                     oldValue, value);
    HTMLFrameElementBase::parseAttribute(name, oldValue, value);
  }
}

bool HTMLIFrameElement::layoutObjectIsNeeded(const ComputedStyle& style) {
  return contentFrame() && HTMLElement::layoutObjectIsNeeded(style);
}

LayoutObject* HTMLIFrameElement::createLayoutObject(const ComputedStyle&) {
  return new LayoutIFrame(this);
}

Node::InsertionNotificationRequest HTMLIFrameElement::insertedInto(
    ContainerNode* insertionPoint) {
  InsertionNotificationRequest result =
      HTMLFrameElementBase::insertedInto(insertionPoint);
  if (insertionPoint->isInDocumentTree() && document().isHTMLDocument())
    toHTMLDocument(document()).addExtraNamedItem(m_name);
  logAddElementIfIsolatedWorldAndInDocument("iframe", srcAttr);
  return result;
}

void HTMLIFrameElement::removedFrom(ContainerNode* insertionPoint) {
  HTMLFrameElementBase::removedFrom(insertionPoint);
  if (insertionPoint->isInDocumentTree() && document().isHTMLDocument())
    toHTMLDocument(document()).removeExtraNamedItem(m_name);
}

bool HTMLIFrameElement::isInteractiveContent() const {
  return true;
}

void HTMLIFrameElement::permissionsValueWasSet() {
  if (!initializePermissionsAttribute())
    return;

  String invalidTokens;
  m_delegatedPermissions =
      m_permissions->parseDelegatedPermissions(invalidTokens);
  if (!invalidTokens.isNull())
    document().addConsoleMessage(ConsoleMessage::create(
        OtherMessageSource, ErrorMessageLevel,
        "Error while parsing the 'permissions' attribute: " + invalidTokens));
  setSynchronizedLazyAttribute(permissionsAttr, m_permissions->value());
  frameOwnerPropertiesChanged();
}

void HTMLIFrameElement::sandboxValueWasSet() {
  String invalidTokens;
  setSandboxFlags(m_sandbox->value().isNull()
                      ? SandboxNone
                      : parseSandboxPolicy(m_sandbox->tokens(), invalidTokens));
  if (!invalidTokens.isNull())
    document().addConsoleMessage(ConsoleMessage::create(
        OtherMessageSource, ErrorMessageLevel,
        "Error while parsing the 'sandbox' attribute: " + invalidTokens));
  setSynchronizedLazyAttribute(sandboxAttr, m_sandbox->value());
}

ReferrerPolicy HTMLIFrameElement::referrerPolicyAttribute() {
  return m_referrerPolicy;
}

bool HTMLIFrameElement::initializePermissionsAttribute() {
  if (!RuntimeEnabledFeatures::permissionDelegationEnabled())
    return false;

  if (!m_permissions)
    m_permissions = HTMLIFrameElementPermissions::create(this);
  return true;
}

}  // namespace blink

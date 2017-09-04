/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGURIReference.h"

#include "core/XLinkNames.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/svg/SVGElement.h"
#include "platform/weborigin/KURL.h"

namespace blink {

SVGURIReference::SVGURIReference(SVGElement* element)
    : m_href(SVGAnimatedHref::create(element)) {
  ASSERT(element);
  m_href->addToPropertyMap(element);
}

DEFINE_TRACE(SVGURIReference) {
  visitor->trace(m_href);
}

bool SVGURIReference::isKnownAttribute(const QualifiedName& attrName) {
  return SVGAnimatedHref::isKnownAttribute(attrName);
}

const AtomicString& SVGURIReference::legacyHrefString(
    const SVGElement& element) {
  if (element.hasAttribute(SVGNames::hrefAttr))
    return element.getAttribute(SVGNames::hrefAttr);
  return element.getAttribute(XLinkNames::hrefAttr);
}

KURL SVGURIReference::legacyHrefURL(const Document& document) const {
  return document.completeURL(stripLeadingAndTrailingHTMLSpaces(hrefString()));
}

SVGURLReferenceResolver::SVGURLReferenceResolver(const String& urlString,
                                                 const Document& document)
    : m_relativeUrl(urlString),
      m_document(&document),
      m_isLocal(urlString.startsWith('#')) {}

KURL SVGURLReferenceResolver::absoluteUrl() const {
  if (m_absoluteUrl.isNull())
    m_absoluteUrl = m_document->completeURL(m_relativeUrl);
  return m_absoluteUrl;
}

bool SVGURLReferenceResolver::isLocal() const {
  return m_isLocal ||
         equalIgnoringFragmentIdentifier(absoluteUrl(), m_document->url());
}

AtomicString SVGURLReferenceResolver::fragmentIdentifier() const {
  // If this is a "fragment-only" URL, then the reference is always local, so
  // just return what's after the '#' as the fragment.
  if (m_isLocal)
    return AtomicString(m_relativeUrl.substring(1));
  return AtomicString(absoluteUrl().fragmentIdentifier());
}

AtomicString SVGURIReference::fragmentIdentifierFromIRIString(
    const String& urlString,
    const TreeScope& treeScope) {
  SVGURLReferenceResolver resolver(urlString, treeScope.document());
  if (!resolver.isLocal())
    return emptyAtom;
  return resolver.fragmentIdentifier();
}

Element* SVGURIReference::targetElementFromIRIString(
    const String& urlString,
    const TreeScope& treeScope,
    AtomicString* fragmentIdentifier) {
  AtomicString id = fragmentIdentifierFromIRIString(urlString, treeScope);
  if (id.isEmpty())
    return nullptr;
  if (fragmentIdentifier)
    *fragmentIdentifier = id;
  return treeScope.getElementById(id);
}

}  // namespace blink

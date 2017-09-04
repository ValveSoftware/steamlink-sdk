/*
 * Copyright (C) 2004, 2005, 2008, 2009 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#ifndef SVGURIReference_h
#define SVGURIReference_h

#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "core/svg/SVGAnimatedHref.h"
#include "platform/heap/Handle.h"

namespace blink {

class Element;

class CORE_EXPORT SVGURIReference : public GarbageCollectedMixin {
 public:
  virtual ~SVGURIReference() {}

  bool isKnownAttribute(const QualifiedName&);

  // Use this for accesses to 'href' or 'xlink:href' (in that order) for
  // elements where both are allowed and don't necessarily inherit from
  // SVGURIReference.
  static const AtomicString& legacyHrefString(const SVGElement&);

  // Like above, but for elements that inherit from SVGURIReference. Resolves
  // against the base URL of the passed Document.
  KURL legacyHrefURL(const Document&) const;

  static AtomicString fragmentIdentifierFromIRIString(const String&,
                                                      const TreeScope&);
  static Element* targetElementFromIRIString(const String&,
                                             const TreeScope&,
                                             AtomicString* = nullptr);

  const String& hrefString() const { return m_href->currentValue()->value(); }

  // JS API
  SVGAnimatedHref* href() const { return m_href.get(); }

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit SVGURIReference(SVGElement*);

 private:
  Member<SVGAnimatedHref> m_href;
};

// Helper class used to resolve fragment references. Handles the 'local url
// flag' per https://drafts.csswg.org/css-values/#local-urls .
class SVGURLReferenceResolver {
  STACK_ALLOCATED();

 public:
  SVGURLReferenceResolver(const String& urlString, const Document&);

  bool isLocal() const;
  KURL absoluteUrl() const;
  AtomicString fragmentIdentifier() const;

 private:
  const String& m_relativeUrl;
  Member<const Document> m_document;
  mutable KURL m_absoluteUrl;
  bool m_isLocal;
};

}  // namespace blink

#endif  // SVGURIReference_h

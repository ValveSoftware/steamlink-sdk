/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
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

#ifndef HTMLTableCellElement_h
#define HTMLTableCellElement_h

#include "core/html/HTMLTablePartElement.h"

namespace WebCore {

class HTMLTableCellElement FINAL : public HTMLTablePartElement {
public:
    DECLARE_ELEMENT_FACTORY_WITH_TAGNAME(HTMLTableCellElement);

    int cellIndex() const;

    int colSpan() const;
    int rowSpan() const;

    void setCellIndex(int);

    const AtomicString& abbr() const;
    const AtomicString& axis() const;
    void setColSpan(int);
    const AtomicString& headers() const;
    void setRowSpan(int);
    const AtomicString& scope() const;

    HTMLTableCellElement* cellAbove() const;

private:
    HTMLTableCellElement(const QualifiedName&, Document&);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE;
    virtual const StylePropertySet* additionalPresentationAttributeStyle() OVERRIDE;

    virtual bool isURLAttribute(const Attribute&) const OVERRIDE;
    virtual bool hasLegalLinkAttribute(const QualifiedName&) const OVERRIDE;
    virtual const QualifiedName& subResourceAttributeName() const OVERRIDE;
};

inline bool isHTMLTableCellElement(const Element& element)
{
    return element.hasTagName(HTMLNames::tdTag) || element.hasTagName(HTMLNames::thTag);
}

inline bool isHTMLTableCellElement(const HTMLElement& element)
{
    return element.hasLocalName(HTMLNames::tdTag) || element.hasLocalName(HTMLNames::thTag);
}

DEFINE_HTMLELEMENT_TYPE_CASTS_WITH_FUNCTION(HTMLTableCellElement);

} // namespace

#endif

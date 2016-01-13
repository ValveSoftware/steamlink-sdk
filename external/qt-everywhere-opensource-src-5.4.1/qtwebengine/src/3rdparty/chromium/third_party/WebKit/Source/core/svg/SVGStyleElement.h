/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGStyleElement_h
#define SVGStyleElement_h

#include "core/SVGNames.h"
#include "core/dom/StyleElement.h"
#include "core/svg/SVGElement.h"

namespace WebCore {

class SVGStyleElement FINAL : public SVGElement
                            , public StyleElement {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(SVGStyleElement);
public:
    static PassRefPtrWillBeRawPtr<SVGStyleElement> create(Document&, bool createdByParser);
    virtual ~SVGStyleElement();

    using StyleElement::sheet;

    bool disabled() const;
    void setDisabled(bool);

    virtual const AtomicString& type() const OVERRIDE;
    void setType(const AtomicString&);

    virtual const AtomicString& media() const OVERRIDE;
    void setMedia(const AtomicString&);

    virtual String title() const OVERRIDE;
    void setTitle(const AtomicString&);

    virtual void trace(Visitor*) OVERRIDE;

private:
    SVGStyleElement(Document&, bool createdByParser);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void didNotifySubtreeInsertionsToDocument() OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0) OVERRIDE;

    virtual void finishParsingChildren() OVERRIDE;
    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE { return false; }

    virtual bool sheetLoaded() OVERRIDE { return StyleElement::sheetLoaded(document()); }
    virtual void startLoadingDynamicSheet() OVERRIDE { StyleElement::startLoadingDynamicSheet(document()); }
    virtual Timer<SVGElement>* svgLoadEventTimer() OVERRIDE { return &m_svgLoadEventTimer; }

    Timer<SVGElement> m_svgLoadEventTimer;
};

} // namespace WebCore

#endif // SVGStyleElement_h

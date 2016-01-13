/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGTextPathElement_h
#define SVGTextPathElement_h

#include "core/SVGNames.h"
#include "core/svg/SVGTextContentElement.h"
#include "core/svg/SVGURIReference.h"

namespace WebCore {

enum SVGTextPathMethodType {
    SVGTextPathMethodUnknown = 0,
    SVGTextPathMethodAlign,
    SVGTextPathMethodStretch
};

enum SVGTextPathSpacingType {
    SVGTextPathSpacingUnknown = 0,
    SVGTextPathSpacingAuto,
    SVGTextPathSpacingExact
};

template<> const SVGEnumerationStringEntries& getStaticStringEntries<SVGTextPathMethodType>();
template<> const SVGEnumerationStringEntries& getStaticStringEntries<SVGTextPathSpacingType>();

class SVGTextPathElement FINAL : public SVGTextContentElement,
                                 public SVGURIReference {
public:
    // Forward declare enumerations in the W3C naming scheme, for IDL generation.
    enum {
        TEXTPATH_METHODTYPE_UNKNOWN = SVGTextPathMethodUnknown,
        TEXTPATH_METHODTYPE_ALIGN = SVGTextPathMethodAlign,
        TEXTPATH_METHODTYPE_STRETCH = SVGTextPathMethodStretch,
        TEXTPATH_SPACINGTYPE_UNKNOWN = SVGTextPathSpacingUnknown,
        TEXTPATH_SPACINGTYPE_AUTO = SVGTextPathSpacingAuto,
        TEXTPATH_SPACINGTYPE_EXACT = SVGTextPathSpacingExact
    };

    DECLARE_NODE_FACTORY(SVGTextPathElement);

    SVGAnimatedLength* startOffset() const { return m_startOffset.get(); }
    SVGAnimatedEnumeration<SVGTextPathMethodType>* method() { return m_method.get(); }
    SVGAnimatedEnumeration<SVGTextPathSpacingType>* spacing() { return m_spacing.get(); }

private:
    explicit SVGTextPathElement(Document&);

    virtual ~SVGTextPathElement();

    void clearResourceReferences();

    virtual void buildPendingResource() OVERRIDE;
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;

    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;
    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE;

    virtual bool selfHasRelativeLengths() const OVERRIDE;

    RefPtr<SVGAnimatedLength> m_startOffset;
    RefPtr<SVGAnimatedEnumeration<SVGTextPathMethodType> > m_method;
    RefPtr<SVGAnimatedEnumeration<SVGTextPathSpacingType> > m_spacing;
};

} // namespace WebCore

#endif

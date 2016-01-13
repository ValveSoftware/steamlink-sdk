/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
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

#ifndef SVGTextContentElement_h
#define SVGTextContentElement_h

#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGGraphicsElement.h"
#include "core/svg/SVGPointTearOff.h"

namespace WebCore {

class ExceptionState;

enum SVGLengthAdjustType {
    SVGLengthAdjustUnknown,
    SVGLengthAdjustSpacing,
    SVGLengthAdjustSpacingAndGlyphs
};
template<> const SVGEnumerationStringEntries& getStaticStringEntries<SVGLengthAdjustType>();

class SVGTextContentElement : public SVGGraphicsElement {
public:
    // Forward declare enumerations in the W3C naming scheme, for IDL generation.
    enum {
        LENGTHADJUST_UNKNOWN = SVGLengthAdjustUnknown,
        LENGTHADJUST_SPACING = SVGLengthAdjustSpacing,
        LENGTHADJUST_SPACINGANDGLYPHS = SVGLengthAdjustSpacingAndGlyphs
    };

    unsigned getNumberOfChars();
    float getComputedTextLength();
    float getSubStringLength(unsigned charnum, unsigned nchars, ExceptionState&);
    PassRefPtr<SVGPointTearOff> getStartPositionOfChar(unsigned charnum, ExceptionState&);
    PassRefPtr<SVGPointTearOff> getEndPositionOfChar(unsigned charnum, ExceptionState&);
    PassRefPtr<SVGRectTearOff> getExtentOfChar(unsigned charnum, ExceptionState&);
    float getRotationOfChar(unsigned charnum, ExceptionState&);
    int getCharNumAtPosition(PassRefPtr<SVGPointTearOff>, ExceptionState&);
    void selectSubString(unsigned charnum, unsigned nchars, ExceptionState&);

    static SVGTextContentElement* elementFromRenderer(RenderObject*);

    SVGAnimatedLength* textLength() { return m_textLength.get(); }
    bool textLengthIsSpecifiedByUser() { return m_textLengthIsSpecifiedByUser; }
    SVGAnimatedEnumeration<SVGLengthAdjustType>* lengthAdjust() { return m_lengthAdjust.get(); }

protected:
    SVGTextContentElement(const QualifiedName&, Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE FINAL;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE FINAL;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;

    virtual bool selfHasRelativeLengths() const OVERRIDE;

private:
    virtual bool isTextContent() const OVERRIDE FINAL { return true; }

    RefPtr<SVGAnimatedLength> m_textLength;
    bool m_textLengthIsSpecifiedByUser;
    RefPtr<SVGAnimatedEnumeration<SVGLengthAdjustType> > m_lengthAdjust;
};

inline bool isSVGTextContentElement(const Node& node)
{
    return node.isSVGElement() && toSVGElement(node).isTextContent();
}

DEFINE_ELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGTextContentElement);

} // namespace WebCore

#endif

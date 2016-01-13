/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
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

#include "config.h"
#include "core/svg/SVGTextContentElement.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/SVGNames.h"
#include "core/XMLNames.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrame.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/svg/RenderSVGResource.h"
#include "core/rendering/svg/SVGTextQuery.h"

namespace WebCore {

template<> const SVGEnumerationStringEntries& getStaticStringEntries<SVGLengthAdjustType>()
{
    DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
    if (entries.isEmpty()) {
        entries.append(std::make_pair(SVGLengthAdjustSpacing, "spacing"));
        entries.append(std::make_pair(SVGLengthAdjustSpacingAndGlyphs, "spacingAndGlyphs"));
    }
    return entries;
}

// SVGTextContentElement's 'textLength' attribute needs special handling.
// It should return getComputedTextLength() when textLength is not specified manually.
class SVGAnimatedTextLength FINAL : public SVGAnimatedLength {
public:
    static PassRefPtr<SVGAnimatedTextLength> create(SVGTextContentElement* contextElement)
    {
        return adoptRef(new SVGAnimatedTextLength(contextElement));
    }

    virtual SVGLengthTearOff* baseVal() OVERRIDE
    {
        SVGTextContentElement* textContentElement = toSVGTextContentElement(contextElement());
        if (!textContentElement->textLengthIsSpecifiedByUser())
            baseValue()->newValueSpecifiedUnits(LengthTypeNumber, textContentElement->getComputedTextLength());

        return SVGAnimatedLength::baseVal();
    }

private:
    SVGAnimatedTextLength(SVGTextContentElement* contextElement)
        : SVGAnimatedLength(contextElement, SVGNames::textLengthAttr, SVGLength::create(LengthModeOther), ForbidNegativeLengths)
    {
    }
};


SVGTextContentElement::SVGTextContentElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document)
    , m_textLength(SVGAnimatedTextLength::create(this))
    , m_textLengthIsSpecifiedByUser(false)
    , m_lengthAdjust(SVGAnimatedEnumeration<SVGLengthAdjustType>::create(this, SVGNames::lengthAdjustAttr, SVGLengthAdjustSpacing))
{
    addToPropertyMap(m_textLength);
    addToPropertyMap(m_lengthAdjust);
}

unsigned SVGTextContentElement::getNumberOfChars()
{
    document().updateLayoutIgnorePendingStylesheets();
    return SVGTextQuery(renderer()).numberOfCharacters();
}

float SVGTextContentElement::getComputedTextLength()
{
    document().updateLayoutIgnorePendingStylesheets();
    return SVGTextQuery(renderer()).textLength();
}

float SVGTextContentElement::getSubStringLength(unsigned charnum, unsigned nchars, ExceptionState& exceptionState)
{
    document().updateLayoutIgnorePendingStylesheets();

    unsigned numberOfChars = getNumberOfChars();
    if (charnum >= numberOfChars) {
        exceptionState.throwDOMException(IndexSizeError, ExceptionMessages::indexExceedsMaximumBound("charnum", charnum, getNumberOfChars()));
        return 0.0f;
    }

    if (nchars > numberOfChars - charnum)
        nchars = numberOfChars - charnum;

    return SVGTextQuery(renderer()).subStringLength(charnum, nchars);
}

PassRefPtr<SVGPointTearOff> SVGTextContentElement::getStartPositionOfChar(unsigned charnum, ExceptionState& exceptionState)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (charnum > getNumberOfChars()) {
        exceptionState.throwDOMException(IndexSizeError, ExceptionMessages::indexExceedsMaximumBound("charnum", charnum, getNumberOfChars()));
        return nullptr;
    }

    FloatPoint point = SVGTextQuery(renderer()).startPositionOfCharacter(charnum);
    return SVGPointTearOff::create(SVGPoint::create(point), 0, PropertyIsNotAnimVal);
}

PassRefPtr<SVGPointTearOff> SVGTextContentElement::getEndPositionOfChar(unsigned charnum, ExceptionState& exceptionState)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (charnum > getNumberOfChars()) {
        exceptionState.throwDOMException(IndexSizeError, ExceptionMessages::indexExceedsMaximumBound("charnum", charnum, getNumberOfChars()));
        return nullptr;
    }

    FloatPoint point = SVGTextQuery(renderer()).endPositionOfCharacter(charnum);
    return SVGPointTearOff::create(SVGPoint::create(point), 0, PropertyIsNotAnimVal);
}

PassRefPtr<SVGRectTearOff> SVGTextContentElement::getExtentOfChar(unsigned charnum, ExceptionState& exceptionState)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (charnum > getNumberOfChars()) {
        exceptionState.throwDOMException(IndexSizeError, ExceptionMessages::indexExceedsMaximumBound("charnum", charnum, getNumberOfChars()));
        return nullptr;
    }

    FloatRect rect = SVGTextQuery(renderer()).extentOfCharacter(charnum);
    return SVGRectTearOff::create(SVGRect::create(rect), 0, PropertyIsNotAnimVal);
}

float SVGTextContentElement::getRotationOfChar(unsigned charnum, ExceptionState& exceptionState)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (charnum > getNumberOfChars()) {
        exceptionState.throwDOMException(IndexSizeError, ExceptionMessages::indexExceedsMaximumBound("charnum", charnum, getNumberOfChars()));
        return 0.0f;
    }

    return SVGTextQuery(renderer()).rotationOfCharacter(charnum);
}

int SVGTextContentElement::getCharNumAtPosition(PassRefPtr<SVGPointTearOff> point, ExceptionState& exceptionState)
{
    document().updateLayoutIgnorePendingStylesheets();
    return SVGTextQuery(renderer()).characterNumberAtPosition(point->target()->value());
}

void SVGTextContentElement::selectSubString(unsigned charnum, unsigned nchars, ExceptionState& exceptionState)
{
    unsigned numberOfChars = getNumberOfChars();
    if (charnum >= numberOfChars) {
        exceptionState.throwDOMException(IndexSizeError, ExceptionMessages::indexExceedsMaximumBound("charnum", charnum, getNumberOfChars()));
        return;
    }

    if (nchars > numberOfChars - charnum)
        nchars = numberOfChars - charnum;

    ASSERT(document().frame());

    // Find selection start
    VisiblePosition start(firstPositionInNode(const_cast<SVGTextContentElement*>(this)));
    for (unsigned i = 0; i < charnum; ++i)
        start = start.next();

    // Find selection end
    VisiblePosition end(start);
    for (unsigned i = 0; i < nchars; ++i)
        end = end.next();

    document().frame()->selection().setSelection(VisibleSelection(start, end));
}

bool SVGTextContentElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        supportedAttributes.add(SVGNames::lengthAdjustAttr);
        supportedAttributes.add(SVGNames::textLengthAttr);
        supportedAttributes.add(XMLNames::spaceAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

bool SVGTextContentElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name.matches(XMLNames::spaceAttr))
        return true;
    return SVGGraphicsElement::isPresentationAttribute(name);
}

void SVGTextContentElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (!isSupportedAttribute(name))
        SVGGraphicsElement::collectStyleForPresentationAttribute(name, value, style);
    else if (name.matches(XMLNames::spaceAttr)) {
        DEFINE_STATIC_LOCAL(const AtomicString, preserveString, ("preserve", AtomicString::ConstructFromLiteral));

        if (value == preserveString)
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWhiteSpace, CSSValuePre);
        else
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWhiteSpace, CSSValueNowrap);
    }
}

void SVGTextContentElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name))
        SVGGraphicsElement::parseAttribute(name, value);
    else if (name == SVGNames::lengthAdjustAttr) {
        m_lengthAdjust->setBaseValueAsString(value, parseError);
    } else if (name == SVGNames::textLengthAttr) {
        m_textLength->setBaseValueAsString(value, parseError);
    } else if (name.matches(XMLNames::spaceAttr)) {
    } else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

void SVGTextContentElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGGraphicsElement::svgAttributeChanged(attrName);
        return;
    }

    if (attrName == SVGNames::textLengthAttr)
        m_textLengthIsSpecifiedByUser = true;

    SVGElement::InvalidationGuard invalidationGuard(this);

    if (RenderObject* renderer = this->renderer())
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
}

bool SVGTextContentElement::selfHasRelativeLengths() const
{
    // Any element of the <text> subtree is advertized as using relative lengths.
    // On any window size change, we have to relayout the text subtree, as the
    // effective 'on-screen' font size may change.
    return true;
}

SVGTextContentElement* SVGTextContentElement::elementFromRenderer(RenderObject* renderer)
{
    if (!renderer)
        return 0;

    if (!renderer->isSVGText() && !renderer->isSVGInline())
        return 0;

    SVGElement* element = toSVGElement(renderer->node());
    ASSERT(element);
    return isSVGTextContentElement(*element) ? toSVGTextContentElement(element) : 0;
}

}

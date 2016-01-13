/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "core/svg/SVGPathSegList.h"

#include "core/SVGNames.h"
#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGPathBlender.h"
#include "core/svg/SVGPathByteStreamBuilder.h"
#include "core/svg/SVGPathByteStreamSource.h"
#include "core/svg/SVGPathElement.h"
#include "core/svg/SVGPathParser.h"
#include "core/svg/SVGPathSegListBuilder.h"
#include "core/svg/SVGPathSegListSource.h"
#include "core/svg/SVGPathUtilities.h"

namespace WebCore {

SVGPathSegList::SVGPathSegList(SVGPathElement* contextElement, SVGPathSegRole role)
    : m_contextElement(contextElement)
    , m_role(role)
    , m_listSyncedToByteStream(true)
{
    ASSERT(contextElement);
}

SVGPathSegList::SVGPathSegList(SVGPathElement* contextElement, SVGPathSegRole role, PassOwnPtr<SVGPathByteStream> byteStream)
    : m_contextElement(contextElement)
    , m_role(role)
    , m_byteStream(byteStream)
    , m_listSyncedToByteStream(true)
{
    ASSERT(contextElement);
}

SVGPathSegList::~SVGPathSegList()
{
}

PassRefPtr<SVGPathSegList> SVGPathSegList::clone()
{
    RefPtr<SVGPathSegList> svgPathSegList = adoptRef(new SVGPathSegList(m_contextElement, m_role, byteStream()->copy()));
    svgPathSegList->invalidateList();
    return svgPathSegList.release();
}

PassRefPtr<SVGPropertyBase> SVGPathSegList::cloneForAnimation(const String& value) const
{
    RefPtr<SVGPathSegList> svgPathSegList = SVGPathSegList::create(m_contextElement);
    svgPathSegList->setValueAsString(value, IGNORE_EXCEPTION);
    return svgPathSegList;
}

const SVGPathByteStream* SVGPathSegList::byteStream() const
{
    if (!m_byteStream) {
        m_byteStream = SVGPathByteStream::create();

        if (!Base::isEmpty()) {
            SVGPathByteStreamBuilder builder;
            builder.setCurrentByteStream(m_byteStream.get());

            SVGPathSegListSource source(begin(), end());

            SVGPathParser parser;
            parser.setCurrentConsumer(&builder);
            parser.setCurrentSource(&source);
            parser.parsePathDataFromSource(UnalteredParsing);
        }
    }

    return m_byteStream.get();
}

void SVGPathSegList::updateListFromByteStream()
{
    if (m_listSyncedToByteStream)
        return;

    Base::clear();

    if (m_byteStream && !m_byteStream->isEmpty()) {
        SVGPathSegListBuilder builder;
        builder.setCurrentSVGPathElement(m_contextElement);
        builder.setCurrentSVGPathSegList(this);
        builder.setCurrentSVGPathSegRole(PathSegUnalteredRole);

        SVGPathByteStreamSource source(m_byteStream.get());

        SVGPathParser parser;
        parser.setCurrentConsumer(&builder);
        parser.setCurrentSource(&source);
        parser.parsePathDataFromSource(UnalteredParsing);
    }

    m_listSyncedToByteStream = true;
}

void SVGPathSegList::invalidateList()
{
    m_listSyncedToByteStream = false;
    Base::clear();
}

PassRefPtr<SVGPathSeg> SVGPathSegList::appendItem(PassRefPtr<SVGPathSeg> passItem)
{
    updateListFromByteStream();
    RefPtr<SVGPathSeg> item = Base::appendItem(passItem);

    if (m_byteStream) {
        SVGPathByteStreamBuilder builder;
        builder.setCurrentByteStream(m_byteStream.get());

        SVGPathSegListSource source(lastAppended(), end());

        SVGPathParser parser;
        parser.setCurrentConsumer(&builder);
        parser.setCurrentSource(&source);
        parser.parsePathDataFromSource(UnalteredParsing, false);
    }

    return item.release();
}

String SVGPathSegList::valueAsString() const
{
    String string;
    buildStringFromByteStream(byteStream(), string, UnalteredParsing);
    return string;
}

void SVGPathSegList::setValueAsString(const String& string, ExceptionState& exceptionState)
{
    invalidateList();
    if (!m_byteStream)
        m_byteStream = SVGPathByteStream::create();
    if (!buildSVGPathByteStreamFromString(string, m_byteStream.get(), UnalteredParsing))
        exceptionState.throwDOMException(SyntaxError, "Problem parsing path \"" + string + "\"");
}

void SVGPathSegList::add(PassRefPtrWillBeRawPtr<SVGPropertyBase> other, SVGElement*)
{
    RefPtr<SVGPathSegList> otherList = toSVGPathSegList(other);
    if (length() != otherList->length())
        return;

    byteStream(); // create |m_byteStream| if not exist.
    addToSVGPathByteStream(m_byteStream.get(), otherList->byteStream());
    invalidateList();
}

void SVGPathSegList::calculateAnimatedValue(SVGAnimationElement* animationElement, float percentage, unsigned repeatCount, PassRefPtr<SVGPropertyBase> fromValue, PassRefPtr<SVGPropertyBase> toValue, PassRefPtr<SVGPropertyBase> toAtEndOfDurationValue, SVGElement*)
{
    invalidateList();

    ASSERT(animationElement);
    bool isToAnimation = animationElement->animationMode() == ToAnimation;

    const RefPtr<SVGPathSegList> from = toSVGPathSegList(fromValue);
    const RefPtr<SVGPathSegList> to = toSVGPathSegList(toValue);
    const RefPtr<SVGPathSegList> toAtEndOfDuration = toSVGPathSegList(toAtEndOfDurationValue);

    const SVGPathByteStream* toStream = to->byteStream();
    const SVGPathByteStream* fromStream = from->byteStream();
    OwnPtr<SVGPathByteStream> copy;

    // If no 'to' value is given, nothing to animate.
    if (!toStream->size())
        return;

    if (isToAnimation) {
        copy = byteStream()->copy();
        fromStream = copy.get();
    }

    // If the 'from' value is given and it's length doesn't match the 'to' value list length, fallback to a discrete animation.
    if (fromStream->size() != toStream->size() && fromStream->size()) {
        if (percentage < 0.5) {
            if (!isToAnimation) {
                m_byteStream = fromStream->copy();
                return;
            }
        } else {
            m_byteStream = toStream->copy();
            return;
        }
    }

    OwnPtr<SVGPathByteStream> lastAnimatedStream = m_byteStream.release();

    m_byteStream = SVGPathByteStream::create();
    SVGPathByteStreamBuilder builder;
    builder.setCurrentByteStream(m_byteStream.get());

    SVGPathByteStreamSource fromSource(fromStream);
    SVGPathByteStreamSource toSource(toStream);

    SVGPathBlender blender;
    blender.blendAnimatedPath(percentage, &fromSource, &toSource, &builder);

    // Handle additive='sum'.
    if (!fromStream->size() || (animationElement->isAdditive() && !isToAnimation))
        addToSVGPathByteStream(m_byteStream.get(), lastAnimatedStream.get());

    // Handle accumulate='sum'.
    if (animationElement->isAccumulated() && repeatCount) {
        const SVGPathByteStream* toAtEndOfDurationStream = toAtEndOfDuration->byteStream();
        addToSVGPathByteStream(m_byteStream.get(), toAtEndOfDurationStream, repeatCount);
    }
}

float SVGPathSegList::calculateDistance(PassRefPtr<SVGPropertyBase> to, SVGElement*)
{
    // FIXME: Support paced animations.
    return -1;
}

}

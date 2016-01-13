/*
 * Copyright (C) 2004, 2005, 2007, 2009 Apple Inc. All rights reserved.
 *           (C) 2005 Rob Buis <buis@kde.org>
 *           (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/rendering/svg/SVGRenderTreeAsText.h"

#include "core/rendering/InlineTextBox.h"
#include "core/rendering/RenderTreeAsText.h"
#include "core/rendering/svg/RenderSVGGradientStop.h"
#include "core/rendering/svg/RenderSVGImage.h"
#include "core/rendering/svg/RenderSVGInlineText.h"
#include "core/rendering/svg/RenderSVGResourceClipper.h"
#include "core/rendering/svg/RenderSVGResourceFilter.h"
#include "core/rendering/svg/RenderSVGResourceLinearGradient.h"
#include "core/rendering/svg/RenderSVGResourceMarker.h"
#include "core/rendering/svg/RenderSVGResourceMasker.h"
#include "core/rendering/svg/RenderSVGResourcePattern.h"
#include "core/rendering/svg/RenderSVGResourceRadialGradient.h"
#include "core/rendering/svg/RenderSVGResourceSolidColor.h"
#include "core/rendering/svg/RenderSVGRoot.h"
#include "core/rendering/svg/RenderSVGShape.h"
#include "core/rendering/svg/RenderSVGText.h"
#include "core/rendering/svg/SVGInlineTextBox.h"
#include "core/rendering/svg/SVGRootInlineBox.h"
#include "core/svg/LinearGradientAttributes.h"
#include "core/svg/PatternAttributes.h"
#include "core/svg/RadialGradientAttributes.h"
#include "core/svg/SVGCircleElement.h"
#include "core/svg/SVGEllipseElement.h"
#include "core/svg/SVGLineElement.h"
#include "core/svg/SVGLinearGradientElement.h"
#include "core/svg/SVGPathElement.h"
#include "core/svg/SVGPathUtilities.h"
#include "core/svg/SVGPatternElement.h"
#include "core/svg/SVGPointList.h"
#include "core/svg/SVGPolyElement.h"
#include "core/svg/SVGRadialGradientElement.h"
#include "core/svg/SVGRectElement.h"
#include "core/svg/SVGStopElement.h"
#include "platform/graphics/DashArray.h"
#include "platform/graphics/GraphicsTypes.h"

#include <math.h>
#include <memory>

namespace WebCore {

/** class + iomanip to help streaming list separators, i.e. ", " in string "a, b, c, d"
 * Can be used in cases where you don't know which item in the list is the first
 * one to be printed, but still want to avoid strings like ", b, c".
 */
class TextStreamSeparator {
public:
    TextStreamSeparator(const String& s)
        : m_separator(s)
        , m_needToSeparate(false)
    {
    }

private:
    friend TextStream& operator<<(TextStream&, TextStreamSeparator&);

    String m_separator;
    bool m_needToSeparate;
};

TextStream& operator<<(TextStream& ts, TextStreamSeparator& sep)
{
    if (sep.m_needToSeparate)
        ts << sep.m_separator;
    else
        sep.m_needToSeparate = true;
    return ts;
}

template<typename ValueType>
static void writeNameValuePair(TextStream& ts, const char* name, ValueType value)
{
    ts << " [" << name << "=" << value << "]";
}

template<typename ValueType>
static void writeNameAndQuotedValue(TextStream& ts, const char* name, ValueType value)
{
    ts << " [" << name << "=\"" << value << "\"]";
}

static void writeIfNotEmpty(TextStream& ts, const char* name, const String& value)
{
    if (!value.isEmpty())
        writeNameValuePair(ts, name, value);
}

template<typename ValueType>
static void writeIfNotDefault(TextStream& ts, const char* name, ValueType value, ValueType defaultValue)
{
    if (value != defaultValue)
        writeNameValuePair(ts, name, value);
}

TextStream& operator<<(TextStream& ts, const AffineTransform& transform)
{
    if (transform.isIdentity())
        ts << "identity";
    else
        ts << "{m=(("
           << transform.a() << "," << transform.b()
           << ")("
           << transform.c() << "," << transform.d()
           << ")) t=("
           << transform.e() << "," << transform.f()
           << ")}";

    return ts;
}

static TextStream& operator<<(TextStream& ts, const WindRule rule)
{
    switch (rule) {
    case RULE_NONZERO:
        ts << "NON-ZERO";
        break;
    case RULE_EVENODD:
        ts << "EVEN-ODD";
        break;
    }

    return ts;
}

namespace {

template<typename Enum>
String SVGEnumerationToString(Enum value)
{
    const SVGEnumerationStringEntries& entries = getStaticStringEntries<Enum>();

    SVGEnumerationStringEntries::const_iterator it = entries.begin();
    SVGEnumerationStringEntries::const_iterator itEnd = entries.end();
    for (; it != itEnd; ++it) {
        if (value == it->first)
            return it->second;
    }

    ASSERT_NOT_REACHED();
    return String();
}

}

static TextStream& operator<<(TextStream& ts, const SVGUnitTypes::SVGUnitType& unitType)
{
    ts << SVGEnumerationToString<SVGUnitTypes::SVGUnitType>(unitType);
    return ts;
}

static TextStream& operator<<(TextStream& ts, const SVGMarkerUnitsType& markerUnit)
{
    ts << SVGEnumerationToString<SVGMarkerUnitsType>(markerUnit);
    return ts;
}

TextStream& operator<<(TextStream& ts, const Color& c)
{
    return ts << c.nameForRenderTreeAsText();
}

// FIXME: Maybe this should be in KCanvasRenderingStyle.cpp
static TextStream& operator<<(TextStream& ts, const DashArray& a)
{
    ts << "{";
    DashArray::const_iterator end = a.end();
    for (DashArray::const_iterator it = a.begin(); it != end; ++it) {
        if (it != a.begin())
            ts << ", ";
        ts << *it;
    }
    ts << "}";
    return ts;
}

// FIXME: Maybe this should be in GraphicsTypes.cpp
static TextStream& operator<<(TextStream& ts, LineCap style)
{
    switch (style) {
    case ButtCap:
        ts << "BUTT";
        break;
    case RoundCap:
        ts << "ROUND";
        break;
    case SquareCap:
        ts << "SQUARE";
        break;
    }
    return ts;
}

// FIXME: Maybe this should be in GraphicsTypes.cpp
static TextStream& operator<<(TextStream& ts, LineJoin style)
{
    switch (style) {
    case MiterJoin:
        ts << "MITER";
        break;
    case RoundJoin:
        ts << "ROUND";
        break;
    case BevelJoin:
        ts << "BEVEL";
        break;
    }
    return ts;
}

static TextStream& operator<<(TextStream& ts, const SVGSpreadMethodType& type)
{
    ts << SVGEnumerationToString<SVGSpreadMethodType>(type).upper();
    return ts;
}

static void writeSVGPaintingResource(TextStream& ts, RenderSVGResource* resource)
{
    if (resource->resourceType() == SolidColorResourceType) {
        ts << "[type=SOLID] [color=" << static_cast<RenderSVGResourceSolidColor*>(resource)->color() << "]";
        return;
    }

    // All other resources derive from RenderSVGResourceContainer
    RenderSVGResourceContainer* container = static_cast<RenderSVGResourceContainer*>(resource);
    SVGElement* element = container->element();
    ASSERT(element);

    if (resource->resourceType() == PatternResourceType)
        ts << "[type=PATTERN]";
    else if (resource->resourceType() == LinearGradientResourceType)
        ts << "[type=LINEAR-GRADIENT]";
    else if (resource->resourceType() == RadialGradientResourceType)
        ts << "[type=RADIAL-GRADIENT]";

    ts << " [id=\"" << element->getIdAttribute() << "\"]";
}

static void writeStyle(TextStream& ts, const RenderObject& object)
{
    const RenderStyle* style = object.style();
    const SVGRenderStyle* svgStyle = style->svgStyle();

    if (!object.localTransform().isIdentity())
        writeNameValuePair(ts, "transform", object.localTransform());
    writeIfNotDefault(ts, "image rendering", style->imageRendering(), RenderStyle::initialImageRendering());
    writeIfNotDefault(ts, "opacity", style->opacity(), RenderStyle::initialOpacity());
    if (object.isSVGShape()) {
        const RenderSVGShape& shape = static_cast<const RenderSVGShape&>(object);
        ASSERT(shape.element());

        bool hasFallback;
        if (RenderSVGResource* strokePaintingResource = RenderSVGResource::strokePaintingResource(const_cast<RenderSVGShape*>(&shape), shape.style(), hasFallback)) {
            TextStreamSeparator s(" ");
            ts << " [stroke={" << s;
            writeSVGPaintingResource(ts, strokePaintingResource);

            SVGLengthContext lengthContext(shape.element());
            double dashOffset = svgStyle->strokeDashOffset()->value(lengthContext);
            double strokeWidth = svgStyle->strokeWidth()->value(lengthContext);
            RefPtr<SVGLengthList> dashes = svgStyle->strokeDashArray();

            DashArray dashArray;
            SVGLengthList::ConstIterator it = dashes->begin();
            SVGLengthList::ConstIterator itEnd = dashes->end();
            for (; it != itEnd; ++it)
                dashArray.append(it->value(lengthContext));

            writeIfNotDefault(ts, "opacity", svgStyle->strokeOpacity(), 1.0f);
            writeIfNotDefault(ts, "stroke width", strokeWidth, 1.0);
            writeIfNotDefault(ts, "miter limit", svgStyle->strokeMiterLimit(), 4.0f);
            writeIfNotDefault(ts, "line cap", svgStyle->capStyle(), ButtCap);
            writeIfNotDefault(ts, "line join", svgStyle->joinStyle(), MiterJoin);
            writeIfNotDefault(ts, "dash offset", dashOffset, 0.0);
            if (!dashArray.isEmpty())
                writeNameValuePair(ts, "dash array", dashArray);

            ts << "}]";
        }

        if (RenderSVGResource* fillPaintingResource = RenderSVGResource::fillPaintingResource(const_cast<RenderSVGShape*>(&shape), shape.style(), hasFallback)) {
            TextStreamSeparator s(" ");
            ts << " [fill={" << s;
            writeSVGPaintingResource(ts, fillPaintingResource);

            writeIfNotDefault(ts, "opacity", svgStyle->fillOpacity(), 1.0f);
            writeIfNotDefault(ts, "fill rule", svgStyle->fillRule(), RULE_NONZERO);
            ts << "}]";
        }
        writeIfNotDefault(ts, "clip rule", svgStyle->clipRule(), RULE_NONZERO);
    }

    writeIfNotEmpty(ts, "start marker", svgStyle->markerStartResource());
    writeIfNotEmpty(ts, "middle marker", svgStyle->markerMidResource());
    writeIfNotEmpty(ts, "end marker", svgStyle->markerEndResource());
}

static TextStream& writePositionAndStyle(TextStream& ts, const RenderObject& object)
{
    ts << " " << enclosingIntRect(const_cast<RenderObject&>(object).absoluteClippedOverflowRect());
    writeStyle(ts, object);
    return ts;
}

static TextStream& operator<<(TextStream& ts, const RenderSVGShape& shape)
{
    writePositionAndStyle(ts, shape);

    SVGElement* svgElement = shape.element();
    ASSERT(svgElement);
    SVGLengthContext lengthContext(svgElement);

    if (isSVGRectElement(*svgElement)) {
        SVGRectElement& element = toSVGRectElement(*svgElement);
        writeNameValuePair(ts, "x", element.x()->currentValue()->value(lengthContext));
        writeNameValuePair(ts, "y", element.y()->currentValue()->value(lengthContext));
        writeNameValuePair(ts, "width", element.width()->currentValue()->value(lengthContext));
        writeNameValuePair(ts, "height", element.height()->currentValue()->value(lengthContext));
    } else if (isSVGLineElement(*svgElement)) {
        SVGLineElement& element = toSVGLineElement(*svgElement);
        writeNameValuePair(ts, "x1", element.x1()->currentValue()->value(lengthContext));
        writeNameValuePair(ts, "y1", element.y1()->currentValue()->value(lengthContext));
        writeNameValuePair(ts, "x2", element.x2()->currentValue()->value(lengthContext));
        writeNameValuePair(ts, "y2", element.y2()->currentValue()->value(lengthContext));
    } else if (isSVGEllipseElement(*svgElement)) {
        SVGEllipseElement& element = toSVGEllipseElement(*svgElement);
        writeNameValuePair(ts, "cx", element.cx()->currentValue()->value(lengthContext));
        writeNameValuePair(ts, "cy", element.cy()->currentValue()->value(lengthContext));
        writeNameValuePair(ts, "rx", element.rx()->currentValue()->value(lengthContext));
        writeNameValuePair(ts, "ry", element.ry()->currentValue()->value(lengthContext));
    } else if (isSVGCircleElement(*svgElement)) {
        SVGCircleElement& element = toSVGCircleElement(*svgElement);
        writeNameValuePair(ts, "cx", element.cx()->currentValue()->value(lengthContext));
        writeNameValuePair(ts, "cy", element.cy()->currentValue()->value(lengthContext));
        writeNameValuePair(ts, "r", element.r()->currentValue()->value(lengthContext));
    } else if (isSVGPolyElement(*svgElement)) {
        writeNameAndQuotedValue(ts, "points", toSVGPolyElement(*svgElement).points()->currentValue()->valueAsString());
    } else if (isSVGPathElement(*svgElement)) {
        String pathString;
        // FIXME: We should switch to UnalteredParsing here - this will affect the path dumping output of dozens of tests.
        buildStringFromByteStream(toSVGPathElement(*svgElement).pathByteStream(), pathString, NormalizedParsing);
        writeNameAndQuotedValue(ts, "data", pathString);
    } else
        ASSERT_NOT_REACHED();
    return ts;
}

static TextStream& operator<<(TextStream& ts, const RenderSVGRoot& root)
{
    return writePositionAndStyle(ts, root);
}

static void writeRenderSVGTextBox(TextStream& ts, const RenderSVGText& text)
{
    SVGRootInlineBox* box = toSVGRootInlineBox(text.firstRootBox());
    if (!box)
        return;

    ts << " " << enclosingIntRect(FloatRect(text.location(), FloatSize(box->logicalWidth(), box->logicalHeight())));

    // FIXME: Remove this hack, once the new text layout engine is completly landed. We want to preserve the old layout test results for now.
    ts << " contains 1 chunk(s)";

    if (text.parent() && (text.parent()->style()->visitedDependentColor(CSSPropertyColor) != text.style()->visitedDependentColor(CSSPropertyColor)))
        writeNameValuePair(ts, "color", text.resolveColor(CSSPropertyColor).nameForRenderTreeAsText());
}

static inline void writeSVGInlineTextBox(TextStream& ts, SVGInlineTextBox* textBox, int indent)
{
    Vector<SVGTextFragment>& fragments = textBox->textFragments();
    if (fragments.isEmpty())
        return;

    RenderSVGInlineText& textRenderer = toRenderSVGInlineText(textBox->textRenderer());

    const SVGRenderStyle* svgStyle = textRenderer.style()->svgStyle();
    String text = textBox->textRenderer().text();

    unsigned fragmentsSize = fragments.size();
    for (unsigned i = 0; i < fragmentsSize; ++i) {
        SVGTextFragment& fragment = fragments.at(i);
        writeIndent(ts, indent + 1);

        unsigned startOffset = fragment.characterOffset;
        unsigned endOffset = fragment.characterOffset + fragment.length;

        // FIXME: Remove this hack, once the new text layout engine is completly landed. We want to preserve the old layout test results for now.
        ts << "chunk 1 ";
        ETextAnchor anchor = svgStyle->textAnchor();
        bool isVerticalText = svgStyle->isVerticalWritingMode();
        if (anchor == TA_MIDDLE) {
            ts << "(middle anchor";
            if (isVerticalText)
                ts << ", vertical";
            ts << ") ";
        } else if (anchor == TA_END) {
            ts << "(end anchor";
            if (isVerticalText)
                ts << ", vertical";
            ts << ") ";
        } else if (isVerticalText)
            ts << "(vertical) ";
        startOffset -= textBox->start();
        endOffset -= textBox->start();
        // </hack>

        ts << "text run " << i + 1 << " at (" << fragment.x << "," << fragment.y << ")";
        ts << " startOffset " << startOffset << " endOffset " << endOffset;
        if (isVerticalText)
            ts << " height " << fragment.height;
        else
            ts << " width " << fragment.width;

        if (!textBox->isLeftToRightDirection() || textBox->dirOverride()) {
            ts << (textBox->isLeftToRightDirection() ? " LTR" : " RTL");
            if (textBox->dirOverride())
                ts << " override";
        }

        ts << ": " << quoteAndEscapeNonPrintables(text.substring(fragment.characterOffset, fragment.length)) << "\n";
    }
}

static inline void writeSVGInlineTextBoxes(TextStream& ts, const RenderText& text, int indent)
{
    for (InlineTextBox* box = text.firstTextBox(); box; box = box->nextTextBox()) {
        if (!box->isSVGInlineTextBox())
            continue;

        writeSVGInlineTextBox(ts, toSVGInlineTextBox(box), indent);
    }
}

static void writeStandardPrefix(TextStream& ts, const RenderObject& object, int indent)
{
    writeIndent(ts, indent);
    ts << object.renderName();

    if (object.node())
        ts << " {" << object.node()->nodeName() << "}";
}

static void writeChildren(TextStream& ts, const RenderObject& object, int indent)
{
    for (RenderObject* child = object.slowFirstChild(); child; child = child->nextSibling())
        write(ts, *child, indent + 1);
}

static inline void writeCommonGradientProperties(TextStream& ts, SVGSpreadMethodType spreadMethod, const AffineTransform& gradientTransform, SVGUnitTypes::SVGUnitType gradientUnits)
{
    writeNameValuePair(ts, "gradientUnits", gradientUnits);

    if (spreadMethod != SVGSpreadMethodPad)
        ts << " [spreadMethod=" << spreadMethod << "]";

    if (!gradientTransform.isIdentity())
        ts << " [gradientTransform=" << gradientTransform << "]";
}

void writeSVGResourceContainer(TextStream& ts, const RenderObject& object, int indent)
{
    writeStandardPrefix(ts, object, indent);

    Element* element = toElement(object.node());
    const AtomicString& id = element->getIdAttribute();
    writeNameAndQuotedValue(ts, "id", id);

    RenderSVGResourceContainer* resource = toRenderSVGResourceContainer(const_cast<RenderObject*>(&object));
    ASSERT(resource);

    if (resource->resourceType() == MaskerResourceType) {
        RenderSVGResourceMasker* masker = toRenderSVGResourceMasker(resource);
        writeNameValuePair(ts, "maskUnits", masker->maskUnits());
        writeNameValuePair(ts, "maskContentUnits", masker->maskContentUnits());
        ts << "\n";
    } else if (resource->resourceType() == FilterResourceType) {
        RenderSVGResourceFilter* filter = toRenderSVGResourceFilter(resource);
        writeNameValuePair(ts, "filterUnits", filter->filterUnits());
        writeNameValuePair(ts, "primitiveUnits", filter->primitiveUnits());
        ts << "\n";
        // Creating a placeholder filter which is passed to the builder.
        FloatRect dummyRect;
        IntRect dummyIntRect;
        RefPtr<SVGFilter> dummyFilter = SVGFilter::create(AffineTransform(), dummyIntRect, dummyRect, dummyRect, true);
        if (RefPtr<SVGFilterBuilder> builder = filter->buildPrimitives(dummyFilter.get())) {
            if (FilterEffect* lastEffect = builder->lastEffect())
                lastEffect->externalRepresentation(ts, indent + 1);
        }
    } else if (resource->resourceType() == ClipperResourceType) {
        writeNameValuePair(ts, "clipPathUnits", toRenderSVGResourceClipper(resource)->clipPathUnits());
        ts << "\n";
    } else if (resource->resourceType() == MarkerResourceType) {
        RenderSVGResourceMarker* marker = toRenderSVGResourceMarker(resource);
        writeNameValuePair(ts, "markerUnits", marker->markerUnits());
        ts << " [ref at " << marker->referencePoint() << "]";
        ts << " [angle=";
        if (marker->angle() == -1)
            ts << "auto" << "]\n";
        else
            ts << marker->angle() << "]\n";
    } else if (resource->resourceType() == PatternResourceType) {
        RenderSVGResourcePattern* pattern = static_cast<RenderSVGResourcePattern*>(resource);

        // Dump final results that are used for rendering. No use in asking SVGPatternElement for its patternUnits(), as it may
        // link to other patterns using xlink:href, we need to build the full inheritance chain, aka. collectPatternProperties()
        PatternAttributes attributes;
        toSVGPatternElement(pattern->element())->collectPatternAttributes(attributes);

        writeNameValuePair(ts, "patternUnits", attributes.patternUnits());
        writeNameValuePair(ts, "patternContentUnits", attributes.patternContentUnits());

        AffineTransform transform = attributes.patternTransform();
        if (!transform.isIdentity())
            ts << " [patternTransform=" << transform << "]";
        ts << "\n";
    } else if (resource->resourceType() == LinearGradientResourceType) {
        RenderSVGResourceLinearGradient* gradient = static_cast<RenderSVGResourceLinearGradient*>(resource);

        // Dump final results that are used for rendering. No use in asking SVGGradientElement for its gradientUnits(), as it may
        // link to other gradients using xlink:href, we need to build the full inheritance chain, aka. collectGradientProperties()
        LinearGradientAttributes attributes;
        toSVGLinearGradientElement(gradient->element())->collectGradientAttributes(attributes);
        writeCommonGradientProperties(ts, attributes.spreadMethod(), attributes.gradientTransform(), attributes.gradientUnits());

        ts << " [start=" << gradient->startPoint(attributes) << "] [end=" << gradient->endPoint(attributes) << "]\n";
    }  else if (resource->resourceType() == RadialGradientResourceType) {
        RenderSVGResourceRadialGradient* gradient = toRenderSVGResourceRadialGradient(resource);

        // Dump final results that are used for rendering. No use in asking SVGGradientElement for its gradientUnits(), as it may
        // link to other gradients using xlink:href, we need to build the full inheritance chain, aka. collectGradientProperties()
        RadialGradientAttributes attributes;
        toSVGRadialGradientElement(gradient->element())->collectGradientAttributes(attributes);
        writeCommonGradientProperties(ts, attributes.spreadMethod(), attributes.gradientTransform(), attributes.gradientUnits());

        FloatPoint focalPoint = gradient->focalPoint(attributes);
        FloatPoint centerPoint = gradient->centerPoint(attributes);
        float radius = gradient->radius(attributes);
        float focalRadius = gradient->focalRadius(attributes);

        ts << " [center=" << centerPoint << "] [focal=" << focalPoint << "] [radius=" << radius << "] [focalRadius=" << focalRadius << "]\n";
    } else
        ts << "\n";
    writeChildren(ts, object, indent);
}

void writeSVGContainer(TextStream& ts, const RenderObject& container, int indent)
{
    // Currently RenderSVGResourceFilterPrimitive has no meaningful output.
    if (container.isSVGResourceFilterPrimitive())
        return;
    writeStandardPrefix(ts, container, indent);
    writePositionAndStyle(ts, container);
    ts << "\n";
    writeResources(ts, container, indent);
    writeChildren(ts, container, indent);
}

void write(TextStream& ts, const RenderSVGRoot& root, int indent)
{
    writeStandardPrefix(ts, root, indent);
    ts << root << "\n";
    writeChildren(ts, root, indent);
}

void writeSVGText(TextStream& ts, const RenderSVGText& text, int indent)
{
    writeStandardPrefix(ts, text, indent);
    writeRenderSVGTextBox(ts, text);
    ts << "\n";
    writeResources(ts, text, indent);
    writeChildren(ts, text, indent);
}

void writeSVGInlineText(TextStream& ts, const RenderSVGInlineText& text, int indent)
{
    writeStandardPrefix(ts, text, indent);
    ts << " " << enclosingIntRect(FloatRect(text.firstRunOrigin(), text.floatLinesBoundingBox().size())) << "\n";
    writeResources(ts, text, indent);
    writeSVGInlineTextBoxes(ts, text, indent);
}

void writeSVGImage(TextStream& ts, const RenderSVGImage& image, int indent)
{
    writeStandardPrefix(ts, image, indent);
    writePositionAndStyle(ts, image);
    ts << "\n";
    writeResources(ts, image, indent);
}

void write(TextStream& ts, const RenderSVGShape& shape, int indent)
{
    writeStandardPrefix(ts, shape, indent);
    ts << shape << "\n";
    writeResources(ts, shape, indent);
}

void writeSVGGradientStop(TextStream& ts, const RenderSVGGradientStop& stop, int indent)
{
    writeStandardPrefix(ts, stop, indent);

    SVGStopElement* stopElement = toSVGStopElement(stop.node());
    ASSERT(stopElement);

    RenderStyle* style = stop.style();
    if (!style)
        return;

    ts << " [offset=" << stopElement->offset()->currentValue()->value() << "] [color=" << stopElement->stopColorIncludingOpacity() << "]\n";
}

void writeResources(TextStream& ts, const RenderObject& object, int indent)
{
    const RenderStyle* style = object.style();
    const SVGRenderStyle* svgStyle = style->svgStyle();

    // FIXME: We want to use SVGResourcesCache to determine which resources are present, instead of quering the resource <-> id cache.
    // For now leave the DRT output as is, but later on we should change this so cycles are properly ignored in the DRT output.
    RenderObject& renderer = const_cast<RenderObject&>(object);
    if (!svgStyle->maskerResource().isEmpty()) {
        if (RenderSVGResourceMasker* masker = getRenderSVGResourceById<RenderSVGResourceMasker>(object.document(), svgStyle->maskerResource())) {
            writeIndent(ts, indent);
            ts << " ";
            writeNameAndQuotedValue(ts, "masker", svgStyle->maskerResource());
            ts << " ";
            writeStandardPrefix(ts, *masker, 0);
            ts << " " << masker->resourceBoundingBox(&renderer) << "\n";
        }
    }
    if (!svgStyle->clipperResource().isEmpty()) {
        if (RenderSVGResourceClipper* clipper = getRenderSVGResourceById<RenderSVGResourceClipper>(object.document(), svgStyle->clipperResource())) {
            writeIndent(ts, indent);
            ts << " ";
            writeNameAndQuotedValue(ts, "clipPath", svgStyle->clipperResource());
            ts << " ";
            writeStandardPrefix(ts, *clipper, 0);
            ts << " " << clipper->resourceBoundingBox(&renderer) << "\n";
        }
    }
    if (!svgStyle->filterResource().isEmpty()) {
        if (RenderSVGResourceFilter* filter = getRenderSVGResourceById<RenderSVGResourceFilter>(object.document(), svgStyle->filterResource())) {
            writeIndent(ts, indent);
            ts << " ";
            writeNameAndQuotedValue(ts, "filter", svgStyle->filterResource());
            ts << " ";
            writeStandardPrefix(ts, *filter, 0);
            ts << " " << filter->resourceBoundingBox(&renderer) << "\n";
        }
    }
}

} // namespace WebCore

/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009, 2011 Apple Inc. All rights reserved.
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

#include "core/html/HTMLAreaElement.h"

#include "core/HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLMapElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutImage.h"
#include "platform/graphics/Path.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {

// Adapt a double to the allowed range of a LayoutUnit and narrow it to float precision.
float clampCoordinate(double value)
{
    return LayoutUnit(value).toFloat();
}

}

using namespace HTMLNames;

inline HTMLAreaElement::HTMLAreaElement(Document& document)
    : HTMLAnchorElement(areaTag, document)
    , m_shape(Rect)
{
}

// An explicit empty destructor should be in HTMLAreaElement.cpp, because
// if an implicit destructor is used or an empty destructor is defined in
// HTMLAreaElement.h, when including HTMLAreaElement.h, msvc tries to expand
// the destructor and causes a compile error because of lack of blink::Path
// definition.
HTMLAreaElement::~HTMLAreaElement()
{
}

DEFINE_NODE_FACTORY(HTMLAreaElement)

void HTMLAreaElement::parseAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& value)
{
    if (name == shapeAttr) {
        if (equalIgnoringASCIICase(value, "default")) {
            m_shape = Default;
        } else if (equalIgnoringASCIICase(value, "circle") || equalIgnoringASCIICase(value, "circ")) {
            m_shape = Circle;
        } else if (equalIgnoringASCIICase(value, "polygon") || equalIgnoringASCIICase(value, "poly")) {
            m_shape = Poly;
        } else {
            // The missing (and implicitly invalid) value default for the
            // 'shape' attribute is 'rect'.
            m_shape = Rect;
        }
        invalidateCachedPath();
    } else if (name == coordsAttr) {
        m_coords = parseHTMLListOfFloatingPointNumbers(value.getString());
        invalidateCachedPath();
    } else if (name == altAttr || name == accesskeyAttr) {
        // Do nothing.
    } else {
        HTMLAnchorElement::parseAttribute(name, oldValue, value);
    }
}

void HTMLAreaElement::invalidateCachedPath()
{
    m_path = nullptr;
}

bool HTMLAreaElement::pointInArea(const LayoutPoint& location, const LayoutObject* containerObject) const
{
    return getPath(containerObject).contains(FloatPoint(location));
}

LayoutRect HTMLAreaElement::computeAbsoluteRect(const LayoutObject* containerObject) const
{
    if (!containerObject)
        return LayoutRect();

    // FIXME: This doesn't work correctly with transforms.
    FloatPoint absPos = containerObject->localToAbsolute();

    Path path = getPath(containerObject);
    path.translate(toFloatSize(absPos));
    return enclosingLayoutRect(path.boundingRect());
}

Path HTMLAreaElement::getPath(const LayoutObject* containerObject) const
{
    if (!containerObject)
        return Path();

    // Always recompute for default shape because it depends on container object's size
    // and is cheap.
    if (m_shape == Default) {
        Path path;
        // No need to zoom because it is already applied in containerObject->borderBoxRect().
        if (containerObject->isBox())
            path.addRect(FloatRect(toLayoutBox(containerObject)->borderBoxRect()));
        m_path = nullptr;
        return path;
    }

    Path path;
    if (m_path) {
        path = *m_path;
    } else {
        if (m_coords.isEmpty())
            return path;

        switch (m_shape) {
        case Poly:
            if (m_coords.size() >= 6) {
                int numPoints = m_coords.size() / 2;
                path.moveTo(FloatPoint(clampCoordinate(m_coords[0]), clampCoordinate(m_coords[1])));
                for (int i = 1; i < numPoints; ++i)
                    path.addLineTo(FloatPoint(clampCoordinate(m_coords[i * 2]), clampCoordinate(m_coords[i * 2 + 1])));
                path.closeSubpath();
                path.setWindRule(RULE_EVENODD);
            }
            break;
        case Circle:
            if (m_coords.size() >= 3 && m_coords[2] > 0) {
                float r = clampCoordinate(m_coords[2]);
                path.addEllipse(FloatRect(clampCoordinate(m_coords[0]) - r, clampCoordinate(m_coords[1]) - r, 2 * r, 2 * r));
            }
            break;
        case Rect:
            if (m_coords.size() >= 4) {
                float x0 = clampCoordinate(m_coords[0]);
                float y0 = clampCoordinate(m_coords[1]);
                float x1 = clampCoordinate(m_coords[2]);
                float y1 = clampCoordinate(m_coords[3]);
                path.addRect(FloatRect(x0, y0, x1 - x0, y1 - y0));
            }
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        // Cache the original path, not depending on containerObject.
        m_path = wrapUnique(new Path(path));
    }

    // Zoom the path into coordinates of the container object.
    float zoomFactor = containerObject->styleRef().effectiveZoom();
    if (zoomFactor != 1.0f) {
        AffineTransform zoomTransform;
        zoomTransform.scale(zoomFactor);
        path.transform(zoomTransform);
    }
    return path;
}

HTMLImageElement* HTMLAreaElement::imageElement() const
{
    if (HTMLMapElement* mapElement = Traversal<HTMLMapElement>::firstAncestor(*this))
        return mapElement->imageElement();
    return nullptr;
}

bool HTMLAreaElement::isKeyboardFocusable() const
{
    return isFocusable();
}

bool HTMLAreaElement::isMouseFocusable() const
{
    return isFocusable();
}

bool HTMLAreaElement::layoutObjectIsFocusable() const
{
    HTMLImageElement* image = imageElement();
    if (!image || !image->layoutObject() || image->layoutObject()->style()->visibility() != VISIBLE)
        return false;

    return supportsFocus() && Element::tabIndex() >= 0;
}

void HTMLAreaElement::setFocus(bool shouldBeFocused)
{
    if (focused() == shouldBeFocused)
        return;

    HTMLAnchorElement::setFocus(shouldBeFocused);

    HTMLImageElement* imageElement = this->imageElement();
    if (!imageElement)
        return;

    LayoutObject* layoutObject = imageElement->layoutObject();
    if (!layoutObject || !layoutObject->isImage())
        return;

    toLayoutImage(layoutObject)->areaElementFocusChanged(this);
}

void HTMLAreaElement::updateFocusAppearance(SelectionBehaviorOnFocus selectionBehavior)
{
    document().updateStyleAndLayoutTreeForNode(this);
    if (!isFocusable())
        return;

    if (HTMLImageElement* imageElement = this->imageElement())
        imageElement->updateFocusAppearance(selectionBehavior);
}

} // namespace blink

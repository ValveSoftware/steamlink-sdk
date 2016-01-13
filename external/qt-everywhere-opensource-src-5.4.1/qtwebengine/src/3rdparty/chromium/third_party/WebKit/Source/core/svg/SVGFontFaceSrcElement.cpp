/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#if ENABLE(SVG_FONTS)
#include "core/svg/SVGFontFaceSrcElement.h"

#include "core/SVGNames.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSValueList.h"
#include "core/dom/ElementTraversal.h"
#include "core/svg/SVGFontFaceElement.h"
#include "core/svg/SVGFontFaceNameElement.h"
#include "core/svg/SVGFontFaceUriElement.h"

namespace WebCore {

using namespace SVGNames;

inline SVGFontFaceSrcElement::SVGFontFaceSrcElement(Document& document)
    : SVGElement(font_face_srcTag, document)
{
    ScriptWrappable::init(this);
}

DEFINE_NODE_FACTORY(SVGFontFaceSrcElement)

PassRefPtrWillBeRawPtr<CSSValueList> SVGFontFaceSrcElement::srcValue() const
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    for (SVGElement* element = Traversal<SVGElement>::firstChild(*this); element; element = Traversal<SVGElement>::nextSibling(*element)) {
        RefPtrWillBeRawPtr<CSSFontFaceSrcValue> srcValue = nullptr;
        if (isSVGFontFaceUriElement(*element))
            srcValue = toSVGFontFaceUriElement(*element).srcValue();
        else if (isSVGFontFaceNameElement(*element))
            srcValue = toSVGFontFaceNameElement(*element).srcValue();

        if (srcValue && srcValue->resource().length())
            list->append(srcValue);
    }
    return list;
}

void SVGFontFaceSrcElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    if (isSVGFontFaceElement(parentNode()))
        toSVGFontFaceElement(*parentNode()).rebuildFontFace();
}

}

#endif // ENABLE(SVG_FONTS)

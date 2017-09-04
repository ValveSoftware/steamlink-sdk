/*
 * Copyright (C) 2007, 2010 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGViewSpec.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/SVGNames.h"
#include "core/dom/ExceptionCode.h"
#include "core/svg/SVGAnimatedTransformList.h"
#include "core/svg/SVGParserUtilities.h"
#include "wtf/text/ParsingUtilities.h"

namespace blink {

SVGViewSpec::SVGViewSpec(SVGSVGElement* contextElement)
    // Note: addToPropertyMap is not needed, as SVGViewSpec do not correspond to
    // an element.  We make tear-offs' contextElement the target element of
    // SVGViewSpec.  This contextElement will be only used for keeping this
    // alive from the tearoff.  SVGSVGElement holds a strong-ref to this
    // SVGViewSpec, so this is kept alive as:
    // AnimatedProperty tearoff -(contextElement)-> SVGSVGElement -(RefPtr)->
    //    SVGViewSpec.
    : SVGFitToViewBox(contextElement, PropertyMapPolicySkip),
      m_contextElement(contextElement),
      m_transform(SVGAnimatedTransformList::create(contextElement,
                                                   SVGNames::transformAttr)) {
  ASSERT(m_contextElement);

  viewBox()->setReadOnly();
  preserveAspectRatio()->setReadOnly();
  m_transform->setReadOnly();
  // Note: addToPropertyMap is not needed, as SVGViewSpec do not correspond to
  // an element.
}

DEFINE_TRACE(SVGViewSpec) {
  visitor->trace(m_contextElement);
  visitor->trace(m_transform);
  SVGFitToViewBox::trace(visitor);
}

bool SVGViewSpec::parseViewSpec(const String& spec) {
  if (spec.isEmpty() || !m_contextElement)
    return false;
  if (spec.is8Bit()) {
    const LChar* ptr = spec.characters8();
    const LChar* end = ptr + spec.length();
    return parseViewSpecInternal(ptr, end);
  }
  const UChar* ptr = spec.characters16();
  const UChar* end = ptr + spec.length();
  return parseViewSpecInternal(ptr, end);
}

void SVGViewSpec::reset() {
  resetZoomAndPan();
  m_transform->baseValue()->clear();
  updateViewBox(FloatRect());
  ASSERT(preserveAspectRatio());
  preserveAspectRatio()->baseValue()->setAlign(
      SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymid);
  preserveAspectRatio()->baseValue()->setMeetOrSlice(
      SVGPreserveAspectRatio::kSvgMeetorsliceMeet);
  m_viewTargetString = emptyString();
}

namespace {

enum ViewSpecFunctionType {
  Unknown,
  PreserveAspectRatio,
  Transform,
  ViewBox,
  ViewTarget,
  ZoomAndPan,
};

template <typename CharType>
static ViewSpecFunctionType scanViewSpecFunction(const CharType*& ptr,
                                                 const CharType* end) {
  DCHECK_LT(ptr, end);
  switch (*ptr) {
    case 'v':
      if (skipToken(ptr, end, "viewBox"))
        return ViewBox;
      if (skipToken(ptr, end, "viewTarget"))
        return ViewTarget;
      break;
    case 'z':
      if (skipToken(ptr, end, "zoomAndPan"))
        return ZoomAndPan;
      break;
    case 'p':
      if (skipToken(ptr, end, "preserveAspectRatio"))
        return PreserveAspectRatio;
      break;
    case 't':
      if (skipToken(ptr, end, "transform"))
        return Transform;
      break;
  }
  return Unknown;
}

}  // namespace

template <typename CharType>
bool SVGViewSpec::parseViewSpecInternal(const CharType* ptr,
                                        const CharType* end) {
  if (!skipToken(ptr, end, "svgView"))
    return false;

  if (!skipExactly<CharType>(ptr, end, '('))
    return false;

  while (ptr < end && *ptr != ')') {
    ViewSpecFunctionType functionType = scanViewSpecFunction(ptr, end);
    if (functionType == Unknown)
      return false;

    if (!skipExactly<CharType>(ptr, end, '('))
      return false;

    switch (functionType) {
      case ViewBox: {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        if (!(parseNumber(ptr, end, x) && parseNumber(ptr, end, y) &&
              parseNumber(ptr, end, width) &&
              parseNumber(ptr, end, height, DisallowWhitespace)))
          return false;
        updateViewBox(FloatRect(x, y, width, height));
        break;
      }
      case ViewTarget: {
        const CharType* viewTargetStart = ptr;
        skipUntil<CharType>(ptr, end, ')');
        if (ptr == viewTargetStart)
          return false;
        m_viewTargetString = String(viewTargetStart, ptr - viewTargetStart);
        break;
      }
      case ZoomAndPan:
        if (!parseZoomAndPan(ptr, end))
          return false;
        break;
      case PreserveAspectRatio:
        if (!preserveAspectRatio()->baseValue()->parse(ptr, end, false))
          return false;
        break;
      case Transform:
        m_transform->baseValue()->parse(ptr, end);
        break;
      default:
        NOTREACHED();
        break;
    }

    if (!skipExactly<CharType>(ptr, end, ')'))
      return false;

    skipExactly<CharType>(ptr, end, ';');
  }
  return skipExactly<CharType>(ptr, end, ')');
}

}  // namespace blink

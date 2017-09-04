/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
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

#include "core/svg/SVGPreserveAspectRatio.h"

#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGParserUtilities.h"
#include "platform/geometry/FloatRect.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/text/ParsingUtilities.h"
#include "wtf/text/WTFString.h"

namespace blink {

SVGPreserveAspectRatio::SVGPreserveAspectRatio() {
  setDefault();
}

void SVGPreserveAspectRatio::setDefault() {
  m_align = kSvgPreserveaspectratioXmidymid;
  m_meetOrSlice = kSvgMeetorsliceMeet;
}

SVGPreserveAspectRatio* SVGPreserveAspectRatio::clone() const {
  SVGPreserveAspectRatio* preserveAspectRatio = create();

  preserveAspectRatio->m_align = m_align;
  preserveAspectRatio->m_meetOrSlice = m_meetOrSlice;

  return preserveAspectRatio;
}

template <typename CharType>
SVGParsingError SVGPreserveAspectRatio::parseInternal(const CharType*& ptr,
                                                      const CharType* end,
                                                      bool validate) {
  SVGPreserveAspectRatioType align = kSvgPreserveaspectratioXmidymid;
  SVGMeetOrSliceType meetOrSlice = kSvgMeetorsliceMeet;

  setAlign(align);
  setMeetOrSlice(meetOrSlice);

  const CharType* start = ptr;
  if (!skipOptionalSVGSpaces(ptr, end))
    return SVGParsingError(SVGParseStatus::ExpectedEnumeration, ptr - start);

  if (*ptr == 'n') {
    if (!skipToken(ptr, end, "none"))
      return SVGParsingError(SVGParseStatus::ExpectedEnumeration, ptr - start);
    align = kSvgPreserveaspectratioNone;
    skipOptionalSVGSpaces(ptr, end);
  } else if (*ptr == 'x') {
    if ((end - ptr) < 8)
      return SVGParsingError(SVGParseStatus::ExpectedEnumeration, ptr - start);
    if (ptr[1] != 'M' || ptr[4] != 'Y' || ptr[5] != 'M')
      return SVGParsingError(SVGParseStatus::ExpectedEnumeration, ptr - start);
    if (ptr[2] == 'i') {
      if (ptr[3] == 'n') {
        if (ptr[6] == 'i') {
          if (ptr[7] == 'n')
            align = kSvgPreserveaspectratioXminymin;
          else if (ptr[7] == 'd')
            align = kSvgPreserveaspectratioXminymid;
          else
            return SVGParsingError(SVGParseStatus::ExpectedEnumeration,
                                   ptr - start);
        } else if (ptr[6] == 'a' && ptr[7] == 'x') {
          align = kSvgPreserveaspectratioXminymax;
        } else {
          return SVGParsingError(SVGParseStatus::ExpectedEnumeration,
                                 ptr - start);
        }
      } else if (ptr[3] == 'd') {
        if (ptr[6] == 'i') {
          if (ptr[7] == 'n')
            align = kSvgPreserveaspectratioXmidymin;
          else if (ptr[7] == 'd')
            align = kSvgPreserveaspectratioXmidymid;
          else
            return SVGParsingError(SVGParseStatus::ExpectedEnumeration,
                                   ptr - start);
        } else if (ptr[6] == 'a' && ptr[7] == 'x') {
          align = kSvgPreserveaspectratioXmidymax;
        } else {
          return SVGParsingError(SVGParseStatus::ExpectedEnumeration,
                                 ptr - start);
        }
      } else {
        return SVGParsingError(SVGParseStatus::ExpectedEnumeration,
                               ptr - start);
      }
    } else if (ptr[2] == 'a' && ptr[3] == 'x') {
      if (ptr[6] == 'i') {
        if (ptr[7] == 'n')
          align = kSvgPreserveaspectratioXmaxymin;
        else if (ptr[7] == 'd')
          align = kSvgPreserveaspectratioXmaxymid;
        else
          return SVGParsingError(SVGParseStatus::ExpectedEnumeration,
                                 ptr - start);
      } else if (ptr[6] == 'a' && ptr[7] == 'x') {
        align = kSvgPreserveaspectratioXmaxymax;
      } else {
        return SVGParsingError(SVGParseStatus::ExpectedEnumeration,
                               ptr - start);
      }
    } else {
      return SVGParsingError(SVGParseStatus::ExpectedEnumeration, ptr - start);
    }
    ptr += 8;
    skipOptionalSVGSpaces(ptr, end);
  } else {
    return SVGParsingError(SVGParseStatus::ExpectedEnumeration, ptr - start);
  }

  if (ptr < end) {
    if (*ptr == 'm') {
      if (!skipToken(ptr, end, "meet"))
        return SVGParsingError(SVGParseStatus::ExpectedEnumeration,
                               ptr - start);
      skipOptionalSVGSpaces(ptr, end);
    } else if (*ptr == 's') {
      if (!skipToken(ptr, end, "slice"))
        return SVGParsingError(SVGParseStatus::ExpectedEnumeration,
                               ptr - start);
      skipOptionalSVGSpaces(ptr, end);
      if (align != kSvgPreserveaspectratioNone)
        meetOrSlice = kSvgMeetorsliceSlice;
    }
  }

  if (end != ptr && validate)
    return SVGParsingError(SVGParseStatus::TrailingGarbage, ptr - start);

  setAlign(align);
  setMeetOrSlice(meetOrSlice);

  return SVGParseStatus::NoError;
}

SVGParsingError SVGPreserveAspectRatio::setValueAsString(const String& string) {
  setDefault();

  if (string.isEmpty())
    return SVGParseStatus::NoError;

  if (string.is8Bit()) {
    const LChar* ptr = string.characters8();
    const LChar* end = ptr + string.length();
    return parseInternal(ptr, end, true);
  }
  const UChar* ptr = string.characters16();
  const UChar* end = ptr + string.length();
  return parseInternal(ptr, end, true);
}

bool SVGPreserveAspectRatio::parse(const LChar*& ptr,
                                   const LChar* end,
                                   bool validate) {
  return parseInternal(ptr, end, validate) == SVGParseStatus::NoError;
}

bool SVGPreserveAspectRatio::parse(const UChar*& ptr,
                                   const UChar* end,
                                   bool validate) {
  return parseInternal(ptr, end, validate) == SVGParseStatus::NoError;
}

void SVGPreserveAspectRatio::transformRect(FloatRect& destRect,
                                           FloatRect& srcRect) {
  if (m_align == kSvgPreserveaspectratioNone)
    return;

  FloatSize imageSize = srcRect.size();
  float origDestWidth = destRect.width();
  float origDestHeight = destRect.height();
  switch (m_meetOrSlice) {
    case SVGPreserveAspectRatio::kSvgMeetorsliceUnknown:
      break;
    case SVGPreserveAspectRatio::kSvgMeetorsliceMeet: {
      float widthToHeightMultiplier = srcRect.height() / srcRect.width();
      if (origDestHeight > origDestWidth * widthToHeightMultiplier) {
        destRect.setHeight(origDestWidth * widthToHeightMultiplier);
        switch (m_align) {
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXminymid:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymid:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymid:
            destRect.setY(destRect.y() + origDestHeight / 2 -
                          destRect.height() / 2);
            break;
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXminymax:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymax:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymax:
            destRect.setY(destRect.y() + origDestHeight - destRect.height());
            break;
          default:
            break;
        }
      }
      if (origDestWidth > origDestHeight / widthToHeightMultiplier) {
        destRect.setWidth(origDestHeight / widthToHeightMultiplier);
        switch (m_align) {
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymin:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymid:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymax:
            destRect.setX(destRect.x() + origDestWidth / 2 -
                          destRect.width() / 2);
            break;
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymin:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymid:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymax:
            destRect.setX(destRect.x() + origDestWidth - destRect.width());
            break;
          default:
            break;
        }
      }
      break;
    }
    case SVGPreserveAspectRatio::kSvgMeetorsliceSlice: {
      float widthToHeightMultiplier = srcRect.height() / srcRect.width();
      // If the destination height is less than the height of the image we'll be
      // drawing.
      if (origDestHeight < origDestWidth * widthToHeightMultiplier) {
        float destToSrcMultiplier = srcRect.width() / destRect.width();
        srcRect.setHeight(destRect.height() * destToSrcMultiplier);
        switch (m_align) {
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXminymid:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymid:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymid:
            srcRect.setY(srcRect.y() + imageSize.height() / 2 -
                         srcRect.height() / 2);
            break;
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXminymax:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymax:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymax:
            srcRect.setY(srcRect.y() + imageSize.height() - srcRect.height());
            break;
          default:
            break;
        }
      }
      // If the destination width is less than the width of the image we'll be
      // drawing.
      if (origDestWidth < origDestHeight / widthToHeightMultiplier) {
        float destToSrcMultiplier = srcRect.height() / destRect.height();
        srcRect.setWidth(destRect.width() * destToSrcMultiplier);
        switch (m_align) {
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymin:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymid:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymax:
            srcRect.setX(srcRect.x() + imageSize.width() / 2 -
                         srcRect.width() / 2);
            break;
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymin:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymid:
          case SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymax:
            srcRect.setX(srcRect.x() + imageSize.width() - srcRect.width());
            break;
          default:
            break;
        }
      }
      break;
    }
  }
}

AffineTransform SVGPreserveAspectRatio::getCTM(float logicalX,
                                               float logicalY,
                                               float logicalWidth,
                                               float logicalHeight,
                                               float physicalWidth,
                                               float physicalHeight) const {
  ASSERT(logicalWidth);
  ASSERT(logicalHeight);
  ASSERT(physicalWidth);
  ASSERT(physicalHeight);

  AffineTransform transform;
  if (m_align == kSvgPreserveaspectratioUnknown)
    return transform;

  double extendedLogicalX = logicalX;
  double extendedLogicalY = logicalY;
  double extendedLogicalWidth = logicalWidth;
  double extendedLogicalHeight = logicalHeight;
  double extendedPhysicalWidth = physicalWidth;
  double extendedPhysicalHeight = physicalHeight;
  double logicalRatio = extendedLogicalWidth / extendedLogicalHeight;
  double physicalRatio = extendedPhysicalWidth / extendedPhysicalHeight;

  if (m_align == kSvgPreserveaspectratioNone) {
    transform.scaleNonUniform(extendedPhysicalWidth / extendedLogicalWidth,
                              extendedPhysicalHeight / extendedLogicalHeight);
    transform.translate(-extendedLogicalX, -extendedLogicalY);
    return transform;
  }

  if ((logicalRatio < physicalRatio &&
       (m_meetOrSlice == kSvgMeetorsliceMeet)) ||
      (logicalRatio >= physicalRatio &&
       (m_meetOrSlice == kSvgMeetorsliceSlice))) {
    transform.scaleNonUniform(extendedPhysicalHeight / extendedLogicalHeight,
                              extendedPhysicalHeight / extendedLogicalHeight);

    if (m_align == kSvgPreserveaspectratioXminymin ||
        m_align == kSvgPreserveaspectratioXminymid ||
        m_align == kSvgPreserveaspectratioXminymax)
      transform.translate(-extendedLogicalX, -extendedLogicalY);
    else if (m_align == kSvgPreserveaspectratioXmidymin ||
             m_align == kSvgPreserveaspectratioXmidymid ||
             m_align == kSvgPreserveaspectratioXmidymax)
      transform.translate(-extendedLogicalX -
                              (extendedLogicalWidth -
                               extendedPhysicalWidth * extendedLogicalHeight /
                                   extendedPhysicalHeight) /
                                  2,
                          -extendedLogicalY);
    else
      transform.translate(
          -extendedLogicalX - (extendedLogicalWidth -
                               extendedPhysicalWidth * extendedLogicalHeight /
                                   extendedPhysicalHeight),
          -extendedLogicalY);

    return transform;
  }

  transform.scaleNonUniform(extendedPhysicalWidth / extendedLogicalWidth,
                            extendedPhysicalWidth / extendedLogicalWidth);

  if (m_align == kSvgPreserveaspectratioXminymin ||
      m_align == kSvgPreserveaspectratioXmidymin ||
      m_align == kSvgPreserveaspectratioXmaxymin)
    transform.translate(-extendedLogicalX, -extendedLogicalY);
  else if (m_align == kSvgPreserveaspectratioXminymid ||
           m_align == kSvgPreserveaspectratioXmidymid ||
           m_align == kSvgPreserveaspectratioXmaxymid)
    transform.translate(-extendedLogicalX,
                        -extendedLogicalY -
                            (extendedLogicalHeight -
                             extendedPhysicalHeight * extendedLogicalWidth /
                                 extendedPhysicalWidth) /
                                2);
  else
    transform.translate(
        -extendedLogicalX,
        -extendedLogicalY - (extendedLogicalHeight -
                             extendedPhysicalHeight * extendedLogicalWidth /
                                 extendedPhysicalWidth));

  return transform;
}

String SVGPreserveAspectRatio::valueAsString() const {
  StringBuilder builder;

  const char* alignString = "";
  switch (m_align) {
    case kSvgPreserveaspectratioNone:
      alignString = "none";
      break;
    case kSvgPreserveaspectratioXminymin:
      alignString = "xMinYMin";
      break;
    case kSvgPreserveaspectratioXmidymin:
      alignString = "xMidYMin";
      break;
    case kSvgPreserveaspectratioXmaxymin:
      alignString = "xMaxYMin";
      break;
    case kSvgPreserveaspectratioXminymid:
      alignString = "xMinYMid";
      break;
    case kSvgPreserveaspectratioXmidymid:
      alignString = "xMidYMid";
      break;
    case kSvgPreserveaspectratioXmaxymid:
      alignString = "xMaxYMid";
      break;
    case kSvgPreserveaspectratioXminymax:
      alignString = "xMinYMax";
      break;
    case kSvgPreserveaspectratioXmidymax:
      alignString = "xMidYMax";
      break;
    case kSvgPreserveaspectratioXmaxymax:
      alignString = "xMaxYMax";
      break;
    case kSvgPreserveaspectratioUnknown:
      alignString = "unknown";
      break;
  }
  builder.append(alignString);

  const char* meetOrSliceString = "";
  switch (m_meetOrSlice) {
    default:
    case kSvgMeetorsliceUnknown:
      break;
    case kSvgMeetorsliceMeet:
      meetOrSliceString = " meet";
      break;
    case kSvgMeetorsliceSlice:
      meetOrSliceString = " slice";
      break;
  }
  builder.append(meetOrSliceString);
  return builder.toString();
}

void SVGPreserveAspectRatio::add(SVGPropertyBase* other, SVGElement*) {
  ASSERT_NOT_REACHED();
}

void SVGPreserveAspectRatio::calculateAnimatedValue(
    SVGAnimationElement* animationElement,
    float percentage,
    unsigned repeatCount,
    SVGPropertyBase* fromValue,
    SVGPropertyBase* toValue,
    SVGPropertyBase*,
    SVGElement*) {
  ASSERT(animationElement);

  bool useToValue;
  animationElement->animateDiscreteType(percentage, false, true, useToValue);

  SVGPreserveAspectRatio* preserveAspectRatioToUse =
      useToValue ? toSVGPreserveAspectRatio(toValue)
                 : toSVGPreserveAspectRatio(fromValue);

  m_align = preserveAspectRatioToUse->m_align;
  m_meetOrSlice = preserveAspectRatioToUse->m_meetOrSlice;
}

float SVGPreserveAspectRatio::calculateDistance(SVGPropertyBase* toValue,
                                                SVGElement* contextElement) {
  // No paced animations for SVGPreserveAspectRatio.
  return -1;
}

}  // namespace blink

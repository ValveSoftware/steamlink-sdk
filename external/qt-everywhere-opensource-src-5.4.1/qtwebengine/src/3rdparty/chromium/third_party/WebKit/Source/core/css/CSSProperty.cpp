/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
#include "core/css/CSSProperty.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSValueList.h"
#include "core/rendering/style/RenderStyleConstants.h"

namespace WebCore {

struct SameSizeAsCSSProperty {
    uint32_t bitfields;
    void* value;
};

COMPILE_ASSERT(sizeof(CSSProperty) == sizeof(SameSizeAsCSSProperty), CSSProperty_should_stay_small);

CSSPropertyID StylePropertyMetadata::shorthandID() const
{
    if (!m_isSetFromShorthand)
        return CSSPropertyInvalid;

    Vector<StylePropertyShorthand, 4> shorthands;
    getMatchingShorthandsForLonghand(static_cast<CSSPropertyID>(m_propertyID), &shorthands);
    ASSERT(shorthands.size() && m_indexInShorthandsVector >= 0 && m_indexInShorthandsVector < shorthands.size());
    return shorthands.at(m_indexInShorthandsVector).id();
}

void CSSProperty::wrapValueInCommaSeparatedList()
{
    RefPtrWillBeRawPtr<CSSValue> value = m_value.release();
    m_value = CSSValueList::createCommaSeparated();
    toCSSValueList(m_value.get())->append(value.release());
}

enum LogicalBoxSide { BeforeSide, EndSide, AfterSide, StartSide };
enum PhysicalBoxSide { TopSide, RightSide, BottomSide, LeftSide };

static CSSPropertyID resolveToPhysicalProperty(TextDirection direction, WritingMode writingMode, LogicalBoxSide logicalSide, const StylePropertyShorthand& shorthand)
{
    if (direction == LTR) {
        if (writingMode == TopToBottomWritingMode) {
            // The common case. The logical and physical box sides match.
            // Left = Start, Right = End, Before = Top, After = Bottom
            return shorthand.properties()[logicalSide];
        }

        if (writingMode == BottomToTopWritingMode) {
            // Start = Left, End = Right, Before = Bottom, After = Top.
            switch (logicalSide) {
            case StartSide:
                return shorthand.properties()[LeftSide];
            case EndSide:
                return shorthand.properties()[RightSide];
            case BeforeSide:
                return shorthand.properties()[BottomSide];
            default:
                return shorthand.properties()[TopSide];
            }
        }

        if (writingMode == LeftToRightWritingMode) {
            // Start = Top, End = Bottom, Before = Left, After = Right.
            switch (logicalSide) {
            case StartSide:
                return shorthand.properties()[TopSide];
            case EndSide:
                return shorthand.properties()[BottomSide];
            case BeforeSide:
                return shorthand.properties()[LeftSide];
            default:
                return shorthand.properties()[RightSide];
            }
        }

        // Start = Top, End = Bottom, Before = Right, After = Left
        switch (logicalSide) {
        case StartSide:
            return shorthand.properties()[TopSide];
        case EndSide:
            return shorthand.properties()[BottomSide];
        case BeforeSide:
            return shorthand.properties()[RightSide];
        default:
            return shorthand.properties()[LeftSide];
        }
    }

    if (writingMode == TopToBottomWritingMode) {
        // Start = Right, End = Left, Before = Top, After = Bottom
        switch (logicalSide) {
        case StartSide:
            return shorthand.properties()[RightSide];
        case EndSide:
            return shorthand.properties()[LeftSide];
        case BeforeSide:
            return shorthand.properties()[TopSide];
        default:
            return shorthand.properties()[BottomSide];
        }
    }

    if (writingMode == BottomToTopWritingMode) {
        // Start = Right, End = Left, Before = Bottom, After = Top
        switch (logicalSide) {
        case StartSide:
            return shorthand.properties()[RightSide];
        case EndSide:
            return shorthand.properties()[LeftSide];
        case BeforeSide:
            return shorthand.properties()[BottomSide];
        default:
            return shorthand.properties()[TopSide];
        }
    }

    if (writingMode == LeftToRightWritingMode) {
        // Start = Bottom, End = Top, Before = Left, After = Right
        switch (logicalSide) {
        case StartSide:
            return shorthand.properties()[BottomSide];
        case EndSide:
            return shorthand.properties()[TopSide];
        case BeforeSide:
            return shorthand.properties()[LeftSide];
        default:
            return shorthand.properties()[RightSide];
        }
    }

    // Start = Bottom, End = Top, Before = Right, After = Left
    switch (logicalSide) {
    case StartSide:
        return shorthand.properties()[BottomSide];
    case EndSide:
        return shorthand.properties()[TopSide];
    case BeforeSide:
        return shorthand.properties()[RightSide];
    default:
        return shorthand.properties()[LeftSide];
    }
}

enum LogicalExtent { LogicalWidth, LogicalHeight };

static CSSPropertyID resolveToPhysicalProperty(WritingMode writingMode, LogicalExtent logicalSide, const CSSPropertyID* properties)
{
    if (writingMode == TopToBottomWritingMode || writingMode == BottomToTopWritingMode)
        return properties[logicalSide];
    return logicalSide == LogicalWidth ? properties[1] : properties[0];
}

static const StylePropertyShorthand& borderDirections()
{
    static const CSSPropertyID properties[4] = { CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, borderDirections, (CSSPropertyBorder, properties, WTF_ARRAY_LENGTH(properties)));
    return borderDirections;
}

CSSPropertyID CSSProperty::resolveDirectionAwareProperty(CSSPropertyID propertyID, TextDirection direction, WritingMode writingMode)
{
    switch (propertyID) {
    case CSSPropertyWebkitMarginEnd:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, marginShorthand());
    case CSSPropertyWebkitMarginStart:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, marginShorthand());
    case CSSPropertyWebkitMarginBefore:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, marginShorthand());
    case CSSPropertyWebkitMarginAfter:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, marginShorthand());
    case CSSPropertyWebkitPaddingEnd:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, paddingShorthand());
    case CSSPropertyWebkitPaddingStart:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, paddingShorthand());
    case CSSPropertyWebkitPaddingBefore:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, paddingShorthand());
    case CSSPropertyWebkitPaddingAfter:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, paddingShorthand());
    case CSSPropertyWebkitBorderEnd:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, borderDirections());
    case CSSPropertyWebkitBorderStart:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, borderDirections());
    case CSSPropertyWebkitBorderBefore:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, borderDirections());
    case CSSPropertyWebkitBorderAfter:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, borderDirections());
    case CSSPropertyWebkitBorderEndColor:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, borderColorShorthand());
    case CSSPropertyWebkitBorderStartColor:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, borderColorShorthand());
    case CSSPropertyWebkitBorderBeforeColor:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, borderColorShorthand());
    case CSSPropertyWebkitBorderAfterColor:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, borderColorShorthand());
    case CSSPropertyWebkitBorderEndStyle:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, borderStyleShorthand());
    case CSSPropertyWebkitBorderStartStyle:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, borderStyleShorthand());
    case CSSPropertyWebkitBorderBeforeStyle:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, borderStyleShorthand());
    case CSSPropertyWebkitBorderAfterStyle:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, borderStyleShorthand());
    case CSSPropertyWebkitBorderEndWidth:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, borderWidthShorthand());
    case CSSPropertyWebkitBorderStartWidth:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, borderWidthShorthand());
    case CSSPropertyWebkitBorderBeforeWidth:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, borderWidthShorthand());
    case CSSPropertyWebkitBorderAfterWidth:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, borderWidthShorthand());
    case CSSPropertyWebkitLogicalWidth: {
        const CSSPropertyID properties[2] = { CSSPropertyWidth, CSSPropertyHeight };
        return resolveToPhysicalProperty(writingMode, LogicalWidth, properties);
    }
    case CSSPropertyWebkitLogicalHeight: {
        const CSSPropertyID properties[2] = { CSSPropertyWidth, CSSPropertyHeight };
        return resolveToPhysicalProperty(writingMode, LogicalHeight, properties);
    }
    case CSSPropertyWebkitMinLogicalWidth: {
        const CSSPropertyID properties[2] = { CSSPropertyMinWidth, CSSPropertyMinHeight };
        return resolveToPhysicalProperty(writingMode, LogicalWidth, properties);
    }
    case CSSPropertyWebkitMinLogicalHeight: {
        const CSSPropertyID properties[2] = { CSSPropertyMinWidth, CSSPropertyMinHeight };
        return resolveToPhysicalProperty(writingMode, LogicalHeight, properties);
    }
    case CSSPropertyWebkitMaxLogicalWidth: {
        const CSSPropertyID properties[2] = { CSSPropertyMaxWidth, CSSPropertyMaxHeight };
        return resolveToPhysicalProperty(writingMode, LogicalWidth, properties);
    }
    case CSSPropertyWebkitMaxLogicalHeight: {
        const CSSPropertyID properties[2] = { CSSPropertyMaxWidth, CSSPropertyMaxHeight };
        return resolveToPhysicalProperty(writingMode, LogicalHeight, properties);
    }
    default:
        return propertyID;
    }
}

bool CSSProperty::isInheritedProperty(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyBorderCollapse:
    case CSSPropertyBorderSpacing:
    case CSSPropertyCaptionSide:
    case CSSPropertyClipRule:
    case CSSPropertyColor:
    case CSSPropertyColorInterpolation:
    case CSSPropertyColorInterpolationFilters:
    case CSSPropertyColorRendering:
    case CSSPropertyCursor:
    case CSSPropertyDirection:
    case CSSPropertyEmptyCells:
    case CSSPropertyFill:
    case CSSPropertyFillOpacity:
    case CSSPropertyFillRule:
    case CSSPropertyFont:
    case CSSPropertyFontFamily:
    case CSSPropertyFontSize:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariant:
    case CSSPropertyFontWeight:
    case CSSPropertyGlyphOrientationHorizontal:
    case CSSPropertyGlyphOrientationVertical:
    case CSSPropertyImageRendering:
    case CSSPropertyLetterSpacing:
    case CSSPropertyLineHeight:
    case CSSPropertyListStyle:
    case CSSPropertyListStyleImage:
    case CSSPropertyListStylePosition:
    case CSSPropertyListStyleType:
    case CSSPropertyMarker:
    case CSSPropertyMarkerEnd:
    case CSSPropertyMarkerMid:
    case CSSPropertyMarkerStart:
    case CSSPropertyOrphans:
    case CSSPropertyPointerEvents:
    case CSSPropertyQuotes:
    case CSSPropertyResize:
    case CSSPropertyShapeRendering:
    case CSSPropertySpeak:
    case CSSPropertyStroke:
    case CSSPropertyStrokeDasharray:
    case CSSPropertyStrokeDashoffset:
    case CSSPropertyStrokeLinecap:
    case CSSPropertyStrokeLinejoin:
    case CSSPropertyStrokeMiterlimit:
    case CSSPropertyStrokeOpacity:
    case CSSPropertyStrokeWidth:
    case CSSPropertyTabSize:
    case CSSPropertyTextAlign:
    case CSSPropertyTextAlignLast:
    case CSSPropertyTextJustify:
    case CSSPropertyTextAnchor:
    case CSSPropertyTextIndent:
    case CSSPropertyTextRendering:
    case CSSPropertyTextShadow:
    case CSSPropertyTextTransform:
    case CSSPropertyTouchActionDelay:
    case CSSPropertyVisibility:
    case CSSPropertyWebkitAspectRatio:
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderVerticalSpacing:
    case CSSPropertyWebkitBoxDirection:
    case CSSPropertyWebkitFontFeatureSettings:
    case CSSPropertyFontKerning:
    case CSSPropertyWebkitFontSmoothing:
    case CSSPropertyFontVariantLigatures:
    case CSSPropertyWebkitLocale:
    case CSSPropertyWebkitHighlight:
    case CSSPropertyWebkitHyphenateCharacter:
    case CSSPropertyWebkitLineBoxContain:
    case CSSPropertyWebkitLineBreak:
    case CSSPropertyWebkitPrintColorAdjust:
    case CSSPropertyWebkitRtlOrdering:
    case CSSPropertyWebkitRubyPosition:
    case CSSPropertyWebkitTapHighlightColor:
    case CSSPropertyWebkitTextCombine:
    case CSSPropertyTextUnderlinePosition:
    case CSSPropertyWebkitTextDecorationsInEffect:
    case CSSPropertyWebkitTextEmphasis:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextEmphasisPosition:
    case CSSPropertyWebkitTextEmphasisStyle:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextOrientation:
    case CSSPropertyWebkitTextSecurity:
    case CSSPropertyWebkitTextStroke:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyWebkitTextStrokeWidth:
    case CSSPropertyWebkitUserModify:
    case CSSPropertyWebkitUserSelect:
    case CSSPropertyWebkitWritingMode:
    case CSSPropertyWhiteSpace:
    case CSSPropertyWidows:
    case CSSPropertyWordBreak:
    case CSSPropertyWordSpacing:
    case CSSPropertyWordWrap:
    case CSSPropertyWritingMode:
        return true;
    case CSSPropertyAnimation:
    case CSSPropertyAnimationDelay:
    case CSSPropertyAnimationDirection:
    case CSSPropertyAnimationDuration:
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyAnimationName:
    case CSSPropertyAnimationPlayState:
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyAlignmentBaseline:
    case CSSPropertyBackground:
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundBlendMode:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyBaselineShift:
    case CSSPropertyBorder:
    case CSSPropertyBorderBottom:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderColor:
    case CSSPropertyBorderImage:
    case CSSPropertyBorderImageOutset:
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyBorderImageSlice:
    case CSSPropertyBorderImageSource:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyBorderLeft:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRadius:
    case CSSPropertyBorderRight:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderStyle:
    case CSSPropertyBorderTop:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyBorderWidth:
    case CSSPropertyBottom:
    case CSSPropertyBoxShadow:
    case CSSPropertyBoxSizing:
    case CSSPropertyBufferedRendering:
    case CSSPropertyClear:
    case CSSPropertyClip:
    case CSSPropertyClipPath:
    case CSSPropertyContent:
    case CSSPropertyCounterIncrement:
    case CSSPropertyCounterReset:
    case CSSPropertyDisplay:
    case CSSPropertyDominantBaseline:
    case CSSPropertyEnableBackground:
    case CSSPropertyFilter:
    case CSSPropertyFloat:
    case CSSPropertyFloodColor:
    case CSSPropertyFloodOpacity:
    case CSSPropertyFontStretch:
    case CSSPropertyHeight:
    case CSSPropertyIsolation:
    case CSSPropertyJustifySelf:
    case CSSPropertyLeft:
    case CSSPropertyLightingColor:
    case CSSPropertyMargin:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyMask:
    case CSSPropertyMaskType:
    case CSSPropertyMaskSourceType:
    case CSSPropertyMaxHeight:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMinWidth:
    case CSSPropertyMixBlendMode:
    case CSSPropertyObjectFit:
    case CSSPropertyObjectPosition:
    case CSSPropertyOpacity:
    case CSSPropertyOutline:
    case CSSPropertyOutlineColor:
    case CSSPropertyOutlineOffset:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOutlineWidth:
    case CSSPropertyOverflow:
    case CSSPropertyOverflowWrap:
    case CSSPropertyOverflowX:
    case CSSPropertyOverflowY:
    case CSSPropertyPadding:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyPage:
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
    case CSSPropertyPageBreakInside:
    case CSSPropertyPaintOrder:
    case CSSPropertyPosition:
    case CSSPropertyRight:
    case CSSPropertyScrollBehavior:
    case CSSPropertySize:
    case CSSPropertySrc:
    case CSSPropertyStopColor:
    case CSSPropertyStopOpacity:
    case CSSPropertyTableLayout:
    case CSSPropertyTextDecoration:
    case CSSPropertyTextDecorationLine:
    case CSSPropertyTextDecorationStyle:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyTextLineThroughColor:
    case CSSPropertyTextLineThroughMode:
    case CSSPropertyTextLineThroughStyle:
    case CSSPropertyTextLineThroughWidth:
    case CSSPropertyTextOverflow:
    case CSSPropertyTextOverlineColor:
    case CSSPropertyTextOverlineMode:
    case CSSPropertyTextOverlineStyle:
    case CSSPropertyTextOverlineWidth:
    case CSSPropertyTextUnderlineColor:
    case CSSPropertyTextUnderlineMode:
    case CSSPropertyTextUnderlineStyle:
    case CSSPropertyTextUnderlineWidth:
    case CSSPropertyTop:
    case CSSPropertyTouchAction:
    case CSSPropertyTransition:
    case CSSPropertyTransitionDelay:
    case CSSPropertyTransitionDuration:
    case CSSPropertyTransitionProperty:
    case CSSPropertyTransitionTimingFunction:
    case CSSPropertyUnicodeBidi:
    case CSSPropertyUnicodeRange:
    case CSSPropertyVectorEffect:
    case CSSPropertyVerticalAlign:
    case CSSPropertyAlignContent:
    case CSSPropertyAlignItems:
    case CSSPropertyAlignSelf:
    case CSSPropertyWebkitAnimation:
    case CSSPropertyWebkitAnimationDelay:
    case CSSPropertyWebkitAnimationDirection:
    case CSSPropertyWebkitAnimationDuration:
    case CSSPropertyWebkitAnimationFillMode:
    case CSSPropertyWebkitAnimationIterationCount:
    case CSSPropertyWebkitAnimationName:
    case CSSPropertyWebkitAnimationPlayState:
    case CSSPropertyWebkitAnimationTimingFunction:
    case CSSPropertyWebkitAppearance:
    case CSSPropertyBackfaceVisibility:
    case CSSPropertyWebkitBackfaceVisibility:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundComposite:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyWebkitBorderAfter:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderAfterWidth:
    case CSSPropertyWebkitBorderBefore:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitBorderEnd:
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderFit:
    case CSSPropertyWebkitBorderImage:
    case CSSPropertyWebkitBorderRadius:
    case CSSPropertyWebkitBorderStart:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBoxAlign:
    case CSSPropertyWebkitBoxDecorationBreak:
    case CSSPropertyWebkitBoxFlex:
    case CSSPropertyWebkitBoxFlexGroup:
    case CSSPropertyWebkitBoxLines:
    case CSSPropertyWebkitBoxOrdinalGroup:
    case CSSPropertyWebkitBoxOrient:
    case CSSPropertyWebkitBoxPack:
    case CSSPropertyWebkitBoxReflect:
    case CSSPropertyWebkitBoxShadow:
    case CSSPropertyInternalCallback:
    case CSSPropertyWebkitClipPath:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
    case CSSPropertyWebkitColumnBreakInside:
    case CSSPropertyWebkitColumnCount:
    case CSSPropertyColumnFill:
    case CSSPropertyWebkitColumnGap:
    case CSSPropertyWebkitColumnRule:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitColumnRuleStyle:
    case CSSPropertyWebkitColumnRuleWidth:
    case CSSPropertyWebkitColumnSpan:
    case CSSPropertyWebkitColumnWidth:
    case CSSPropertyWebkitColumns:
    case CSSPropertyWebkitFilter:
    case CSSPropertyFlex:
    case CSSPropertyFlexBasis:
    case CSSPropertyFlexDirection:
    case CSSPropertyFlexFlow:
    case CSSPropertyFlexGrow:
    case CSSPropertyFlexShrink:
    case CSSPropertyFlexWrap:
    case CSSPropertyWebkitFontSizeDelta:
    case CSSPropertyGridArea:
    case CSSPropertyGridAutoColumns:
    case CSSPropertyGridAutoFlow:
    case CSSPropertyGridAutoRows:
    case CSSPropertyGridColumn:
    case CSSPropertyGridColumnEnd:
    case CSSPropertyGridColumnStart:
    case CSSPropertyGrid:
    case CSSPropertyGridTemplate:
    case CSSPropertyGridTemplateColumns:
    case CSSPropertyGridTemplateRows:
    case CSSPropertyGridRow:
    case CSSPropertyGridRowEnd:
    case CSSPropertyGridRowStart:
    case CSSPropertyGridTemplateAreas:
    case CSSPropertyJustifyContent:
    case CSSPropertyWebkitLineClamp:
    case CSSPropertyWebkitLogicalHeight:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyWebkitMarginAfter:
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginCollapse:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyInternalMarqueeDirection:
    case CSSPropertyInternalMarqueeIncrement:
    case CSSPropertyInternalMarqueeRepetition:
    case CSSPropertyInternalMarqueeSpeed:
    case CSSPropertyOrder:
    case CSSPropertyInternalMarqueeStyle:
    case CSSPropertyWebkitMask:
    case CSSPropertyWebkitMaskBoxImage:
    case CSSPropertyWebkitMaskBoxImageOutset:
    case CSSPropertyWebkitMaskBoxImageRepeat:
    case CSSPropertyWebkitMaskBoxImageSlice:
    case CSSPropertyWebkitMaskBoxImageSource:
    case CSSPropertyWebkitMaskBoxImageWidth:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyWebkitMaskComposite:
    case CSSPropertyWebkitMaskImage:
    case CSSPropertyWebkitMaskOrigin:
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
    case CSSPropertyWebkitMaskRepeat:
    case CSSPropertyWebkitMaskRepeatX:
    case CSSPropertyWebkitMaskRepeatY:
    case CSSPropertyWebkitMaskSize:
    case CSSPropertyWebkitMaxLogicalWidth:
    case CSSPropertyWebkitMaxLogicalHeight:
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyWebkitMinLogicalHeight:
    case CSSPropertyWebkitPaddingAfter:
    case CSSPropertyWebkitPaddingBefore:
    case CSSPropertyWebkitPaddingEnd:
    case CSSPropertyWebkitPaddingStart:
    case CSSPropertyPerspective:
    case CSSPropertyWebkitPerspective:
    case CSSPropertyPerspectiveOrigin:
    case CSSPropertyWebkitPerspectiveOrigin:
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY:
    case CSSPropertyTransform:
    case CSSPropertyWebkitTransform:
    case CSSPropertyTransformOrigin:
    case CSSPropertyWebkitTransformOrigin:
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ:
    case CSSPropertyTransformStyle:
    case CSSPropertyWebkitTransformStyle:
    case CSSPropertyWebkitTransition:
    case CSSPropertyWebkitTransitionDelay:
    case CSSPropertyWebkitTransitionDuration:
    case CSSPropertyWebkitTransitionProperty:
    case CSSPropertyWebkitTransitionTimingFunction:
    case CSSPropertyWebkitUserDrag:
    case CSSPropertyWebkitWrapFlow:
    case CSSPropertyShapeMargin:
    case CSSPropertyShapeImageThreshold:
    case CSSPropertyShapeOutside:
    case CSSPropertyWebkitWrapThrough:
    case CSSPropertyWebkitAppRegion:
    case CSSPropertyWidth:
    case CSSPropertyWillChange:
    case CSSPropertyMaxZoom:
    case CSSPropertyMinZoom:
    case CSSPropertyOrientation:
    case CSSPropertyUserZoom:
    case CSSPropertyZIndex:
    case CSSPropertyZoom:
    case CSSPropertyAll:
        return false;
    case CSSPropertyInvalid:
        ASSERT_NOT_REACHED();
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool CSSProperty::isAffectedByAllProperty(CSSPropertyID propertyID)
{
    if (propertyID == CSSPropertyAll)
        return false;

    // all shorthand spec says:
    // The all property is a shorthand that resets all CSS properties except
    // direction and unicode-bidi. It only accepts the CSS-wide keywords.
    // c.f. http://dev.w3.org/csswg/css-cascade/#all-shorthand
    // So CSSPropertyUnicodeBidi and CSSPropertyDirection are not
    // affected by all property.
    return propertyID != CSSPropertyUnicodeBidi && propertyID != CSSPropertyDirection;
}

} // namespace WebCore

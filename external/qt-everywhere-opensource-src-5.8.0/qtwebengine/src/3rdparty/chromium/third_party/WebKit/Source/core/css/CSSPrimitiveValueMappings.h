/*
 * Copyright (C) 2007 Alexey Proskuryakov <ap@nypop.com>.
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2009 Jeff Schiller <codedread@gmail.com>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CSSPrimitiveValueMappings_h
#define CSSPrimitiveValueMappings_h

#include "core/CSSValueKeywords.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSReflectionDirection.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/style/SVGComputedStyleDefs.h"
#include "core/style/LineClampValue.h"
#include "platform/Length.h"
#include "platform/ThemeTypes.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontSmoothingMode.h"
#include "platform/fonts/TextRenderingMode.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/text/TextDirection.h"
#include "platform/text/TextRun.h"
#include "platform/text/UnicodeBidi.h"
#include "platform/text/WritingMode.h"
#include "public/platform/WebBlendMode.h"
#include "wtf/MathExtras.h"

namespace blink {

template<> inline short CSSPrimitiveValue::convertTo() const
{
    ASSERT(isNumber());
    return clampTo<short>(getDoubleValue());
}

template<> inline unsigned short CSSPrimitiveValue::convertTo() const
{
    ASSERT(isNumber());
    return clampTo<unsigned short>(getDoubleValue());
}

template<> inline int CSSPrimitiveValue::convertTo() const
{
    ASSERT(isNumber());
    return clampTo<int>(getDoubleValue());
}

template<> inline unsigned CSSPrimitiveValue::convertTo() const
{
    ASSERT(isNumber());
    return clampTo<unsigned>(getDoubleValue());
}

template<> inline float CSSPrimitiveValue::convertTo() const
{
    ASSERT(isNumber());
    return clampTo<float>(getDoubleValue());
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(LineClampValue i)
    : CSSValue(PrimitiveClass)
{
    init(i.isPercentage() ? UnitType::Percentage : UnitType::Integer);
    m_value.num = static_cast<double>(i.value());
}

template<> inline LineClampValue CSSPrimitiveValue::convertTo() const
{
    if (type() == UnitType::Integer)
        return LineClampValue(clampTo<int>(m_value.num), LineClampLineCount);

    if (type() == UnitType::Percentage)
        return LineClampValue(clampTo<int>(m_value.num), LineClampPercentage);

    ASSERT_NOT_REACHED();
    return LineClampValue();
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(CSSReflectionDirection e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case ReflectionAbove:
        m_value.valueID = CSSValueAbove;
        break;
    case ReflectionBelow:
        m_value.valueID = CSSValueBelow;
        break;
    case ReflectionLeft:
        m_value.valueID = CSSValueLeft;
        break;
    case ReflectionRight:
        m_value.valueID = CSSValueRight;
    }
}

template<> inline CSSReflectionDirection CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAbove:
        return ReflectionAbove;
    case CSSValueBelow:
        return ReflectionBelow;
    case CSSValueLeft:
        return ReflectionLeft;
    case CSSValueRight:
        return ReflectionRight;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return ReflectionBelow;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ColumnFill columnFill)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (columnFill) {
    case ColumnFillAuto:
        m_value.valueID = CSSValueAuto;
        break;
    case ColumnFillBalance:
        m_value.valueID = CSSValueBalance;
        break;
    }
}

template<> inline ColumnFill CSSPrimitiveValue::convertTo() const
{
    if (type() == UnitType::ValueID) {
        if (m_value.valueID == CSSValueBalance)
            return ColumnFillBalance;
        if (m_value.valueID == CSSValueAuto)
            return ColumnFillAuto;
    }
    ASSERT_NOT_REACHED();
    return ColumnFillBalance;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ColumnSpan columnSpan)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (columnSpan) {
    case ColumnSpanAll:
        m_value.valueID = CSSValueAll;
        break;
    case ColumnSpanNone:
        m_value.valueID = CSSValueNone;
        break;
    }
}

template<> inline ColumnSpan CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAll:
        return ColumnSpanAll;
    default:
        ASSERT_NOT_REACHED();
        // fall-through
    case CSSValueNone:
        return ColumnSpanNone;
    }
}


template<> inline CSSPrimitiveValue::CSSPrimitiveValue(PrintColorAdjust value)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (value) {
    case PrintColorAdjustExact:
        m_value.valueID = CSSValueExact;
        break;
    case PrintColorAdjustEconomy:
        m_value.valueID = CSSValueEconomy;
        break;
    }
}

template<> inline PrintColorAdjust CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueEconomy:
        return PrintColorAdjustEconomy;
    case CSSValueExact:
        return PrintColorAdjustExact;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return PrintColorAdjustEconomy;
}


template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBorderStyle e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case BorderStyleNone:
        m_value.valueID = CSSValueNone;
        break;
    case BorderStyleHidden:
        m_value.valueID = CSSValueHidden;
        break;
    case BorderStyleInset:
        m_value.valueID = CSSValueInset;
        break;
    case BorderStyleGroove:
        m_value.valueID = CSSValueGroove;
        break;
    case BorderStyleRidge:
        m_value.valueID = CSSValueRidge;
        break;
    case BorderStyleOutset:
        m_value.valueID = CSSValueOutset;
        break;
    case BorderStyleDotted:
        m_value.valueID = CSSValueDotted;
        break;
    case BorderStyleDashed:
        m_value.valueID = CSSValueDashed;
        break;
    case BorderStyleSolid:
        m_value.valueID = CSSValueSolid;
        break;
    case BorderStyleDouble:
        m_value.valueID = CSSValueDouble;
        break;
    }
}

template<> inline EBorderStyle CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    if (m_value.valueID == CSSValueAuto) // Valid for CSS outline-style
        return BorderStyleDotted;
    return (EBorderStyle)(m_value.valueID - CSSValueNone);
}

template<> inline OutlineIsAuto CSSPrimitiveValue::convertTo() const
{
    if (m_value.valueID == CSSValueAuto)
        return OutlineIsAutoOn;
    return OutlineIsAutoOff;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(CompositeOperator e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case CompositeClear:
        m_value.valueID = CSSValueClear;
        break;
    case CompositeCopy:
        m_value.valueID = CSSValueCopy;
        break;
    case CompositeSourceOver:
        m_value.valueID = CSSValueSourceOver;
        break;
    case CompositeSourceIn:
        m_value.valueID = CSSValueSourceIn;
        break;
    case CompositeSourceOut:
        m_value.valueID = CSSValueSourceOut;
        break;
    case CompositeSourceAtop:
        m_value.valueID = CSSValueSourceAtop;
        break;
    case CompositeDestinationOver:
        m_value.valueID = CSSValueDestinationOver;
        break;
    case CompositeDestinationIn:
        m_value.valueID = CSSValueDestinationIn;
        break;
    case CompositeDestinationOut:
        m_value.valueID = CSSValueDestinationOut;
        break;
    case CompositeDestinationAtop:
        m_value.valueID = CSSValueDestinationAtop;
        break;
    case CompositeXOR:
        m_value.valueID = CSSValueXor;
        break;
    case CompositePlusLighter:
        m_value.valueID = CSSValuePlusLighter;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

template<> inline CompositeOperator CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueClear:
        return CompositeClear;
    case CSSValueCopy:
        return CompositeCopy;
    case CSSValueSourceOver:
        return CompositeSourceOver;
    case CSSValueSourceIn:
        return CompositeSourceIn;
    case CSSValueSourceOut:
        return CompositeSourceOut;
    case CSSValueSourceAtop:
        return CompositeSourceAtop;
    case CSSValueDestinationOver:
        return CompositeDestinationOver;
    case CSSValueDestinationIn:
        return CompositeDestinationIn;
    case CSSValueDestinationOut:
        return CompositeDestinationOut;
    case CSSValueDestinationAtop:
        return CompositeDestinationAtop;
    case CSSValueXor:
        return CompositeXOR;
    case CSSValuePlusLighter:
        return CompositePlusLighter;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return CompositeClear;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ControlPart e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case NoControlPart:
        m_value.valueID = CSSValueNone;
        break;
    case CheckboxPart:
        m_value.valueID = CSSValueCheckbox;
        break;
    case RadioPart:
        m_value.valueID = CSSValueRadio;
        break;
    case PushButtonPart:
        m_value.valueID = CSSValuePushButton;
        break;
    case SquareButtonPart:
        m_value.valueID = CSSValueSquareButton;
        break;
    case ButtonPart:
        m_value.valueID = CSSValueButton;
        break;
    case ButtonBevelPart:
        m_value.valueID = CSSValueButtonBevel;
        break;
    case InnerSpinButtonPart:
        m_value.valueID = CSSValueInnerSpinButton;
        break;
    case ListboxPart:
        m_value.valueID = CSSValueListbox;
        break;
    case ListItemPart:
        m_value.valueID = CSSValueListitem;
        break;
    case MediaEnterFullscreenButtonPart:
        m_value.valueID = CSSValueMediaEnterFullscreenButton;
        break;
    case MediaExitFullscreenButtonPart:
        m_value.valueID = CSSValueMediaExitFullscreenButton;
        break;
    case MediaPlayButtonPart:
        m_value.valueID = CSSValueMediaPlayButton;
        break;
    case MediaOverlayPlayButtonPart:
        m_value.valueID = CSSValueMediaOverlayPlayButton;
        break;
    case MediaMuteButtonPart:
        m_value.valueID = CSSValueMediaMuteButton;
        break;
    case MediaToggleClosedCaptionsButtonPart:
        m_value.valueID = CSSValueMediaToggleClosedCaptionsButton;
        break;
    case MediaCastOffButtonPart:
        m_value.valueID = CSSValueInternalMediaCastOffButton;
        break;
    case MediaOverlayCastOffButtonPart:
        m_value.valueID = CSSValueInternalMediaOverlayCastOffButton;
        break;
    case MediaSliderPart:
        m_value.valueID = CSSValueMediaSlider;
        break;
    case MediaSliderThumbPart:
        m_value.valueID = CSSValueMediaSliderthumb;
        break;
    case MediaVolumeSliderContainerPart:
        m_value.valueID = CSSValueMediaVolumeSliderContainer;
        break;
    case MediaVolumeSliderPart:
        m_value.valueID = CSSValueMediaVolumeSlider;
        break;
    case MediaVolumeSliderThumbPart:
        m_value.valueID = CSSValueMediaVolumeSliderthumb;
        break;
    case MediaControlsBackgroundPart:
        m_value.valueID = CSSValueMediaControlsBackground;
        break;
    case MediaControlsFullscreenBackgroundPart:
        m_value.valueID = CSSValueMediaControlsFullscreenBackground;
        break;
    case MediaFullScreenVolumeSliderPart:
        m_value.valueID = CSSValueMediaFullscreenVolumeSlider;
        break;
    case MediaFullScreenVolumeSliderThumbPart:
        m_value.valueID = CSSValueMediaFullscreenVolumeSliderThumb;
        break;
    case MediaCurrentTimePart:
        m_value.valueID = CSSValueMediaCurrentTimeDisplay;
        break;
    case MediaTimeRemainingPart:
        m_value.valueID = CSSValueMediaTimeRemainingDisplay;
        break;
    case MediaTrackSelectionCheckmarkPart:
        m_value.valueID = CSSValueInternalMediaTrackSelectionCheckmark;
        break;
    case MediaClosedCaptionsIconPart:
        m_value.valueID = CSSValueInternalMediaClosedCaptionsIcon;
        break;
    case MediaSubtitlesIconPart:
        m_value.valueID = CSSValueInternalMediaSubtitlesIcon;
        break;
    case MenulistPart:
        m_value.valueID = CSSValueMenulist;
        break;
    case MenulistButtonPart:
        m_value.valueID = CSSValueMenulistButton;
        break;
    case MenulistTextPart:
        m_value.valueID = CSSValueMenulistText;
        break;
    case MenulistTextFieldPart:
        m_value.valueID = CSSValueMenulistTextfield;
        break;
    case MeterPart:
        m_value.valueID = CSSValueMeter;
        break;
    case ProgressBarPart:
        m_value.valueID = CSSValueProgressBar;
        break;
    case ProgressBarValuePart:
        m_value.valueID = CSSValueProgressBarValue;
        break;
    case SliderHorizontalPart:
        m_value.valueID = CSSValueSliderHorizontal;
        break;
    case SliderVerticalPart:
        m_value.valueID = CSSValueSliderVertical;
        break;
    case SliderThumbHorizontalPart:
        m_value.valueID = CSSValueSliderthumbHorizontal;
        break;
    case SliderThumbVerticalPart:
        m_value.valueID = CSSValueSliderthumbVertical;
        break;
    case CaretPart:
        m_value.valueID = CSSValueCaret;
        break;
    case SearchFieldPart:
        m_value.valueID = CSSValueSearchfield;
        break;
    case SearchFieldCancelButtonPart:
        m_value.valueID = CSSValueSearchfieldCancelButton;
        break;
    case TextFieldPart:
        m_value.valueID = CSSValueTextfield;
        break;
    case TextAreaPart:
        m_value.valueID = CSSValueTextarea;
        break;
    case CapsLockIndicatorPart:
        m_value.valueID = CSSValueCapsLockIndicator;
        break;
    }
}

template<> inline ControlPart CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    if (m_value.valueID == CSSValueNone)
        return NoControlPart;
    return ControlPart(m_value.valueID - CSSValueCheckbox + 1);
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBackfaceVisibility e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case BackfaceVisibilityVisible:
        m_value.valueID = CSSValueVisible;
        break;
    case BackfaceVisibilityHidden:
        m_value.valueID = CSSValueHidden;
        break;
    }
}

template<> inline EBackfaceVisibility CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueVisible:
        return BackfaceVisibilityVisible;
    case CSSValueHidden:
        return BackfaceVisibilityHidden;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BackfaceVisibilityHidden;
}


template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EFillAttachment e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case ScrollBackgroundAttachment:
        m_value.valueID = CSSValueScroll;
        break;
    case LocalBackgroundAttachment:
        m_value.valueID = CSSValueLocal;
        break;
    case FixedBackgroundAttachment:
        m_value.valueID = CSSValueFixed;
        break;
    }
}

template<> inline EFillAttachment CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueScroll:
        return ScrollBackgroundAttachment;
    case CSSValueLocal:
        return LocalBackgroundAttachment;
    case CSSValueFixed:
        return FixedBackgroundAttachment;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return ScrollBackgroundAttachment;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EFillBox e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case BorderFillBox:
        m_value.valueID = CSSValueBorderBox;
        break;
    case PaddingFillBox:
        m_value.valueID = CSSValuePaddingBox;
        break;
    case ContentFillBox:
        m_value.valueID = CSSValueContentBox;
        break;
    case TextFillBox:
        m_value.valueID = CSSValueText;
        break;
    }
}

template<> inline EFillBox CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueBorder:
    case CSSValueBorderBox:
        return BorderFillBox;
    case CSSValuePadding:
    case CSSValuePaddingBox:
        return PaddingFillBox;
    case CSSValueContent:
    case CSSValueContentBox:
        return ContentFillBox;
    case CSSValueText:
        return TextFillBox;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BorderFillBox;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EFillRepeat e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case RepeatFill:
        m_value.valueID = CSSValueRepeat;
        break;
    case NoRepeatFill:
        m_value.valueID = CSSValueNoRepeat;
        break;
    case RoundFill:
        m_value.valueID = CSSValueRound;
        break;
    case SpaceFill:
        m_value.valueID = CSSValueSpace;
        break;
    }
}

template<> inline EFillRepeat CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueRepeat:
        return RepeatFill;
    case CSSValueNoRepeat:
        return NoRepeatFill;
    case CSSValueRound:
        return RoundFill;
    case CSSValueSpace:
        return SpaceFill;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return RepeatFill;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBoxPack e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case BoxPackStart:
        m_value.valueID = CSSValueStart;
        break;
    case BoxPackCenter:
        m_value.valueID = CSSValueCenter;
        break;
    case BoxPackEnd:
        m_value.valueID = CSSValueEnd;
        break;
    case BoxPackJustify:
        m_value.valueID = CSSValueJustify;
        break;
    }
}

template<> inline EBoxPack CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueStart:
        return BoxPackStart;
    case CSSValueEnd:
        return BoxPackEnd;
    case CSSValueCenter:
        return BoxPackCenter;
    case CSSValueJustify:
        return BoxPackJustify;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BoxPackJustify;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBoxAlignment e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case BSTRETCH:
        m_value.valueID = CSSValueStretch;
        break;
    case BSTART:
        m_value.valueID = CSSValueStart;
        break;
    case BCENTER:
        m_value.valueID = CSSValueCenter;
        break;
    case BEND:
        m_value.valueID = CSSValueEnd;
        break;
    case BBASELINE:
        m_value.valueID = CSSValueBaseline;
        break;
    }
}

template<> inline EBoxAlignment CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueStretch:
        return BSTRETCH;
    case CSSValueStart:
        return BSTART;
    case CSSValueEnd:
        return BEND;
    case CSSValueCenter:
        return BCENTER;
    case CSSValueBaseline:
        return BBASELINE;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BSTRETCH;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBoxDecorationBreak e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case BoxDecorationBreakSlice:
        m_value.valueID = CSSValueSlice;
        break;
    case BoxDecorationBreakClone:
        m_value.valueID = CSSValueClone;
        break;
    }
}

template<> inline EBoxDecorationBreak CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueSlice:
        return BoxDecorationBreakSlice;
    case CSSValueClone:
        return BoxDecorationBreakClone;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BoxDecorationBreakSlice;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(BackgroundEdgeOrigin e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case TopEdge:
        m_value.valueID = CSSValueTop;
        break;
    case RightEdge:
        m_value.valueID = CSSValueRight;
        break;
    case BottomEdge:
        m_value.valueID = CSSValueBottom;
        break;
    case LeftEdge:
        m_value.valueID = CSSValueLeft;
        break;
    }
}

template<> inline BackgroundEdgeOrigin CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueTop:
        return TopEdge;
    case CSSValueRight:
        return RightEdge;
    case CSSValueBottom:
        return BottomEdge;
    case CSSValueLeft:
        return LeftEdge;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TopEdge;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBoxSizing e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case BoxSizingBorderBox:
        m_value.valueID = CSSValueBorderBox;
        break;
    case BoxSizingContentBox:
        m_value.valueID = CSSValueContentBox;
        break;
    }
}

template<> inline EBoxSizing CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueBorderBox:
        return BoxSizingBorderBox;
    case CSSValueContentBox:
        return BoxSizingContentBox;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BoxSizingBorderBox;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBoxDirection e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case BNORMAL:
        m_value.valueID = CSSValueNormal;
        break;
    case BREVERSE:
        m_value.valueID = CSSValueReverse;
        break;
    }
}

template<> inline EBoxDirection CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNormal:
        return BNORMAL;
    case CSSValueReverse:
        return BREVERSE;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BNORMAL;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBoxLines e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case SINGLE:
        m_value.valueID = CSSValueSingle;
        break;
    case MULTIPLE:
        m_value.valueID = CSSValueMultiple;
        break;
    }
}

template<> inline EBoxLines CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueSingle:
        return SINGLE;
    case CSSValueMultiple:
        return MULTIPLE;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return SINGLE;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBoxOrient e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case HORIZONTAL:
        m_value.valueID = CSSValueHorizontal;
        break;
    case VERTICAL:
        m_value.valueID = CSSValueVertical;
        break;
    }
}

template<> inline EBoxOrient CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueHorizontal:
    case CSSValueInlineAxis:
        return HORIZONTAL;
    case CSSValueVertical:
    case CSSValueBlockAxis:
        return VERTICAL;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return HORIZONTAL;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ECaptionSide e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case CaptionSideLeft:
        m_value.valueID = CSSValueLeft;
        break;
    case CaptionSideRight:
        m_value.valueID = CSSValueRight;
        break;
    case CaptionSideTop:
        m_value.valueID = CSSValueTop;
        break;
    case CaptionSideBottom:
        m_value.valueID = CSSValueBottom;
        break;
    }
}

template<> inline ECaptionSide CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueLeft:
        return CaptionSideLeft;
    case CSSValueRight:
        return CaptionSideRight;
    case CSSValueTop:
        return CaptionSideTop;
    case CSSValueBottom:
        return CaptionSideBottom;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return CaptionSideTop;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EClear e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case ClearNone:
        m_value.valueID = CSSValueNone;
        break;
    case ClearLeft:
        m_value.valueID = CSSValueLeft;
        break;
    case ClearRight:
        m_value.valueID = CSSValueRight;
        break;
    case ClearBoth:
        m_value.valueID = CSSValueBoth;
        break;
    }
}

template<> inline EClear CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNone:
        return ClearNone;
    case CSSValueLeft:
        return ClearLeft;
    case CSSValueRight:
        return ClearRight;
    case CSSValueBoth:
        return ClearBoth;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return ClearNone;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ECursor e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case CURSOR_AUTO:
        m_value.valueID = CSSValueAuto;
        break;
    case CURSOR_CROSS:
        m_value.valueID = CSSValueCrosshair;
        break;
    case CURSOR_DEFAULT:
        m_value.valueID = CSSValueDefault;
        break;
    case CURSOR_POINTER:
        m_value.valueID = CSSValuePointer;
        break;
    case CURSOR_MOVE:
        m_value.valueID = CSSValueMove;
        break;
    case CURSOR_CELL:
        m_value.valueID = CSSValueCell;
        break;
    case CURSOR_VERTICAL_TEXT:
        m_value.valueID = CSSValueVerticalText;
        break;
    case CURSOR_CONTEXT_MENU:
        m_value.valueID = CSSValueContextMenu;
        break;
    case CURSOR_ALIAS:
        m_value.valueID = CSSValueAlias;
        break;
    case CURSOR_COPY:
        m_value.valueID = CSSValueCopy;
        break;
    case CURSOR_NONE:
        m_value.valueID = CSSValueNone;
        break;
    case CURSOR_PROGRESS:
        m_value.valueID = CSSValueProgress;
        break;
    case CURSOR_NO_DROP:
        m_value.valueID = CSSValueNoDrop;
        break;
    case CURSOR_NOT_ALLOWED:
        m_value.valueID = CSSValueNotAllowed;
        break;
    case CURSOR_ZOOM_IN:
        m_value.valueID = CSSValueZoomIn;
        break;
    case CURSOR_ZOOM_OUT:
        m_value.valueID = CSSValueZoomOut;
        break;
    case CURSOR_E_RESIZE:
        m_value.valueID = CSSValueEResize;
        break;
    case CURSOR_NE_RESIZE:
        m_value.valueID = CSSValueNeResize;
        break;
    case CURSOR_NW_RESIZE:
        m_value.valueID = CSSValueNwResize;
        break;
    case CURSOR_N_RESIZE:
        m_value.valueID = CSSValueNResize;
        break;
    case CURSOR_SE_RESIZE:
        m_value.valueID = CSSValueSeResize;
        break;
    case CURSOR_SW_RESIZE:
        m_value.valueID = CSSValueSwResize;
        break;
    case CURSOR_S_RESIZE:
        m_value.valueID = CSSValueSResize;
        break;
    case CURSOR_W_RESIZE:
        m_value.valueID = CSSValueWResize;
        break;
    case CURSOR_EW_RESIZE:
        m_value.valueID = CSSValueEwResize;
        break;
    case CURSOR_NS_RESIZE:
        m_value.valueID = CSSValueNsResize;
        break;
    case CURSOR_NESW_RESIZE:
        m_value.valueID = CSSValueNeswResize;
        break;
    case CURSOR_NWSE_RESIZE:
        m_value.valueID = CSSValueNwseResize;
        break;
    case CURSOR_COL_RESIZE:
        m_value.valueID = CSSValueColResize;
        break;
    case CURSOR_ROW_RESIZE:
        m_value.valueID = CSSValueRowResize;
        break;
    case CURSOR_TEXT:
        m_value.valueID = CSSValueText;
        break;
    case CURSOR_WAIT:
        m_value.valueID = CSSValueWait;
        break;
    case CURSOR_HELP:
        m_value.valueID = CSSValueHelp;
        break;
    case CURSOR_ALL_SCROLL:
        m_value.valueID = CSSValueAllScroll;
        break;
    case CURSOR_WEBKIT_GRAB:
        m_value.valueID = CSSValueWebkitGrab;
        break;
    case CURSOR_WEBKIT_GRABBING:
        m_value.valueID = CSSValueWebkitGrabbing;
        break;
    }
}

template<> inline ECursor CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueCopy:
        return CURSOR_COPY;
    case CSSValueWebkitZoomIn:
        return CURSOR_ZOOM_IN;
    case CSSValueWebkitZoomOut:
        return CURSOR_ZOOM_OUT;
    case CSSValueNone:
        return CURSOR_NONE;
    default:
        return static_cast<ECursor>(m_value.valueID - CSSValueAuto);
    }
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EDisplay e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case INLINE:
        m_value.valueID = CSSValueInline;
        break;
    case BLOCK:
        m_value.valueID = CSSValueBlock;
        break;
    case LIST_ITEM:
        m_value.valueID = CSSValueListItem;
        break;
    case INLINE_BLOCK:
        m_value.valueID = CSSValueInlineBlock;
        break;
    case TABLE:
        m_value.valueID = CSSValueTable;
        break;
    case INLINE_TABLE:
        m_value.valueID = CSSValueInlineTable;
        break;
    case TABLE_ROW_GROUP:
        m_value.valueID = CSSValueTableRowGroup;
        break;
    case TABLE_HEADER_GROUP:
        m_value.valueID = CSSValueTableHeaderGroup;
        break;
    case TABLE_FOOTER_GROUP:
        m_value.valueID = CSSValueTableFooterGroup;
        break;
    case TABLE_ROW:
        m_value.valueID = CSSValueTableRow;
        break;
    case TABLE_COLUMN_GROUP:
        m_value.valueID = CSSValueTableColumnGroup;
        break;
    case TABLE_COLUMN:
        m_value.valueID = CSSValueTableColumn;
        break;
    case TABLE_CELL:
        m_value.valueID = CSSValueTableCell;
        break;
    case TABLE_CAPTION:
        m_value.valueID = CSSValueTableCaption;
        break;
    case BOX:
        m_value.valueID = CSSValueWebkitBox;
        break;
    case INLINE_BOX:
        m_value.valueID = CSSValueWebkitInlineBox;
        break;
    case FLEX:
        m_value.valueID = CSSValueFlex;
        break;
    case INLINE_FLEX:
        m_value.valueID = CSSValueInlineFlex;
        break;
    case GRID:
        m_value.valueID = CSSValueGrid;
        break;
    case INLINE_GRID:
        m_value.valueID = CSSValueInlineGrid;
        break;
    case NONE:
        m_value.valueID = CSSValueNone;
        break;
    }
}

template<> inline EDisplay CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    if (m_value.valueID == CSSValueNone)
        return NONE;

    if (m_value.valueID == CSSValueWebkitFlex)
        return FLEX;
    if (m_value.valueID == CSSValueWebkitInlineFlex)
        return INLINE_FLEX;

    EDisplay display = static_cast<EDisplay>(m_value.valueID - CSSValueInline);
    ASSERT(display >= INLINE && display <= NONE);
    return display;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EEmptyCells e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case EmptyCellsShow:
        m_value.valueID = CSSValueShow;
        break;
    case EmptyCellsHide:
        m_value.valueID = CSSValueHide;
        break;
    }
}

template<> inline EEmptyCells CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueShow:
        return EmptyCellsShow;
    case CSSValueHide:
        return EmptyCellsHide;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return EmptyCellsShow;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EFlexDirection e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case FlowRow:
        m_value.valueID = CSSValueRow;
        break;
    case FlowRowReverse:
        m_value.valueID = CSSValueRowReverse;
        break;
    case FlowColumn:
        m_value.valueID = CSSValueColumn;
        break;
    case FlowColumnReverse:
        m_value.valueID = CSSValueColumnReverse;
        break;
    }
}

template<> inline EFlexDirection CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueRow:
        return FlowRow;
    case CSSValueRowReverse:
        return FlowRowReverse;
    case CSSValueColumn:
        return FlowColumn;
    case CSSValueColumnReverse:
        return FlowColumnReverse;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return FlowRow;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EFlexWrap e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case FlexNoWrap:
        m_value.valueID = CSSValueNowrap;
        break;
    case FlexWrap:
        m_value.valueID = CSSValueWrap;
        break;
    case FlexWrapReverse:
        m_value.valueID = CSSValueWrapReverse;
        break;
    }
}

template<> inline EFlexWrap CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNowrap:
        return FlexNoWrap;
    case CSSValueWrap:
        return FlexWrap;
    case CSSValueWrapReverse:
        return FlexWrapReverse;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return FlexNoWrap;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EFloat e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case NoFloat:
        m_value.valueID = CSSValueNone;
        break;
    case LeftFloat:
        m_value.valueID = CSSValueLeft;
        break;
    case RightFloat:
        m_value.valueID = CSSValueRight;
        break;
    }
}

template<> inline EFloat CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueLeft:
        return LeftFloat;
    case CSSValueRight:
        return RightFloat;
    case CSSValueNone:
        return NoFloat;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return NoFloat;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(Hyphens e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case HyphensAuto:
        m_value.valueID = CSSValueAuto;
        break;
    case HyphensManual:
        m_value.valueID = CSSValueManual;
        break;
    case HyphensNone:
        m_value.valueID = CSSValueNone;
        break;
    }
}

template<> inline Hyphens CSSPrimitiveValue::convertTo() const
{
    DCHECK(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return HyphensAuto;
    case CSSValueManual:
        return HyphensManual;
    case CSSValueNone:
        return HyphensNone;
    default:
        break;
    }

    NOTREACHED();
    return HyphensManual;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(LineBreak e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case LineBreakAuto:
        m_value.valueID = CSSValueAuto;
        break;
    case LineBreakLoose:
        m_value.valueID = CSSValueLoose;
        break;
    case LineBreakNormal:
        m_value.valueID = CSSValueNormal;
        break;
    case LineBreakStrict:
        m_value.valueID = CSSValueStrict;
        break;
    case LineBreakAfterWhiteSpace:
        m_value.valueID = CSSValueAfterWhiteSpace;
        break;
    }
}

template<> inline LineBreak CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return LineBreakAuto;
    case CSSValueLoose:
        return LineBreakLoose;
    case CSSValueNormal:
        return LineBreakNormal;
    case CSSValueStrict:
        return LineBreakStrict;
    case CSSValueAfterWhiteSpace:
        return LineBreakAfterWhiteSpace;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return LineBreakAuto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EListStylePosition e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case ListStylePositionOutside:
        m_value.valueID = CSSValueOutside;
        break;
    case ListStylePositionInside:
        m_value.valueID = CSSValueInside;
        break;
    }
}

template<> inline EListStylePosition CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueOutside:
        return ListStylePositionOutside;
    case CSSValueInside:
        return ListStylePositionInside;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return ListStylePositionOutside;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EListStyleType e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case ArabicIndic:
        m_value.valueID = CSSValueArabicIndic;
        break;
    case Armenian:
        m_value.valueID = CSSValueArmenian;
        break;
    case Bengali:
        m_value.valueID = CSSValueBengali;
        break;
    case Cambodian:
        m_value.valueID = CSSValueCambodian;
        break;
    case Circle:
        m_value.valueID = CSSValueCircle;
        break;
    case CjkEarthlyBranch:
        m_value.valueID = CSSValueCjkEarthlyBranch;
        break;
    case CjkHeavenlyStem:
        m_value.valueID = CSSValueCjkHeavenlyStem;
        break;
    case CJKIdeographic:
        m_value.valueID = CSSValueCjkIdeographic;
        break;
    case DecimalLeadingZero:
        m_value.valueID = CSSValueDecimalLeadingZero;
        break;
    case DecimalListStyle:
        m_value.valueID = CSSValueDecimal;
        break;
    case Devanagari:
        m_value.valueID = CSSValueDevanagari;
        break;
    case Disc:
        m_value.valueID = CSSValueDisc;
        break;
    case EthiopicHalehame:
        m_value.valueID = CSSValueEthiopicHalehame;
        break;
    case EthiopicHalehameAm:
        m_value.valueID = CSSValueEthiopicHalehameAm;
        break;
    case EthiopicHalehameTiEt:
        m_value.valueID = CSSValueEthiopicHalehameTiEt;
        break;
    case EthiopicHalehameTiEr:
        m_value.valueID = CSSValueEthiopicHalehameTiEr;
        break;
    case Georgian:
        m_value.valueID = CSSValueGeorgian;
        break;
    case Gujarati:
        m_value.valueID = CSSValueGujarati;
        break;
    case Gurmukhi:
        m_value.valueID = CSSValueGurmukhi;
        break;
    case Hangul:
        m_value.valueID = CSSValueHangul;
        break;
    case HangulConsonant:
        m_value.valueID = CSSValueHangulConsonant;
        break;
    case KoreanHangulFormal:
        m_value.valueID = CSSValueKoreanHangulFormal;
        break;
    case KoreanHanjaFormal:
        m_value.valueID = CSSValueKoreanHanjaFormal;
        break;
    case KoreanHanjaInformal:
        m_value.valueID = CSSValueKoreanHanjaInformal;
        break;
    case Hebrew:
        m_value.valueID = CSSValueHebrew;
        break;
    case Hiragana:
        m_value.valueID = CSSValueHiragana;
        break;
    case HiraganaIroha:
        m_value.valueID = CSSValueHiraganaIroha;
        break;
    case Kannada:
        m_value.valueID = CSSValueKannada;
        break;
    case Katakana:
        m_value.valueID = CSSValueKatakana;
        break;
    case KatakanaIroha:
        m_value.valueID = CSSValueKatakanaIroha;
        break;
    case Khmer:
        m_value.valueID = CSSValueKhmer;
        break;
    case Lao:
        m_value.valueID = CSSValueLao;
        break;
    case LowerAlpha:
        m_value.valueID = CSSValueLowerAlpha;
        break;
    case LowerArmenian:
        m_value.valueID = CSSValueLowerArmenian;
        break;
    case LowerGreek:
        m_value.valueID = CSSValueLowerGreek;
        break;
    case LowerLatin:
        m_value.valueID = CSSValueLowerLatin;
        break;
    case LowerRoman:
        m_value.valueID = CSSValueLowerRoman;
        break;
    case Malayalam:
        m_value.valueID = CSSValueMalayalam;
        break;
    case Mongolian:
        m_value.valueID = CSSValueMongolian;
        break;
    case Myanmar:
        m_value.valueID = CSSValueMyanmar;
        break;
    case NoneListStyle:
        m_value.valueID = CSSValueNone;
        break;
    case Oriya:
        m_value.valueID = CSSValueOriya;
        break;
    case Persian:
        m_value.valueID = CSSValuePersian;
        break;
    case SimpChineseFormal:
        m_value.valueID = CSSValueSimpChineseFormal;
        break;
    case SimpChineseInformal:
        m_value.valueID = CSSValueSimpChineseInformal;
        break;
    case Square:
        m_value.valueID = CSSValueSquare;
        break;
    case Telugu:
        m_value.valueID = CSSValueTelugu;
        break;
    case Thai:
        m_value.valueID = CSSValueThai;
        break;
    case Tibetan:
        m_value.valueID = CSSValueTibetan;
        break;
    case TradChineseFormal:
        m_value.valueID = CSSValueTradChineseFormal;
        break;
    case TradChineseInformal:
        m_value.valueID = CSSValueTradChineseInformal;
        break;
    case UpperAlpha:
        m_value.valueID = CSSValueUpperAlpha;
        break;
    case UpperArmenian:
        m_value.valueID = CSSValueUpperArmenian;
        break;
    case UpperLatin:
        m_value.valueID = CSSValueUpperLatin;
        break;
    case UpperRoman:
        m_value.valueID = CSSValueUpperRoman;
        break;
    case Urdu:
        m_value.valueID = CSSValueUrdu;
        break;
    }
}

template<> inline EListStyleType CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNone:
        return NoneListStyle;
    default:
        return static_cast<EListStyleType>(m_value.valueID - CSSValueDisc);
    }
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EMarginCollapse e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case MarginCollapseCollapse:
        m_value.valueID = CSSValueCollapse;
        break;
    case MarginCollapseSeparate:
        m_value.valueID = CSSValueSeparate;
        break;
    case MarginCollapseDiscard:
        m_value.valueID = CSSValueDiscard;
        break;
    }
}

template<> inline EMarginCollapse CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueCollapse:
        return MarginCollapseCollapse;
    case CSSValueSeparate:
        return MarginCollapseSeparate;
    case CSSValueDiscard:
        return MarginCollapseDiscard;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return MarginCollapseCollapse;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EOverflow e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case OverflowVisible:
        m_value.valueID = CSSValueVisible;
        break;
    case OverflowHidden:
        m_value.valueID = CSSValueHidden;
        break;
    case OverflowScroll:
        m_value.valueID = CSSValueScroll;
        break;
    case OverflowAuto:
        m_value.valueID = CSSValueAuto;
        break;
    case OverflowOverlay:
        m_value.valueID = CSSValueOverlay;
        break;
    case OverflowPagedX:
        m_value.valueID = CSSValueWebkitPagedX;
        break;
    case OverflowPagedY:
        m_value.valueID = CSSValueWebkitPagedY;
        break;
    }
}

template<> inline EOverflow CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueVisible:
        return OverflowVisible;
    case CSSValueHidden:
        return OverflowHidden;
    case CSSValueScroll:
        return OverflowScroll;
    case CSSValueAuto:
        return OverflowAuto;
    case CSSValueOverlay:
        return OverflowOverlay;
    case CSSValueWebkitPagedX:
        return OverflowPagedX;
    case CSSValueWebkitPagedY:
        return OverflowPagedY;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return OverflowVisible;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBreak e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    default:
        ASSERT_NOT_REACHED();
    case BreakAuto:
        m_value.valueID = CSSValueAuto;
        break;
    case BreakAlways:
        m_value.valueID = CSSValueAlways;
        break;
    case BreakAvoid:
        m_value.valueID = CSSValueAvoid;
        break;
    case BreakAvoidPage:
        m_value.valueID = CSSValueAvoidPage;
        break;
    case BreakPage:
        m_value.valueID = CSSValuePage;
        break;
    case BreakLeft:
        m_value.valueID = CSSValueLeft;
        break;
    case BreakRight:
        m_value.valueID = CSSValueRight;
        break;
    case BreakRecto:
        m_value.valueID = CSSValueRecto;
        break;
    case BreakVerso:
        m_value.valueID = CSSValueVerso;
        break;
    case BreakAvoidColumn:
        m_value.valueID = CSSValueAvoidColumn;
        break;
    case BreakColumn:
        m_value.valueID = CSSValueColumn;
        break;
    }
}

template<> inline EBreak CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    default:
        ASSERT_NOT_REACHED();
    case CSSValueAuto:
        return BreakAuto;
    case CSSValueAvoid:
        return BreakAvoid;
    case CSSValueAlways:
        return BreakAlways;
    case CSSValueAvoidPage:
        return BreakAvoidPage;
    case CSSValuePage:
        return BreakPage;
    case CSSValueLeft:
        return BreakLeft;
    case CSSValueRight:
        return BreakRight;
    case CSSValueRecto:
        return BreakRecto;
    case CSSValueVerso:
        return BreakVerso;
    case CSSValueAvoidColumn:
        return BreakAvoidColumn;
    case CSSValueColumn:
        return BreakColumn;
    }
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EPosition e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case StaticPosition:
        m_value.valueID = CSSValueStatic;
        break;
    case RelativePosition:
        m_value.valueID = CSSValueRelative;
        break;
    case AbsolutePosition:
        m_value.valueID = CSSValueAbsolute;
        break;
    case FixedPosition:
        m_value.valueID = CSSValueFixed;
        break;
    case StickyPosition:
        m_value.valueID = CSSValueSticky;
        break;
    }
}

template<> inline EPosition CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueStatic:
        return StaticPosition;
    case CSSValueRelative:
        return RelativePosition;
    case CSSValueAbsolute:
        return AbsolutePosition;
    case CSSValueFixed:
        return FixedPosition;
    case CSSValueSticky:
        return StickyPosition;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return StaticPosition;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EResize e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case RESIZE_BOTH:
        m_value.valueID = CSSValueBoth;
        break;
    case RESIZE_HORIZONTAL:
        m_value.valueID = CSSValueHorizontal;
        break;
    case RESIZE_VERTICAL:
        m_value.valueID = CSSValueVertical;
        break;
    case RESIZE_NONE:
        m_value.valueID = CSSValueNone;
        break;
    }
}

template<> inline EResize CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueBoth:
        return RESIZE_BOTH;
    case CSSValueHorizontal:
        return RESIZE_HORIZONTAL;
    case CSSValueVertical:
        return RESIZE_VERTICAL;
    case CSSValueAuto:
        ASSERT_NOT_REACHED(); // Depends on settings, thus should be handled by the caller.
        return RESIZE_NONE;
    case CSSValueNone:
        return RESIZE_NONE;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return RESIZE_NONE;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ETableLayout e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case TableLayoutAuto:
        m_value.valueID = CSSValueAuto;
        break;
    case TableLayoutFixed:
        m_value.valueID = CSSValueFixed;
        break;
    }
}

template<> inline ETableLayout CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueFixed:
        return TableLayoutFixed;
    case CSSValueAuto:
        return TableLayoutAuto;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TableLayoutAuto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ETextAlign e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case TASTART:
        m_value.valueID = CSSValueStart;
        break;
    case TAEND:
        m_value.valueID = CSSValueEnd;
        break;
    case LEFT:
        m_value.valueID = CSSValueLeft;
        break;
    case RIGHT:
        m_value.valueID = CSSValueRight;
        break;
    case CENTER:
        m_value.valueID = CSSValueCenter;
        break;
    case JUSTIFY:
        m_value.valueID = CSSValueJustify;
        break;
    case WEBKIT_LEFT:
        m_value.valueID = CSSValueWebkitLeft;
        break;
    case WEBKIT_RIGHT:
        m_value.valueID = CSSValueWebkitRight;
        break;
    case WEBKIT_CENTER:
        m_value.valueID = CSSValueWebkitCenter;
        break;
    }
}

template<> inline ETextAlign CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueWebkitAuto: // Legacy -webkit-auto. Eqiuvalent to start.
    case CSSValueStart:
        return TASTART;
    case CSSValueEnd:
        return TAEND;
    case CSSValueInternalCenter:
        return CENTER;
    default:
        return static_cast<ETextAlign>(m_value.valueID - CSSValueLeft);
    }
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextAlignLast e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case TextAlignLastStart:
        m_value.valueID = CSSValueStart;
        break;
    case TextAlignLastEnd:
        m_value.valueID = CSSValueEnd;
        break;
    case TextAlignLastLeft:
        m_value.valueID = CSSValueLeft;
        break;
    case TextAlignLastRight:
        m_value.valueID = CSSValueRight;
        break;
    case TextAlignLastCenter:
        m_value.valueID = CSSValueCenter;
        break;
    case TextAlignLastJustify:
        m_value.valueID = CSSValueJustify;
        break;
    case TextAlignLastAuto:
        m_value.valueID = CSSValueAuto;
        break;
    }
}

template<> inline TextAlignLast CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return TextAlignLastAuto;
    case CSSValueStart:
        return TextAlignLastStart;
    case CSSValueEnd:
        return TextAlignLastEnd;
    case CSSValueLeft:
        return TextAlignLastLeft;
    case CSSValueRight:
        return TextAlignLastRight;
    case CSSValueCenter:
        return TextAlignLastCenter;
    case CSSValueJustify:
        return TextAlignLastJustify;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextAlignLastAuto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextJustify e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case TextJustifyAuto:
        m_value.valueID = CSSValueAuto;
        break;
    case TextJustifyNone:
        m_value.valueID = CSSValueNone;
        break;
    case TextJustifyInterWord:
        m_value.valueID = CSSValueInterWord;
        break;
    case TextJustifyDistribute:
        m_value.valueID = CSSValueDistribute;
        break;
    }
}

template<> inline TextJustify CSSPrimitiveValue::convertTo() const
{
    switch (m_value.valueID) {
    case CSSValueAuto:
        return TextJustifyAuto;
    case CSSValueNone:
        return TextJustifyNone;
    case CSSValueInterWord:
        return TextJustifyInterWord;
    case CSSValueDistribute:
        return TextJustifyDistribute;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextJustifyAuto;
}

template<> inline TextDecoration CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNone:
        return TextDecorationNone;
    case CSSValueUnderline:
        return TextDecorationUnderline;
    case CSSValueOverline:
        return TextDecorationOverline;
    case CSSValueLineThrough:
        return TextDecorationLineThrough;
    case CSSValueBlink:
        return TextDecorationBlink;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextDecorationNone;
}

template<> inline TextDecorationStyle CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueSolid:
        return TextDecorationStyleSolid;
    case CSSValueDouble:
        return TextDecorationStyleDouble;
    case CSSValueDotted:
        return TextDecorationStyleDotted;
    case CSSValueDashed:
        return TextDecorationStyleDashed;
    case CSSValueWavy:
        return TextDecorationStyleWavy;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextDecorationStyleSolid;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextUnderlinePosition e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case TextUnderlinePositionAuto:
        m_value.valueID = CSSValueAuto;
        break;
    case TextUnderlinePositionUnder:
        m_value.valueID = CSSValueUnder;
        break;
    }

    // FIXME: Implement support for 'under left' and 'under right' values.
}

template<> inline TextUnderlinePosition CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return TextUnderlinePositionAuto;
    case CSSValueUnder:
        return TextUnderlinePositionUnder;
    default:
        break;
    }

    // FIXME: Implement support for 'under left' and 'under right' values.

    ASSERT_NOT_REACHED();
    return TextUnderlinePositionAuto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ETextSecurity e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case TSNONE:
        m_value.valueID = CSSValueNone;
        break;
    case TSDISC:
        m_value.valueID = CSSValueDisc;
        break;
    case TSCIRCLE:
        m_value.valueID = CSSValueCircle;
        break;
    case TSSQUARE:
        m_value.valueID = CSSValueSquare;
        break;
    }
}

template<> inline ETextSecurity CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNone:
        return TSNONE;
    case CSSValueDisc:
        return TSDISC;
    case CSSValueCircle:
        return TSCIRCLE;
    case CSSValueSquare:
        return TSSQUARE;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TSNONE;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ETextTransform e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case CAPITALIZE:
        m_value.valueID = CSSValueCapitalize;
        break;
    case UPPERCASE:
        m_value.valueID = CSSValueUppercase;
        break;
    case LOWERCASE:
        m_value.valueID = CSSValueLowercase;
        break;
    case TTNONE:
        m_value.valueID = CSSValueNone;
        break;
    }
}

template<> inline ETextTransform CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueCapitalize:
        return CAPITALIZE;
    case CSSValueUppercase:
        return UPPERCASE;
    case CSSValueLowercase:
        return LOWERCASE;
    case CSSValueNone:
        return TTNONE;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TTNONE;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EUnicodeBidi e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case UBNormal:
        m_value.valueID = CSSValueNormal;
        break;
    case Embed:
        m_value.valueID = CSSValueEmbed;
        break;
    case Override:
        m_value.valueID = CSSValueBidiOverride;
        break;
    case Isolate:
        m_value.valueID = CSSValueIsolate;
        break;
    case IsolateOverride:
        m_value.valueID = CSSValueIsolateOverride;
        break;
    case Plaintext:
        m_value.valueID = CSSValuePlaintext;
        break;
    }
}

template<> inline EUnicodeBidi CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNormal:
        return UBNormal;
    case CSSValueEmbed:
        return Embed;
    case CSSValueBidiOverride:
        return Override;
    case CSSValueIsolate:
    case CSSValueWebkitIsolate:
        return Isolate;
    case CSSValueIsolateOverride:
    case CSSValueWebkitIsolateOverride:
        return IsolateOverride;
    case CSSValuePlaintext:
    case CSSValueWebkitPlaintext:
        return Plaintext;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return UBNormal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EUserDrag e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case DRAG_AUTO:
        m_value.valueID = CSSValueAuto;
        break;
    case DRAG_NONE:
        m_value.valueID = CSSValueNone;
        break;
    case DRAG_ELEMENT:
        m_value.valueID = CSSValueElement;
        break;
    default:
        break;
    }
}

template<> inline EUserDrag CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return DRAG_AUTO;
    case CSSValueNone:
        return DRAG_NONE;
    case CSSValueElement:
        return DRAG_ELEMENT;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return DRAG_AUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EUserModify e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case READ_ONLY:
        m_value.valueID = CSSValueReadOnly;
        break;
    case READ_WRITE:
        m_value.valueID = CSSValueReadWrite;
        break;
    case READ_WRITE_PLAINTEXT_ONLY:
        m_value.valueID = CSSValueReadWritePlaintextOnly;
        break;
    }
}

template<> inline EUserModify CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueReadOnly:
        return READ_ONLY;
    case CSSValueReadWrite:
        return READ_WRITE;
    case CSSValueReadWritePlaintextOnly:
        return READ_WRITE_PLAINTEXT_ONLY;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return READ_ONLY;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EUserSelect e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case SELECT_NONE:
        m_value.valueID = CSSValueNone;
        break;
    case SELECT_TEXT:
        m_value.valueID = CSSValueText;
        break;
    case SELECT_ALL:
        m_value.valueID = CSSValueAll;
        break;
    }
}

template<> inline EUserSelect CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return SELECT_TEXT;
    case CSSValueNone:
        return SELECT_NONE;
    case CSSValueText:
        return SELECT_TEXT;
    case CSSValueAll:
        return SELECT_ALL;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return SELECT_TEXT;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EVerticalAlign a)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (a) {
    case VerticalAlignTop:
        m_value.valueID = CSSValueTop;
        break;
    case VerticalAlignBottom:
        m_value.valueID = CSSValueBottom;
        break;
    case VerticalAlignMiddle:
        m_value.valueID = CSSValueMiddle;
        break;
    case VerticalAlignBaseline:
        m_value.valueID = CSSValueBaseline;
        break;
    case VerticalAlignTextBottom:
        m_value.valueID = CSSValueTextBottom;
        break;
    case VerticalAlignTextTop:
        m_value.valueID = CSSValueTextTop;
        break;
    case VerticalAlignSub:
        m_value.valueID = CSSValueSub;
        break;
    case VerticalAlignSuper:
        m_value.valueID = CSSValueSuper;
        break;
    case VerticalAlignBaselineMiddle:
        m_value.valueID = CSSValueWebkitBaselineMiddle;
        break;
    case VerticalAlignLength:
        m_value.valueID = CSSValueInvalid;
    }
}

template<> inline EVerticalAlign CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueTop:
        return VerticalAlignTop;
    case CSSValueBottom:
        return VerticalAlignBottom;
    case CSSValueMiddle:
        return VerticalAlignMiddle;
    case CSSValueBaseline:
        return VerticalAlignBaseline;
    case CSSValueTextBottom:
        return VerticalAlignTextBottom;
    case CSSValueTextTop:
        return VerticalAlignTextTop;
    case CSSValueSub:
        return VerticalAlignSub;
    case CSSValueSuper:
        return VerticalAlignSuper;
    case CSSValueWebkitBaselineMiddle:
        return VerticalAlignBaselineMiddle;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return VerticalAlignTop;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EVisibility e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case VISIBLE:
        m_value.valueID = CSSValueVisible;
        break;
    case HIDDEN:
        m_value.valueID = CSSValueHidden;
        break;
    case COLLAPSE:
        m_value.valueID = CSSValueCollapse;
        break;
    }
}

template<> inline EVisibility CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueHidden:
        return HIDDEN;
    case CSSValueVisible:
        return VISIBLE;
    case CSSValueCollapse:
        return COLLAPSE;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return VISIBLE;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EWhiteSpace e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case NORMAL:
        m_value.valueID = CSSValueNormal;
        break;
    case PRE:
        m_value.valueID = CSSValuePre;
        break;
    case PRE_WRAP:
        m_value.valueID = CSSValuePreWrap;
        break;
    case PRE_LINE:
        m_value.valueID = CSSValuePreLine;
        break;
    case NOWRAP:
        m_value.valueID = CSSValueNowrap;
        break;
    case KHTML_NOWRAP:
        m_value.valueID = CSSValueWebkitNowrap;
        break;
    }
}

template<> inline EWhiteSpace CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueWebkitNowrap:
        return KHTML_NOWRAP;
    case CSSValueNowrap:
        return NOWRAP;
    case CSSValuePre:
        return PRE;
    case CSSValuePreWrap:
        return PRE_WRAP;
    case CSSValuePreLine:
        return PRE_LINE;
    case CSSValueNormal:
        return NORMAL;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return NORMAL;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EWordBreak e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case NormalWordBreak:
        m_value.valueID = CSSValueNormal;
        break;
    case BreakAllWordBreak:
        m_value.valueID = CSSValueBreakAll;
        break;
    case BreakWordBreak:
        m_value.valueID = CSSValueBreakWord;
        break;
    case KeepAllWordBreak:
        m_value.valueID = CSSValueKeepAll;
        break;
    }
}

template<> inline EWordBreak CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueBreakAll:
        return BreakAllWordBreak;
    case CSSValueBreakWord:
        return BreakWordBreak;
    case CSSValueNormal:
        return NormalWordBreak;
    case CSSValueKeepAll:
        return KeepAllWordBreak;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return NormalWordBreak;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EOverflowWrap e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case NormalOverflowWrap:
        m_value.valueID = CSSValueNormal;
        break;
    case BreakOverflowWrap:
        m_value.valueID = CSSValueBreakWord;
        break;
    }
}

template<> inline EOverflowWrap CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueBreakWord:
        return BreakOverflowWrap;
    case CSSValueNormal:
        return NormalOverflowWrap;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return NormalOverflowWrap;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextDirection e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case LTR:
        m_value.valueID = CSSValueLtr;
        break;
    case RTL:
        m_value.valueID = CSSValueRtl;
        break;
    }
}

template<> inline TextDirection CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueLtr:
        return LTR;
    case CSSValueRtl:
        return RTL;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return LTR;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(WritingMode e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case TopToBottomWritingMode:
        m_value.valueID = CSSValueHorizontalTb;
        break;
    case RightToLeftWritingMode:
        m_value.valueID = CSSValueVerticalRl;
        break;
    case LeftToRightWritingMode:
        m_value.valueID = CSSValueVerticalLr;
        break;
    }
}

template<> inline WritingMode CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueHorizontalTb:
    case CSSValueLr:
    case CSSValueLrTb:
    case CSSValueRl:
    case CSSValueRlTb:
        return TopToBottomWritingMode;
    case CSSValueVerticalRl:
    case CSSValueTb:
    case CSSValueTbRl:
        return RightToLeftWritingMode;
    case CSSValueVerticalLr:
        return LeftToRightWritingMode;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TopToBottomWritingMode;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextCombine e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case TextCombineNone:
        m_value.valueID = CSSValueNone;
        break;
    case TextCombineAll:
        m_value.valueID = CSSValueAll;
        break;
    }
}

template<> inline TextCombine CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNone:
        return TextCombineNone;
    case CSSValueAll:
    case CSSValueHorizontal: // -webkit-text-combine
        return TextCombineAll;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextCombineNone;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(RubyPosition position)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (position) {
    case RubyPositionBefore:
        m_value.valueID = CSSValueBefore;
        break;
    case RubyPositionAfter:
        m_value.valueID = CSSValueAfter;
        break;
    }
}

template<> inline RubyPosition CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueBefore:
        return RubyPositionBefore;
    case CSSValueAfter:
        return RubyPositionAfter;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return RubyPositionBefore;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextEmphasisPosition position)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (position) {
    case TextEmphasisPositionOver:
        m_value.valueID = CSSValueOver;
        break;
    case TextEmphasisPositionUnder:
        m_value.valueID = CSSValueUnder;
        break;
    }
}

template<> inline TextEmphasisPosition CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueOver:
        return TextEmphasisPositionOver;
    case CSSValueUnder:
        return TextEmphasisPositionUnder;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextEmphasisPositionOver;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextOverflow overflow)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (overflow) {
    case TextOverflowClip:
        m_value.valueID = CSSValueClip;
        break;
    case TextOverflowEllipsis:
        m_value.valueID = CSSValueEllipsis;
        break;
    }
}

template<> inline TextOverflow CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueClip:
        return TextOverflowClip;
    case CSSValueEllipsis:
        return TextOverflowEllipsis;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextOverflowClip;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextEmphasisFill fill)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (fill) {
    case TextEmphasisFillFilled:
        m_value.valueID = CSSValueFilled;
        break;
    case TextEmphasisFillOpen:
        m_value.valueID = CSSValueOpen;
        break;
    }
}

template<> inline TextEmphasisFill CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueFilled:
        return TextEmphasisFillFilled;
    case CSSValueOpen:
        return TextEmphasisFillOpen;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextEmphasisFillFilled;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextEmphasisMark mark)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (mark) {
    case TextEmphasisMarkDot:
        m_value.valueID = CSSValueDot;
        break;
    case TextEmphasisMarkCircle:
        m_value.valueID = CSSValueCircle;
        break;
    case TextEmphasisMarkDoubleCircle:
        m_value.valueID = CSSValueDoubleCircle;
        break;
    case TextEmphasisMarkTriangle:
        m_value.valueID = CSSValueTriangle;
        break;
    case TextEmphasisMarkSesame:
        m_value.valueID = CSSValueSesame;
        break;
    case TextEmphasisMarkNone:
    case TextEmphasisMarkAuto:
    case TextEmphasisMarkCustom:
        ASSERT_NOT_REACHED();
        m_value.valueID = CSSValueNone;
        break;
    }
}

template<> inline TextEmphasisMark CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNone:
        return TextEmphasisMarkNone;
    case CSSValueDot:
        return TextEmphasisMarkDot;
    case CSSValueCircle:
        return TextEmphasisMarkCircle;
    case CSSValueDoubleCircle:
        return TextEmphasisMarkDoubleCircle;
    case CSSValueTriangle:
        return TextEmphasisMarkTriangle;
    case CSSValueSesame:
        return TextEmphasisMarkSesame;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextEmphasisMarkNone;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextOrientation e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case TextOrientationSideways:
        m_value.valueID = CSSValueSideways;
        break;
    case TextOrientationMixed:
        m_value.valueID = CSSValueMixed;
        break;
    case TextOrientationUpright:
        m_value.valueID = CSSValueUpright;
        break;
    }
}

template<> inline TextOrientation CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueSideways:
    case CSSValueSidewaysRight:
        return TextOrientationSideways;
    case CSSValueMixed:
    case CSSValueVerticalRight: // -webkit-text-orientation
        return TextOrientationMixed;
    case CSSValueUpright:
        return TextOrientationUpright;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TextOrientationMixed;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EPointerEvents e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case PE_NONE:
        m_value.valueID = CSSValueNone;
        break;
    case PE_STROKE:
        m_value.valueID = CSSValueStroke;
        break;
    case PE_FILL:
        m_value.valueID = CSSValueFill;
        break;
    case PE_PAINTED:
        m_value.valueID = CSSValuePainted;
        break;
    case PE_VISIBLE:
        m_value.valueID = CSSValueVisible;
        break;
    case PE_VISIBLE_STROKE:
        m_value.valueID = CSSValueVisibleStroke;
        break;
    case PE_VISIBLE_FILL:
        m_value.valueID = CSSValueVisibleFill;
        break;
    case PE_VISIBLE_PAINTED:
        m_value.valueID = CSSValueVisiblePainted;
        break;
    case PE_AUTO:
        m_value.valueID = CSSValueAuto;
        break;
    case PE_ALL:
        m_value.valueID = CSSValueAll;
        break;
    case PE_BOUNDINGBOX:
        m_value.valueID = CSSValueBoundingBox;
        break;
    }
}

template<> inline EPointerEvents CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAll:
        return PE_ALL;
    case CSSValueAuto:
        return PE_AUTO;
    case CSSValueNone:
        return PE_NONE;
    case CSSValueVisiblePainted:
        return PE_VISIBLE_PAINTED;
    case CSSValueVisibleFill:
        return PE_VISIBLE_FILL;
    case CSSValueVisibleStroke:
        return PE_VISIBLE_STROKE;
    case CSSValueVisible:
        return PE_VISIBLE;
    case CSSValuePainted:
        return PE_PAINTED;
    case CSSValueFill:
        return PE_FILL;
    case CSSValueStroke:
        return PE_STROKE;
    case CSSValueBoundingBox:
        return PE_BOUNDINGBOX;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return PE_ALL;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(FontDescription::Kerning kerning)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (kerning) {
    case FontDescription::AutoKerning:
        m_value.valueID = CSSValueAuto;
        return;
    case FontDescription::NormalKerning:
        m_value.valueID = CSSValueNormal;
        return;
    case FontDescription::NoneKerning:
        m_value.valueID = CSSValueNone;
        return;
    }

    ASSERT_NOT_REACHED();
    m_value.valueID = CSSValueAuto;
}

template<> inline FontDescription::Kerning CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return FontDescription::AutoKerning;
    case CSSValueNormal:
        return FontDescription::NormalKerning;
    case CSSValueNone:
        return FontDescription::NoneKerning;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return FontDescription::AutoKerning;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ObjectFit fit)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (fit) {
    case ObjectFitFill:
        m_value.valueID = CSSValueFill;
        break;
    case ObjectFitContain:
        m_value.valueID = CSSValueContain;
        break;
    case ObjectFitCover:
        m_value.valueID = CSSValueCover;
        break;
    case ObjectFitNone:
        m_value.valueID = CSSValueNone;
        break;
    case ObjectFitScaleDown:
        m_value.valueID = CSSValueScaleDown;
        break;
    }
}

template<> inline ObjectFit CSSPrimitiveValue::convertTo() const
{
    switch (m_value.valueID) {
    case CSSValueFill:
        return ObjectFitFill;
    case CSSValueContain:
        return ObjectFitContain;
    case CSSValueCover:
        return ObjectFitCover;
    case CSSValueNone:
        return ObjectFitNone;
    case CSSValueScaleDown:
        return ObjectFitScaleDown;
    default:
        ASSERT_NOT_REACHED();
        return ObjectFitFill;
    }
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EFillSizeType fillSize)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (fillSize) {
    case Contain:
        m_value.valueID = CSSValueContain;
        break;
    case Cover:
        m_value.valueID = CSSValueCover;
        break;
    case SizeNone:
        m_value.valueID = CSSValueNone;
        break;
    case SizeLength:
    default:
        ASSERT_NOT_REACHED();
    }
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(FontSmoothingMode smoothing)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (smoothing) {
    case AutoSmoothing:
        m_value.valueID = CSSValueAuto;
        return;
    case NoSmoothing:
        m_value.valueID = CSSValueNone;
        return;
    case Antialiased:
        m_value.valueID = CSSValueAntialiased;
        return;
    case SubpixelAntialiased:
        m_value.valueID = CSSValueSubpixelAntialiased;
        return;
    }

    ASSERT_NOT_REACHED();
    m_value.valueID = CSSValueAuto;
}

template<> inline FontSmoothingMode CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return AutoSmoothing;
    case CSSValueNone:
        return NoSmoothing;
    case CSSValueAntialiased:
        return Antialiased;
    case CSSValueSubpixelAntialiased:
        return SubpixelAntialiased;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return AutoSmoothing;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(FontWeight weight)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (weight) {
    case FontWeight900:
        m_value.valueID = CSSValue900;
        return;
    case FontWeight800:
        m_value.valueID = CSSValue800;
        return;
    case FontWeight700:
        m_value.valueID = CSSValueBold;
        return;
    case FontWeight600:
        m_value.valueID = CSSValue600;
        return;
    case FontWeight500:
        m_value.valueID = CSSValue500;
        return;
    case FontWeight400:
        m_value.valueID = CSSValueNormal;
        return;
    case FontWeight300:
        m_value.valueID = CSSValue300;
        return;
    case FontWeight200:
        m_value.valueID = CSSValue200;
        return;
    case FontWeight100:
        m_value.valueID = CSSValue100;
        return;
    }

    ASSERT_NOT_REACHED();
    m_value.valueID = CSSValueNormal;
}

template<> inline FontWeight CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueBold:
        return FontWeightBold;
    case CSSValueNormal:
        return FontWeightNormal;
    case CSSValue900:
        return FontWeight900;
    case CSSValue800:
        return FontWeight800;
    case CSSValue700:
        return FontWeight700;
    case CSSValue600:
        return FontWeight600;
    case CSSValue500:
        return FontWeight500;
    case CSSValue400:
        return FontWeight400;
    case CSSValue300:
        return FontWeight300;
    case CSSValue200:
        return FontWeight200;
    case CSSValue100:
        return FontWeight100;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return FontWeightNormal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(FontStyle italic)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (italic) {
    case FontStyleNormal:
        m_value.valueID = CSSValueNormal;
        return;
    case FontStyleOblique:
        m_value.valueID = CSSValueOblique;
        return;
    case FontStyleItalic:
        m_value.valueID = CSSValueItalic;
        return;
    }

    ASSERT_NOT_REACHED();
    m_value.valueID = CSSValueNormal;
}

template<> inline FontStyle CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueOblique:
        return FontStyleOblique;
    case CSSValueItalic:
        return FontStyleItalic;
    case CSSValueNormal:
        return FontStyleNormal;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return FontStyleNormal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(FontStretch stretch)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (stretch) {
    case FontStretchUltraCondensed:
        m_value.valueID = CSSValueUltraCondensed;
        return;
    case FontStretchExtraCondensed:
        m_value.valueID = CSSValueExtraCondensed;
        return;
    case FontStretchCondensed:
        m_value.valueID = CSSValueCondensed;
        return;
    case FontStretchSemiCondensed:
        m_value.valueID = CSSValueSemiCondensed;
        return;
    case FontStretchNormal:
        m_value.valueID = CSSValueNormal;
        return;
    case FontStretchSemiExpanded:
        m_value.valueID = CSSValueSemiExpanded;
        return;
    case FontStretchExpanded:
        m_value.valueID = CSSValueExpanded;
        return;
    case FontStretchExtraExpanded:
        m_value.valueID = CSSValueExtraExpanded;
        return;
    case FontStretchUltraExpanded:
        m_value.valueID = CSSValueUltraExpanded;
        return;
    }

    ASSERT_NOT_REACHED();
    m_value.valueID = CSSValueNormal;
}

template<> inline FontStretch CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueUltraCondensed:
        return FontStretchUltraCondensed;
    case CSSValueExtraCondensed:
        return FontStretchExtraCondensed;
    case CSSValueCondensed:
        return FontStretchCondensed;
    case CSSValueSemiCondensed:
        return FontStretchSemiCondensed;
    case CSSValueNormal:
        return FontStretchNormal;
    case CSSValueSemiExpanded:
        return FontStretchSemiExpanded;
    case CSSValueExpanded:
        return FontStretchExpanded;
    case CSSValueExtraExpanded:
        return FontStretchExtraExpanded;
    case CSSValueUltraExpanded:
        return FontStretchUltraExpanded;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return FontStretchNormal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextRenderingMode e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case AutoTextRendering:
        m_value.valueID = CSSValueAuto;
        break;
    case OptimizeSpeed:
        m_value.valueID = CSSValueOptimizeSpeed;
        break;
    case OptimizeLegibility:
        m_value.valueID = CSSValueOptimizeLegibility;
        break;
    case GeometricPrecision:
        m_value.valueID = CSSValueGeometricPrecision;
        break;
    }
}

template<> inline TextRenderingMode CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return AutoTextRendering;
    case CSSValueOptimizeSpeed:
        return OptimizeSpeed;
    case CSSValueOptimizeLegibility:
        return OptimizeLegibility;
    case CSSValueGeometricPrecision:
        return GeometricPrecision;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return AutoTextRendering;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ESpeak e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case SpeakNone:
        m_value.valueID = CSSValueNone;
        break;
    case SpeakNormal:
        m_value.valueID = CSSValueNormal;
        break;
    case SpeakSpellOut:
        m_value.valueID = CSSValueSpellOut;
        break;
    case SpeakDigits:
        m_value.valueID = CSSValueDigits;
        break;
    case SpeakLiteralPunctuation:
        m_value.valueID = CSSValueLiteralPunctuation;
        break;
    case SpeakNoPunctuation:
        m_value.valueID = CSSValueNoPunctuation;
        break;
    }
}

template<> inline Order CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueLogical:
        return LogicalOrder;
    case CSSValueVisual:
        return VisualOrder;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return LogicalOrder;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(Order e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case LogicalOrder:
        m_value.valueID = CSSValueLogical;
        break;
    case VisualOrder:
        m_value.valueID = CSSValueVisual;
        break;
    }
}

template<> inline ESpeak CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNone:
        return SpeakNone;
    case CSSValueNormal:
        return SpeakNormal;
    case CSSValueSpellOut:
        return SpeakSpellOut;
    case CSSValueDigits:
        return SpeakDigits;
    case CSSValueLiteralPunctuation:
        return SpeakLiteralPunctuation;
    case CSSValueNoPunctuation:
        return SpeakNoPunctuation;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return SpeakNormal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(WebBlendMode blendMode)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (blendMode) {
    case WebBlendModeNormal:
        m_value.valueID = CSSValueNormal;
        break;
    case WebBlendModeMultiply:
        m_value.valueID = CSSValueMultiply;
        break;
    case WebBlendModeScreen:
        m_value.valueID = CSSValueScreen;
        break;
    case WebBlendModeOverlay:
        m_value.valueID = CSSValueOverlay;
        break;
    case WebBlendModeDarken:
        m_value.valueID = CSSValueDarken;
        break;
    case WebBlendModeLighten:
        m_value.valueID = CSSValueLighten;
        break;
    case WebBlendModeColorDodge:
        m_value.valueID = CSSValueColorDodge;
        break;
    case WebBlendModeColorBurn:
        m_value.valueID = CSSValueColorBurn;
        break;
    case WebBlendModeHardLight:
        m_value.valueID = CSSValueHardLight;
        break;
    case WebBlendModeSoftLight:
        m_value.valueID = CSSValueSoftLight;
        break;
    case WebBlendModeDifference:
        m_value.valueID = CSSValueDifference;
        break;
    case WebBlendModeExclusion:
        m_value.valueID = CSSValueExclusion;
        break;
    case WebBlendModeHue:
        m_value.valueID = CSSValueHue;
        break;
    case WebBlendModeSaturation:
        m_value.valueID = CSSValueSaturation;
        break;
    case WebBlendModeColor:
        m_value.valueID = CSSValueColor;
        break;
    case WebBlendModeLuminosity:
        m_value.valueID = CSSValueLuminosity;
        break;
    }
}

template<> inline WebBlendMode CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNormal:
        return WebBlendModeNormal;
    case CSSValueMultiply:
        return WebBlendModeMultiply;
    case CSSValueScreen:
        return WebBlendModeScreen;
    case CSSValueOverlay:
        return WebBlendModeOverlay;
    case CSSValueDarken:
        return WebBlendModeDarken;
    case CSSValueLighten:
        return WebBlendModeLighten;
    case CSSValueColorDodge:
        return WebBlendModeColorDodge;
    case CSSValueColorBurn:
        return WebBlendModeColorBurn;
    case CSSValueHardLight:
        return WebBlendModeHardLight;
    case CSSValueSoftLight:
        return WebBlendModeSoftLight;
    case CSSValueDifference:
        return WebBlendModeDifference;
    case CSSValueExclusion:
        return WebBlendModeExclusion;
    case CSSValueHue:
        return WebBlendModeHue;
    case CSSValueSaturation:
        return WebBlendModeSaturation;
    case CSSValueColor:
        return WebBlendModeColor;
    case CSSValueLuminosity:
        return WebBlendModeLuminosity;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return WebBlendModeNormal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(LineCap e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case ButtCap:
        m_value.valueID = CSSValueButt;
        break;
    case RoundCap:
        m_value.valueID = CSSValueRound;
        break;
    case SquareCap:
        m_value.valueID = CSSValueSquare;
        break;
    }
}

template<> inline LineCap CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueButt:
        return ButtCap;
    case CSSValueRound:
        return RoundCap;
    case CSSValueSquare:
        return SquareCap;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return ButtCap;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(LineJoin e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case MiterJoin:
        m_value.valueID = CSSValueMiter;
        break;
    case RoundJoin:
        m_value.valueID = CSSValueRound;
        break;
    case BevelJoin:
        m_value.valueID = CSSValueBevel;
        break;
    }
}

template<> inline LineJoin CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueMiter:
        return MiterJoin;
    case CSSValueRound:
        return RoundJoin;
    case CSSValueBevel:
        return BevelJoin;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return MiterJoin;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(WindRule e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case RULE_NONZERO:
        m_value.valueID = CSSValueNonzero;
        break;
    case RULE_EVENODD:
        m_value.valueID = CSSValueEvenodd;
        break;
    }
}

template<> inline WindRule CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNonzero:
        return RULE_NONZERO;
    case CSSValueEvenodd:
        return RULE_EVENODD;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return RULE_NONZERO;
}


template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EAlignmentBaseline e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case AB_AUTO:
        m_value.valueID = CSSValueAuto;
        break;
    case AB_BASELINE:
        m_value.valueID = CSSValueBaseline;
        break;
    case AB_BEFORE_EDGE:
        m_value.valueID = CSSValueBeforeEdge;
        break;
    case AB_TEXT_BEFORE_EDGE:
        m_value.valueID = CSSValueTextBeforeEdge;
        break;
    case AB_MIDDLE:
        m_value.valueID = CSSValueMiddle;
        break;
    case AB_CENTRAL:
        m_value.valueID = CSSValueCentral;
        break;
    case AB_AFTER_EDGE:
        m_value.valueID = CSSValueAfterEdge;
        break;
    case AB_TEXT_AFTER_EDGE:
        m_value.valueID = CSSValueTextAfterEdge;
        break;
    case AB_IDEOGRAPHIC:
        m_value.valueID = CSSValueIdeographic;
        break;
    case AB_ALPHABETIC:
        m_value.valueID = CSSValueAlphabetic;
        break;
    case AB_HANGING:
        m_value.valueID = CSSValueHanging;
        break;
    case AB_MATHEMATICAL:
        m_value.valueID = CSSValueMathematical;
        break;
    }
}

template<> inline EAlignmentBaseline CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return AB_AUTO;
    case CSSValueBaseline:
        return AB_BASELINE;
    case CSSValueBeforeEdge:
        return AB_BEFORE_EDGE;
    case CSSValueTextBeforeEdge:
        return AB_TEXT_BEFORE_EDGE;
    case CSSValueMiddle:
        return AB_MIDDLE;
    case CSSValueCentral:
        return AB_CENTRAL;
    case CSSValueAfterEdge:
        return AB_AFTER_EDGE;
    case CSSValueTextAfterEdge:
        return AB_TEXT_AFTER_EDGE;
    case CSSValueIdeographic:
        return AB_IDEOGRAPHIC;
    case CSSValueAlphabetic:
        return AB_ALPHABETIC;
    case CSSValueHanging:
        return AB_HANGING;
    case CSSValueMathematical:
        return AB_MATHEMATICAL;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return AB_AUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBorderCollapse e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case BorderCollapseSeparate:
        m_value.valueID = CSSValueSeparate;
        break;
    case BorderCollapseCollapse:
        m_value.valueID = CSSValueCollapse;
        break;
    }
}

template<> inline EBorderCollapse CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueSeparate:
        return BorderCollapseSeparate;
    case CSSValueCollapse:
        return BorderCollapseCollapse;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BorderCollapseSeparate;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EImageRendering e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case ImageRenderingAuto:
        m_value.valueID = CSSValueAuto;
        break;
    case ImageRenderingOptimizeSpeed:
        m_value.valueID = CSSValueOptimizeSpeed;
        break;
    case ImageRenderingOptimizeQuality:
        m_value.valueID = CSSValueOptimizeQuality;
        break;
    case ImageRenderingPixelated:
        m_value.valueID = CSSValuePixelated;
        break;
    case ImageRenderingOptimizeContrast:
        m_value.valueID = CSSValueWebkitOptimizeContrast;
        break;
    }
}

template<> inline EImageRendering CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return ImageRenderingAuto;
    case CSSValueOptimizeSpeed:
        return ImageRenderingOptimizeSpeed;
    case CSSValueOptimizeQuality:
        return ImageRenderingOptimizeQuality;
    case CSSValuePixelated:
        return ImageRenderingPixelated;
    case CSSValueWebkitOptimizeContrast:
        return ImageRenderingOptimizeContrast;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return ImageRenderingAuto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ETransformStyle3D e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case TransformStyle3DFlat:
        m_value.valueID = CSSValueFlat;
        break;
    case TransformStyle3DPreserve3D:
        m_value.valueID = CSSValuePreserve3d;
        break;
    }
}

template<> inline ETransformStyle3D CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueFlat:
        return TransformStyle3DFlat;
    case CSSValuePreserve3d:
        return TransformStyle3DPreserve3D;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TransformStyle3DFlat;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBufferedRendering e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case BR_AUTO:
        m_value.valueID = CSSValueAuto;
        break;
    case BR_DYNAMIC:
        m_value.valueID = CSSValueDynamic;
        break;
    case BR_STATIC:
        m_value.valueID = CSSValueStatic;
        break;
    }
}

template<> inline EBufferedRendering CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return BR_AUTO;
    case CSSValueDynamic:
        return BR_DYNAMIC;
    case CSSValueStatic:
        return BR_STATIC;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return BR_AUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EColorInterpolation e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case CI_AUTO:
        m_value.valueID = CSSValueAuto;
        break;
    case CI_SRGB:
        m_value.valueID = CSSValueSRGB;
        break;
    case CI_LINEARRGB:
        m_value.valueID = CSSValueLinearRGB;
        break;
    }
}

template<> inline EColorInterpolation CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueSRGB:
        return CI_SRGB;
    case CSSValueLinearRGB:
        return CI_LINEARRGB;
    case CSSValueAuto:
        return CI_AUTO;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return CI_AUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EColorRendering e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case CR_AUTO:
        m_value.valueID = CSSValueAuto;
        break;
    case CR_OPTIMIZESPEED:
        m_value.valueID = CSSValueOptimizeSpeed;
        break;
    case CR_OPTIMIZEQUALITY:
        m_value.valueID = CSSValueOptimizeQuality;
        break;
    }
}

template<> inline EColorRendering CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueOptimizeSpeed:
        return CR_OPTIMIZESPEED;
    case CSSValueOptimizeQuality:
        return CR_OPTIMIZEQUALITY;
    case CSSValueAuto:
        return CR_AUTO;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return CR_AUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EDominantBaseline e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case DB_AUTO:
        m_value.valueID = CSSValueAuto;
        break;
    case DB_USE_SCRIPT:
        m_value.valueID = CSSValueUseScript;
        break;
    case DB_NO_CHANGE:
        m_value.valueID = CSSValueNoChange;
        break;
    case DB_RESET_SIZE:
        m_value.valueID = CSSValueResetSize;
        break;
    case DB_CENTRAL:
        m_value.valueID = CSSValueCentral;
        break;
    case DB_MIDDLE:
        m_value.valueID = CSSValueMiddle;
        break;
    case DB_TEXT_BEFORE_EDGE:
        m_value.valueID = CSSValueTextBeforeEdge;
        break;
    case DB_TEXT_AFTER_EDGE:
        m_value.valueID = CSSValueTextAfterEdge;
        break;
    case DB_IDEOGRAPHIC:
        m_value.valueID = CSSValueIdeographic;
        break;
    case DB_ALPHABETIC:
        m_value.valueID = CSSValueAlphabetic;
        break;
    case DB_HANGING:
        m_value.valueID = CSSValueHanging;
        break;
    case DB_MATHEMATICAL:
        m_value.valueID = CSSValueMathematical;
        break;
    }
}

template<> inline EDominantBaseline CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return DB_AUTO;
    case CSSValueUseScript:
        return DB_USE_SCRIPT;
    case CSSValueNoChange:
        return DB_NO_CHANGE;
    case CSSValueResetSize:
        return DB_RESET_SIZE;
    case CSSValueIdeographic:
        return DB_IDEOGRAPHIC;
    case CSSValueAlphabetic:
        return DB_ALPHABETIC;
    case CSSValueHanging:
        return DB_HANGING;
    case CSSValueMathematical:
        return DB_MATHEMATICAL;
    case CSSValueCentral:
        return DB_CENTRAL;
    case CSSValueMiddle:
        return DB_MIDDLE;
    case CSSValueTextAfterEdge:
        return DB_TEXT_AFTER_EDGE;
    case CSSValueTextBeforeEdge:
        return DB_TEXT_BEFORE_EDGE;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return DB_AUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EShapeRendering e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case SR_AUTO:
        m_value.valueID = CSSValueAuto;
        break;
    case SR_OPTIMIZESPEED:
        m_value.valueID = CSSValueOptimizeSpeed;
        break;
    case SR_CRISPEDGES:
        m_value.valueID = CSSValueCrispEdges;
        break;
    case SR_GEOMETRICPRECISION:
        m_value.valueID = CSSValueGeometricPrecision;
        break;
    }
}

template<> inline EShapeRendering CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return SR_AUTO;
    case CSSValueOptimizeSpeed:
        return SR_OPTIMIZESPEED;
    case CSSValueCrispEdges:
        return SR_CRISPEDGES;
    case CSSValueGeometricPrecision:
        return SR_GEOMETRICPRECISION;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return SR_AUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ETextAnchor e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case TA_START:
        m_value.valueID = CSSValueStart;
        break;
    case TA_MIDDLE:
        m_value.valueID = CSSValueMiddle;
        break;
    case TA_END:
        m_value.valueID = CSSValueEnd;
        break;
    }
}

template<> inline ETextAnchor CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueStart:
        return TA_START;
    case CSSValueMiddle:
        return TA_MIDDLE;
    case CSSValueEnd:
        return TA_END;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TA_START;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EVectorEffect e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case VE_NONE:
        m_value.valueID = CSSValueNone;
        break;
    case VE_NON_SCALING_STROKE:
        m_value.valueID = CSSValueNonScalingStroke;
        break;
    }
}

template<> inline EVectorEffect CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNone:
        return VE_NONE;
    case CSSValueNonScalingStroke:
        return VE_NON_SCALING_STROKE;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return VE_NONE;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EPaintOrderType e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case PT_FILL:
        m_value.valueID = CSSValueFill;
        break;
    case PT_STROKE:
        m_value.valueID = CSSValueStroke;
        break;
    case PT_MARKERS:
        m_value.valueID = CSSValueMarkers;
        break;
    default:
        ASSERT_NOT_REACHED();
        m_value.valueID = CSSValueFill;
        break;
    }
}

template<> inline EPaintOrderType CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueFill:
        return PT_FILL;
    case CSSValueStroke:
        return PT_STROKE;
    case CSSValueMarkers:
        return PT_MARKERS;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return PT_NONE;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EMaskType e)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (e) {
    case MT_LUMINANCE:
        m_value.valueID = CSSValueLuminance;
        break;
    case MT_ALPHA:
        m_value.valueID = CSSValueAlpha;
        break;
    }
}

template<> inline EMaskType CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueLuminance:
        return MT_LUMINANCE;
    case CSSValueAlpha:
        return MT_ALPHA;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return MT_LUMINANCE;
}

template<> inline TouchAction CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueNone:
        return TouchActionNone;
    case CSSValueAuto:
        return TouchActionAuto;
    case CSSValuePanLeft:
        return TouchActionPanLeft;
    case CSSValuePanRight:
        return TouchActionPanRight;
    case CSSValuePanX:
        return TouchActionPanX;
    case CSSValuePanUp:
        return TouchActionPanUp;
    case CSSValuePanDown:
        return TouchActionPanDown;
    case CSSValuePanY:
        return TouchActionPanY;
    case CSSValueManipulation:
        return TouchActionManipulation;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return TouchActionNone;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EIsolation i)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (i) {
    case IsolationAuto:
        m_value.valueID = CSSValueAuto;
        break;
    case IsolationIsolate:
        m_value.valueID = CSSValueIsolate;
        break;
    }
}

template<> inline EIsolation CSSPrimitiveValue::convertTo() const
{
    ASSERT(isValueID());
    switch (m_value.valueID) {
    case CSSValueAuto:
        return IsolationAuto;
    case CSSValueIsolate:
        return IsolationIsolate;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return IsolationAuto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(CSSBoxType cssBox)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (cssBox) {
    case MarginBox:
        m_value.valueID = CSSValueMarginBox;
        break;
    case BorderBox:
        m_value.valueID = CSSValueBorderBox;
        break;
    case PaddingBox:
        m_value.valueID = CSSValuePaddingBox;
        break;
    case ContentBox:
        m_value.valueID = CSSValueContentBox;
        break;
    case BoxMissing:
        // The missing box should convert to a null primitive value.
        ASSERT_NOT_REACHED();
    }
}

template<> inline CSSBoxType CSSPrimitiveValue::convertTo() const
{
    switch (getValueID()) {
    case CSSValueMarginBox:
        return MarginBox;
    case CSSValueBorderBox:
        return BorderBox;
    case CSSValuePaddingBox:
        return PaddingBox;
    case CSSValueContentBox:
        return ContentBox;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return ContentBox;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ItemPosition itemPosition)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (itemPosition) {
    case ItemPositionAuto:
        m_value.valueID = CSSValueAuto;
        break;
    case ItemPositionStretch:
        m_value.valueID = CSSValueStretch;
        break;
    case ItemPositionBaseline:
        m_value.valueID = CSSValueBaseline;
        break;
    case ItemPositionLastBaseline:
        m_value.valueID = CSSValueLastBaseline;
        break;
    case ItemPositionCenter:
        m_value.valueID = CSSValueCenter;
        break;
    case ItemPositionStart:
        m_value.valueID = CSSValueStart;
        break;
    case ItemPositionEnd:
        m_value.valueID = CSSValueEnd;
        break;
    case ItemPositionSelfStart:
        m_value.valueID = CSSValueSelfStart;
        break;
    case ItemPositionSelfEnd:
        m_value.valueID = CSSValueSelfEnd;
        break;
    case ItemPositionFlexStart:
        m_value.valueID = CSSValueFlexStart;
        break;
    case ItemPositionFlexEnd:
        m_value.valueID = CSSValueFlexEnd;
        break;
    case ItemPositionLeft:
        m_value.valueID = CSSValueLeft;
        break;
    case ItemPositionRight:
        m_value.valueID = CSSValueRight;
        break;
    }
}

template<> inline ItemPosition CSSPrimitiveValue::convertTo() const
{
    switch (m_value.valueID) {
    case CSSValueAuto:
        return ItemPositionAuto;
    case CSSValueStretch:
        return ItemPositionStretch;
    case CSSValueBaseline:
        return ItemPositionBaseline;
    case CSSValueLastBaseline:
        return ItemPositionLastBaseline;
    case CSSValueCenter:
        return ItemPositionCenter;
    case CSSValueStart:
        return ItemPositionStart;
    case CSSValueEnd:
        return ItemPositionEnd;
    case CSSValueSelfStart:
        return ItemPositionSelfStart;
    case CSSValueSelfEnd:
        return ItemPositionSelfEnd;
    case CSSValueFlexStart:
        return ItemPositionFlexStart;
    case CSSValueFlexEnd:
        return ItemPositionFlexEnd;
    case CSSValueLeft:
        return ItemPositionLeft;
    case CSSValueRight:
        return ItemPositionRight;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return ItemPositionAuto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ContentPosition contentPosition)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (contentPosition) {
    case ContentPositionNormal:
        m_value.valueID = CSSValueNormal;
        break;
    case ContentPositionBaseline:
        m_value.valueID = CSSValueBaseline;
        break;
    case ContentPositionLastBaseline:
        m_value.valueID = CSSValueLastBaseline;
        break;
    case ContentPositionCenter:
        m_value.valueID = CSSValueCenter;
        break;
    case ContentPositionStart:
        m_value.valueID = CSSValueStart;
        break;
    case ContentPositionEnd:
        m_value.valueID = CSSValueEnd;
        break;
    case ContentPositionFlexStart:
        m_value.valueID = CSSValueFlexStart;
        break;
    case ContentPositionFlexEnd:
        m_value.valueID = CSSValueFlexEnd;
        break;
    case ContentPositionLeft:
        m_value.valueID = CSSValueLeft;
        break;
    case ContentPositionRight:
        m_value.valueID = CSSValueRight;
        break;
    }
}

template<> inline ContentPosition CSSPrimitiveValue::convertTo() const
{
    switch (m_value.valueID) {
    case CSSValueNormal:
        return ContentPositionNormal;
    case CSSValueBaseline:
        return ContentPositionBaseline;
    case CSSValueLastBaseline:
        return ContentPositionLastBaseline;
    case CSSValueCenter:
        return ContentPositionCenter;
    case CSSValueStart:
        return ContentPositionStart;
    case CSSValueEnd:
        return ContentPositionEnd;
    case CSSValueFlexStart:
        return ContentPositionFlexStart;
    case CSSValueFlexEnd:
        return ContentPositionFlexEnd;
    case CSSValueLeft:
        return ContentPositionLeft;
    case CSSValueRight:
        return ContentPositionRight;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return ContentPositionNormal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ContentDistributionType contentDistribution)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (contentDistribution) {
    case ContentDistributionDefault:
        m_value.valueID = CSSValueDefault;
        break;
    case ContentDistributionSpaceBetween:
        m_value.valueID = CSSValueSpaceBetween;
        break;
    case ContentDistributionSpaceAround:
        m_value.valueID = CSSValueSpaceAround;
        break;
    case ContentDistributionSpaceEvenly:
        m_value.valueID = CSSValueSpaceEvenly;
        break;
    case ContentDistributionStretch:
        m_value.valueID = CSSValueStretch;
        break;
    }
}

template<> inline ContentDistributionType CSSPrimitiveValue::convertTo() const
{
    switch (m_value.valueID) {
    case CSSValueSpaceBetween:
        return ContentDistributionSpaceBetween;
    case CSSValueSpaceAround:
        return ContentDistributionSpaceAround;
    case CSSValueSpaceEvenly:
        return ContentDistributionSpaceEvenly;
    case CSSValueStretch:
        return ContentDistributionStretch;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return ContentDistributionStretch;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(OverflowAlignment overflowAlignment)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (overflowAlignment) {
    case OverflowAlignmentDefault:
        m_value.valueID = CSSValueDefault;
        break;
    case OverflowAlignmentUnsafe:
        m_value.valueID = CSSValueUnsafe;
        break;
    case OverflowAlignmentSafe:
        m_value.valueID = CSSValueSafe;
        break;
    }
}

template<> inline OverflowAlignment CSSPrimitiveValue::convertTo() const
{
    switch (m_value.valueID) {
    case CSSValueUnsafe:
        return OverflowAlignmentUnsafe;
    case CSSValueSafe:
        return OverflowAlignmentSafe;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return OverflowAlignmentUnsafe;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ScrollBehavior behavior)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (behavior) {
    case ScrollBehaviorAuto:
        m_value.valueID = CSSValueAuto;
        break;
    case ScrollBehaviorSmooth:
        m_value.valueID = CSSValueSmooth;
        break;
    case ScrollBehaviorInstant:
        // Behavior 'instant' is only allowed in ScrollOptions arguments passed to
        // CSSOM scroll APIs.
        ASSERT_NOT_REACHED();
    }
}

template<> inline ScrollBehavior CSSPrimitiveValue::convertTo() const
{
    switch (getValueID()) {
    case CSSValueAuto:
        return ScrollBehaviorAuto;
    case CSSValueSmooth:
        return ScrollBehaviorSmooth;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return ScrollBehaviorAuto;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ScrollSnapType snapType)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (snapType) {
    case ScrollSnapTypeNone:
        m_value.valueID = CSSValueNone;
        break;
    case ScrollSnapTypeMandatory:
        m_value.valueID = CSSValueMandatory;
        break;
    case ScrollSnapTypeProximity:
        m_value.valueID = CSSValueProximity;
        break;
    }
}

template<> inline ScrollSnapType CSSPrimitiveValue::convertTo() const
{
    switch (getValueID()) {
    case CSSValueNone:
        return ScrollSnapTypeNone;
    case CSSValueMandatory:
        return ScrollSnapTypeMandatory;
    case CSSValueProximity:
        return ScrollSnapTypeProximity;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return ScrollSnapTypeNone;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(Containment snapType)
    : CSSValue(PrimitiveClass)
{
    init(UnitType::ValueID);
    switch (snapType) {
    case ContainsNone:
        m_value.valueID = CSSValueNone;
        break;
    case ContainsStrict:
        m_value.valueID = CSSValueStrict;
        break;
    case ContainsContent:
        m_value.valueID = CSSValueContent;
        break;
    case ContainsPaint:
        m_value.valueID = CSSValuePaint;
        break;
    case ContainsStyle:
        m_value.valueID = CSSValueStyle;
        break;
    case ContainsLayout:
        m_value.valueID = CSSValueLayout;
        break;
    case ContainsSize:
        m_value.valueID = CSSValueSize;
        break;
    }
}

template<> inline Containment CSSPrimitiveValue::convertTo() const
{
    switch (getValueID()) {
    case CSSValueNone:
        return ContainsNone;
    case CSSValueStrict:
        return ContainsStrict;
    case CSSValueContent:
        return ContainsContent;
    case CSSValuePaint:
        return ContainsPaint;
    case CSSValueStyle:
        return ContainsStyle;
    case CSSValueLayout:
        return ContainsLayout;
    case CSSValueSize:
        return ContainsSize;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return ContainsNone;
}

} // namespace blink

#endif

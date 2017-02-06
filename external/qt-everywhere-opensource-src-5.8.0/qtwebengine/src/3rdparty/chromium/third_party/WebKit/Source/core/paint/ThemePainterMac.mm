/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Google, Inc.
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

#import "core/paint/ThemePainterMac.h"

#import "core/layout/LayoutProgress.h"
#import "core/layout/LayoutThemeMac.h"
#import "core/layout/LayoutView.h"
#import "core/paint/PaintInfo.h"
#import "platform/geometry/FloatRoundedRect.h"
#import "platform/graphics/BitmapImage.h"
#import "platform/graphics/GraphicsContextStateSaver.h"
#import "platform/graphics/Image.h"
#import "platform/graphics/ImageBuffer.h"
#import "platform/mac/ColorMac.h"
#import "platform/mac/LocalCurrentGraphicsContext.h"
#import "platform/mac/ThemeMac.h"
#import "platform/mac/WebCoreNSCellExtras.h"
#if defined(USE_APPSTORE_COMPLIANT_CODE)
#include "platform/LayoutTestSupport.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThemeEngine.h"
#endif
#import <AvailabilityMacros.h>
#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <math.h>

// The methods in this file are specific to the Mac OS X platform.

// Forward declare Mac SPIs.
#ifndef USE_APPSTORE_COMPLIANT_CODE
extern "C" {
void _NSDrawCarbonThemeBezel(NSRect frame, BOOL enabled, BOOL flipped);
// Request for public API: rdar://13787640
void _NSDrawCarbonThemeListBox(NSRect frame, BOOL enabled, BOOL flipped, BOOL always_yes);
}
#endif

namespace blink {

#if defined(USE_APPSTORE_COMPLIANT_CODE)
bool useMockTheme()
{
    return LayoutTestSupport::isMockThemeEnabledForTest();
}

WebThemeEngine::State getWebThemeState(const LayoutObject& o)
{
    if (!LayoutTheme::isEnabled(o))
        return WebThemeEngine::StateDisabled;
    if (useMockTheme() && LayoutTheme::isReadOnlyControl(o))
        return WebThemeEngine::StateReadonly;
    if (LayoutTheme::isPressed(o))
        return WebThemeEngine::StatePressed;
    if (useMockTheme() && LayoutTheme::isFocused(o))
        return WebThemeEngine::StateFocused;
    if (LayoutTheme::isHovered(o))
        return WebThemeEngine::StateHover;

    return WebThemeEngine::StateNormal;
}
#endif

ThemePainterMac::ThemePainterMac(LayoutThemeMac& layoutTheme, Theme* platformTheme)
    : ThemePainter(platformTheme)
    , m_layoutTheme(layoutTheme)
{
}

bool ThemePainterMac::paintTextField(const LayoutObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context, r);

#if __MAC_OS_X_VERSION_MIN_REQUIRED <= 1070
    bool useNSTextFieldCell = o.styleRef().hasAppearance()
        && o.styleRef().visitedDependentColor(CSSPropertyBackgroundColor) == Color::white
        && !o.styleRef().hasBackgroundImage();

    // We do not use NSTextFieldCell to draw styled text fields on Lion and
    // SnowLeopard because there are a number of bugs on those platforms that
    // require NSTextFieldCell to be in charge of painting its own
    // background. We need WebCore to paint styled backgrounds, so we'll use
    // this AppKit SPI function instead.
    if (!useNSTextFieldCell) {
#ifndef USE_APPSTORE_COMPLIANT_CODE
        _NSDrawCarbonThemeBezel(r, LayoutTheme::isEnabled(o) && !LayoutTheme::isReadOnlyControl(o), YES);
        return false;
#endif
    }
#endif

    NSTextFieldCell *textField = m_layoutTheme.textField();

    GraphicsContextStateSaver stateSaver(paintInfo.context);

    [textField setEnabled:(LayoutTheme::isEnabled(o) && !LayoutTheme::isReadOnlyControl(o))];
    [textField drawWithFrame:NSRect(r) inView:m_layoutTheme.documentViewFor(o)];

    [textField setControlView:nil];

    return false;
}

bool ThemePainterMac::paintCapsLockIndicator(const LayoutObject&, const PaintInfo& paintInfo, const IntRect& r)
{
    // This draws the caps lock indicator as it was done by
    // WKDrawCapsLockIndicator.
    LocalCurrentGraphicsContext localContext(paintInfo.context, r);
    CGContextRef c = localContext.cgContext();
    CGMutablePathRef shape = CGPathCreateMutable();

    // To draw the caps lock indicator, draw the shape into a small
    // square that is then scaled to the size of r.
    const CGFloat kSquareSize = 17;

    // Create a rounted square shape.
    CGPathMoveToPoint(shape, NULL, 16.5, 4.5);
    CGPathAddArc(shape, NULL, 12.5, 12.5, 4, 0,        M_PI_2,   false);
    CGPathAddArc(shape, NULL, 4.5,  12.5, 4, M_PI_2,   M_PI,     false);
    CGPathAddArc(shape, NULL, 4.5,  4.5,  4, M_PI,     3*M_PI/2, false);
    CGPathAddArc(shape, NULL, 12.5, 4.5,  4, 3*M_PI/2, 0,        false);

    // Draw the arrow - note this is drawing in a flipped coordinate system, so
    // the arrow is pointing down.
    CGPathMoveToPoint(shape, NULL, 8.5, 2);  // Tip point.
    CGPathAddLineToPoint(shape, NULL, 4,     7);
    CGPathAddLineToPoint(shape, NULL, 6.25,  7);
    CGPathAddLineToPoint(shape, NULL, 6.25,  10.25);
    CGPathAddLineToPoint(shape, NULL, 10.75, 10.25);
    CGPathAddLineToPoint(shape, NULL, 10.75, 7);
    CGPathAddLineToPoint(shape, NULL, 13,    7);
    CGPathAddLineToPoint(shape, NULL, 8.5,   2);

    // Draw the rectangle that underneath (or above in the flipped system) the
    // arrow.
    CGPathAddLineToPoint(shape, NULL, 10.75, 12);
    CGPathAddLineToPoint(shape, NULL, 6.25,  12);
    CGPathAddLineToPoint(shape, NULL, 6.25,  14.25);
    CGPathAddLineToPoint(shape, NULL, 10.75, 14.25);
    CGPathAddLineToPoint(shape, NULL, 10.75, 12);

    // Scale and translate the shape.
    CGRect cgr = r;
    CGFloat maxX = CGRectGetMaxX(cgr);
    CGFloat minY = CGRectGetMinY(cgr);
    CGFloat heightScale = r.height() / kSquareSize;
    CGAffineTransform transform = CGAffineTransformMake(
        heightScale, 0,  // A  B
        0, heightScale,  // C  D
        maxX - r.height(), minY);  // Tx Ty

    CGMutablePathRef paintPath = CGPathCreateMutable();
    CGPathAddPath(paintPath, &transform, shape);
    CGPathRelease(shape);

    CGContextSetRGBFillColor(c, 0, 0, 0, 0.4);
    CGContextBeginPath(c);
    CGContextAddPath(c, paintPath);
    CGContextFillPath(c);
    CGPathRelease(paintPath);

    return false;
}

bool ThemePainterMac::paintTextArea(const LayoutObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context, r);
#ifndef USE_APPSTORE_COMPLIANT_CODE
    _NSDrawCarbonThemeListBox(r, LayoutTheme::isEnabled(o) && !LayoutTheme::isReadOnlyControl(o), YES, YES);
#else
    // Copy from ThemePainterDefault.cpp.
    // WebThemeEngine does not handle border rounded corner and background image
    // so return true to draw CSS border and background.
    if (o.styleRef().hasBorderRadius() || o.styleRef().hasBackgroundImage())
        return true;

    ControlPart part = o.styleRef().appearance();

    WebThemeEngine::ExtraParams extraParams;
    extraParams.textField.isTextArea = part == TextAreaPart;
    extraParams.textField.isListbox = part == ListboxPart;

    WebCanvas* canvas = paintInfo.context.canvas();

    Color backgroundColor = o.resolveColor(CSSPropertyBackgroundColor);
    extraParams.textField.backgroundColor = backgroundColor.rgb();

    Platform::current()->themeEngine()->paint(canvas, WebThemeEngine::PartTextField, getWebThemeState(o), WebRect(r), &extraParams);
#endif
    return false;
}

bool ThemePainterMac::paintMenuList(const LayoutObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    m_layoutTheme.setPopupButtonCellState(o, r);

    NSPopUpButtonCell* popupButton = m_layoutTheme.popupButton();

    float zoomLevel = o.styleRef().effectiveZoom();
    IntSize size = m_layoutTheme.popupButtonSizes()[[popupButton controlSize]];
    size.setHeight(size.height() * zoomLevel);
    size.setWidth(r.width());

    // Now inflate it to account for the shadow.
    IntRect inflatedRect = r;
    if (r.width() >= m_layoutTheme.minimumMenuListSize(o.styleRef()))
        inflatedRect = ThemeMac::inflateRect(inflatedRect, size, m_layoutTheme.popupButtonMargins(), zoomLevel);

    LocalCurrentGraphicsContext localContext(paintInfo.context, ThemeMac::inflateRectForFocusRing(inflatedRect));

    if (zoomLevel != 1.0f) {
        inflatedRect.setWidth(inflatedRect.width() / zoomLevel);
        inflatedRect.setHeight(inflatedRect.height() / zoomLevel);
        paintInfo.context.translate(inflatedRect.x(), inflatedRect.y());
        paintInfo.context.scale(zoomLevel, zoomLevel);
        paintInfo.context.translate(-inflatedRect.x(), -inflatedRect.y());
    }

    NSView *view = m_layoutTheme.documentViewFor(o);
    [popupButton drawWithFrame:inflatedRect inView:view];
    if (LayoutTheme::isFocused(o) && o.styleRef().outlineStyleIsAuto())
        [popupButton cr_drawFocusRingWithFrame:inflatedRect inView:view];
    [popupButton setControlView:nil];

    return false;
}

bool ThemePainterMac::paintProgressBar(const LayoutObject& layoutObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!layoutObject.isProgress())
        return true;

    float zoomLevel = layoutObject.styleRef().effectiveZoom();
    NSControlSize controlSize = m_layoutTheme.controlSizeForFont(layoutObject.styleRef());
    IntSize size = m_layoutTheme.progressBarSizes()[controlSize];
    size.setHeight(size.height() * zoomLevel);
    size.setWidth(rect.width());

    // Now inflate it to account for the shadow.
    IntRect inflatedRect = rect;
    if (rect.height() <= m_layoutTheme.minimumProgressBarHeight(layoutObject.styleRef()))
        inflatedRect = ThemeMac::inflateRect(inflatedRect, size, m_layoutTheme.progressBarMargins(controlSize), zoomLevel);

    const LayoutProgress& layoutProgress = toLayoutProgress(layoutObject);
    HIThemeTrackDrawInfo trackInfo;
    trackInfo.version = 0;
    if (controlSize == NSRegularControlSize)
        trackInfo.kind = layoutProgress.position() < 0 ? kThemeLargeIndeterminateBar : kThemeLargeProgressBar;
    else
        trackInfo.kind = layoutProgress.position() < 0 ? kThemeMediumIndeterminateBar : kThemeMediumProgressBar;

    trackInfo.bounds = IntRect(IntPoint(), inflatedRect.size());
    trackInfo.min = 0;
    trackInfo.max = std::numeric_limits<SInt32>::max();
    trackInfo.value = lround(layoutProgress.position() * nextafter(trackInfo.max, 0));
    trackInfo.trackInfo.progress.phase = lround(layoutProgress.animationProgress() * nextafter(LayoutThemeMac::progressAnimationNumFrames, 0));
    trackInfo.attributes = kThemeTrackHorizontal;
    trackInfo.enableState = LayoutTheme::isActive(layoutObject) ? kThemeTrackActive : kThemeTrackInactive;
    trackInfo.reserved = 0;
    trackInfo.filler1 = 0;

    std::unique_ptr<ImageBuffer> imageBuffer = ImageBuffer::create(inflatedRect.size());
    if (!imageBuffer)
        return true;

    IntRect clipRect = IntRect(IntPoint(), inflatedRect.size());
    LocalCurrentGraphicsContext localContext(imageBuffer->canvas(), 1, clipRect);
    CGContextRef cgContext = localContext.cgContext();
    HIThemeDrawTrack(&trackInfo, 0, cgContext, kHIThemeOrientationNormal);

    GraphicsContextStateSaver stateSaver(paintInfo.context);

    if (!layoutProgress.styleRef().isLeftToRightDirection()) {
        paintInfo.context.translate(2 * inflatedRect.x() + inflatedRect.width(), 0);
        paintInfo.context.scale(-1, 1);
    }

    if (!paintInfo.context.contextDisabled())
        imageBuffer->draw(paintInfo.context, FloatRect(inflatedRect.location(), FloatSize(imageBuffer->size())), nullptr, SkXfermode::kSrcOver_Mode);
    return false;
}

bool ThemePainterMac::paintMenuListButton(const LayoutObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    IntRect bounds = IntRect(r.x() + o.styleRef().borderLeftWidth(),
                             r.y() + o.styleRef().borderTopWidth(),
                             r.width() - o.styleRef().borderLeftWidth() - o.styleRef().borderRightWidth(),
                             r.height() - o.styleRef().borderTopWidth() - o.styleRef().borderBottomWidth());
    // Since we actually know the size of the control here, we restrict the font
    // scale to make sure the arrows will fit vertically in the bounds
    float fontScale = std::min(o.styleRef().fontSize() / LayoutThemeMac::baseFontSize,
        bounds.height() / (LayoutThemeMac::menuListBaseArrowHeight * 2 + LayoutThemeMac::menuListBaseSpaceBetweenArrows));
    float centerY = bounds.y() + bounds.height() / 2.0f;
    float arrowHeight = LayoutThemeMac::menuListBaseArrowHeight * fontScale;
    float arrowWidth = LayoutThemeMac::menuListBaseArrowWidth * fontScale;
    float leftEdge = bounds.maxX() - LayoutThemeMac::menuListArrowPaddingRight * o.styleRef().effectiveZoom() - arrowWidth;
    float spaceBetweenArrows = LayoutThemeMac::menuListBaseSpaceBetweenArrows * fontScale;

    if (bounds.width() < arrowWidth + LayoutThemeMac::menuListArrowPaddingLeft * o.styleRef().effectiveZoom())
        return false;

    Color color = o.styleRef().visitedDependentColor(CSSPropertyColor);
    SkPaint paint = paintInfo.context.fillPaint();
    paint.setAntiAlias(true);
    paint.setColor(color.rgb());

    SkPath arrow1;
    arrow1.moveTo(leftEdge, centerY - spaceBetweenArrows / 2.0f);
    arrow1.lineTo(leftEdge + arrowWidth, centerY - spaceBetweenArrows / 2.0f);
    arrow1.lineTo(leftEdge + arrowWidth / 2.0f, centerY - spaceBetweenArrows / 2.0f - arrowHeight);

    // Draw the top arrow.
    paintInfo.context.drawPath(arrow1, paint);

    SkPath arrow2;
    arrow2.moveTo(leftEdge, centerY + spaceBetweenArrows / 2.0f);
    arrow2.lineTo(leftEdge + arrowWidth, centerY + spaceBetweenArrows / 2.0f);
    arrow2.lineTo(leftEdge + arrowWidth / 2.0f, centerY + spaceBetweenArrows / 2.0f + arrowHeight);

    // Draw the bottom arrow.
    paintInfo.context.drawPath(arrow2, paint);
    return false;
}

bool ThemePainterMac::paintSliderTrack(const LayoutObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    paintSliderTicks(o, paintInfo, r);

    float zoomLevel = o.styleRef().effectiveZoom();
    FloatRect unzoomedRect = r;

    if (o.styleRef().appearance() ==  SliderHorizontalPart || o.styleRef().appearance() ==  MediaSliderPart) {
        unzoomedRect.setY(ceilf(unzoomedRect.y() + unzoomedRect.height() / 2 - zoomLevel * LayoutThemeMac::sliderTrackWidth / 2));
        unzoomedRect.setHeight(zoomLevel * LayoutThemeMac::sliderTrackWidth);
    } else if (o.styleRef().appearance() == SliderVerticalPart) {
        unzoomedRect.setX(ceilf(unzoomedRect.x() + unzoomedRect.width() / 2 - zoomLevel * LayoutThemeMac::sliderTrackWidth / 2));
        unzoomedRect.setWidth(zoomLevel * LayoutThemeMac::sliderTrackWidth);
    }

    if (zoomLevel != 1) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
    }

    GraphicsContextStateSaver stateSaver(paintInfo.context);
    if (zoomLevel != 1) {
        paintInfo.context.translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context.scale(zoomLevel, zoomLevel);
        paintInfo.context.translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    Color fillColor(205, 205, 205);
    Color borderGradientTopColor(109, 109, 109);
    Color borderGradientBottomColor(181, 181, 181);
    Color shadowColor(0, 0, 0, 118);

    if (!LayoutTheme::isEnabled(o)) {
        Color tintColor(255, 255, 255, 128);
        fillColor = fillColor.blend(tintColor);
        borderGradientTopColor = borderGradientTopColor.blend(tintColor);
        borderGradientBottomColor = borderGradientBottomColor.blend(tintColor);
        shadowColor = shadowColor.blend(tintColor);
    }

    Color tintColor;
    if (!LayoutTheme::isEnabled(o))
        tintColor = Color(255, 255, 255, 128);

    bool isVerticalSlider = o.styleRef().appearance() == SliderVerticalPart;

    float fillRadiusSize = (LayoutThemeMac::sliderTrackWidth - LayoutThemeMac::sliderTrackBorderWidth) / 2;
    FloatSize fillRadius(fillRadiusSize, fillRadiusSize);
    FloatRect fillBounds(enclosedIntRect(unzoomedRect));
    FloatRoundedRect fillRect(fillBounds, fillRadius, fillRadius, fillRadius, fillRadius);
    paintInfo.context.fillRoundedRect(fillRect, fillColor);

    FloatSize shadowOffset(isVerticalSlider ? 1 : 0,
                           isVerticalSlider ? 0 : 1);
    float shadowBlur = 3;
    float shadowSpread = 0;
    paintInfo.context.save();
    paintInfo.context.drawInnerShadow(fillRect, shadowColor, shadowOffset, shadowBlur, shadowSpread);
    paintInfo.context.restore();

    RefPtr<Gradient> borderGradient = Gradient::create(fillBounds.minXMinYCorner(),
        isVerticalSlider ? fillBounds.maxXMinYCorner() : fillBounds.minXMaxYCorner());
    borderGradient->addColorStop(0.0, borderGradientTopColor);
    borderGradient->addColorStop(1.0, borderGradientBottomColor);

    FloatRect borderRect(unzoomedRect);
    borderRect.inflate(-LayoutThemeMac::sliderTrackBorderWidth / 2.0);
    float borderRadiusSize = (isVerticalSlider ? borderRect.width() : borderRect.height()) / 2;
    FloatSize borderRadius(borderRadiusSize, borderRadiusSize);
    FloatRoundedRect borderRRect(borderRect, borderRadius, borderRadius, borderRadius, borderRadius);
    paintInfo.context.setStrokeThickness(LayoutThemeMac::sliderTrackBorderWidth);
    SkPaint borderPaint(paintInfo.context.strokePaint());
    borderGradient->applyToPaint(borderPaint, SkMatrix::I());
    paintInfo.context.drawRRect(borderRRect, borderPaint);

    return false;
}


bool ThemePainterMac::paintSliderThumb(const LayoutObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    GraphicsContextStateSaver stateSaver(paintInfo.context);
    float zoomLevel = o.styleRef().effectiveZoom();

    FloatRect unzoomedRect(r.x(), r.y(), LayoutThemeMac::sliderThumbWidth, LayoutThemeMac::sliderThumbHeight);
    if (zoomLevel != 1.0f) {
        paintInfo.context.translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context.scale(zoomLevel, zoomLevel);
        paintInfo.context.translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    Color fillGradientTopColor(250, 250, 250);
    Color fillGradientUpperMiddleColor(244, 244, 244);
    Color fillGradientLowerMiddleColor(236, 236, 236);
    Color fillGradientBottomColor(238, 238, 238);
    Color borderGradientTopColor(151, 151, 151);
    Color borderGradientBottomColor(128, 128, 128);
    Color shadowColor(0, 0, 0, 36);

    if (!LayoutTheme::isEnabled(o)) {
        Color tintColor(255, 255, 255, 128);
        fillGradientTopColor = fillGradientTopColor.blend(tintColor);
        fillGradientUpperMiddleColor = fillGradientUpperMiddleColor.blend(tintColor);
        fillGradientLowerMiddleColor = fillGradientLowerMiddleColor.blend(tintColor);
        fillGradientBottomColor = fillGradientBottomColor.blend(tintColor);
        borderGradientTopColor = borderGradientTopColor.blend(tintColor);
        borderGradientBottomColor = borderGradientBottomColor.blend(tintColor);
        shadowColor = shadowColor.blend(tintColor);
    } else if (LayoutTheme::isPressed(o)) {
        Color tintColor(0, 0, 0, 32);
        fillGradientTopColor = fillGradientTopColor.blend(tintColor);
        fillGradientUpperMiddleColor = fillGradientUpperMiddleColor.blend(tintColor);
        fillGradientLowerMiddleColor = fillGradientLowerMiddleColor.blend(tintColor);
        fillGradientBottomColor = fillGradientBottomColor.blend(tintColor);
        borderGradientTopColor = borderGradientTopColor.blend(tintColor);
        borderGradientBottomColor = borderGradientBottomColor.blend(tintColor);
        shadowColor = shadowColor.blend(tintColor);
    }

    FloatRect borderBounds = unzoomedRect;
    borderBounds.inflate(LayoutThemeMac::sliderThumbBorderWidth / 2.0);

    borderBounds.inflate(-LayoutThemeMac::sliderThumbBorderWidth);
    FloatSize shadowOffset(0, 1);
    paintInfo.context.setShadow(shadowOffset, LayoutThemeMac::sliderThumbShadowBlur, shadowColor);
    paintInfo.context.setFillColor(Color::black);
    paintInfo.context.fillEllipse(borderBounds);
    paintInfo.context.setDrawLooper(nullptr);

    IntRect fillBounds = enclosedIntRect(unzoomedRect);
    RefPtr<Gradient> fillGradient = Gradient::create(fillBounds.minXMinYCorner(), fillBounds.minXMaxYCorner());
    fillGradient->addColorStop(0.0, fillGradientTopColor);
    fillGradient->addColorStop(0.52, fillGradientUpperMiddleColor);
    fillGradient->addColorStop(0.52, fillGradientLowerMiddleColor);
    fillGradient->addColorStop(1.0, fillGradientBottomColor);
    SkPaint fillPaint(paintInfo.context.fillPaint());
    fillGradient->applyToPaint(fillPaint, SkMatrix::I());
    paintInfo.context.drawOval(borderBounds, fillPaint);

    RefPtr<Gradient> borderGradient = Gradient::create(fillBounds.minXMinYCorner(), fillBounds.minXMaxYCorner());
    borderGradient->addColorStop(0.0, borderGradientTopColor);
    borderGradient->addColorStop(1.0, borderGradientBottomColor);
    paintInfo.context.setStrokeThickness(LayoutThemeMac::sliderThumbBorderWidth);
    SkPaint borderPaint(paintInfo.context.strokePaint());
    borderGradient->applyToPaint(borderPaint, SkMatrix::I());
    paintInfo.context.drawOval(borderBounds, borderPaint);

    if (LayoutTheme::isFocused(o)) {
        Path borderPath;
        borderPath.addEllipse(borderBounds);
        paintInfo.context.drawFocusRing(borderPath, 5, -2, m_layoutTheme.focusRingColor());
    }

    return false;
}

// We don't use controlSizeForFont() for search field decorations because it
// needs to fit into the search field. The font size will already be modified by
// setFontFromControlSize() called on the search field.
static NSControlSize searchFieldControlSizeForFont(const ComputedStyle& style)
{
    int fontSize = style.fontSize();
    if (fontSize >= 13)
        return NSRegularControlSize;
    if (fontSize >= 11)
        return NSSmallControlSize;
    return NSMiniControlSize;
}

bool ThemePainterMac::paintSearchField(const LayoutObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context, r);

    NSSearchFieldCell* search = m_layoutTheme.search();
    m_layoutTheme.setSearchCellState(o, r);
    [search setControlSize:searchFieldControlSizeForFont(o.styleRef())];

    GraphicsContextStateSaver stateSaver(paintInfo.context);

    float zoomLevel = o.styleRef().effectiveZoom();

    IntRect unzoomedRect = r;

    if (zoomLevel != 1.0f) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        paintInfo.context.translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context.scale(zoomLevel, zoomLevel);
        paintInfo.context.translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    // Set the search button to nil before drawing. Then reset it so we can
    // draw it later.
    [search setSearchButtonCell:nil];

    [search drawWithFrame:NSRect(unzoomedRect) inView:m_layoutTheme.documentViewFor(o)];

    [search setControlView:nil];
    [search resetSearchButtonCell];

    return false;
}

bool ThemePainterMac::paintSearchFieldCancelButton(const LayoutObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    if (!o.node())
        return false;
    Element* input = o.node()->shadowHost();
    if (!input)
        input = toElement(o.node());

    if (!input->layoutObject()->isBox())
        return false;

    GraphicsContextStateSaver stateSaver(paintInfo.context);

    float zoomLevel = o.styleRef().effectiveZoom();
    FloatRect unzoomedRect(r);
    if (zoomLevel != 1.0f) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        paintInfo.context.translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context.scale(zoomLevel, zoomLevel);
        paintInfo.context.translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    Color fillColor(200, 200, 200);

    if (LayoutTheme::isPressed(o)) {
        Color tintColor(0, 0, 0, 32);
        fillColor = fillColor.blend(tintColor);
    }

    float centerX = unzoomedRect.x() + unzoomedRect.width() / 2;
    float centerY = unzoomedRect.y() + unzoomedRect.height() / 2;
    // The line width is 3px on a regular sized, high DPI NSCancelButtonCell
    // (which is 28px wide).
    float lineWidth = unzoomedRect.width() * 3 / 28;
    // The line length is 16px on a regular sized, high DPI NSCancelButtonCell.
    float lineLength = unzoomedRect.width() * 16 / 28;

    Path xPath;
    FloatSize lineRectRadius(lineWidth / 2, lineWidth / 2);
    xPath.addRoundedRect(FloatRect(-lineLength / 2, -lineWidth / 2, lineLength, lineWidth),
        lineRectRadius, lineRectRadius, lineRectRadius, lineRectRadius);
    xPath.addRoundedRect(FloatRect(-lineWidth / 2, -lineLength / 2, lineWidth, lineLength),
        lineRectRadius, lineRectRadius, lineRectRadius, lineRectRadius);

    paintInfo.context.translate(centerX, centerY);
    paintInfo.context.rotate(deg2rad(45.0));
    paintInfo.context.clipOut(xPath);
    paintInfo.context.rotate(deg2rad(-45.0));
    paintInfo.context.translate(-centerX, -centerY);

    paintInfo.context.setFillColor(fillColor);
    paintInfo.context.fillEllipse(unzoomedRect);

    return false;
}

} // namespace blink

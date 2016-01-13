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

#import "config.h"
#import "core/rendering/RenderThemeChromiumMac.h"

#import "CSSValueKeywords.h"
#import "HTMLNames.h"
#import "UserAgentStyleSheets.h"
#import "core/css/CSSValueList.h"
#import "core/dom/Document.h"
#import "core/dom/Element.h"
#import "core/fileapi/FileList.h"
#import "core/html/HTMLInputElement.h"
#import "core/html/HTMLMediaElement.h"
#import "core/html/HTMLMeterElement.h"
#import "core/html/TimeRanges.h"
#import "core/html/shadow/MediaControlElements.h"
#import "core/frame/FrameView.h"
#import "core/rendering/PaintInfo.h"
#import "core/rendering/RenderLayer.h"
#import "core/rendering/RenderMedia.h"
#import "core/rendering/RenderMediaControls.h"
#import "core/rendering/RenderMeter.h"
#import "core/rendering/RenderProgress.h"
#import "core/rendering/RenderSlider.h"
#import "core/rendering/RenderView.h"
#import "core/rendering/style/ShadowList.h"
#import "platform/LayoutTestSupport.h"
#import "platform/SharedBuffer.h"
#import "platform/graphics/BitmapImage.h"
#import "platform/graphics/GraphicsContextStateSaver.h"
#import "platform/graphics/Image.h"
#import "platform/graphics/ImageBuffer.h"
#import "platform/mac/ColorMac.h"
#import "platform/mac/LocalCurrentGraphicsContext.h"
#import "platform/mac/ThemeMac.h"
#import "platform/mac/WebCoreNSCellExtras.h"
#import "platform/text/PlatformLocale.h"
#import "platform/text/StringTruncator.h"

#import <AvailabilityMacros.h>
#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <math.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>

using namespace std;

// The methods in this file are specific to the Mac OS X platform.

// We estimate the animation rate of a Mac OS X progress bar is 33 fps.
// Hard code the value here because we haven't found API for it.
const double progressAnimationFrameRate = 0.033;

// Mac OS X progress bar animation seems to have 256 frames.
const double progressAnimationNumFrames = 256;

@interface WebCoreRenderThemeNotificationObserver : NSObject
{
    WebCore::RenderTheme *_theme;
}

- (id)initWithTheme:(WebCore::RenderTheme *)theme;
- (void)systemColorsDidChange:(NSNotification *)notification;

@end

@implementation WebCoreRenderThemeNotificationObserver

- (id)initWithTheme:(WebCore::RenderTheme *)theme
{
    if (!(self = [super init]))
        return nil;

    _theme = theme;
    return self;
}

- (void)systemColorsDidChange:(NSNotification *)unusedNotification
{
    ASSERT_UNUSED(unusedNotification, [[unusedNotification name] isEqualToString:NSSystemColorsDidChangeNotification]);
    _theme->platformColorsDidChange();
}

@end

@interface NSTextFieldCell (WKDetails)
- (CFDictionaryRef)_coreUIDrawOptionsWithFrame:(NSRect)cellFrame inView:(NSView *)controlView includeFocus:(BOOL)includeFocus;
@end


@interface WebCoreTextFieldCell : NSTextFieldCell
- (CFDictionaryRef)_coreUIDrawOptionsWithFrame:(NSRect)cellFrame inView:(NSView *)controlView includeFocus:(BOOL)includeFocus;
@end

@implementation WebCoreTextFieldCell
- (CFDictionaryRef)_coreUIDrawOptionsWithFrame:(NSRect)cellFrame inView:(NSView *)controlView includeFocus:(BOOL)includeFocus
{
    // FIXME: This is a post-Lion-only workaround for <rdar://problem/11385461>. When that bug is resolved, we should remove this code.
    CFMutableDictionaryRef coreUIDrawOptions = CFDictionaryCreateMutableCopy(NULL, 0, [super _coreUIDrawOptionsWithFrame:cellFrame inView:controlView includeFocus:includeFocus]);
    CFDictionarySetValue(coreUIDrawOptions, @"borders only", kCFBooleanTrue);
    return (CFDictionaryRef)[NSMakeCollectable(coreUIDrawOptions) autorelease];
}
@end

@interface RTCMFlippedView : NSView
{}

- (BOOL)isFlipped;
- (NSText *)currentEditor;

@end

@implementation RTCMFlippedView

- (BOOL)isFlipped {
    return [[NSGraphicsContext currentContext] isFlipped];
}

- (NSText *)currentEditor {
    return nil;
}

@end

// Forward declare Mac SPIs.
extern "C" {
void _NSDrawCarbonThemeBezel(NSRect frame, BOOL enabled, BOOL flipped);
// Request for public API: rdar://13787640
void _NSDrawCarbonThemeListBox(NSRect frame, BOOL enabled, BOOL flipped, BOOL always_yes);
}

namespace WebCore {

using namespace HTMLNames;

enum {
    topMargin,
    rightMargin,
    bottomMargin,
    leftMargin
};

enum {
    topPadding,
    rightPadding,
    bottomPadding,
    leftPadding
};

RenderThemeChromiumMac::RenderThemeChromiumMac()
    : m_notificationObserver(AdoptNS, [[WebCoreRenderThemeNotificationObserver alloc] initWithTheme:this])
{
    [[NSNotificationCenter defaultCenter] addObserver:m_notificationObserver.get()
                                                        selector:@selector(systemColorsDidChange:)
                                                            name:NSSystemColorsDidChangeNotification
                                                          object:nil];
}

RenderThemeChromiumMac::~RenderThemeChromiumMac()
{
    [[NSNotificationCenter defaultCenter] removeObserver:m_notificationObserver.get()];
}

Color RenderThemeChromiumMac::platformActiveSelectionBackgroundColor() const
{
    NSColor* color = [[NSColor selectedTextBackgroundColor] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    return Color(static_cast<int>(255.0 * [color redComponent]), static_cast<int>(255.0 * [color greenComponent]), static_cast<int>(255.0 * [color blueComponent]));
}

Color RenderThemeChromiumMac::platformInactiveSelectionBackgroundColor() const
{
    NSColor* color = [[NSColor secondarySelectedControlColor] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    return Color(static_cast<int>(255.0 * [color redComponent]), static_cast<int>(255.0 * [color greenComponent]), static_cast<int>(255.0 * [color blueComponent]));
}

Color RenderThemeChromiumMac::platformActiveSelectionForegroundColor() const
{
    return Color::black;
}

Color RenderThemeChromiumMac::platformActiveListBoxSelectionBackgroundColor() const
{
    NSColor* color = [[NSColor alternateSelectedControlColor] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    return Color(static_cast<int>(255.0 * [color redComponent]), static_cast<int>(255.0 * [color greenComponent]), static_cast<int>(255.0 * [color blueComponent]));
}

Color RenderThemeChromiumMac::platformActiveListBoxSelectionForegroundColor() const
{
    return Color::white;
}

Color RenderThemeChromiumMac::platformInactiveListBoxSelectionForegroundColor() const
{
    return Color::black;
}

Color RenderThemeChromiumMac::platformFocusRingColor() const
{
    static const RGBA32 oldAquaFocusRingColor = 0xFF7DADD9;
    if (usesTestModeFocusRingColor())
        return oldAquaFocusRingColor;

    return systemColor(CSSValueWebkitFocusRingColor);
}

Color RenderThemeChromiumMac::platformInactiveListBoxSelectionBackgroundColor() const
{
    return platformInactiveSelectionBackgroundColor();
}

static FontWeight toFontWeight(NSInteger appKitFontWeight)
{
    ASSERT(appKitFontWeight > 0 && appKitFontWeight < 15);
    if (appKitFontWeight > 14)
        appKitFontWeight = 14;
    else if (appKitFontWeight < 1)
        appKitFontWeight = 1;

    static FontWeight fontWeights[] = {
        FontWeight100,
        FontWeight100,
        FontWeight200,
        FontWeight300,
        FontWeight400,
        FontWeight500,
        FontWeight600,
        FontWeight600,
        FontWeight700,
        FontWeight800,
        FontWeight800,
        FontWeight900,
        FontWeight900,
        FontWeight900
    };
    return fontWeights[appKitFontWeight - 1];
}

void RenderThemeChromiumMac::systemFont(CSSValueID cssValueId, FontDescription& fontDescription) const
{
    DEFINE_STATIC_LOCAL(FontDescription, systemFont, ());
    DEFINE_STATIC_LOCAL(FontDescription, smallSystemFont, ());
    DEFINE_STATIC_LOCAL(FontDescription, menuFont, ());
    DEFINE_STATIC_LOCAL(FontDescription, labelFont, ());
    DEFINE_STATIC_LOCAL(FontDescription, miniControlFont, ());
    DEFINE_STATIC_LOCAL(FontDescription, smallControlFont, ());
    DEFINE_STATIC_LOCAL(FontDescription, controlFont, ());

    FontDescription* cachedDesc;
    NSFont* font = nil;
    switch (cssValueId) {
        case CSSValueSmallCaption:
            cachedDesc = &smallSystemFont;
            if (!smallSystemFont.isAbsoluteSize())
                font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
            break;
        case CSSValueMenu:
            cachedDesc = &menuFont;
            if (!menuFont.isAbsoluteSize())
                font = [NSFont menuFontOfSize:[NSFont systemFontSize]];
            break;
        case CSSValueStatusBar:
            cachedDesc = &labelFont;
            if (!labelFont.isAbsoluteSize())
                font = [NSFont labelFontOfSize:[NSFont labelFontSize]];
            break;
        case CSSValueWebkitMiniControl:
            cachedDesc = &miniControlFont;
            if (!miniControlFont.isAbsoluteSize())
                font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSMiniControlSize]];
            break;
        case CSSValueWebkitSmallControl:
            cachedDesc = &smallControlFont;
            if (!smallControlFont.isAbsoluteSize())
                font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSSmallControlSize]];
            break;
        case CSSValueWebkitControl:
            cachedDesc = &controlFont;
            if (!controlFont.isAbsoluteSize())
                font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSRegularControlSize]];
            break;
        default:
            cachedDesc = &systemFont;
            if (!systemFont.isAbsoluteSize())
                font = [NSFont systemFontOfSize:[NSFont systemFontSize]];
    }

    if (font) {
        NSFontManager *fontManager = [NSFontManager sharedFontManager];
        cachedDesc->setIsAbsoluteSize(true);
        cachedDesc->setGenericFamily(FontDescription::NoFamily);
        cachedDesc->firstFamily().setFamily([font webCoreFamilyName]);
        cachedDesc->setSpecifiedSize([font pointSize]);
        cachedDesc->setWeight(toFontWeight([fontManager weightOfFont:font]));
        cachedDesc->setStyle([fontManager traitsOfFont:font] & NSItalicFontMask ? FontStyleItalic : FontStyleNormal);
    }
    fontDescription = *cachedDesc;
}

static RGBA32 convertNSColorToColor(NSColor *color)
{
    NSColor *colorInColorSpace = [color colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    if (colorInColorSpace) {
        static const double scaleFactor = nextafter(256.0, 0.0);
        return makeRGB(static_cast<int>(scaleFactor * [colorInColorSpace redComponent]),
            static_cast<int>(scaleFactor * [colorInColorSpace greenComponent]),
            static_cast<int>(scaleFactor * [colorInColorSpace blueComponent]));
    }

    // This conversion above can fail if the NSColor in question is an NSPatternColor
    // (as many system colors are). These colors are actually a repeating pattern
    // not just a solid color. To work around this we simply draw a 1x1 image of
    // the color and use that pixel's color. It might be better to use an average of
    // the colors in the pattern instead.
    NSBitmapImageRep *offscreenRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nil
                                                                             pixelsWide:1
                                                                             pixelsHigh:1
                                                                          bitsPerSample:8
                                                                        samplesPerPixel:4
                                                                               hasAlpha:YES
                                                                               isPlanar:NO
                                                                         colorSpaceName:NSDeviceRGBColorSpace
                                                                            bytesPerRow:4
                                                                           bitsPerPixel:32];

    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:offscreenRep]];
    NSEraseRect(NSMakeRect(0, 0, 1, 1));
    [color drawSwatchInRect:NSMakeRect(0, 0, 1, 1)];
    [NSGraphicsContext restoreGraphicsState];

    NSUInteger pixel[4];
    [offscreenRep getPixel:pixel atX:0 y:0];

    [offscreenRep release];

    return makeRGB(pixel[0], pixel[1], pixel[2]);
}

static RGBA32 menuBackgroundColor()
{
    NSBitmapImageRep *offscreenRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nil
                                                                             pixelsWide:1
                                                                             pixelsHigh:1
                                                                          bitsPerSample:8
                                                                        samplesPerPixel:4
                                                                               hasAlpha:YES
                                                                               isPlanar:NO
                                                                         colorSpaceName:NSDeviceRGBColorSpace
                                                                            bytesPerRow:4
                                                                           bitsPerPixel:32];

    CGContextRef context = static_cast<CGContextRef>([[NSGraphicsContext graphicsContextWithBitmapImageRep:offscreenRep] graphicsPort]);
    CGRect rect = CGRectMake(0, 0, 1, 1);
    HIThemeMenuDrawInfo drawInfo;
    drawInfo.version =  0;
    drawInfo.menuType = kThemeMenuTypePopUp;
    HIThemeDrawMenuBackground(&rect, &drawInfo, context, kHIThemeOrientationInverted);

    NSUInteger pixel[4];
    [offscreenRep getPixel:pixel atX:0 y:0];

    [offscreenRep release];

    return makeRGB(pixel[0], pixel[1], pixel[2]);
}

void RenderThemeChromiumMac::platformColorsDidChange()
{
    m_systemColorCache.clear();
    RenderTheme::platformColorsDidChange();
}

Color RenderThemeChromiumMac::systemColor(CSSValueID cssValueId) const
{
    {
        HashMap<int, RGBA32>::iterator it = m_systemColorCache.find(cssValueId);
        if (it != m_systemColorCache.end())
            return it->value;
    }

    Color color;
    bool needsFallback = false;
    switch (cssValueId) {
        case CSSValueActiveborder:
            color = convertNSColorToColor([NSColor keyboardFocusIndicatorColor]);
            break;
        case CSSValueActivecaption:
            color = convertNSColorToColor([NSColor windowFrameTextColor]);
            break;
        case CSSValueAppworkspace:
            color = convertNSColorToColor([NSColor headerColor]);
            break;
        case CSSValueBackground:
            // Use theme independent default
            needsFallback = true;
            break;
        case CSSValueButtonface:
            // We use this value instead of NSColor's controlColor to avoid website incompatibilities.
            // We may want to change this to use the NSColor in future.
            color = 0xFFC0C0C0;
            break;
        case CSSValueButtonhighlight:
            color = convertNSColorToColor([NSColor controlHighlightColor]);
            break;
        case CSSValueButtonshadow:
            color = convertNSColorToColor([NSColor controlShadowColor]);
            break;
        case CSSValueButtontext:
            color = convertNSColorToColor([NSColor controlTextColor]);
            break;
        case CSSValueCaptiontext:
            color = convertNSColorToColor([NSColor textColor]);
            break;
        case CSSValueGraytext:
            color = convertNSColorToColor([NSColor disabledControlTextColor]);
            break;
        case CSSValueHighlight:
            color = convertNSColorToColor([NSColor selectedTextBackgroundColor]);
            break;
        case CSSValueHighlighttext:
            color = convertNSColorToColor([NSColor selectedTextColor]);
            break;
        case CSSValueInactiveborder:
            color = convertNSColorToColor([NSColor controlBackgroundColor]);
            break;
        case CSSValueInactivecaption:
            color = convertNSColorToColor([NSColor controlBackgroundColor]);
            break;
        case CSSValueInactivecaptiontext:
            color = convertNSColorToColor([NSColor textColor]);
            break;
        case CSSValueInfobackground:
            // There is no corresponding NSColor for this so we use a hard coded value.
            color = 0xFFFBFCC5;
            break;
        case CSSValueInfotext:
            color = convertNSColorToColor([NSColor textColor]);
            break;
        case CSSValueMenu:
            color = menuBackgroundColor();
            break;
        case CSSValueMenutext:
            color = convertNSColorToColor([NSColor selectedMenuItemTextColor]);
            break;
        case CSSValueScrollbar:
            color = convertNSColorToColor([NSColor scrollBarColor]);
            break;
        case CSSValueText:
            color = convertNSColorToColor([NSColor textColor]);
            break;
        case CSSValueThreeddarkshadow:
            color = convertNSColorToColor([NSColor controlDarkShadowColor]);
            break;
        case CSSValueThreedshadow:
            color = convertNSColorToColor([NSColor shadowColor]);
            break;
        case CSSValueThreedface:
            // We use this value instead of NSColor's controlColor to avoid website incompatibilities.
            // We may want to change this to use the NSColor in future.
            color = 0xFFC0C0C0;
            break;
        case CSSValueThreedhighlight:
            color = convertNSColorToColor([NSColor highlightColor]);
            break;
        case CSSValueThreedlightshadow:
            color = convertNSColorToColor([NSColor controlLightHighlightColor]);
            break;
        case CSSValueWebkitFocusRingColor:
            color = convertNSColorToColor([NSColor keyboardFocusIndicatorColor]);
            break;
        case CSSValueWindow:
            color = convertNSColorToColor([NSColor windowBackgroundColor]);
            break;
        case CSSValueWindowframe:
            color = convertNSColorToColor([NSColor windowFrameColor]);
            break;
        case CSSValueWindowtext:
            color = convertNSColorToColor([NSColor windowFrameTextColor]);
            break;
        default:
            needsFallback = true;
            break;
    }

    if (needsFallback)
        color = RenderTheme::systemColor(cssValueId);

    m_systemColorCache.set(cssValueId, color.rgb());

    return color;
}

bool RenderThemeChromiumMac::isControlStyled(const RenderStyle* style, const CachedUAStyle* uaStyle) const
{
    ASSERT(uaStyle);
    if (style->appearance() == TextFieldPart || style->appearance() == TextAreaPart || style->appearance() == ListboxPart)
        return style->border() != uaStyle->border || style->boxShadow();

    // FIXME: This is horrible, but there is not much else that can be done.  Menu lists cannot draw properly when
    // scaled.  They can't really draw properly when transformed either.  We can't detect the transform case at style
    // adjustment time so that will just have to stay broken.  We can however detect that we're zooming.  If zooming
    // is in effect we treat it like the control is styled.
    if (style->appearance() == MenulistPart && style->effectiveZoom() != 1.0f)
        return true;
    // FIXME: NSSearchFieldCell doesn't work well when scaled.
    if (style->appearance() == SearchFieldPart && style->effectiveZoom() != 1)
        return true;

    return RenderTheme::isControlStyled(style, uaStyle);
}

const int sliderThumbShadowBlur = 1;

void RenderThemeChromiumMac::adjustRepaintRect(const RenderObject* o, IntRect& r)
{
    ControlPart part = o->style()->appearance();

#if USE(NEW_THEME)
    switch (part) {
        case CheckboxPart:
        case RadioPart:
        case PushButtonPart:
        case SquareButtonPart:
        case ButtonPart:
        case InnerSpinButtonPart:
            return RenderTheme::adjustRepaintRect(o, r);
        default:
            break;
    }
#endif

    float zoomLevel = o->style()->effectiveZoom();

    if (part == MenulistPart) {
        setPopupButtonCellState(o, r);
        IntSize size = popupButtonSizes()[[popupButton() controlSize]];
        size.setHeight(size.height() * zoomLevel);
        size.setWidth(r.width());
        r = inflateRect(r, size, popupButtonMargins(), zoomLevel);
    } else if (part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart) {
        r.setHeight(r.height() + sliderThumbShadowBlur);
    }
}

IntRect RenderThemeChromiumMac::inflateRect(const IntRect& r, const IntSize& size, const int* margins, float zoomLevel) const
{
    // Only do the inflation if the available width/height are too small.  Otherwise try to
    // fit the glow/check space into the available box's width/height.
    int widthDelta = r.width() - (size.width() + margins[leftMargin] * zoomLevel + margins[rightMargin] * zoomLevel);
    int heightDelta = r.height() - (size.height() + margins[topMargin] * zoomLevel + margins[bottomMargin] * zoomLevel);
    IntRect result(r);
    if (widthDelta < 0) {
        result.setX(result.x() - margins[leftMargin] * zoomLevel);
        result.setWidth(result.width() - widthDelta);
    }
    if (heightDelta < 0) {
        result.setY(result.y() - margins[topMargin] * zoomLevel);
        result.setHeight(result.height() - heightDelta);
    }
    return result;
}

FloatRect RenderThemeChromiumMac::convertToPaintingRect(const RenderObject* inputRenderer, const RenderObject* partRenderer, const FloatRect& inputRect, const IntRect& r) const
{
    FloatRect partRect(inputRect);

    // Compute an offset between the part renderer and the input renderer
    FloatSize offsetFromInputRenderer;
    const RenderObject* renderer = partRenderer;
    while (renderer && renderer != inputRenderer) {
        RenderObject* containingRenderer = renderer->container();
        offsetFromInputRenderer -= roundedIntSize(renderer->offsetFromContainer(containingRenderer, LayoutPoint()));
        renderer = containingRenderer;
    }
    // If the input renderer was not a container, something went wrong
    ASSERT(renderer == inputRenderer);
    // Move the rect into partRenderer's coords
    partRect.move(offsetFromInputRenderer);
    // Account for the local drawing offset (tx, ty)
    partRect.move(r.x(), r.y());

    return partRect;
}

void RenderThemeChromiumMac::updateCheckedState(NSCell* cell, const RenderObject* o)
{
    bool oldIndeterminate = [cell state] == NSMixedState;
    bool indeterminate = isIndeterminate(o);
    bool checked = isChecked(o);

    if (oldIndeterminate != indeterminate) {
        [cell setState:indeterminate ? NSMixedState : (checked ? NSOnState : NSOffState)];
        return;
    }

    bool oldChecked = [cell state] == NSOnState;
    if (checked != oldChecked)
        [cell setState:checked ? NSOnState : NSOffState];
}

void RenderThemeChromiumMac::updateEnabledState(NSCell* cell, const RenderObject* o)
{
    bool oldEnabled = [cell isEnabled];
    bool enabled = isEnabled(o);
    if (enabled != oldEnabled)
        [cell setEnabled:enabled];
}

void RenderThemeChromiumMac::updateFocusedState(NSCell* cell, const RenderObject* o)
{
    bool oldFocused = [cell showsFirstResponder];
    bool focused = isFocused(o) && o->style()->outlineStyleIsAuto();
    if (focused != oldFocused)
        [cell setShowsFirstResponder:focused];
}

void RenderThemeChromiumMac::updatePressedState(NSCell* cell, const RenderObject* o)
{
    bool oldPressed = [cell isHighlighted];
    bool pressed = (o->node() && o->node()->active());
    if (pressed != oldPressed)
        [cell setHighlighted:pressed];
}

bool RenderThemeChromiumMac::controlSupportsTints(const RenderObject* o) const
{
    // An alternate way to implement this would be to get the appropriate cell object
    // and call the private _needRedrawOnWindowChangedKeyState method. An advantage of
    // that would be that we would match AppKit behavior more closely, but a disadvantage
    // would be that we would rely on an AppKit SPI method.

    if (!isEnabled(o))
        return false;

    // Checkboxes only have tint when checked.
    if (o->style()->appearance() == CheckboxPart)
        return isChecked(o);

    // For now assume other controls have tint if enabled.
    return true;
}

NSControlSize RenderThemeChromiumMac::controlSizeForFont(RenderStyle* style) const
{
    int fontSize = style->fontSize();
    if (fontSize >= 16)
        return NSRegularControlSize;
    if (fontSize >= 11)
        return NSSmallControlSize;
    return NSMiniControlSize;
}

// We don't use controlSizeForFont() for search field decorations because it needs to fit
// into the search field. The font size will already be modified by
// setFontFromControlSize() called on the search field.
static NSControlSize searchFieldControlSizeForFont(RenderStyle* style)
{
    int fontSize = style->fontSize();
    if (fontSize >= 13)
        return NSRegularControlSize;
    if (fontSize >= 11)
        return NSSmallControlSize;
    return NSMiniControlSize;
}

void RenderThemeChromiumMac::setControlSize(NSCell* cell, const IntSize* sizes, const IntSize& minSize, float zoomLevel)
{
    NSControlSize size;
    if (minSize.width() >= static_cast<int>(sizes[NSRegularControlSize].width() * zoomLevel) &&
        minSize.height() >= static_cast<int>(sizes[NSRegularControlSize].height() * zoomLevel))
        size = NSRegularControlSize;
    else if (minSize.width() >= static_cast<int>(sizes[NSSmallControlSize].width() * zoomLevel) &&
             minSize.height() >= static_cast<int>(sizes[NSSmallControlSize].height() * zoomLevel))
        size = NSSmallControlSize;
    else
        size = NSMiniControlSize;
    if (size != [cell controlSize]) // Only update if we have to, since AppKit does work even if the size is the same.
        [cell setControlSize:size];
}

IntSize RenderThemeChromiumMac::sizeForFont(RenderStyle* style, const IntSize* sizes) const
{
    if (style->effectiveZoom() != 1.0f) {
        IntSize result = sizes[controlSizeForFont(style)];
        return IntSize(result.width() * style->effectiveZoom(), result.height() * style->effectiveZoom());
    }
    return sizes[controlSizeForFont(style)];
}

IntSize RenderThemeChromiumMac::sizeForSystemFont(RenderStyle* style, const IntSize* sizes) const
{
    if (style->effectiveZoom() != 1.0f) {
        IntSize result = sizes[controlSizeForSystemFont(style)];
        return IntSize(result.width() * style->effectiveZoom(), result.height() * style->effectiveZoom());
    }
    return sizes[controlSizeForSystemFont(style)];
}

void RenderThemeChromiumMac::setSizeFromFont(RenderStyle* style, const IntSize* sizes) const
{
    // FIXME: Check is flawed, since it doesn't take min-width/max-width into account.
    IntSize size = sizeForFont(style, sizes);
    if (style->width().isIntrinsicOrAuto() && size.width() > 0)
        style->setWidth(Length(size.width(), Fixed));
    if (style->height().isAuto() && size.height() > 0)
        style->setHeight(Length(size.height(), Fixed));
}

void RenderThemeChromiumMac::setFontFromControlSize(RenderStyle* style, NSControlSize controlSize) const
{
    FontDescription fontDescription;
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setGenericFamily(FontDescription::SerifFamily);

    NSFont* font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:controlSize]];
    fontDescription.firstFamily().setFamily([font webCoreFamilyName]);
    fontDescription.setComputedSize([font pointSize] * style->effectiveZoom());
    fontDescription.setSpecifiedSize([font pointSize] * style->effectiveZoom());

    // Reset line height
    style->setLineHeight(RenderStyle::initialLineHeight());

    if (style->setFontDescription(fontDescription))
        style->font().update(nullptr);
}

NSControlSize RenderThemeChromiumMac::controlSizeForSystemFont(RenderStyle* style) const
{
    float fontSize = style->fontSize();
    float zoomLevel = style->effectiveZoom();
    if (zoomLevel != 1)
        fontSize /= zoomLevel;
    if (fontSize >= [NSFont systemFontSizeForControlSize:NSRegularControlSize])
        return NSRegularControlSize;
    if (fontSize >= [NSFont systemFontSizeForControlSize:NSSmallControlSize])
        return NSSmallControlSize;
    return NSMiniControlSize;
}

bool RenderThemeChromiumMac::paintTextField(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context);

#if __MAC_OS_X_VERSION_MIN_REQUIRED <= 1070
    bool useNSTextFieldCell = o->style()->hasAppearance()
        && o->style()->visitedDependentColor(CSSPropertyBackgroundColor) == Color::white
        && !o->style()->hasBackgroundImage();

    // We do not use NSTextFieldCell to draw styled text fields on Lion and SnowLeopard because
    // there are a number of bugs on those platforms that require NSTextFieldCell to be in charge
    // of painting its own background. We need WebCore to paint styled backgrounds, so we'll use
    // this AppKit SPI function instead.
    if (!useNSTextFieldCell) {
        _NSDrawCarbonThemeBezel(r, isEnabled(o) && !isReadOnlyControl(o), YES);
        return false;
    }
#endif

    NSTextFieldCell *textField = this->textField();

    GraphicsContextStateSaver stateSaver(*paintInfo.context);

    [textField setEnabled:(isEnabled(o) && !isReadOnlyControl(o))];
    [textField drawWithFrame:NSRect(r) inView:documentViewFor(o)];

    [textField setControlView:nil];

    return false;
}

bool RenderThemeChromiumMac::paintCapsLockIndicator(RenderObject*, const PaintInfo& paintInfo, const IntRect& r)
{
    if (paintInfo.context->paintingDisabled())
        return true;

    // This draws the caps lock indicator as it was done by WKDrawCapsLockIndicator.
    LocalCurrentGraphicsContext localContext(paintInfo.context);
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

    // Draw the arrow - note this is drawing in a flipped coordinate system, so the
    // arrow is pointing down.
    CGPathMoveToPoint(shape, NULL, 8.5, 2);  // Tip point.
    CGPathAddLineToPoint(shape, NULL, 4,     7);
    CGPathAddLineToPoint(shape, NULL, 6.25,  7);
    CGPathAddLineToPoint(shape, NULL, 6.25,  10.25);
    CGPathAddLineToPoint(shape, NULL, 10.75, 10.25);
    CGPathAddLineToPoint(shape, NULL, 10.75, 7);
    CGPathAddLineToPoint(shape, NULL, 13,    7);
    CGPathAddLineToPoint(shape, NULL, 8.5,   2);

    // Draw the rectangle that underneath (or above in the flipped system) the arrow.
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

bool RenderThemeChromiumMac::paintTextArea(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context);
    _NSDrawCarbonThemeListBox(r, isEnabled(o) && !isReadOnlyControl(o), YES, YES);
    return false;
}

const int* RenderThemeChromiumMac::popupButtonMargins() const
{
    static const int margins[3][4] =
    {
        { 0, 3, 1, 3 },
        { 0, 3, 2, 3 },
        { 0, 1, 0, 1 }
    };
    return margins[[popupButton() controlSize]];
}

const IntSize* RenderThemeChromiumMac::popupButtonSizes() const
{
    static const IntSize sizes[3] = { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15) };
    return sizes;
}

const int* RenderThemeChromiumMac::popupButtonPadding(NSControlSize size) const
{
    static const int padding[3][4] =
    {
        { 2, 26, 3, 8 },
        { 2, 23, 3, 8 },
        { 2, 22, 3, 10 }
    };
    return padding[size];
}

bool RenderThemeChromiumMac::paintMenuList(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context);
    setPopupButtonCellState(o, r);

    NSPopUpButtonCell* popupButton = this->popupButton();

    float zoomLevel = o->style()->effectiveZoom();
    IntSize size = popupButtonSizes()[[popupButton controlSize]];
    size.setHeight(size.height() * zoomLevel);
    size.setWidth(r.width());

    // Now inflate it to account for the shadow.
    IntRect inflatedRect = r;
    if (r.width() >= minimumMenuListSize(o->style()))
        inflatedRect = inflateRect(inflatedRect, size, popupButtonMargins(), zoomLevel);

    GraphicsContextStateSaver stateSaver(*paintInfo.context);

    // On Leopard, the cell will draw outside of the given rect, so we have to clip to the rect
    paintInfo.context->clip(inflatedRect);

    if (zoomLevel != 1.0f) {
        inflatedRect.setWidth(inflatedRect.width() / zoomLevel);
        inflatedRect.setHeight(inflatedRect.height() / zoomLevel);
        paintInfo.context->translate(inflatedRect.x(), inflatedRect.y());
        paintInfo.context->scale(zoomLevel, zoomLevel);
        paintInfo.context->translate(-inflatedRect.x(), -inflatedRect.y());
    }

    NSView *view = documentViewFor(o);
    [popupButton drawWithFrame:inflatedRect inView:view];
#if !BUTTON_CELL_DRAW_WITH_FRAME_DRAWS_FOCUS_RING
    if (isFocused(o) && o->style()->outlineStyleIsAuto())
        [popupButton _web_drawFocusRingWithFrame:inflatedRect inView:view];
#endif
    [popupButton setControlView:nil];

    return false;
}

IntSize RenderThemeChromiumMac::meterSizeForBounds(const RenderMeter* renderMeter, const IntRect& bounds) const
{
    if (NoControlPart == renderMeter->style()->appearance())
        return bounds.size();

    NSLevelIndicatorCell* cell = levelIndicatorFor(renderMeter);
    // Makes enough room for cell's intrinsic size.
    NSSize cellSize = [cell cellSizeForBounds:IntRect(IntPoint(), bounds.size())];
    return IntSize(bounds.width() < cellSize.width ? cellSize.width : bounds.width(),
                   bounds.height() < cellSize.height ? cellSize.height : bounds.height());
}

bool RenderThemeChromiumMac::paintMeter(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!renderObject->isMeter())
        return true;

    LocalCurrentGraphicsContext localContext(paintInfo.context);

    NSLevelIndicatorCell* cell = levelIndicatorFor(toRenderMeter(renderObject));
    GraphicsContextStateSaver stateSaver(*paintInfo.context);

    [cell drawWithFrame:rect inView:documentViewFor(renderObject)];
    [cell setControlView:nil];
    return false;
}

bool RenderThemeChromiumMac::supportsMeter(ControlPart part) const
{
    switch (part) {
    case RelevancyLevelIndicatorPart:
    case DiscreteCapacityLevelIndicatorPart:
    case RatingLevelIndicatorPart:
    case MeterPart:
    case ContinuousCapacityLevelIndicatorPart:
        return true;
    default:
        return false;
    }
}

NSLevelIndicatorStyle RenderThemeChromiumMac::levelIndicatorStyleFor(ControlPart part) const
{
    switch (part) {
    case RelevancyLevelIndicatorPart:
        return NSRelevancyLevelIndicatorStyle;
    case DiscreteCapacityLevelIndicatorPart:
        return NSDiscreteCapacityLevelIndicatorStyle;
    case RatingLevelIndicatorPart:
        return NSRatingLevelIndicatorStyle;
    case MeterPart:
    case ContinuousCapacityLevelIndicatorPart:
    default:
        return NSContinuousCapacityLevelIndicatorStyle;
    }

}

NSLevelIndicatorCell* RenderThemeChromiumMac::levelIndicatorFor(const RenderMeter* renderMeter) const
{
    RenderStyle* style = renderMeter->style();
    ASSERT(style->appearance() != NoControlPart);

    if (!m_levelIndicator)
        m_levelIndicator.adoptNS([[NSLevelIndicatorCell alloc] initWithLevelIndicatorStyle:NSContinuousCapacityLevelIndicatorStyle]);
    NSLevelIndicatorCell* cell = m_levelIndicator.get();

    HTMLMeterElement* element = renderMeter->meterElement();
    double value = element->value();

    // Because NSLevelIndicatorCell does not support optimum-in-the-middle type coloring,
    // we explicitly control the color instead giving low and high value to NSLevelIndicatorCell as is.
    switch (element->gaugeRegion()) {
    case HTMLMeterElement::GaugeRegionOptimum:
        // Make meter the green
        [cell setWarningValue:value + 1];
        [cell setCriticalValue:value + 2];
        break;
    case HTMLMeterElement::GaugeRegionSuboptimal:
        // Make the meter yellow
        [cell setWarningValue:value - 1];
        [cell setCriticalValue:value + 1];
        break;
    case HTMLMeterElement::GaugeRegionEvenLessGood:
        // Make the meter red
        [cell setWarningValue:value - 2];
        [cell setCriticalValue:value - 1];
        break;
    }

    [cell setLevelIndicatorStyle:levelIndicatorStyleFor(style->appearance())];
    [cell setBaseWritingDirection:style->isLeftToRightDirection() ? NSWritingDirectionLeftToRight : NSWritingDirectionRightToLeft];
    [cell setMinValue:element->min()];
    [cell setMaxValue:element->max()];
    RetainPtr<NSNumber> valueObject = [NSNumber numberWithDouble:value];
    [cell setObjectValue:valueObject.get()];

    return cell;
}

const IntSize* RenderThemeChromiumMac::progressBarSizes() const
{
    static const IntSize sizes[3] = { IntSize(0, 20), IntSize(0, 12), IntSize(0, 12) };
    return sizes;
}

const int* RenderThemeChromiumMac::progressBarMargins(NSControlSize controlSize) const
{
    static const int margins[3][4] =
    {
        { 0, 0, 1, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 1, 0 },
    };
    return margins[controlSize];
}

int RenderThemeChromiumMac::minimumProgressBarHeight(RenderStyle* style) const
{
    return sizeForSystemFont(style, progressBarSizes()).height();
}

double RenderThemeChromiumMac::animationRepeatIntervalForProgressBar(RenderProgress*) const
{
    return progressAnimationFrameRate;
}

double RenderThemeChromiumMac::animationDurationForProgressBar(RenderProgress*) const
{
    return progressAnimationNumFrames * progressAnimationFrameRate;
}

bool RenderThemeChromiumMac::paintProgressBar(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!renderObject->isProgress())
        return true;

    float zoomLevel = renderObject->style()->effectiveZoom();
    NSControlSize controlSize = controlSizeForFont(renderObject->style());
    IntSize size = progressBarSizes()[controlSize];
    size.setHeight(size.height() * zoomLevel);
    size.setWidth(rect.width());

    // Now inflate it to account for the shadow.
    IntRect inflatedRect = rect;
    if (rect.height() <= minimumProgressBarHeight(renderObject->style()))
        inflatedRect = inflateRect(inflatedRect, size, progressBarMargins(controlSize), zoomLevel);

    RenderProgress* renderProgress = toRenderProgress(renderObject);
    HIThemeTrackDrawInfo trackInfo;
    trackInfo.version = 0;
    if (controlSize == NSRegularControlSize)
        trackInfo.kind = renderProgress->position() < 0 ? kThemeLargeIndeterminateBar : kThemeLargeProgressBar;
    else
        trackInfo.kind = renderProgress->position() < 0 ? kThemeMediumIndeterminateBar : kThemeMediumProgressBar;

    trackInfo.bounds = IntRect(IntPoint(), inflatedRect.size());
    trackInfo.min = 0;
    trackInfo.max = numeric_limits<SInt32>::max();
    trackInfo.value = lround(renderProgress->position() * nextafter(trackInfo.max, 0));
    trackInfo.trackInfo.progress.phase = lround(renderProgress->animationProgress() * nextafter(progressAnimationNumFrames, 0));
    trackInfo.attributes = kThemeTrackHorizontal;
    trackInfo.enableState = isActive(renderObject) ? kThemeTrackActive : kThemeTrackInactive;
    trackInfo.reserved = 0;
    trackInfo.filler1 = 0;

    OwnPtr<ImageBuffer> imageBuffer = ImageBuffer::create(inflatedRect.size());
    if (!imageBuffer)
        return true;

    ContextContainer cgContextContainer(imageBuffer->context());
    CGContextRef cgContext = cgContextContainer.context();
    HIThemeDrawTrack(&trackInfo, 0, cgContext, kHIThemeOrientationNormal);

    GraphicsContextStateSaver stateSaver(*paintInfo.context);

    if (!renderProgress->style()->isLeftToRightDirection()) {
        paintInfo.context->translate(2 * inflatedRect.x() + inflatedRect.width(), 0);
        paintInfo.context->scale(-1, 1);
    }

    paintInfo.context->drawImageBuffer(imageBuffer.get(),
        FloatRect(inflatedRect.location(), imageBuffer->size()));
    return false;
}

const float baseFontSize = 11.0f;
const float baseArrowHeight = 4.0f;
const float baseArrowWidth = 5.0f;
const float baseSpaceBetweenArrows = 2.0f;
const int arrowPaddingLeft = 6;
const int arrowPaddingRight = 6;
const int paddingBeforeSeparator = 4;
const int baseBorderRadius = 5;
const int styledPopupPaddingLeft = 8;
const int styledPopupPaddingTop = 1;
const int styledPopupPaddingBottom = 2;

bool RenderThemeChromiumMac::paintMenuListButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    IntRect bounds = IntRect(r.x() + o->style()->borderLeftWidth(),
                             r.y() + o->style()->borderTopWidth(),
                             r.width() - o->style()->borderLeftWidth() - o->style()->borderRightWidth(),
                             r.height() - o->style()->borderTopWidth() - o->style()->borderBottomWidth());
    // Since we actually know the size of the control here, we restrict the font scale to make sure the arrows will fit vertically in the bounds
    float fontScale = min(o->style()->fontSize() / baseFontSize, bounds.height() / (baseArrowHeight * 2 + baseSpaceBetweenArrows));
    float centerY = bounds.y() + bounds.height() / 2.0f;
    float arrowHeight = baseArrowHeight * fontScale;
    float arrowWidth = baseArrowWidth * fontScale;
    float leftEdge = bounds.maxX() - arrowPaddingRight * o->style()->effectiveZoom() - arrowWidth;
    float spaceBetweenArrows = baseSpaceBetweenArrows * fontScale;

    if (bounds.width() < arrowWidth + arrowPaddingLeft * o->style()->effectiveZoom())
        return false;

    GraphicsContextStateSaver stateSaver(*paintInfo.context);

    paintInfo.context->setFillColor(o->style()->visitedDependentColor(CSSPropertyColor));
    paintInfo.context->setStrokeStyle(NoStroke);

    FloatPoint arrow1[3];
    arrow1[0] = FloatPoint(leftEdge, centerY - spaceBetweenArrows / 2.0f);
    arrow1[1] = FloatPoint(leftEdge + arrowWidth, centerY - spaceBetweenArrows / 2.0f);
    arrow1[2] = FloatPoint(leftEdge + arrowWidth / 2.0f, centerY - spaceBetweenArrows / 2.0f - arrowHeight);

    // Draw the top arrow
    paintInfo.context->drawConvexPolygon(3, arrow1, true);

    FloatPoint arrow2[3];
    arrow2[0] = FloatPoint(leftEdge, centerY + spaceBetweenArrows / 2.0f);
    arrow2[1] = FloatPoint(leftEdge + arrowWidth, centerY + spaceBetweenArrows / 2.0f);
    arrow2[2] = FloatPoint(leftEdge + arrowWidth / 2.0f, centerY + spaceBetweenArrows / 2.0f + arrowHeight);

    // Draw the bottom arrow
    paintInfo.context->drawConvexPolygon(3, arrow2, true);
    return false;
}

static const IntSize* menuListButtonSizes()
{
    static const IntSize sizes[3] = { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15) };
    return sizes;
}

void RenderThemeChromiumMac::adjustMenuListStyle(RenderStyle* style, Element* e) const
{
    NSControlSize controlSize = controlSizeForFont(style);

    style->resetBorder();
    style->resetPadding();

    // Height is locked to auto.
    style->setHeight(Length(Auto));

    // White-space is locked to pre
    style->setWhiteSpace(PRE);

    // Set the foreground color to black or gray when we have the aqua look.
    // Cast to RGB32 is to work around a compiler bug.
    style->setColor(e && !e->isDisabledFormControl() ? static_cast<RGBA32>(Color::black) : Color::darkGray);

    // Set the button's vertical size.
    setSizeFromFont(style, menuListButtonSizes());

    // Our font is locked to the appropriate system font size for the control.  To clarify, we first use the CSS-specified font to figure out
    // a reasonable control size, but once that control size is determined, we throw that font away and use the appropriate
    // system font for the control size instead.
    setFontFromControlSize(style, controlSize);
}

const int autofillPopupHorizontalPadding = 4;

// These functions are called with MenuListPart or MenulistButtonPart appearance by RenderMenuList, or with TextFieldPart appearance by AutofillPopupMenuClient.
// We assume only AutofillPopupMenuClient gives TexfieldPart appearance here.
// We want to change only Autofill padding.
// In the future, we have to separate Autofill popup window logic from WebKit to Chromium.
int RenderThemeChromiumMac::popupInternalPaddingLeft(RenderStyle* style) const
{
    if (style->appearance() == TextFieldPart)
        return autofillPopupHorizontalPadding;

    if (style->appearance() == MenulistPart)
        return popupButtonPadding(controlSizeForFont(style))[leftPadding] * style->effectiveZoom();
    if (style->appearance() == MenulistButtonPart)
        return styledPopupPaddingLeft * style->effectiveZoom();
    return 0;
}

int RenderThemeChromiumMac::popupInternalPaddingRight(RenderStyle* style) const
{
    if (style->appearance() == TextFieldPart)
        return autofillPopupHorizontalPadding;

    if (style->appearance() == MenulistPart)
        return popupButtonPadding(controlSizeForFont(style))[rightPadding] * style->effectiveZoom();
    if (style->appearance() == MenulistButtonPart) {
        float fontScale = style->fontSize() / baseFontSize;
        float arrowWidth = baseArrowWidth * fontScale;
        return static_cast<int>(ceilf(arrowWidth + (arrowPaddingLeft + arrowPaddingRight + paddingBeforeSeparator) * style->effectiveZoom()));
    }
    return 0;
}

int RenderThemeChromiumMac::popupInternalPaddingTop(RenderStyle* style) const
{
    if (style->appearance() == MenulistPart)
        return popupButtonPadding(controlSizeForFont(style))[topPadding] * style->effectiveZoom();
    if (style->appearance() == MenulistButtonPart)
        return styledPopupPaddingTop * style->effectiveZoom();
    return 0;
}

int RenderThemeChromiumMac::popupInternalPaddingBottom(RenderStyle* style) const
{
    if (style->appearance() == MenulistPart)
        return popupButtonPadding(controlSizeForFont(style))[bottomPadding] * style->effectiveZoom();
    if (style->appearance() == MenulistButtonPart)
        return styledPopupPaddingBottom * style->effectiveZoom();
    return 0;
}

void RenderThemeChromiumMac::adjustMenuListButtonStyle(RenderStyle* style, Element*) const
{
    float fontScale = style->fontSize() / baseFontSize;

    style->resetPadding();
    style->setBorderRadius(IntSize(int(baseBorderRadius + fontScale - 1), int(baseBorderRadius + fontScale - 1))); // FIXME: Round up?

    const int minHeight = 15;
    style->setMinHeight(Length(minHeight, Fixed));

    style->setLineHeight(RenderStyle::initialLineHeight());
}

void RenderThemeChromiumMac::setPopupButtonCellState(const RenderObject* o, const IntRect& r)
{
    NSPopUpButtonCell* popupButton = this->popupButton();

    // Set the control size based off the rectangle we're painting into.
    setControlSize(popupButton, popupButtonSizes(), r.size(), o->style()->effectiveZoom());

    // Update the various states we respond to.
    updateActiveState(popupButton, o);
    updateCheckedState(popupButton, o);
    updateEnabledState(popupButton, o);
    updatePressedState(popupButton, o);
#if BUTTON_CELL_DRAW_WITH_FRAME_DRAWS_FOCUS_RING
    updateFocusedState(popupButton, o);
#endif
}

const IntSize* RenderThemeChromiumMac::menuListSizes() const
{
    static const IntSize sizes[3] = { IntSize(9, 0), IntSize(5, 0), IntSize(0, 0) };
    return sizes;
}

int RenderThemeChromiumMac::minimumMenuListSize(RenderStyle* style) const
{
    return sizeForSystemFont(style, menuListSizes()).width();
}

const int sliderTrackWidth = 5;
const int sliderTrackBorderWidth = 1;

bool RenderThemeChromiumMac::paintSliderTrack(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    paintSliderTicks(o, paintInfo, r);

    float zoomLevel = o->style()->effectiveZoom();
    FloatRect unzoomedRect = r;

    if (o->style()->appearance() ==  SliderHorizontalPart || o->style()->appearance() ==  MediaSliderPart) {
        unzoomedRect.setY(ceilf(unzoomedRect.y() + unzoomedRect.height() / 2 - zoomLevel * sliderTrackWidth / 2));
        unzoomedRect.setHeight(zoomLevel * sliderTrackWidth);
    } else if (o->style()->appearance() == SliderVerticalPart) {
        unzoomedRect.setX(ceilf(unzoomedRect.x() + unzoomedRect.width() / 2 - zoomLevel * sliderTrackWidth / 2));
        unzoomedRect.setWidth(zoomLevel * sliderTrackWidth);
    }

    if (zoomLevel != 1) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
    }

    GraphicsContextStateSaver stateSaver(*paintInfo.context);
    if (zoomLevel != 1) {
        paintInfo.context->translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context->scale(zoomLevel, zoomLevel);
        paintInfo.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    Color fillColor(205, 205, 205);
    Color borderGradientTopColor(109, 109, 109);
    Color borderGradientBottomColor(181, 181, 181);
    Color shadowColor(0, 0, 0, 118);

    if (!isEnabled(o)) {
        Color tintColor(255, 255, 255, 128);
        fillColor = fillColor.blend(tintColor);
        borderGradientTopColor = borderGradientTopColor.blend(tintColor);
        borderGradientBottomColor = borderGradientBottomColor.blend(tintColor);
        shadowColor = shadowColor.blend(tintColor);
    }

    Color tintColor;
    if (!isEnabled(o))
        tintColor = Color(255, 255, 255, 128);

    bool isVerticalSlider = o->style()->appearance() == SliderVerticalPart;

    int fillRadiusSize = (sliderTrackWidth - sliderTrackBorderWidth) / 2;
    IntSize fillRadius(fillRadiusSize, fillRadiusSize);
    IntRect fillBounds = enclosedIntRect(unzoomedRect);
    RoundedRect fillRect(fillBounds, fillRadius, fillRadius, fillRadius, fillRadius);
    paintInfo.context->fillRoundedRect(fillRect, fillColor);

    IntSize shadowOffset(isVerticalSlider ? 1 : 0,
                         isVerticalSlider ? 0 : 1);
    int shadowBlur = 3;
    int shadowSpread = 0;
    paintInfo.context->save();
    paintInfo.context->drawInnerShadow(fillRect, shadowColor, shadowOffset, shadowBlur, shadowSpread);
    paintInfo.context->restore();

    RefPtr<Gradient> borderGradient = Gradient::create(fillBounds.minXMinYCorner(),
        isVerticalSlider ? fillBounds.maxXMinYCorner() : fillBounds.minXMaxYCorner());
    borderGradient->addColorStop(0.0, borderGradientTopColor);
    borderGradient->addColorStop(1.0, borderGradientBottomColor);
    Path borderPath;
    FloatRect borderRect(unzoomedRect);
    borderRect.inflate(-sliderTrackBorderWidth / 2.0);
    float borderRadiusSize = (isVerticalSlider ? borderRect.width() : borderRect.height()) / 2;
    FloatSize borderRadius(borderRadiusSize, borderRadiusSize);
    borderPath.addRoundedRect(borderRect, borderRadius, borderRadius, borderRadius, borderRadius);
    paintInfo.context->setStrokeGradient(borderGradient);
    paintInfo.context->setStrokeThickness(sliderTrackBorderWidth);
    paintInfo.context->strokePath(borderPath);
    return false;
}

const int sliderThumbWidth = 15;
const int sliderThumbHeight = 15;
const int sliderThumbBorderWidth = 1;

bool RenderThemeChromiumMac::paintSliderThumb(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    GraphicsContextStateSaver stateSaver(*paintInfo.context);
    float zoomLevel = o->style()->effectiveZoom();

    FloatRect unzoomedRect(r.x(), r.y(), sliderThumbWidth, sliderThumbHeight);
    if (zoomLevel != 1.0f) {
        paintInfo.context->translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context->scale(zoomLevel, zoomLevel);
        paintInfo.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    Color fillGradientTopColor(250, 250, 250);
    Color fillGradientUpperMiddleColor(244, 244, 244);
    Color fillGradientLowerMiddleColor(236, 236, 236);
    Color fillGradientBottomColor(238, 238, 238);
    Color borderGradientTopColor(151, 151, 151);
    Color borderGradientBottomColor(128, 128, 128);
    Color shadowColor(0, 0, 0, 36);

    if (!isEnabled(o)) {
        Color tintColor(255, 255, 255, 128);
        fillGradientTopColor = fillGradientTopColor.blend(tintColor);
        fillGradientUpperMiddleColor = fillGradientUpperMiddleColor.blend(tintColor);
        fillGradientLowerMiddleColor = fillGradientLowerMiddleColor.blend(tintColor);
        fillGradientBottomColor = fillGradientBottomColor.blend(tintColor);
        borderGradientTopColor = borderGradientTopColor.blend(tintColor);
        borderGradientBottomColor = borderGradientBottomColor.blend(tintColor);
        shadowColor = shadowColor.blend(tintColor);
    } else if (isPressed(o)) {
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
    borderBounds.inflate(sliderThumbBorderWidth / 2.0);

    borderBounds.inflate(-sliderThumbBorderWidth);
    FloatSize shadowOffset(0, 1);
    paintInfo.context->setShadow(shadowOffset, sliderThumbShadowBlur, shadowColor);
    paintInfo.context->setFillColor(Color::black);
    paintInfo.context->fillEllipse(borderBounds);
    paintInfo.context->clearShadow();

    IntRect fillBounds = enclosedIntRect(unzoomedRect);
    RefPtr<Gradient> fillGradient = Gradient::create(fillBounds.minXMinYCorner(), fillBounds.minXMaxYCorner());
    fillGradient->addColorStop(0.0, fillGradientTopColor);
    fillGradient->addColorStop(0.52, fillGradientUpperMiddleColor);
    fillGradient->addColorStop(0.52, fillGradientLowerMiddleColor);
    fillGradient->addColorStop(1.0, fillGradientBottomColor);
    paintInfo.context->setFillGradient(fillGradient);
    paintInfo.context->fillEllipse(borderBounds);

    RefPtr<Gradient> borderGradient = Gradient::create(fillBounds.minXMinYCorner(), fillBounds.minXMaxYCorner());
    borderGradient->addColorStop(0.0, borderGradientTopColor);
    borderGradient->addColorStop(1.0, borderGradientBottomColor);
    paintInfo.context->setStrokeGradient(borderGradient);
    paintInfo.context->setStrokeThickness(sliderThumbBorderWidth);
    paintInfo.context->strokeEllipse(borderBounds);

    if (isFocused(o)) {
        Path borderPath;
        borderPath.addEllipse(borderBounds);
        paintInfo.context->drawFocusRing(borderPath, 5, -2, focusRingColor());
    }

    return false;
}

bool RenderThemeChromiumMac::paintSearchField(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext localContext(paintInfo.context);

    NSSearchFieldCell* search = this->search();
    setSearchCellState(o, r);
    [search setControlSize:searchFieldControlSizeForFont(o->style())];

    GraphicsContextStateSaver stateSaver(*paintInfo.context);

    float zoomLevel = o->style()->effectiveZoom();

    IntRect unzoomedRect = r;

    if (zoomLevel != 1.0f) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        paintInfo.context->translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context->scale(zoomLevel, zoomLevel);
        paintInfo.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    // Set the search button to nil before drawing.  Then reset it so we can draw it later.
    [search setSearchButtonCell:nil];

    [search drawWithFrame:NSRect(unzoomedRect) inView:documentViewFor(o)];

    [search setControlView:nil];
    [search resetSearchButtonCell];

    return false;
}

void RenderThemeChromiumMac::setSearchCellState(RenderObject* o, const IntRect&)
{
    NSSearchFieldCell* search = this->search();

    // Update the various states we respond to.
    updateActiveState(search, o);
    updateEnabledState(search, o);
    updateFocusedState(search, o);
}

const IntSize* RenderThemeChromiumMac::searchFieldSizes() const
{
    static const IntSize sizes[3] = { IntSize(0, 22), IntSize(0, 19), IntSize(0, 15) };
    return sizes;
}

static const int* searchFieldHorizontalPaddings()
{
    static const int sizes[3] = { 3, 2, 1 };
    return sizes;
}

void RenderThemeChromiumMac::setSearchFieldSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // Use the font size to determine the intrinsic width of the control.
    setSizeFromFont(style, searchFieldSizes());
}

const int searchFieldBorderWidth = 2;
void RenderThemeChromiumMac::adjustSearchFieldStyle(RenderStyle* style, Element*) const
{
    // Override border.
    style->resetBorder();
    const short borderWidth = searchFieldBorderWidth * style->effectiveZoom();
    style->setBorderLeftWidth(borderWidth);
    style->setBorderLeftStyle(INSET);
    style->setBorderRightWidth(borderWidth);
    style->setBorderRightStyle(INSET);
    style->setBorderBottomWidth(borderWidth);
    style->setBorderBottomStyle(INSET);
    style->setBorderTopWidth(borderWidth);
    style->setBorderTopStyle(INSET);

    // Override height.
    style->setHeight(Length(Auto));
    setSearchFieldSize(style);

    NSControlSize controlSize = controlSizeForFont(style);

    // Override padding size to match AppKit text positioning.
    const int verticalPadding = 1 * style->effectiveZoom();
    const int horizontalPadding = searchFieldHorizontalPaddings()[controlSize] * style->effectiveZoom();
    style->setPaddingLeft(Length(horizontalPadding, Fixed));
    style->setPaddingRight(Length(horizontalPadding, Fixed));
    style->setPaddingTop(Length(verticalPadding, Fixed));
    style->setPaddingBottom(Length(verticalPadding, Fixed));

    setFontFromControlSize(style, controlSize);

    style->setBoxShadow(nullptr);
}

bool RenderThemeChromiumMac::paintSearchFieldCancelButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    if (!o->node())
        return false;
    Element* input = o->node()->shadowHost();
    if (!input)
        input = toElement(o->node());

    if (!input->renderer()->isBox())
        return false;

    GraphicsContextStateSaver stateSaver(*paintInfo.context);

    float zoomLevel = o->style()->effectiveZoom();
    FloatRect unzoomedRect(r);
    if (zoomLevel != 1.0f) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        paintInfo.context->translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context->scale(zoomLevel, zoomLevel);
        paintInfo.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    Color fillColor(200, 200, 200);

    if (isPressed(o)) {
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

    paintInfo.context->translate(centerX, centerY);
    paintInfo.context->rotate(deg2rad(45.0));
    paintInfo.context->clipOut(xPath);
    paintInfo.context->rotate(deg2rad(-45.0));
    paintInfo.context->translate(-centerX, -centerY);

    paintInfo.context->setFillColor(fillColor);
    paintInfo.context->fillEllipse(unzoomedRect);

    return false;
}

const IntSize* RenderThemeChromiumMac::cancelButtonSizes() const
{
    static const IntSize sizes[3] = { IntSize(14, 14), IntSize(11, 11), IntSize(9, 9) };
    return sizes;
}

void RenderThemeChromiumMac::adjustSearchFieldCancelButtonStyle(RenderStyle* style, Element*) const
{
    IntSize size = sizeForSystemFont(style, cancelButtonSizes());
    style->setWidth(Length(size.width(), Fixed));
    style->setHeight(Length(size.height(), Fixed));
    style->setBoxShadow(nullptr);
}

const IntSize* RenderThemeChromiumMac::resultsButtonSizes() const
{
    static const IntSize sizes[3] = { IntSize(15, 14), IntSize(16, 13), IntSize(14, 11) };
    return sizes;
}

void RenderThemeChromiumMac::adjustSearchFieldDecorationStyle(RenderStyle* style, Element*) const
{
    NSControlSize controlSize = controlSizeForSystemFont(style);
    IntSize searchFieldSize = searchFieldSizes()[controlSize];
    int width = searchFieldSize.height() / 2 - searchFieldBorderWidth - searchFieldHorizontalPaddings()[controlSize];
    style->setWidth(Length(width, Fixed));
    style->setHeight(Length(0, Fixed));
    style->setBoxShadow(nullptr);
}

bool RenderThemeChromiumMac::paintSearchFieldDecoration(RenderObject*, const PaintInfo&, const IntRect&)
{
    return false;
}

void RenderThemeChromiumMac::adjustSearchFieldResultsDecorationStyle(RenderStyle* style, Element*) const
{
    IntSize size = sizeForSystemFont(style, resultsButtonSizes());
    style->setWidth(Length(size.width(), Fixed));
    style->setHeight(Length(size.height(), Fixed));
    style->setBoxShadow(nullptr);
}

bool RenderThemeChromiumMac::paintSearchFieldResultsDecoration(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    if (!o->node())
        return false;
    Node* input = o->node()->shadowHost();
    if (!input)
        input = o->node();
    if (!input->renderer()->isBox())
        return false;

    GraphicsContextStateSaver stateSaver(*paintInfo.context);

    float zoomLevel = o->style()->effectiveZoom();
    FloatRect unzoomedRect(r);
    if (zoomLevel != 1) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        paintInfo.context->translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context->scale(zoomLevel, zoomLevel);
        paintInfo.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    LocalCurrentGraphicsContext localContext(paintInfo.context);

    NSSearchFieldCell* search = this->search();
    setSearchCellState(input->renderer(), r);
    [search setControlSize:searchFieldControlSizeForFont(o->style())];
    if ([search searchMenuTemplate] != nil)
        [search setSearchMenuTemplate:nil];

    updateActiveState([search searchButtonCell], o);

    [[search searchButtonCell] drawWithFrame:unzoomedRect inView:documentViewFor(o)];
    [[search searchButtonCell] setControlView:nil];
    return false;
}

IntSize RenderThemeChromiumMac::sliderTickSize() const
{
    return IntSize(1, 3);
}

int RenderThemeChromiumMac::sliderTickOffsetFromTrackCenter() const
{
    return -9;
}

void RenderThemeChromiumMac::adjustSliderThumbSize(RenderStyle* style, Element*) const
{
    float zoomLevel = style->effectiveZoom();
    if (style->appearance() == SliderThumbHorizontalPart || style->appearance() == SliderThumbVerticalPart) {
        style->setWidth(Length(static_cast<int>(sliderThumbWidth * zoomLevel), Fixed));
        style->setHeight(Length(static_cast<int>(sliderThumbHeight * zoomLevel), Fixed));
    }

    adjustMediaSliderThumbSize(style);
}

NSPopUpButtonCell* RenderThemeChromiumMac::popupButton() const
{
    if (!m_popupButton) {
        m_popupButton.adoptNS([[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:NO]);
        [m_popupButton.get() setUsesItemFromMenu:NO];
        [m_popupButton.get() setFocusRingType:NSFocusRingTypeExterior];
    }

    return m_popupButton.get();
}

NSSearchFieldCell* RenderThemeChromiumMac::search() const
{
    if (!m_search) {
        m_search.adoptNS([[NSSearchFieldCell alloc] initTextCell:@""]);
        [m_search.get() setBezelStyle:NSTextFieldRoundedBezel];
        [m_search.get() setBezeled:YES];
        [m_search.get() setEditable:YES];
        [m_search.get() setFocusRingType:NSFocusRingTypeExterior];
    }

    return m_search.get();
}

NSMenu* RenderThemeChromiumMac::searchMenuTemplate() const
{
    if (!m_searchMenuTemplate)
        m_searchMenuTemplate.adoptNS([[NSMenu alloc] initWithTitle:@""]);

    return m_searchMenuTemplate.get();
}

NSTextFieldCell* RenderThemeChromiumMac::textField() const
{
    if (!m_textField) {
        m_textField.adoptNS([[WebCoreTextFieldCell alloc] initTextCell:@""]);
        [m_textField.get() setBezeled:YES];
        [m_textField.get() setEditable:YES];
        [m_textField.get() setFocusRingType:NSFocusRingTypeExterior];
#if __MAC_OS_X_VERSION_MIN_REQUIRED <= 1070
        [m_textField.get() setDrawsBackground:YES];
        [m_textField.get() setBackgroundColor:[NSColor whiteColor]];
#else
        // Post-Lion, WebCore can be in charge of paintinng the background thanks to
        // the workaround in place for <rdar://problem/11385461>, which is implemented
        // above as _coreUIDrawOptionsWithFrame.
        [m_textField.get() setDrawsBackground:NO];
#endif
    }

    return m_textField.get();
}

String RenderThemeChromiumMac::fileListNameForWidth(Locale& locale, const FileList* fileList, const Font& font, int width) const
{
    if (width <= 0)
        return String();

    String strToTruncate;
    if (fileList->isEmpty()) {
        strToTruncate = locale.queryString(blink::WebLocalizedString::FileButtonNoFileSelectedLabel);
    } else if (fileList->length() == 1) {
        strToTruncate = [[NSFileManager defaultManager] displayNameAtPath:(fileList->item(0)->path())];
    } else {
        // FIXME: Localization of fileList->length().
        return StringTruncator::rightTruncate(locale.queryString(blink::WebLocalizedString::MultipleFileUploadText, String::number(fileList->length())), width, font, StringTruncator::EnableRoundingHacks);
    }

    return StringTruncator::centerTruncate(strToTruncate, width, font, StringTruncator::EnableRoundingHacks);
}

NSView* FlippedView()
{
    static NSView* view = [[RTCMFlippedView alloc] init];
    return view;
}

RenderTheme& RenderTheme::theme()
{
    DEFINE_STATIC_REF(RenderTheme, renderTheme, (RenderThemeChromiumMac::create()));
    return *renderTheme;
}

PassRefPtr<RenderTheme> RenderThemeChromiumMac::create()
{
    return adoptRef(new RenderThemeChromiumMac);
}

bool RenderThemeChromiumMac::usesTestModeFocusRingColor() const
{
    return isRunningLayoutTest();
}

NSView* RenderThemeChromiumMac::documentViewFor(RenderObject*) const
{
    return FlippedView();
}

// Updates the control tint (a.k.a. active state) of |cell| (from |o|).
// In the Chromium port, the renderer runs as a background process and controls'
// NSCell(s) lack a parent NSView. Therefore controls don't have their tint
// color updated correctly when the application is activated/deactivated.
// FocusController's setActive() is called when the application is
// activated/deactivated, which causes a repaint at which time this code is
// called.
// This function should be called before drawing any NSCell-derived controls,
// unless you're sure it isn't needed.
void RenderThemeChromiumMac::updateActiveState(NSCell* cell, const RenderObject* o)
{
    NSControlTint oldTint = [cell controlTint];
    NSControlTint tint = isActive(o) ? [NSColor currentControlTint] :
                                       static_cast<NSControlTint>(NSClearControlTint);

    if (tint != oldTint)
        [cell setControlTint:tint];
}

bool RenderThemeChromiumMac::shouldShowPlaceholderWhenFocused() const
{
    return true;
}

void RenderThemeChromiumMac::adjustMediaSliderThumbSize(RenderStyle* style) const
{
    RenderMediaControls::adjustMediaSliderThumbSize(style);
}

bool RenderThemeChromiumMac::paintMediaPlayButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return RenderMediaControls::paintMediaControlsPart(MediaPlayButton, object, paintInfo, rect);
}

bool RenderThemeChromiumMac::paintMediaOverlayPlayButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return RenderMediaControls::paintMediaControlsPart(MediaOverlayPlayButton, object, paintInfo, rect);
}

bool RenderThemeChromiumMac::paintMediaMuteButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return RenderMediaControls::paintMediaControlsPart(MediaMuteButton, object, paintInfo, rect);
}

bool RenderThemeChromiumMac::paintMediaSliderTrack(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return RenderMediaControls::paintMediaControlsPart(MediaSlider, object, paintInfo, rect);
}

String RenderThemeChromiumMac::extraFullScreenStyleSheet()
{
    // FIXME: Chromium may wish to style its default media controls differently in fullscreen.
    return String();
}

String RenderThemeChromiumMac::extraDefaultStyleSheet()
{
    return RenderTheme::extraDefaultStyleSheet() +
           String(themeChromiumUserAgentStyleSheet, sizeof(themeChromiumUserAgentStyleSheet)) +
           String(themeMacUserAgentStyleSheet, sizeof(themeMacUserAgentStyleSheet));
}

bool RenderThemeChromiumMac::paintMediaVolumeSliderContainer(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return true;
}

bool RenderThemeChromiumMac::paintMediaVolumeSliderTrack(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return RenderMediaControls::paintMediaControlsPart(MediaVolumeSlider, object, paintInfo, rect);
}

bool RenderThemeChromiumMac::paintMediaVolumeSliderThumb(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return RenderMediaControls::paintMediaControlsPart(MediaVolumeSliderThumb, object, paintInfo, rect);
}

bool RenderThemeChromiumMac::paintMediaSliderThumb(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return RenderMediaControls::paintMediaControlsPart(MediaSliderThumb, object, paintInfo, rect);
}

String RenderThemeChromiumMac::formatMediaControlsTime(float time) const
{
    return RenderMediaControls::formatMediaControlsTime(time);
}

String RenderThemeChromiumMac::formatMediaControlsCurrentTime(float currentTime, float duration) const
{
    return RenderMediaControls::formatMediaControlsCurrentTime(currentTime, duration);
}

bool RenderThemeChromiumMac::paintMediaFullscreenButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return RenderMediaControls::paintMediaControlsPart(MediaEnterFullscreenButton, object, paintInfo, rect);
}

bool RenderThemeChromiumMac::paintMediaToggleClosedCaptionsButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    return RenderMediaControls::paintMediaControlsPart(MediaShowClosedCaptionsButton, object, paintInfo, rect);
}

bool RenderThemeChromiumMac::shouldUseFallbackTheme(RenderStyle* style) const
{
    ControlPart part = style->appearance();
    if (part == CheckboxPart || part == RadioPart)
        return style->effectiveZoom() != 1;
    return false;
}

} // namespace WebCore

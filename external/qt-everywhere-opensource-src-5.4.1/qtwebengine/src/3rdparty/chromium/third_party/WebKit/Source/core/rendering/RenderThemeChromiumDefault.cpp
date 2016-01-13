/*
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008, 2009 Google Inc.
 * Copyright (C) 2009 Kenneth Rohde Christiansen
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
 *
 */

#include "config.h"
#include "core/rendering/RenderThemeChromiumDefault.h"

#include "core/CSSValueKeywords.h"
#include "core/UserAgentStyleSheets.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderProgress.h"
#include "platform/LayoutTestSupport.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "public/platform/Platform.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebThemeEngine.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

static bool useMockTheme()
{
    return isRunningLayoutTest();
}

unsigned RenderThemeChromiumDefault::m_activeSelectionBackgroundColor =
    0xff1e90ff;
unsigned RenderThemeChromiumDefault::m_activeSelectionForegroundColor =
    Color::black;
unsigned RenderThemeChromiumDefault::m_inactiveSelectionBackgroundColor =
    0xffc8c8c8;
unsigned RenderThemeChromiumDefault::m_inactiveSelectionForegroundColor =
    0xff323232;

double RenderThemeChromiumDefault::m_caretBlinkInterval;

static const unsigned defaultButtonBackgroundColor = 0xffdddddd;

static blink::WebThemeEngine::State getWebThemeState(const RenderTheme* theme, const RenderObject* o)
{
    if (!theme->isEnabled(o))
        return blink::WebThemeEngine::StateDisabled;
    if (useMockTheme() && theme->isReadOnlyControl(o))
        return blink::WebThemeEngine::StateReadonly;
    if (theme->isPressed(o))
        return blink::WebThemeEngine::StatePressed;
    if (useMockTheme() && theme->isFocused(o))
        return blink::WebThemeEngine::StateFocused;
    if (theme->isHovered(o))
        return blink::WebThemeEngine::StateHover;

    return blink::WebThemeEngine::StateNormal;
}

PassRefPtr<RenderTheme> RenderThemeChromiumDefault::create()
{
    return adoptRef(new RenderThemeChromiumDefault());
}

// RenderTheme::theme for Android is defined in RenderThemeChromiumAndroid.cpp.
#if !OS(ANDROID)
RenderTheme& RenderTheme::theme()
{
    DEFINE_STATIC_REF(RenderTheme, renderTheme, (RenderThemeChromiumDefault::create()));
    return *renderTheme;
}
#endif

RenderThemeChromiumDefault::RenderThemeChromiumDefault()
{
    m_caretBlinkInterval = RenderTheme::caretBlinkInterval();
}

RenderThemeChromiumDefault::~RenderThemeChromiumDefault()
{
}

bool RenderThemeChromiumDefault::supportsFocusRing(const RenderStyle* style) const
{
    if (useMockTheme()) {
        // Don't use focus rings for buttons when mocking controls.
        return style->appearance() == ButtonPart
            || style->appearance() == PushButtonPart
            || style->appearance() == SquareButtonPart;
    }

    return RenderThemeChromiumSkia::supportsFocusRing(style);
}

Color RenderThemeChromiumDefault::systemColor(CSSValueID cssValueId) const
{
    static const Color defaultButtonGrayColor(0xffdddddd);
    static const Color defaultMenuColor(0xfff7f7f7);

    if (cssValueId == CSSValueButtonface) {
        if (useMockTheme())
            return Color(0xc0, 0xc0, 0xc0);
        return defaultButtonGrayColor;
    }
    if (cssValueId == CSSValueMenu)
        return defaultMenuColor;
    return RenderTheme::systemColor(cssValueId);
}

String RenderThemeChromiumDefault::extraDefaultStyleSheet()
{
#if !OS(WIN)
    return RenderTheme::extraDefaultStyleSheet() +
        RenderThemeChromiumSkia::extraDefaultStyleSheet() +
        String(themeChromiumLinuxUserAgentStyleSheet, sizeof(themeChromiumLinuxUserAgentStyleSheet));
#else
    return RenderTheme::extraDefaultStyleSheet() +
        RenderThemeChromiumSkia::extraDefaultStyleSheet();
#endif
}

bool RenderThemeChromiumDefault::controlSupportsTints(const RenderObject* o) const
{
    return isEnabled(o);
}

Color RenderThemeChromiumDefault::activeListBoxSelectionBackgroundColor() const
{
    return Color(0x28, 0x28, 0x28);
}

Color RenderThemeChromiumDefault::activeListBoxSelectionForegroundColor() const
{
    return Color::black;
}

Color RenderThemeChromiumDefault::inactiveListBoxSelectionBackgroundColor() const
{
    return Color(0xc8, 0xc8, 0xc8);
}

Color RenderThemeChromiumDefault::inactiveListBoxSelectionForegroundColor() const
{
    return Color(0x32, 0x32, 0x32);
}

Color RenderThemeChromiumDefault::platformActiveSelectionBackgroundColor() const
{
    if (useMockTheme())
        return Color(0x00, 0x00, 0xff); // Royal blue.
    return m_activeSelectionBackgroundColor;
}

Color RenderThemeChromiumDefault::platformInactiveSelectionBackgroundColor() const
{
    if (useMockTheme())
        return Color(0x99, 0x99, 0x99); // Medium gray.
    return m_inactiveSelectionBackgroundColor;
}

Color RenderThemeChromiumDefault::platformActiveSelectionForegroundColor() const
{
    if (useMockTheme())
        return Color(0xff, 0xff, 0xcc); // Pale yellow.
    return m_activeSelectionForegroundColor;
}

Color RenderThemeChromiumDefault::platformInactiveSelectionForegroundColor() const
{
    if (useMockTheme())
        return Color::white;
    return m_inactiveSelectionForegroundColor;
}

IntSize RenderThemeChromiumDefault::sliderTickSize() const
{
    if (useMockTheme())
        return IntSize(1, 3);
    return IntSize(1, 6);
}

int RenderThemeChromiumDefault::sliderTickOffsetFromTrackCenter() const
{
    if (useMockTheme())
        return 11;
    return -16;
}

void RenderThemeChromiumDefault::adjustSliderThumbSize(RenderStyle* style, Element* element) const
{
    IntSize size = blink::Platform::current()->themeEngine()->getSize(blink::WebThemeEngine::PartSliderThumb);

    // FIXME: Mock theme doesn't handle zoomed sliders.
    float zoomLevel = useMockTheme() ? 1 : style->effectiveZoom();
    if (style->appearance() == SliderThumbHorizontalPart) {
        style->setWidth(Length(size.width() * zoomLevel, Fixed));
        style->setHeight(Length(size.height() * zoomLevel, Fixed));
    } else if (style->appearance() == SliderThumbVerticalPart) {
        style->setWidth(Length(size.height() * zoomLevel, Fixed));
        style->setHeight(Length(size.width() * zoomLevel, Fixed));
    } else
        RenderThemeChromiumSkia::adjustSliderThumbSize(style, element);
}

void RenderThemeChromiumDefault::setCaretBlinkInterval(double interval)
{
    m_caretBlinkInterval = interval;
}

double RenderThemeChromiumDefault::caretBlinkIntervalInternal() const
{
    return m_caretBlinkInterval;
}

void RenderThemeChromiumDefault::setSelectionColors(
    unsigned activeBackgroundColor,
    unsigned activeForegroundColor,
    unsigned inactiveBackgroundColor,
    unsigned inactiveForegroundColor)
{
    m_activeSelectionBackgroundColor = activeBackgroundColor;
    m_activeSelectionForegroundColor = activeForegroundColor;
    m_inactiveSelectionBackgroundColor = inactiveBackgroundColor;
    m_inactiveSelectionForegroundColor = inactiveForegroundColor;
}

bool RenderThemeChromiumDefault::paintCheckbox(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    if (i.context->paintingDisabled())
        return false;
    blink::WebThemeEngine::ExtraParams extraParams;
    blink::WebCanvas* canvas = i.context->canvas();
    extraParams.button.checked = isChecked(o);
    extraParams.button.indeterminate = isIndeterminate(o);

    float zoomLevel = o->style()->effectiveZoom();
    GraphicsContextStateSaver stateSaver(*i.context, false);
    IntRect unzoomedRect = rect;
    if (zoomLevel != 1) {
        stateSaver.save();
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        i.context->translate(unzoomedRect.x(), unzoomedRect.y());
        i.context->scale(zoomLevel, zoomLevel);
        i.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    blink::Platform::current()->themeEngine()->paint(canvas, blink::WebThemeEngine::PartCheckbox, getWebThemeState(this, o), blink::WebRect(unzoomedRect), &extraParams);
    return false;
}

void RenderThemeChromiumDefault::setCheckboxSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    IntSize size = blink::Platform::current()->themeEngine()->getSize(blink::WebThemeEngine::PartCheckbox);
    float zoomLevel = style->effectiveZoom();
    size.setWidth(size.width() * zoomLevel);
    size.setHeight(size.height() * zoomLevel);
    setSizeIfAuto(style, size);
}

bool RenderThemeChromiumDefault::paintRadio(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    if (i.context->paintingDisabled())
        return false;
    blink::WebThemeEngine::ExtraParams extraParams;
    blink::WebCanvas* canvas = i.context->canvas();
    extraParams.button.checked = isChecked(o);

    blink::Platform::current()->themeEngine()->paint(canvas, blink::WebThemeEngine::PartRadio, getWebThemeState(this, o), blink::WebRect(rect), &extraParams);
    return false;
}

void RenderThemeChromiumDefault::setRadioSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    IntSize size = blink::Platform::current()->themeEngine()->getSize(blink::WebThemeEngine::PartRadio);
    float zoomLevel = style->effectiveZoom();
    size.setWidth(size.width() * zoomLevel);
    size.setHeight(size.height() * zoomLevel);
    setSizeIfAuto(style, size);
}

bool RenderThemeChromiumDefault::paintButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    if (i.context->paintingDisabled())
        return false;
    blink::WebThemeEngine::ExtraParams extraParams;
    blink::WebCanvas* canvas = i.context->canvas();
    extraParams.button.hasBorder = true;
    extraParams.button.backgroundColor = useMockTheme() ? 0xffc0c0c0 : defaultButtonBackgroundColor;
    if (o->hasBackground())
        extraParams.button.backgroundColor = o->resolveColor(CSSPropertyBackgroundColor).rgb();

    blink::Platform::current()->themeEngine()->paint(canvas, blink::WebThemeEngine::PartButton, getWebThemeState(this, o), blink::WebRect(rect), &extraParams);
    return false;
}

bool RenderThemeChromiumDefault::paintTextField(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    // WebThemeEngine does not handle border rounded corner and background image
    // so return true to draw CSS border and background.
    if (o->style()->hasBorderRadius() || o->style()->hasBackgroundImage())
        return true;
    if (i.context->paintingDisabled())
        return false;

    ControlPart part = o->style()->appearance();

    blink::WebThemeEngine::ExtraParams extraParams;
    extraParams.textField.isTextArea = part == TextAreaPart;
    extraParams.textField.isListbox = part == ListboxPart;

    blink::WebCanvas* canvas = i.context->canvas();

    Color backgroundColor = o->resolveColor(CSSPropertyBackgroundColor);
    extraParams.textField.backgroundColor = backgroundColor.rgb();

    blink::Platform::current()->themeEngine()->paint(canvas, blink::WebThemeEngine::PartTextField, getWebThemeState(this, o), blink::WebRect(rect), &extraParams);
    return false;
}

bool RenderThemeChromiumDefault::paintMenuList(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    if (!o->isBox() || i.context->paintingDisabled())
        return false;

    const int right = rect.x() + rect.width();
    const int middle = rect.y() + rect.height() / 2;

    blink::WebThemeEngine::ExtraParams extraParams;
    extraParams.menuList.arrowY = middle;
    const RenderBox* box = toRenderBox(o);
    // Match Chromium Win behaviour of showing all borders if any are shown.
    extraParams.menuList.hasBorder = box->borderRight() || box->borderLeft() || box->borderTop() || box->borderBottom();
    extraParams.menuList.hasBorderRadius = o->style()->hasBorderRadius();
    // Fallback to transparent if the specified color object is invalid.
    Color backgroundColor(Color::transparent);
    if (o->hasBackground())
        backgroundColor = o->resolveColor(CSSPropertyBackgroundColor);
    extraParams.menuList.backgroundColor = backgroundColor.rgb();

    // If we have a background image, don't fill the content area to expose the
    // parent's background. Also, we shouldn't fill the content area if the
    // alpha of the color is 0. The API of Windows GDI ignores the alpha.
    // FIXME: the normal Aura theme doesn't care about this, so we should
    // investigate if we really need fillContentArea.
    extraParams.menuList.fillContentArea = !o->style()->hasBackgroundImage() && backgroundColor.alpha();

    if (useMockTheme()) {
        // The size and position of the drop-down button is different between
        // the mock theme and the regular aura theme.
        int spacingTop = box->borderTop() + box->paddingTop();
        int spacingBottom = box->borderBottom() + box->paddingBottom();
        int spacingRight = box->borderRight() + box->paddingRight();
        extraParams.menuList.arrowX = (o->style()->direction() == RTL) ? rect.x() + 4 + spacingRight: right - 13 - spacingRight;
        extraParams.menuList.arrowHeight = rect.height() - spacingBottom - spacingTop;
    } else {
        extraParams.menuList.arrowX = (o->style()->direction() == RTL) ? rect.x() + 7 : right - 13;
    }

    blink::WebCanvas* canvas = i.context->canvas();

    blink::Platform::current()->themeEngine()->paint(canvas, blink::WebThemeEngine::PartMenuList, getWebThemeState(this, o), blink::WebRect(rect), &extraParams);
    return false;
}

bool RenderThemeChromiumDefault::paintMenuListButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    if (!o->isBox() || i.context->paintingDisabled())
        return false;

    const int right = rect.x() + rect.width();
    const int middle = rect.y() + rect.height() / 2;

    blink::WebThemeEngine::ExtraParams extraParams;
    extraParams.menuList.arrowY = middle;
    extraParams.menuList.hasBorder = false;
    extraParams.menuList.hasBorderRadius = o->style()->hasBorderRadius();
    extraParams.menuList.backgroundColor = Color::transparent;
    extraParams.menuList.fillContentArea = false;

    if (useMockTheme()) {
        const RenderBox* box = toRenderBox(o);
        // The size and position of the drop-down button is different between
        // the mock theme and the regular aura theme.
        int spacingTop = box->borderTop() + box->paddingTop();
        int spacingBottom = box->borderBottom() + box->paddingBottom();
        int spacingRight = box->borderRight() + box->paddingRight();
        extraParams.menuList.arrowX = (o->style()->direction() == RTL) ? rect.x() + 4 + spacingRight: right - 13 - spacingRight;
        extraParams.menuList.arrowHeight = rect.height() - spacingBottom - spacingTop;
    } else {
        extraParams.menuList.arrowX = (o->style()->direction() == RTL) ? rect.x() + 7 : right - 13;
    }

    blink::WebCanvas* canvas = i.context->canvas();

    blink::Platform::current()->themeEngine()->paint(canvas, blink::WebThemeEngine::PartMenuList, getWebThemeState(this, o), blink::WebRect(rect), &extraParams);
    return false;
}

bool RenderThemeChromiumDefault::paintSliderTrack(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    if (i.context->paintingDisabled())
        return false;
    blink::WebThemeEngine::ExtraParams extraParams;
    blink::WebCanvas* canvas = i.context->canvas();
    extraParams.slider.vertical = o->style()->appearance() == SliderVerticalPart;

    paintSliderTicks(o, i, rect);

    // FIXME: Mock theme doesn't handle zoomed sliders.
    float zoomLevel = useMockTheme() ? 1 : o->style()->effectiveZoom();
    GraphicsContextStateSaver stateSaver(*i.context, false);
    IntRect unzoomedRect = rect;
    if (zoomLevel != 1) {
        stateSaver.save();
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        i.context->translate(unzoomedRect.x(), unzoomedRect.y());
        i.context->scale(zoomLevel, zoomLevel);
        i.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    blink::Platform::current()->themeEngine()->paint(canvas, blink::WebThemeEngine::PartSliderTrack, getWebThemeState(this, o), blink::WebRect(unzoomedRect), &extraParams);

    return false;
}

bool RenderThemeChromiumDefault::paintSliderThumb(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    if (i.context->paintingDisabled())
        return false;
    blink::WebThemeEngine::ExtraParams extraParams;
    blink::WebCanvas* canvas = i.context->canvas();
    extraParams.slider.vertical = o->style()->appearance() == SliderThumbVerticalPart;
    extraParams.slider.inDrag = isPressed(o);

    // FIXME: Mock theme doesn't handle zoomed sliders.
    float zoomLevel = useMockTheme() ? 1 : o->style()->effectiveZoom();
    GraphicsContextStateSaver stateSaver(*i.context, false);
    IntRect unzoomedRect = rect;
    if (zoomLevel != 1) {
        stateSaver.save();
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        i.context->translate(unzoomedRect.x(), unzoomedRect.y());
        i.context->scale(zoomLevel, zoomLevel);
        i.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }

    blink::Platform::current()->themeEngine()->paint(canvas, blink::WebThemeEngine::PartSliderThumb, getWebThemeState(this, o), blink::WebRect(unzoomedRect), &extraParams);
    return false;
}

void RenderThemeChromiumDefault::adjustInnerSpinButtonStyle(RenderStyle* style, Element*) const
{
    IntSize size = blink::Platform::current()->themeEngine()->getSize(blink::WebThemeEngine::PartInnerSpinButton);

    style->setWidth(Length(size.width(), Fixed));
    style->setMinWidth(Length(size.width(), Fixed));
}

bool RenderThemeChromiumDefault::paintInnerSpinButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    if (i.context->paintingDisabled())
        return false;
    blink::WebThemeEngine::ExtraParams extraParams;
    blink::WebCanvas* canvas = i.context->canvas();
    extraParams.innerSpin.spinUp = (controlStatesForRenderer(o) & SpinUpControlState);
    extraParams.innerSpin.readOnly = isReadOnlyControl(o);

    blink::Platform::current()->themeEngine()->paint(canvas, blink::WebThemeEngine::PartInnerSpinButton, getWebThemeState(this, o), blink::WebRect(rect), &extraParams);
    return false;
}

bool RenderThemeChromiumDefault::paintProgressBar(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    if (!o->isProgress())
        return true;
    if (i.context->paintingDisabled())
        return false;

    RenderProgress* renderProgress = toRenderProgress(o);
    IntRect valueRect = progressValueRectFor(renderProgress, rect);

    blink::WebThemeEngine::ExtraParams extraParams;
    extraParams.progressBar.determinate = renderProgress->isDeterminate();
    extraParams.progressBar.valueRectX = valueRect.x();
    extraParams.progressBar.valueRectY = valueRect.y();
    extraParams.progressBar.valueRectWidth = valueRect.width();
    extraParams.progressBar.valueRectHeight = valueRect.height();

    DirectionFlippingScope scope(o, i, rect);
    blink::WebCanvas* canvas = i.context->canvas();
    blink::Platform::current()->themeEngine()->paint(canvas, blink::WebThemeEngine::PartProgressBar, getWebThemeState(this, o), blink::WebRect(rect), &extraParams);
    return false;
}

bool RenderThemeChromiumDefault::shouldOpenPickerWithF4Key() const
{
    return true;
}

bool RenderThemeChromiumDefault::shouldUseFallbackTheme(RenderStyle* style) const
{
    if (useMockTheme()) {
        // The mock theme can't handle zoomed controls, so we fall back to the "fallback" theme.
        ControlPart part = style->appearance();
        if (part == CheckboxPart || part == RadioPart)
            return style->effectiveZoom() != 1;
    }
    return RenderTheme::shouldUseFallbackTheme(style);
}

} // namespace WebCore

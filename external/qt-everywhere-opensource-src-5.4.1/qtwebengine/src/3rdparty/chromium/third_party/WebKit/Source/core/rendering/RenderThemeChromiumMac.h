/*
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
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
 *
 */

#ifndef RenderThemeChromiumMac_h
#define RenderThemeChromiumMac_h

#import "core/rendering/RenderTheme.h"
#import "wtf/HashMap.h"
#import "wtf/RetainPtr.h"

OBJC_CLASS WebCoreRenderThemeNotificationObserver;

namespace WebCore {

class RenderThemeChromiumMac FINAL : public RenderTheme {
public:
    static PassRefPtr<RenderTheme> create();

    // A method asking if the control changes its tint when the window has focus or not.
    virtual bool controlSupportsTints(const RenderObject*) const OVERRIDE;

    // A general method asking if any control tinting is supported at all.
    virtual bool supportsControlTints() const OVERRIDE { return true; }

    virtual void adjustRepaintRect(const RenderObject*, IntRect&) OVERRIDE;

    virtual bool isControlStyled(const RenderStyle*, const CachedUAStyle*) const OVERRIDE;

    virtual Color platformActiveSelectionBackgroundColor() const OVERRIDE;
    virtual Color platformInactiveSelectionBackgroundColor() const OVERRIDE;
    virtual Color platformActiveSelectionForegroundColor() const OVERRIDE;
    virtual Color platformActiveListBoxSelectionBackgroundColor() const OVERRIDE;
    virtual Color platformActiveListBoxSelectionForegroundColor() const OVERRIDE;
    virtual Color platformInactiveListBoxSelectionBackgroundColor() const OVERRIDE;
    virtual Color platformInactiveListBoxSelectionForegroundColor() const OVERRIDE;
    virtual Color platformFocusRingColor() const OVERRIDE;

    virtual ScrollbarControlSize scrollbarControlSizeForPart(ControlPart) OVERRIDE { return SmallScrollbar; }

    virtual void platformColorsDidChange() OVERRIDE;

    // System fonts.
    virtual void systemFont(CSSValueID, FontDescription&) const OVERRIDE;

    virtual int minimumMenuListSize(RenderStyle*) const OVERRIDE;

    virtual void adjustSliderThumbSize(RenderStyle*, Element*) const OVERRIDE;

    virtual IntSize sliderTickSize() const OVERRIDE;
    virtual int sliderTickOffsetFromTrackCenter() const OVERRIDE;

    virtual int popupInternalPaddingLeft(RenderStyle*) const OVERRIDE;
    virtual int popupInternalPaddingRight(RenderStyle*) const OVERRIDE;
    virtual int popupInternalPaddingTop(RenderStyle*) const OVERRIDE;
    virtual int popupInternalPaddingBottom(RenderStyle*) const OVERRIDE;

    virtual bool paintCapsLockIndicator(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    virtual bool popsMenuByArrowKeys() const OVERRIDE { return true; }

    virtual IntSize meterSizeForBounds(const RenderMeter*, const IntRect&) const OVERRIDE;
    virtual bool paintMeter(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool supportsMeter(ControlPart) const OVERRIDE;

    // Returns the repeat interval of the animation for the progress bar.
    virtual double animationRepeatIntervalForProgressBar(RenderProgress*) const OVERRIDE;
    // Returns the duration of the animation for the progress bar.
    virtual double animationDurationForProgressBar(RenderProgress*) const OVERRIDE;

    virtual Color systemColor(CSSValueID) const OVERRIDE;

    virtual bool supportsSelectionForegroundColors() const OVERRIDE { return false; }

protected:
    RenderThemeChromiumMac();
    virtual ~RenderThemeChromiumMac();

    virtual bool paintTextField(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    virtual bool paintTextArea(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    virtual bool paintMenuList(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual void adjustMenuListStyle(RenderStyle*, Element*) const OVERRIDE;

    virtual bool paintMenuListButton(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual void adjustMenuListButtonStyle(RenderStyle*, Element*) const OVERRIDE;

    virtual bool paintProgressBar(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    virtual bool paintSliderTrack(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    virtual bool paintSliderThumb(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    virtual bool paintSearchField(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual void adjustSearchFieldStyle(RenderStyle*, Element*) const OVERRIDE;

    virtual void adjustSearchFieldCancelButtonStyle(RenderStyle*, Element*) const OVERRIDE;
    virtual bool paintSearchFieldCancelButton(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    virtual void adjustSearchFieldDecorationStyle(RenderStyle*, Element*) const OVERRIDE;
    virtual bool paintSearchFieldDecoration(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    virtual void adjustSearchFieldResultsDecorationStyle(RenderStyle*, Element*) const OVERRIDE;
    virtual bool paintSearchFieldResultsDecoration(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

private:
    virtual String fileListNameForWidth(Locale&, const FileList*, const Font&, int width) const OVERRIDE;

    IntRect inflateRect(const IntRect&, const IntSize&, const int* margins, float zoomLevel = 1.0f) const;

    FloatRect convertToPaintingRect(const RenderObject* inputRenderer, const RenderObject* partRenderer, const FloatRect& inputRect, const IntRect&) const;

    // Get the control size based off the font. Used by some of the controls (like buttons).
    NSControlSize controlSizeForFont(RenderStyle*) const;
    NSControlSize controlSizeForSystemFont(RenderStyle*) const;
    void setControlSize(NSCell*, const IntSize* sizes, const IntSize& minSize, float zoomLevel = 1.0f);
    void setSizeFromFont(RenderStyle*, const IntSize* sizes) const;
    IntSize sizeForFont(RenderStyle*, const IntSize* sizes) const;
    IntSize sizeForSystemFont(RenderStyle*, const IntSize* sizes) const;
    void setFontFromControlSize(RenderStyle*, NSControlSize) const;

    void updateCheckedState(NSCell*, const RenderObject*);
    void updateEnabledState(NSCell*, const RenderObject*);
    void updateFocusedState(NSCell*, const RenderObject*);
    void updatePressedState(NSCell*, const RenderObject*);

    // Helpers for adjusting appearance and for painting

    void setPopupButtonCellState(const RenderObject*, const IntRect&);
    const IntSize* popupButtonSizes() const;
    const int* popupButtonMargins() const;
    const int* popupButtonPadding(NSControlSize) const;
    const IntSize* menuListSizes() const;

    const IntSize* searchFieldSizes() const;
    const IntSize* cancelButtonSizes() const;
    const IntSize* resultsButtonSizes() const;
    void setSearchCellState(RenderObject*, const IntRect&);
    void setSearchFieldSize(RenderStyle*) const;

    NSPopUpButtonCell* popupButton() const;
    NSSearchFieldCell* search() const;
    NSMenu* searchMenuTemplate() const;
    NSTextFieldCell* textField() const;

    NSLevelIndicatorStyle levelIndicatorStyleFor(ControlPart) const;
    NSLevelIndicatorCell* levelIndicatorFor(const RenderMeter*) const;

    int minimumProgressBarHeight(RenderStyle*) const;
    const IntSize* progressBarSizes() const;
    const int* progressBarMargins(NSControlSize) const;

protected:
    virtual void adjustMediaSliderThumbSize(RenderStyle*) const;
    virtual bool paintMediaPlayButton(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintMediaOverlayPlayButton(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintMediaMuteButton(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintMediaSliderTrack(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual String extraFullScreenStyleSheet() OVERRIDE;

    virtual bool paintMediaSliderThumb(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintMediaVolumeSliderContainer(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintMediaVolumeSliderTrack(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintMediaVolumeSliderThumb(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual String formatMediaControlsTime(float time) const OVERRIDE;
    virtual String formatMediaControlsCurrentTime(float currentTime, float duration) const OVERRIDE;
    virtual bool paintMediaFullscreenButton(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintMediaToggleClosedCaptionsButton(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    // Controls color values returned from platformFocusRingColor(). systemColor() will be used when false.
    bool usesTestModeFocusRingColor() const;
    // A view associated to the contained document. Subclasses may not have such a view and return a fake.
    NSView* documentViewFor(RenderObject*) const;

    virtual bool shouldUseFallbackTheme(RenderStyle*) const OVERRIDE;

private:
    virtual void updateActiveState(NSCell*, const RenderObject*);
    virtual String extraDefaultStyleSheet() OVERRIDE;
    virtual bool shouldShowPlaceholderWhenFocused() const OVERRIDE;

    mutable RetainPtr<NSPopUpButtonCell> m_popupButton;
    mutable RetainPtr<NSSearchFieldCell> m_search;
    mutable RetainPtr<NSMenu> m_searchMenuTemplate;
    mutable RetainPtr<NSLevelIndicatorCell> m_levelIndicator;
    mutable RetainPtr<NSTextFieldCell> m_textField;

    mutable HashMap<int, RGBA32> m_systemColorCache;

    RetainPtr<WebCoreRenderThemeNotificationObserver> m_notificationObserver;
};

} // namespace WebCore

#endif // RenderThemeChromiumMac_h

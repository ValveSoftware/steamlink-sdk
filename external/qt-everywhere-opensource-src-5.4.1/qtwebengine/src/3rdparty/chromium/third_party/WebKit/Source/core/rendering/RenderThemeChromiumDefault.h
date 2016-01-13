/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008, 2009 Google, Inc.
 * All rights reserved.
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

#ifndef RenderThemeChromiumDefault_h
#define RenderThemeChromiumDefault_h

#include "core/rendering/RenderThemeChromiumSkia.h"

namespace WebCore {

class RenderThemeChromiumDefault : public RenderThemeChromiumSkia {
public:
    static PassRefPtr<RenderTheme> create();
    virtual String extraDefaultStyleSheet() OVERRIDE;

    virtual Color systemColor(CSSValueID) const OVERRIDE;

    // A method asking if the control changes its tint when the window has focus or not.
    virtual bool controlSupportsTints(const RenderObject*) const OVERRIDE;

    virtual bool supportsFocusRing(const RenderStyle*) const OVERRIDE;

    // List Box selection color
    virtual Color activeListBoxSelectionBackgroundColor() const;
    virtual Color activeListBoxSelectionForegroundColor() const;
    virtual Color inactiveListBoxSelectionBackgroundColor() const;
    virtual Color inactiveListBoxSelectionForegroundColor() const;

    virtual Color platformActiveSelectionBackgroundColor() const OVERRIDE;
    virtual Color platformInactiveSelectionBackgroundColor() const OVERRIDE;
    virtual Color platformActiveSelectionForegroundColor() const OVERRIDE;
    virtual Color platformInactiveSelectionForegroundColor() const OVERRIDE;

    virtual IntSize sliderTickSize() const OVERRIDE;
    virtual int sliderTickOffsetFromTrackCenter() const OVERRIDE;
    virtual void adjustSliderThumbSize(RenderStyle*, Element*) const OVERRIDE;

    static void setCaretBlinkInterval(double);
    virtual double caretBlinkIntervalInternal() const OVERRIDE;

    virtual bool paintCheckbox(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual void setCheckboxSize(RenderStyle*) const OVERRIDE;

    virtual bool paintRadio(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual void setRadioSize(RenderStyle*) const OVERRIDE;

    virtual bool paintButton(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintTextField(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintMenuList(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintMenuListButton(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintSliderTrack(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;
    virtual bool paintSliderThumb(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    virtual void adjustInnerSpinButtonStyle(RenderStyle*, Element*) const OVERRIDE;
    virtual bool paintInnerSpinButton(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    virtual bool popsMenuBySpaceOrReturn() const OVERRIDE FINAL { return true; }

    virtual bool paintProgressBar(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    virtual bool shouldOpenPickerWithF4Key() const OVERRIDE;

    static void setSelectionColors(unsigned activeBackgroundColor, unsigned activeForegroundColor, unsigned inactiveBackgroundColor, unsigned inactiveForegroundColor);

protected:
    RenderThemeChromiumDefault();
    virtual ~RenderThemeChromiumDefault();
    virtual bool shouldUseFallbackTheme(RenderStyle*) const OVERRIDE;

private:
    static double m_caretBlinkInterval;

    static unsigned m_activeSelectionBackgroundColor;
    static unsigned m_activeSelectionForegroundColor;
    static unsigned m_inactiveSelectionBackgroundColor;
    static unsigned m_inactiveSelectionForegroundColor;
};

} // namespace WebCore

#endif // RenderThemeChromiumDefault_h

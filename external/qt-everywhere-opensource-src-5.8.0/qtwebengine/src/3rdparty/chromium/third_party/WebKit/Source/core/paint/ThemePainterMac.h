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

#ifndef ThemePainterMac_h
#define ThemePainterMac_h

#import "core/paint/ThemePainter.h"

namespace blink {

class LayoutThemeMac;

class ThemePainterMac final : public ThemePainter {
public:
    ThemePainterMac(LayoutThemeMac&, Theme*);

private:
    bool paintCapsLockIndicator(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintTextField(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintTextArea(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintMenuList(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintMenuListButton(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintProgressBar(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintSliderTrack(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintSliderThumb(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintSearchField(const LayoutObject&, const PaintInfo&, const IntRect&) override;
    bool paintSearchFieldCancelButton(const LayoutObject&, const PaintInfo&, const IntRect&) override;

    LayoutThemeMac& m_layoutTheme;
};

} // namespace blink

#endif // ThemePainterMac_h

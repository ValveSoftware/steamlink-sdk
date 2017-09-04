/*
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Computer, Inc.
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

#ifndef ThemePainter_h
#define ThemePainter_h

#include "platform/ThemeTypes.h"
#include "wtf/Allocator.h"

namespace blink {

class IntRect;
class LayoutObject;

struct PaintInfo;

class ThemePainter {
  DISALLOW_NEW();

 public:
  explicit ThemePainter();

  // This method is called to paint the widget as a background of the
  // LayoutObject.  A widget's foreground, e.g., the text of a button, is always
  // rendered by the engine itself.  The boolean return value indicates whether
  // the CSS border/background should also be painted.
  bool paint(const LayoutObject&, const PaintInfo&, const IntRect&);
  bool paintBorderOnly(const LayoutObject&, const PaintInfo&, const IntRect&);
  bool paintDecorations(const LayoutObject&, const PaintInfo&, const IntRect&);

  virtual bool paintCapsLockIndicator(const LayoutObject&,
                                      const PaintInfo&,
                                      const IntRect&) {
    return 0;
  }
  void paintSliderTicks(const LayoutObject&, const PaintInfo&, const IntRect&);

 protected:
  virtual bool paintCheckbox(const LayoutObject&,
                             const PaintInfo&,
                             const IntRect&) {
    return true;
  }
  virtual bool paintRadio(const LayoutObject&,
                          const PaintInfo&,
                          const IntRect&) {
    return true;
  }
  virtual bool paintButton(const LayoutObject&,
                           const PaintInfo&,
                           const IntRect&) {
    return true;
  }
  virtual bool paintInnerSpinButton(const LayoutObject&,
                                    const PaintInfo&,
                                    const IntRect&) {
    return true;
  }
  virtual bool paintTextField(const LayoutObject&,
                              const PaintInfo&,
                              const IntRect&) {
    return true;
  }
  virtual bool paintTextArea(const LayoutObject&,
                             const PaintInfo&,
                             const IntRect&) {
    return true;
  }
  virtual bool paintMenuList(const LayoutObject&,
                             const PaintInfo&,
                             const IntRect&) {
    return true;
  }
  virtual bool paintMenuListButton(const LayoutObject&,
                                   const PaintInfo&,
                                   const IntRect&) {
    return true;
  }
  virtual bool paintProgressBar(const LayoutObject&,
                                const PaintInfo&,
                                const IntRect&) {
    return true;
  }
  virtual bool paintSliderTrack(const LayoutObject&,
                                const PaintInfo&,
                                const IntRect&) {
    return true;
  }
  virtual bool paintSliderThumb(const LayoutObject&,
                                const PaintInfo&,
                                const IntRect&) {
    return true;
  }
  virtual bool paintSearchField(const LayoutObject&,
                                const PaintInfo&,
                                const IntRect&) {
    return true;
  }
  virtual bool paintSearchFieldCancelButton(const LayoutObject&,
                                            const PaintInfo&,
                                            const IntRect&) {
    return true;
  }

  bool paintUsingFallbackTheme(const LayoutObject&,
                               const PaintInfo&,
                               const IntRect&);
  bool paintCheckboxUsingFallbackTheme(const LayoutObject&,
                                       const PaintInfo&,
                                       const IntRect&);
  bool paintRadioUsingFallbackTheme(const LayoutObject&,
                                    const PaintInfo&,
                                    const IntRect&);
};

}  // namespace blink

#endif  // ThemePainter_h

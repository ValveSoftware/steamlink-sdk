// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PLATFORM_FONT_H_
#define UI_GFX_PLATFORM_FONT_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {

class Font;

class GFX_EXPORT PlatformFont : public base::RefCounted<PlatformFont> {
 public:
  // Creates an appropriate PlatformFont implementation.
  static PlatformFont* CreateDefault();
  static PlatformFont* CreateFromNativeFont(NativeFont native_font);
  // Creates a PlatformFont implementation with the specified |font_name|
  // (encoded in UTF-8) and |font_size| in pixels.
  static PlatformFont* CreateFromNameAndSize(const std::string& font_name,
                                             int font_size);

  // Returns a new Font derived from the existing font.
  // |size_delta| is the size in pixels to add to the current font.
  // The style parameter specifies the new style for the font, and is a
  // bitmask of the values: BOLD, ITALIC and UNDERLINE.
  virtual Font DeriveFont(int size_delta, int style) const = 0;

  // Returns the number of vertical pixels needed to display characters from
  // the specified font.  This may include some leading, i.e. height may be
  // greater than just ascent + descent.  Specifically, the Windows and Mac
  // implementations include leading and the Linux one does not.  This may
  // need to be revisited in the future.
  virtual int GetHeight() const = 0;

  // Returns the baseline, or ascent, of the font.
  virtual int GetBaseline() const = 0;

  // Returns the cap height of the font.
  virtual int GetCapHeight() const = 0;

  // Returns the expected number of horizontal pixels needed to display the
  // specified length of characters. Call GetStringWidth() to retrieve the
  // actual number.
  virtual int GetExpectedTextWidth(int length) const = 0;

  // Returns the style of the font.
  virtual int GetStyle() const = 0;

  // Returns the specified font name in UTF-8.
  virtual std::string GetFontName() const = 0;

  // Returns the actually used font name in UTF-8.
  virtual std::string GetActualFontNameForTesting() const = 0;

  // Returns the font size in pixels.
  virtual int GetFontSize() const = 0;

  // Returns the native font handle.
  virtual NativeFont GetNativeFont() const = 0;

 protected:
  virtual ~PlatformFont() {}

 private:
  friend class base::RefCounted<PlatformFont>;
};

}  // namespace gfx

#endif  // UI_GFX_PLATFORM_FONT_H_

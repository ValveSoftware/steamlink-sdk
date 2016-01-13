// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PLATFORM_FONT_WIN_H_
#define UI_GFX_PLATFORM_FONT_WIN_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/platform_font.h"

namespace gfx {

class GFX_EXPORT PlatformFontWin : public PlatformFont {
 public:
  PlatformFontWin();
  explicit PlatformFontWin(NativeFont native_font);
  PlatformFontWin(const std::string& font_name, int font_size);

  // Dialog units to pixels conversion.
  // See http://support.microsoft.com/kb/145994 for details.
  int horizontal_dlus_to_pixels(int dlus) const {
    return dlus * font_ref_->GetDluBaseX() / 4;
  }
  int vertical_dlus_to_pixels(int dlus)  const {
    return dlus * font_ref_->height() / 8;
  }

  // Callback that returns the minimum height that should be used for
  // gfx::Fonts. Optional. If not specified, the minimum font size is 0.
  typedef int (*GetMinimumFontSizeCallback)();
  static GetMinimumFontSizeCallback get_minimum_font_size_callback;

  // Callback that adjusts a LOGFONT to meet suitability requirements of the
  // embedding application. Optional. If not specified, no adjustments are
  // performed other than clamping to a minimum font height if
  // |get_minimum_font_size_callback| is specified.
  typedef void (*AdjustFontCallback)(LOGFONT* lf);
  static AdjustFontCallback adjust_font_callback;

  // Returns the font name for the system locale. Some fonts, particularly
  // East Asian fonts, have different names per locale. If the localized font
  // name could not be retrieved, returns GetFontName().
  std::string GetLocalizedFontName() const;

  // Returns a derived Font with the specified |style| and with height at most
  // |height|. If the height and style of the receiver already match, it is
  // returned. Otherwise, the returned Font will have the largest size such that
  // its height is less than or equal to |height| (since there may not exist a
  // size that matches the exact |height| specified).
  Font DeriveFontWithHeight(int height, int style);

  // Overridden from PlatformFont:
  virtual Font DeriveFont(int size_delta, int style) const OVERRIDE;
  virtual int GetHeight() const OVERRIDE;
  virtual int GetBaseline() const OVERRIDE;
  virtual int GetCapHeight() const OVERRIDE;
  virtual int GetExpectedTextWidth(int length) const OVERRIDE;
  virtual int GetStyle() const OVERRIDE;
  virtual std::string GetFontName() const OVERRIDE;
  virtual std::string GetActualFontNameForTesting() const OVERRIDE;
  virtual int GetFontSize() const OVERRIDE;
  virtual NativeFont GetNativeFont() const OVERRIDE;

 private:
  virtual ~PlatformFontWin() {}

  // Chrome text drawing bottoms out in the Windows GDI functions that take an
  // HFONT (an opaque handle into Windows). To avoid lots of GDI object
  // allocation and destruction, Font indirectly refers to the HFONT by way of
  // an HFontRef. That is, every Font has an HFontRef, which has an HFONT.
  //
  // HFontRef is reference counted. Upon deletion, it deletes the HFONT.
  // By making HFontRef maintain the reference to the HFONT, multiple
  // HFontRefs can share the same HFONT, and Font can provide value semantics.
  class HFontRef : public base::RefCounted<HFontRef> {
   public:
    // This constructor takes control of the HFONT, and will delete it when
    // the HFontRef is deleted.
    HFontRef(HFONT hfont,
             int font_size,
             int height,
             int baseline,
             int cap_height,
             int ave_char_width,
             int style);

    // Accessors
    HFONT hfont() const { return hfont_; }
    int height() const { return height_; }
    int baseline() const { return baseline_; }
    int cap_height() const { return cap_height_; }
    int ave_char_width() const { return ave_char_width_; }
    int style() const { return style_; }
    const std::string& font_name() const { return font_name_; }
    int font_size() const { return font_size_; }
    int requested_font_size() const { return requested_font_size_; }

    // Returns the average character width in dialog units.
    int GetDluBaseX();

   private:
    friend class base::RefCounted<HFontRef>;

    ~HFontRef();

    const HFONT hfont_;
    const int font_size_;
    const int height_;
    const int baseline_;
    const int cap_height_;
    const int ave_char_width_;
    const int style_;
    // Average character width in dialog units. This is queried lazily from the
    // system, with an initial value of -1 meaning it hasn't yet been queried.
    int dlu_base_x_;
    std::string font_name_;

    // If the requested font size is not possible for the font, |font_size_|
    // will be different than |requested_font_size_|. This is stored separately
    // so that code that increases the font size in a loop will not cause the
    // loop to get stuck on the same size.
    int requested_font_size_;

    DISALLOW_COPY_AND_ASSIGN(HFontRef);
  };

  // Initializes this object with a copy of the specified HFONT.
  void InitWithCopyOfHFONT(HFONT hfont);

  // Initializes this object with the specified font name and size.
  void InitWithFontNameAndSize(const std::string& font_name,
                               int font_size);

  // Returns the base font ref. This should ONLY be invoked on the
  // UI thread.
  static HFontRef* GetBaseFontRef();

  // Creates and returns a new HFONTRef from the specified HFONT.
  static HFontRef* CreateHFontRef(HFONT font);

  // Creates a new PlatformFontWin with the specified HFontRef. Used when
  // constructing a Font from a HFONT we don't want to copy.
  explicit PlatformFontWin(HFontRef* hfont_ref);

    // Reference to the base font all fonts are derived from.
  static HFontRef* base_font_ref_;

  // Indirect reference to the HFontRef, which references the underlying HFONT.
  scoped_refptr<HFontRef> font_ref_;

  DISALLOW_COPY_AND_ASSIGN(PlatformFontWin);
};

}  // namespace gfx

#endif  // UI_GFX_PLATFORM_FONT_WIN_H_

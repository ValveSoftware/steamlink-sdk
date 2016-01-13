// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_LIST_H_
#define UI_GFX_FONT_LIST_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "ui/gfx/font.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class FontListImpl;

// FontList represents a list of fonts and provides metrics which are common
// in the fonts.  FontList is copyable and it's quite cheap to copy.
//
// The format of font description string complies with that of Pango detailed at
// http://developer.gnome.org/pango/stable/pango-Fonts.html#pango-font-description-from-string
// The format is "<FONT_FAMILY_LIST>,[STYLES] <SIZE>" where
//     FONT_FAMILY_LIST is a comma-separated list of font family names,
//     STYLES is a space-separated list of style names ("Bold" and "Italic"),
//     SIZE is a font size in pixel with the suffix "px".
// Here are examples of font description string:
//     "Arial, Helvetica, Bold Italic 14px"
//     "Arial, 14px"
class GFX_EXPORT FontList {
 public:
  // Creates a font list with default font names, size and style, which are
  // specified by SetDefaultFontDescription().
  FontList();

  // Creates a font list that is a clone of another font list.
  FontList(const FontList& other);

  // Creates a font list from a string representing font names, styles, and
  // size.
  explicit FontList(const std::string& font_description_string);

  // Creates a font list from font names, styles and size.
  FontList(const std::vector<std::string>& font_names,
           int font_style,
           int font_size);

  // Creates a font list from a Font vector.
  // All fonts in this vector should have the same style and size.
  explicit FontList(const std::vector<Font>& fonts);

  // Creates a font list from a Font.
  explicit FontList(const Font& font);

  ~FontList();

  // Copies the given font list into this object.
  FontList& operator=(const FontList& other);

  // Sets the description string for default FontList construction. If it's
  // empty, FontList will initialize using the default Font constructor.
  //
  // The client code must call this function before any call of the default
  // constructor. This should be done on the UI thread.
  //
  // ui::ResourceBundle may call this function more than once when UI language
  // is changed.
  static void SetDefaultFontDescription(const std::string& font_description);

  // Returns a new FontList with the same font names but resized and the given
  // style. |size_delta| is the size in pixels to add to the current font size.
  // |font_style| specifies the new style, which is a bitmask of the values:
  // Font::BOLD, Font::ITALIC and Font::UNDERLINE.
  FontList Derive(int size_delta, int font_style) const;

  // Returns a new FontList with the same font names and style but resized.
  // |size_delta| is the size in pixels to add to the current font size.
  FontList DeriveWithSizeDelta(int size_delta) const;

  // Returns a new FontList with the same font names and size but the given
  // style. |font_style| specifies the new style, which is a bitmask of the
  // values: Font::BOLD, Font::ITALIC and Font::UNDERLINE.
  FontList DeriveWithStyle(int font_style) const;

  // Returns the height of this font list, which is max(ascent) + max(descent)
  // for all the fonts in the font list.
  int GetHeight() const;

  // Returns the baseline of this font list, which is max(baseline) for all the
  // fonts in the font list.
  int GetBaseline() const;

  // Returns the cap height of this font list.
  // Currently returns the cap height of the primary font.
  int GetCapHeight() const;

  // Returns the expected number of horizontal pixels needed to display the
  // specified length of characters. Call GetStringWidth() to retrieve the
  // actual number.
  int GetExpectedTextWidth(int length) const;

  // Returns the |gfx::Font::FontStyle| style flags for this font list.
  int GetFontStyle() const;

  // Returns a string representing font names, styles, and size. If the FontList
  // is initialized by a vector of Font, use the first font's style and size
  // for the description.
  const std::string& GetFontDescriptionString() const;

  // Returns the font size in pixels.
  int GetFontSize() const;

  // Returns the Font vector.
  const std::vector<Font>& GetFonts() const;

  // Returns the first font in the list.
  const Font& GetPrimaryFont() const;

 private:
  explicit FontList(FontListImpl* impl);

  static const scoped_refptr<FontListImpl>& GetDefaultImpl();

  scoped_refptr<FontListImpl> impl_;
};

}  // namespace gfx

#endif  // UI_GFX_FONT_LIST_H_

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_FONT_DEV_H_
#define PPAPI_CPP_DEV_FONT_DEV_H_

#include <string>

#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

struct PP_FontDescription_Dev;

namespace pp {

class ImageData;
class InstanceHandle;
class Point;
class Rect;

// FontDescription_Dev ---------------------------------------------------------

class FontDescription_Dev {
 public:
  FontDescription_Dev();
  FontDescription_Dev(const FontDescription_Dev& other);
  ~FontDescription_Dev();

  FontDescription_Dev& operator=(const FontDescription_Dev& other);

  const PP_FontDescription_Dev& pp_font_description() const {
    return pp_font_description_;
  }

  Var face() const { return face_; }
  void set_face(const Var& face) {
    face_ = face;
    pp_font_description_.face = face_.pp_var();
  }

  PP_FontFamily_Dev family() const { return pp_font_description_.family; }
  void set_family(PP_FontFamily_Dev f) { pp_font_description_.family = f; }

  uint32_t size() const { return pp_font_description_.size; }
  void set_size(uint32_t s) { pp_font_description_.size = s; }

  PP_FontWeight_Dev weight() const { return pp_font_description_.weight; }
  void set_weight(PP_FontWeight_Dev w) { pp_font_description_.weight = w; }

  bool italic() const { return PP_ToBool(pp_font_description_.italic); }
  void set_italic(bool i) { pp_font_description_.italic = PP_FromBool(i); }

  bool small_caps() const {
    return PP_ToBool(pp_font_description_.small_caps);
  }
  void set_small_caps(bool s) {
    pp_font_description_.small_caps = PP_FromBool(s);
  }

  int letter_spacing() const { return pp_font_description_.letter_spacing; }
  void set_letter_spacing(int s) { pp_font_description_.letter_spacing = s; }

  int word_spacing() const { return pp_font_description_.word_spacing; }
  void set_word_spacing(int w) { pp_font_description_.word_spacing = w; }

 private:
  friend class Font_Dev;

  Var face_;  // Manages memory for pp_font_description_.face
  PP_FontDescription_Dev pp_font_description_;
};

// TextRun_Dev -----------------------------------------------------------------

class TextRun_Dev {
 public:
  TextRun_Dev();
  TextRun_Dev(const std::string& text,
              bool rtl = false,
              bool override_direction = false);
  TextRun_Dev(const TextRun_Dev& other);
  ~TextRun_Dev();

  TextRun_Dev& operator=(const TextRun_Dev& other);

  const PP_TextRun_Dev& pp_text_run() const {
    return pp_text_run_;
  }

 private:
  Var text_;  // Manages memory for the reference in pp_text_run_.
  PP_TextRun_Dev pp_text_run_;
};

// Font ------------------------------------------------------------------------

// Provides access to system fonts.
class Font_Dev : public Resource {
 public:
  // Creates an is_null() Font object.
  Font_Dev();

  explicit Font_Dev(PP_Resource resource);
  Font_Dev(const InstanceHandle& instance,
           const FontDescription_Dev& description);
  Font_Dev(const Font_Dev& other);

  Font_Dev& operator=(const Font_Dev& other);

  // PPB_Font methods:
  static Var GetFontFamilies(const InstanceHandle& instance);
  bool Describe(FontDescription_Dev* description,
                PP_FontMetrics_Dev* metrics) const;
  bool DrawTextAt(ImageData* dest,
                  const TextRun_Dev& text,
                  const Point& position,
                  uint32_t color,
                  const Rect& clip,
                  bool image_data_is_opaque) const;
  int32_t MeasureText(const TextRun_Dev& text) const;
  uint32_t CharacterOffsetForPixel(const TextRun_Dev& text,
                                   int32_t pixel_position) const;
  int32_t PixelOffsetForCharacter(const TextRun_Dev& text,
                                  uint32_t char_offset) const;

  // Convenience function that assumes a left-to-right string with no clipping.
  bool DrawSimpleText(ImageData* dest,
                      const std::string& text,
                      const Point& position,
                      uint32_t color,
                      bool image_data_is_opaque = false) const;

  // Convenience function that assumes a left-to-right string.
  int32_t MeasureSimpleText(const std::string& text) const;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_FONT_DEV_H_

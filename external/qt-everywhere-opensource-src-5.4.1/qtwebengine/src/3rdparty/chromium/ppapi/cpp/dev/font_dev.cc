// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/font_dev.h"

#include <algorithm>

#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Font_Dev>() {
  return PPB_FONT_DEV_INTERFACE;
}

}  // namespace

// FontDescription_Dev ---------------------------------------------------------

FontDescription_Dev::FontDescription_Dev() {
  pp_font_description_.face = face_.pp_var();
  set_family(PP_FONTFAMILY_DEFAULT);
  set_size(0);
  set_weight(PP_FONTWEIGHT_NORMAL);
  set_italic(false);
  set_small_caps(false);
  set_letter_spacing(0);
  set_word_spacing(0);
}

FontDescription_Dev::FontDescription_Dev(const FontDescription_Dev& other) {
  set_face(other.face());
  set_family(other.family());
  set_size(other.size());
  set_weight(other.weight());
  set_italic(other.italic());
  set_small_caps(other.small_caps());
  set_letter_spacing(other.letter_spacing());
  set_word_spacing(other.word_spacing());
}

FontDescription_Dev::~FontDescription_Dev() {
}

FontDescription_Dev& FontDescription_Dev::operator=(
    const FontDescription_Dev& other) {
  pp_font_description_ = other.pp_font_description_;

  // Be careful about the refcount of the string, the copy that operator= made
  // above didn't copy a ref.
  pp_font_description_.face = PP_MakeUndefined();
  set_face(other.face());

  return *this;
}

// TextRun_Dev -----------------------------------------------------------------

TextRun_Dev::TextRun_Dev() {
  pp_text_run_.text = text_.pp_var();
  pp_text_run_.rtl = PP_FALSE;
  pp_text_run_.override_direction = PP_FALSE;
}

TextRun_Dev::TextRun_Dev(const std::string& text,
                         bool rtl,
                         bool override_direction)
    : text_(text) {
  pp_text_run_.text = text_.pp_var();
  pp_text_run_.rtl = PP_FromBool(rtl);
  pp_text_run_.override_direction = PP_FromBool(override_direction);
}

TextRun_Dev::TextRun_Dev(const TextRun_Dev& other) : text_(other.text_) {
  pp_text_run_.text = text_.pp_var();
  pp_text_run_.rtl = other.pp_text_run_.rtl;
  pp_text_run_.override_direction = other.pp_text_run_.override_direction;
}

TextRun_Dev::~TextRun_Dev() {
}

TextRun_Dev& TextRun_Dev::operator=(const TextRun_Dev& other) {
  pp_text_run_ = other.pp_text_run_;
  text_ = other.text_;
  pp_text_run_.text = text_.pp_var();
  return *this;
}

// Font ------------------------------------------------------------------------

Font_Dev::Font_Dev() : Resource() {
}

Font_Dev::Font_Dev(PP_Resource resource) : Resource(resource) {
}

Font_Dev::Font_Dev(const InstanceHandle& instance,
                   const FontDescription_Dev& description) {
  if (!has_interface<PPB_Font_Dev>())
    return;
  PassRefFromConstructor(get_interface<PPB_Font_Dev>()->Create(
      instance.pp_instance(), &description.pp_font_description()));
}

Font_Dev::Font_Dev(const Font_Dev& other) : Resource(other) {
}

Font_Dev& Font_Dev::operator=(const Font_Dev& other) {
  Resource::operator=(other);
  return *this;
}

// static
Var Font_Dev::GetFontFamilies(const InstanceHandle& instance) {
  if (!has_interface<PPB_Font_Dev>())
    return Var();
  return Var(PASS_REF, get_interface<PPB_Font_Dev>()->GetFontFamilies(
                 instance.pp_instance()));
}

bool Font_Dev::Describe(FontDescription_Dev* description,
                        PP_FontMetrics_Dev* metrics) const {
  if (!has_interface<PPB_Font_Dev>())
    return false;

  // Be careful with ownership of the |face| string. It will come back with
  // a ref of 1, which we want to assign to the |face_| member of the C++ class.
  if (!get_interface<PPB_Font_Dev>()->Describe(
      pp_resource(), &description->pp_font_description_, metrics))
    return false;
  description->face_ = Var(PASS_REF,
                           description->pp_font_description_.face);

  return true;
}

bool Font_Dev::DrawTextAt(ImageData* dest,
                          const TextRun_Dev& text,
                          const Point& position,
                          uint32_t color,
                          const Rect& clip,
                          bool image_data_is_opaque) const {
  if (!has_interface<PPB_Font_Dev>())
    return false;
  return PP_ToBool(get_interface<PPB_Font_Dev>()->DrawTextAt(
      pp_resource(),
      dest->pp_resource(),
      &text.pp_text_run(),
      &position.pp_point(),
      color,
      &clip.pp_rect(),
      PP_FromBool(image_data_is_opaque)));
}

int32_t Font_Dev::MeasureText(const TextRun_Dev& text) const {
  if (!has_interface<PPB_Font_Dev>())
    return -1;
  return get_interface<PPB_Font_Dev>()->MeasureText(pp_resource(),
                                                    &text.pp_text_run());
}

uint32_t Font_Dev::CharacterOffsetForPixel(const TextRun_Dev& text,
                                           int32_t pixel_position) const {
  if (!has_interface<PPB_Font_Dev>())
    return 0;
  return get_interface<PPB_Font_Dev>()->CharacterOffsetForPixel(
      pp_resource(), &text.pp_text_run(), pixel_position);

}

int32_t Font_Dev::PixelOffsetForCharacter(const TextRun_Dev& text,
                                          uint32_t char_offset) const {
  if (!has_interface<PPB_Font_Dev>())
    return 0;
  return get_interface<PPB_Font_Dev>()->PixelOffsetForCharacter(
      pp_resource(), &text.pp_text_run(), char_offset);
}

bool Font_Dev::DrawSimpleText(ImageData* dest,
                              const std::string& text,
                              const Point& position,
                              uint32_t color,
                              bool image_data_is_opaque) const {
  return DrawTextAt(dest, TextRun_Dev(text), position, color,
                    Rect(dest->size()), image_data_is_opaque);
}

int32_t Font_Dev::MeasureSimpleText(const std::string& text) const {
  return MeasureText(TextRun_Dev(text));
}

}  // namespace pp

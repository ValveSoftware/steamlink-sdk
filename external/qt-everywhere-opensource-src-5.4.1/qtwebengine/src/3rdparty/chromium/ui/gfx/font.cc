// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/platform_font.h"

namespace gfx {

////////////////////////////////////////////////////////////////////////////////
// Font, public:

Font::Font() : platform_font_(PlatformFont::CreateDefault()) {
}

Font::Font(const Font& other) : platform_font_(other.platform_font_) {
}

Font& Font::operator=(const Font& other) {
  platform_font_ = other.platform_font_;
  return *this;
}

Font::Font(NativeFont native_font)
    : platform_font_(PlatformFont::CreateFromNativeFont(native_font)) {
}

Font::Font(PlatformFont* platform_font) : platform_font_(platform_font) {
}

Font::Font(const std::string& font_name, int font_size)
    : platform_font_(PlatformFont::CreateFromNameAndSize(font_name,
                                                         font_size)) {
}

Font::~Font() {
}

Font Font::Derive(int size_delta, int style) const {
  return platform_font_->DeriveFont(size_delta, style);
}

int Font::GetHeight() const {
  return platform_font_->GetHeight();
}

int Font::GetBaseline() const {
  return platform_font_->GetBaseline();
}

int Font::GetCapHeight() const {
  return platform_font_->GetCapHeight();
}

int Font::GetExpectedTextWidth(int length) const {
  return platform_font_->GetExpectedTextWidth(length);
}

int Font::GetStyle() const {
  return platform_font_->GetStyle();
}

std::string Font::GetFontName() const {
  return platform_font_->GetFontName();
}

std::string Font::GetActualFontNameForTesting() const {
  return platform_font_->GetActualFontNameForTesting();
}

int Font::GetFontSize() const {
  return platform_font_->GetFontSize();
}

NativeFont Font::GetNativeFont() const {
  return platform_font_->GetNativeFont();
}

}  // namespace gfx

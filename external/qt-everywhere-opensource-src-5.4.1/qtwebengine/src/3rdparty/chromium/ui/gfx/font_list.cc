// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_list.h"

#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "ui/gfx/font_list_impl.h"

namespace {

// Font description of the default font set.
base::LazyInstance<std::string>::Leaky g_default_font_description =
    LAZY_INSTANCE_INITIALIZER;

// The default instance of gfx::FontListImpl.
base::LazyInstance<scoped_refptr<gfx::FontListImpl> >::Leaky g_default_impl =
    LAZY_INSTANCE_INITIALIZER;
bool g_default_impl_initialized = false;

}  // namespace

namespace gfx {

FontList::FontList() : impl_(GetDefaultImpl()) {}

FontList::FontList(const FontList& other) : impl_(other.impl_) {}

FontList::FontList(const std::string& font_description_string)
    : impl_(new FontListImpl(font_description_string)) {}

FontList::FontList(const std::vector<std::string>& font_names,
                   int font_style,
                   int font_size)
    : impl_(new FontListImpl(font_names, font_style, font_size)) {}

FontList::FontList(const std::vector<Font>& fonts)
    : impl_(new FontListImpl(fonts)) {}

FontList::FontList(const Font& font) : impl_(new FontListImpl(font)) {}

FontList::~FontList() {}

FontList& FontList::operator=(const FontList& other) {
  impl_ = other.impl_;
  return *this;
}

// static
void FontList::SetDefaultFontDescription(const std::string& font_description) {
  // The description string must end with "px" for size in pixel, or must be
  // the empty string, which specifies to use a single default font.
  DCHECK(font_description.empty() ||
         EndsWith(font_description, "px", true));

  g_default_font_description.Get() = font_description;
  g_default_impl_initialized = false;
}

FontList FontList::Derive(int size_delta, int font_style) const {
  return FontList(impl_->Derive(size_delta, font_style));
}

FontList FontList::DeriveWithSizeDelta(int size_delta) const {
  return Derive(size_delta, GetFontStyle());
}

FontList FontList::DeriveWithStyle(int font_style) const {
  return Derive(0, font_style);
}

int FontList::GetHeight() const {
  return impl_->GetHeight();
}

int FontList::GetBaseline() const {
  return impl_->GetBaseline();
}

int FontList::GetCapHeight() const {
  return impl_->GetCapHeight();
}

int FontList::GetExpectedTextWidth(int length) const {
  return impl_->GetExpectedTextWidth(length);
}

int FontList::GetFontStyle() const {
  return impl_->GetFontStyle();
}

const std::string& FontList::GetFontDescriptionString() const {
  return impl_->GetFontDescriptionString();
}

int FontList::GetFontSize() const {
  return impl_->GetFontSize();
}

const std::vector<Font>& FontList::GetFonts() const {
  return impl_->GetFonts();
}

const Font& FontList::GetPrimaryFont() const {
  return impl_->GetPrimaryFont();
}

FontList::FontList(FontListImpl* impl) : impl_(impl) {}

// static
const scoped_refptr<FontListImpl>& FontList::GetDefaultImpl() {
  // SetDefaultFontDescription() must be called and the default font description
  // must be set earlier than any call of this function.
  DCHECK(!(g_default_font_description == NULL))  // != is not overloaded.
      << "SetDefaultFontDescription has not been called.";

  if (!g_default_impl_initialized) {
    g_default_impl.Get() =
        g_default_font_description.Get().empty() ?
            new FontListImpl(Font()) :
            new FontListImpl(g_default_font_description.Get());
    g_default_impl_initialized = true;
  }

  return g_default_impl.Get();
}

}  // namespace gfx

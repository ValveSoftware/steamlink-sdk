// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_mac.h"

#include <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"

namespace gfx {

namespace {

// Returns an autoreleased NSFont created with the passed-in specifications.
NSFont* NSFontWithSpec(const std::string& font_name,
                       int font_size,
                       int font_style) {
  NSFontSymbolicTraits trait_bits = 0;
  if (font_style & Font::BOLD)
    trait_bits |= NSFontBoldTrait;
  if (font_style & Font::ITALIC)
    trait_bits |= NSFontItalicTrait;
  // The Mac doesn't support underline as a font trait, so just drop it.
  // (Underlines must be added as an attribute on an NSAttributedString.)
  NSDictionary* traits = @{ NSFontSymbolicTrait : @(trait_bits) };

  NSDictionary* attrs = @{
    NSFontFamilyAttribute : base::SysUTF8ToNSString(font_name),
    NSFontTraitsAttribute : traits
  };
  NSFontDescriptor* descriptor =
      [NSFontDescriptor fontDescriptorWithFontAttributes:attrs];
  NSFont* font = [NSFont fontWithDescriptor:descriptor size:font_size];
  if (font)
    return font;

  // Make one fallback attempt by looking up via font name rather than font
  // family name.
  attrs = @{
    NSFontNameAttribute : base::SysUTF8ToNSString(font_name),
    NSFontTraitsAttribute : traits
  };
  descriptor = [NSFontDescriptor fontDescriptorWithFontAttributes:attrs];
  return [NSFont fontWithDescriptor:descriptor size:font_size];
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, public:

PlatformFontMac::PlatformFontMac()
    : native_font_([[NSFont systemFontOfSize:[NSFont systemFontSize]] retain]),
      font_name_(base::SysNSStringToUTF8([native_font_ familyName])),
      font_size_([NSFont systemFontSize]),
      font_style_(Font::NORMAL) {
  CalculateMetrics();
}

PlatformFontMac::PlatformFontMac(NativeFont native_font)
    : native_font_([native_font retain]),
      font_name_(base::SysNSStringToUTF8([native_font_ familyName])),
      font_size_([native_font_ pointSize]),
      font_style_(Font::NORMAL) {
  NSFontSymbolicTraits traits = [[native_font fontDescriptor] symbolicTraits];
  if (traits & NSFontItalicTrait)
    font_style_ |= Font::ITALIC;
  if (traits & NSFontBoldTrait)
    font_style_ |= Font::BOLD;

  CalculateMetrics();
}

PlatformFontMac::PlatformFontMac(const std::string& font_name,
                                 int font_size)
    : native_font_([NSFontWithSpec(font_name, font_size, Font::NORMAL) retain]),
      font_name_(font_name),
      font_size_(font_size),
      font_style_(Font::NORMAL) {
  CalculateMetrics();
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, PlatformFont implementation:

Font PlatformFontMac::DeriveFont(int size_delta, int style) const {
  return Font(new PlatformFontMac(font_name_, font_size_ + size_delta, style));
}

int PlatformFontMac::GetHeight() const {
  return height_;
}

int PlatformFontMac::GetBaseline() const {
  return ascent_;
}

int PlatformFontMac::GetCapHeight() const {
  return cap_height_;
}

int PlatformFontMac::GetExpectedTextWidth(int length) const {
  return length * average_width_;
}

int PlatformFontMac::GetStyle() const {
  return font_style_;
}

std::string PlatformFontMac::GetFontName() const {
  return font_name_;
}

std::string PlatformFontMac::GetActualFontNameForTesting() const {
  return base::SysNSStringToUTF8([native_font_ familyName]);
}

int PlatformFontMac::GetFontSize() const {
  return font_size_;
}

NativeFont PlatformFontMac::GetNativeFont() const {
  return [[native_font_.get() retain] autorelease];
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, private:

PlatformFontMac::PlatformFontMac(const std::string& font_name,
                                 int font_size,
                                 int font_style)
    : native_font_([NSFontWithSpec(font_name, font_size, font_style) retain]),
      font_name_(font_name),
      font_size_(font_size),
      font_style_(font_style) {
  CalculateMetrics();
}

PlatformFontMac::~PlatformFontMac() {
}

void PlatformFontMac::CalculateMetrics() {
  NSFont* font = native_font_.get();
  if (!font) {
    // This object was constructed from a font name that doesn't correspond to
    // an actual font. Don't waste time working out metrics.
    height_ = 0;
    ascent_ = 0;
    cap_height_ = 0;
    average_width_ = 0;
    return;
  }

  base::scoped_nsobject<NSLayoutManager> layout_manager(
      [[NSLayoutManager alloc] init]);
  height_ = [layout_manager defaultLineHeightForFont:font];
  ascent_ = [font ascender];
  cap_height_ = [font capHeight];
  average_width_ =
      NSWidth([font boundingRectForGlyph:[font glyphWithName:@"x"]]);
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFont, public:

// static
PlatformFont* PlatformFont::CreateDefault() {
  return new PlatformFontMac;
}

// static
PlatformFont* PlatformFont::CreateFromNativeFont(NativeFont native_font) {
  return new PlatformFontMac(native_font);
}

// static
PlatformFont* PlatformFont::CreateFromNameAndSize(const std::string& font_name,
                                                  int font_size) {
  return new PlatformFontMac(font_name, font_size);
}

}  // namespace gfx

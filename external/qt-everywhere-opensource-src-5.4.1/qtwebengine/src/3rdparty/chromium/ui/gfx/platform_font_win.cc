// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_win.h"

#include <windows.h>
#include <math.h>

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "base/win/win_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/win/scoped_set_map_mode.h"

namespace {

// If the tmWeight field of a TEXTMETRIC structure has a value >= this, the
// font is bold.
const int kTextMetricWeightBold = 700;

// Returns the minimum font size, using the minimum size callback, if set.
int GetMinimumFontSize() {
  int min_font_size = 0;
  if (gfx::PlatformFontWin::get_minimum_font_size_callback)
    min_font_size = gfx::PlatformFontWin::get_minimum_font_size_callback();
  return min_font_size;
}

// Returns either minimum font allowed for a current locale or
// lf_height + size_delta value.
int AdjustFontSize(int lf_height, int size_delta) {
  if (lf_height < 0) {
    lf_height -= size_delta;
  } else {
    lf_height += size_delta;
  }
  const int min_font_size = GetMinimumFontSize();
  // Make sure lf_height is not smaller than allowed min font size for current
  // locale.
  if (abs(lf_height) < min_font_size) {
    return lf_height < 0 ? -min_font_size : min_font_size;
  } else {
    return lf_height;
  }
}

// Sets style properties on |font_info| based on |font_style|.
void SetLogFontStyle(int font_style, LOGFONT* font_info) {
  font_info->lfUnderline = (font_style & gfx::Font::UNDERLINE) != 0;
  font_info->lfItalic = (font_style & gfx::Font::ITALIC) != 0;
  font_info->lfWeight = (font_style & gfx::Font::BOLD) ? FW_BOLD : FW_NORMAL;
}

}  // namespace

namespace gfx {

// static
PlatformFontWin::HFontRef* PlatformFontWin::base_font_ref_;

// static
PlatformFontWin::AdjustFontCallback
    PlatformFontWin::adjust_font_callback = NULL;
PlatformFontWin::GetMinimumFontSizeCallback
    PlatformFontWin::get_minimum_font_size_callback = NULL;

////////////////////////////////////////////////////////////////////////////////
// PlatformFontWin, public

PlatformFontWin::PlatformFontWin() : font_ref_(GetBaseFontRef()) {
}

PlatformFontWin::PlatformFontWin(NativeFont native_font) {
  InitWithCopyOfHFONT(native_font);
}

PlatformFontWin::PlatformFontWin(const std::string& font_name,
                                 int font_size) {
  InitWithFontNameAndSize(font_name, font_size);
}

Font PlatformFontWin::DeriveFontWithHeight(int height, int style) {
  DCHECK_GE(height, 0);
  if (GetHeight() == height && GetStyle() == style)
    return Font(this);

  // CreateFontIndirect() doesn't return the largest size for the given height
  // when decreasing the height. Iterate to find it.
  if (GetHeight() > height) {
    const int min_font_size = GetMinimumFontSize();
    Font font = DeriveFont(-1, style);
    int font_height = font.GetHeight();
    int font_size = font.GetFontSize();
    while (font_height > height && font_size != min_font_size) {
      font = font.Derive(-1, style);
      if (font_height == font.GetHeight() && font_size == font.GetFontSize())
        break;
      font_height = font.GetHeight();
      font_size = font.GetFontSize();
    }
    return font;
  }

  LOGFONT font_info;
  GetObject(GetNativeFont(), sizeof(LOGFONT), &font_info);
  font_info.lfHeight = height;
  SetLogFontStyle(style, &font_info);

  HFONT hfont = CreateFontIndirect(&font_info);
  return Font(new PlatformFontWin(CreateHFontRef(hfont)));
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontWin, PlatformFont implementation:

Font PlatformFontWin::DeriveFont(int size_delta, int style) const {
  LOGFONT font_info;
  GetObject(GetNativeFont(), sizeof(LOGFONT), &font_info);
  const int requested_font_size = font_ref_->requested_font_size();
  font_info.lfHeight = AdjustFontSize(-requested_font_size, size_delta);
  SetLogFontStyle(style, &font_info);

  HFONT hfont = CreateFontIndirect(&font_info);
  return Font(new PlatformFontWin(CreateHFontRef(hfont)));
}

int PlatformFontWin::GetHeight() const {
  return font_ref_->height();
}

int PlatformFontWin::GetBaseline() const {
  return font_ref_->baseline();
}

int PlatformFontWin::GetCapHeight() const {
  return font_ref_->cap_height();
}

int PlatformFontWin::GetExpectedTextWidth(int length) const {
  return length * std::min(font_ref_->GetDluBaseX(),
                           font_ref_->ave_char_width());
}

int PlatformFontWin::GetStyle() const {
  return font_ref_->style();
}

std::string PlatformFontWin::GetFontName() const {
  return font_ref_->font_name();
}

std::string PlatformFontWin::GetActualFontNameForTesting() const {
  // With the current implementation on Windows, HFontRef::font_name() returns
  // the font name taken from the HFONT handle, but it's not the name that comes
  // from the font's metadata.  See http://crbug.com/327287
  return font_ref_->font_name();
}

std::string PlatformFontWin::GetLocalizedFontName() const {
  base::win::ScopedCreateDC memory_dc(CreateCompatibleDC(NULL));
  if (!memory_dc.Get())
    return GetFontName();

  // When a font has a localized name for a language matching the system
  // locale, GetTextFace() returns the localized name.
  base::win::ScopedSelectObject font(memory_dc, font_ref_->hfont());
  wchar_t localized_font_name[LF_FACESIZE];
  int length = GetTextFace(memory_dc, arraysize(localized_font_name),
                           &localized_font_name[0]);
  if (length <= 0)
    return GetFontName();
  return base::SysWideToUTF8(localized_font_name);
}

int PlatformFontWin::GetFontSize() const {
  return font_ref_->font_size();
}

NativeFont PlatformFontWin::GetNativeFont() const {
  return font_ref_->hfont();
}

////////////////////////////////////////////////////////////////////////////////
// Font, private:

void PlatformFontWin::InitWithCopyOfHFONT(HFONT hfont) {
  DCHECK(hfont);
  LOGFONT font_info;
  GetObject(hfont, sizeof(LOGFONT), &font_info);
  font_ref_ = CreateHFontRef(CreateFontIndirect(&font_info));
}

void PlatformFontWin::InitWithFontNameAndSize(const std::string& font_name,
                                              int font_size) {
  HFONT hf = ::CreateFont(-font_size, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                          base::UTF8ToUTF16(font_name).c_str());
  font_ref_ = CreateHFontRef(hf);
}

// static
PlatformFontWin::HFontRef* PlatformFontWin::GetBaseFontRef() {
  if (base_font_ref_ == NULL) {
    NONCLIENTMETRICS metrics;
    base::win::GetNonClientMetrics(&metrics);

    if (adjust_font_callback)
      adjust_font_callback(&metrics.lfMessageFont);
    metrics.lfMessageFont.lfHeight =
        AdjustFontSize(metrics.lfMessageFont.lfHeight, 0);
    HFONT font = CreateFontIndirect(&metrics.lfMessageFont);
    DLOG_ASSERT(font);
    base_font_ref_ = PlatformFontWin::CreateHFontRef(font);
    // base_font_ref_ is global, up the ref count so it's never deleted.
    base_font_ref_->AddRef();
  }
  return base_font_ref_;
}

PlatformFontWin::HFontRef* PlatformFontWin::CreateHFontRef(HFONT font) {
  TEXTMETRIC font_metrics;

  {
    base::win::ScopedGetDC screen_dc(NULL);
    base::win::ScopedSelectObject scoped_font(screen_dc, font);
    gfx::ScopedSetMapMode mode(screen_dc, MM_TEXT);
    GetTextMetrics(screen_dc, &font_metrics);
  }

  const int height = std::max<int>(1, font_metrics.tmHeight);
  const int baseline = std::max<int>(1, font_metrics.tmAscent);
  const int cap_height =
      std::max<int>(1, font_metrics.tmAscent - font_metrics.tmInternalLeading);
  const int ave_char_width = std::max<int>(1, font_metrics.tmAveCharWidth);
  const int font_size =
      std::max<int>(1, font_metrics.tmHeight - font_metrics.tmInternalLeading);
  int style = 0;
  if (font_metrics.tmItalic)
    style |= Font::ITALIC;
  if (font_metrics.tmUnderlined)
    style |= Font::UNDERLINE;
  if (font_metrics.tmWeight >= kTextMetricWeightBold)
    style |= Font::BOLD;

  return new HFontRef(font, font_size, height, baseline, cap_height,
                      ave_char_width, style);
}

PlatformFontWin::PlatformFontWin(HFontRef* hfont_ref) : font_ref_(hfont_ref) {
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontWin::HFontRef:

PlatformFontWin::HFontRef::HFontRef(HFONT hfont,
                                    int font_size,
                                    int height,
                                    int baseline,
                                    int cap_height,
                                    int ave_char_width,
                                    int style)
    : hfont_(hfont),
      font_size_(font_size),
      height_(height),
      baseline_(baseline),
      cap_height_(cap_height),
      ave_char_width_(ave_char_width),
      style_(style),
      dlu_base_x_(-1),
      requested_font_size_(font_size) {
  DLOG_ASSERT(hfont);

  LOGFONT font_info;
  GetObject(hfont_, sizeof(LOGFONT), &font_info);
  font_name_ = base::UTF16ToUTF8(base::string16(font_info.lfFaceName));
  if (font_info.lfHeight < 0)
    requested_font_size_ = -font_info.lfHeight;
}

int PlatformFontWin::HFontRef::GetDluBaseX() {
  if (dlu_base_x_ != -1)
    return dlu_base_x_;

  base::win::ScopedGetDC screen_dc(NULL);
  base::win::ScopedSelectObject font(screen_dc, hfont_);
  gfx::ScopedSetMapMode mode(screen_dc, MM_TEXT);

  // Yes, this is how Microsoft recommends calculating the dialog unit
  // conversions. See: http://support.microsoft.com/kb/125681
  SIZE ave_text_size;
  GetTextExtentPoint32(screen_dc,
                       L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
                       52, &ave_text_size);
  dlu_base_x_ = (ave_text_size.cx / 26 + 1) / 2;

  DCHECK_NE(dlu_base_x_, -1);
  return dlu_base_x_;
}

PlatformFontWin::HFontRef::~HFontRef() {
  DeleteObject(hfont_);
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFont, public:

// static
PlatformFont* PlatformFont::CreateDefault() {
  return new PlatformFontWin;
}

// static
PlatformFont* PlatformFont::CreateFromNativeFont(NativeFont native_font) {
  return new PlatformFontWin(native_font);
}

// static
PlatformFont* PlatformFont::CreateFromNameAndSize(const std::string& font_name,
                                                  int font_size) {
  return new PlatformFontWin(font_name, font_size);
}

}  // namespace gfx

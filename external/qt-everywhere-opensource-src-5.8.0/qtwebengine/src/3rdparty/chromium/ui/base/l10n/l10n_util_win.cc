// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util_win.h"

#include <windowsx.h>
#include <algorithm>
#include <iterator>

#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/i18n.h"
#include "base/win/windows_version.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/win/dpi.h"
#include "ui/strings/grit/app_locale_settings.h"

namespace {

void AdjustLogFont(const base::string16& font_family,
                   double font_size_scaler,
                   double dpi_scale,
                   LOGFONT* logfont) {
  DCHECK(font_size_scaler > 0);
  font_size_scaler = std::max(std::min(font_size_scaler, 2.0), 0.7);
  // Font metrics are computed in pixels and scale in high-DPI mode.
  // Normalized by the DPI scale factor in order to work in DIP with
  // Views/Aura. Call with dpi_scale=1 to keep the size in pixels.
  font_size_scaler /= dpi_scale;
  logfont->lfHeight = static_cast<long>(font_size_scaler *
      static_cast<double>(abs(logfont->lfHeight)) + 0.5) *
      (logfont->lfHeight > 0 ? 1 : -1);

  // TODO(jungshik): We may want to check the existence of the font.
  // If it's not installed, we shouldn't adjust the font.
  if (font_family != L"default") {
    int name_len = std::min(static_cast<int>(font_family.size()),
                            LF_FACESIZE -1);
    memcpy(logfont->lfFaceName, font_family.data(), name_len * sizeof(WORD));
    logfont->lfFaceName[name_len] = 0;
  }
}

bool IsFontPresent(const wchar_t* font_name) {
  HFONT hfont = CreateFont(12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                           font_name);
  if (hfont == NULL)
    return false;
  HDC dc = GetDC(0);
  HGDIOBJ oldFont = static_cast<HFONT>(SelectObject(dc, hfont));
  WCHAR actual_font_name[LF_FACESIZE];
  int size_ret = GetTextFace(dc, LF_FACESIZE, actual_font_name);
  SelectObject(dc, oldFont);
  DeleteObject(hfont);
  ReleaseDC(0, dc);
  // We don't have to worry about East Asian fonts with locale-dependent
  // names here.
  return (size_ret != 0) && wcscmp(font_name, actual_font_name) == 0;
}

class OverrideLocaleHolder {
 public:
  OverrideLocaleHolder() {}
  const std::vector<std::string>& value() const { return value_; }
  void swap_value(std::vector<std::string>* override_value) {
    value_.swap(*override_value);
  }
 private:
  std::vector<std::string> value_;
  DISALLOW_COPY_AND_ASSIGN(OverrideLocaleHolder);
};

base::LazyInstance<OverrideLocaleHolder> override_locale_holder =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace l10n_util {

int GetExtendedStyles() {
  return !base::i18n::IsRTL() ? 0 : WS_EX_LAYOUTRTL | WS_EX_RTLREADING;
}

int GetExtendedTooltipStyles() {
  return !base::i18n::IsRTL() ? 0 : WS_EX_LAYOUTRTL;
}

void HWNDSetRTLLayout(HWND hwnd) {
  DWORD ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);

  // We don't have to do anything if the style is already set for the HWND.
  if (!(ex_style & WS_EX_LAYOUTRTL)) {
    ex_style |= WS_EX_LAYOUTRTL;
    ::SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);

    // Right-to-left layout changes are not applied to the window immediately
    // so we should make sure a WM_PAINT is sent to the window by invalidating
    // the entire window rect.
    ::InvalidateRect(hwnd, NULL, true);
  }
}

bool IsLocaleSupportedByOS(const std::string& locale) {
  // Block Amharic on Windows XP unless 'Abyssinica SIL' font is present.
  // On Win XP, no Ethiopic/Amahric font is availabel out of box. We hard-coded
  // 'Abyssinica SIL' in the resource bundle to use in the UI. Check
  // for its presence to determine whether or not to support Amharic UI on XP.
  return (base::win::GetVersion() >= base::win::VERSION_VISTA ||
      !base::LowerCaseEqualsASCII(locale, "am") ||
      IsFontPresent(L"Abyssinica SIL"));
}

bool NeedOverrideDefaultUIFont(base::string16* override_font_family,
                               double* font_size_scaler) {
  // This is rather simple-minded to deal with the UI font size
  // issue for some Indian locales (ml, bn, hi) for which
  // the default Windows fonts are too small to be legible.  For those
  // locales, IDS_UI_FONT_FAMILY is set to an actual font family to
  // use while for other locales, it's set to 'default'.

  // XP and Vista or later have different font size issues and
  // we need separate ui font specifications.
  int ui_font_family_id = IDS_UI_FONT_FAMILY;
  int ui_font_size_scaler_id = IDS_UI_FONT_SIZE_SCALER;
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    ui_font_family_id = IDS_UI_FONT_FAMILY_XP;
    ui_font_size_scaler_id = IDS_UI_FONT_SIZE_SCALER_XP;
  }

  base::string16 ui_font_family = GetStringUTF16(ui_font_family_id);
  int scaler100;
  if (!base::StringToInt(l10n_util::GetStringUTF16(ui_font_size_scaler_id),
                         &scaler100))
    return false;

  // We use the OS default in two cases:
  // 1) The resource bundle has 'default' and '100' for font family and
  //    font scaler.
  // 2) The resource bundle is not available for some reason and
  //    ui_font_family is empty.
  if ((ui_font_family == L"default" && scaler100 == 100) ||
      ui_font_family.empty())
    return false;
  if (override_font_family && font_size_scaler) {
    override_font_family->swap(ui_font_family);
    *font_size_scaler = scaler100 / 100.0;
  }
  return true;
}

void AdjustUIFont(LOGFONT* logfont) {
  float dpi_scale = display::win::GetDPIScale();
  if (display::Display::HasForceDeviceScaleFactor()) {
    // If the scale is forced, we don't need to adjust it here.
    dpi_scale = 1.0f;
  }
  AdjustUIFontForDIP(dpi_scale, logfont);
}

void AdjustUIFontForDIP(float dpi_scale, LOGFONT* logfont) {
  base::string16 ui_font_family = L"default";
  double ui_font_size_scaler = 1;
  if (NeedOverrideDefaultUIFont(&ui_font_family, &ui_font_size_scaler) ||
      dpi_scale != 1) {
    AdjustLogFont(ui_font_family, ui_font_size_scaler, dpi_scale, logfont);
  }
}

void AdjustUIFontForWindow(HWND hwnd) {
  base::string16 ui_font_family;
  double ui_font_size_scaler;
  if (NeedOverrideDefaultUIFont(&ui_font_family, &ui_font_size_scaler)) {
    LOGFONT logfont;
    if (GetObject(GetWindowFont(hwnd), sizeof(logfont), &logfont)) {
      double dpi_scale = 1;
      AdjustLogFont(ui_font_family, ui_font_size_scaler, dpi_scale, &logfont);
      HFONT hfont = CreateFontIndirect(&logfont);
      if (hfont)
        SetWindowFont(hwnd, hfont, FALSE);
    }
  }
}

void OverrideLocaleWithUILanguageList() {
  std::vector<base::string16> ui_languages;
  if (base::win::i18n::GetThreadPreferredUILanguageList(&ui_languages)) {
    std::vector<std::string> ascii_languages;
    ascii_languages.reserve(ui_languages.size());
    std::transform(ui_languages.begin(), ui_languages.end(),
                   std::back_inserter(ascii_languages), &base::UTF16ToASCII);
    override_locale_holder.Get().swap_value(&ascii_languages);
  } else {
    NOTREACHED() << "Failed to determine the UI language for locale override.";
  }
}

const std::vector<std::string>& GetLocaleOverrides() {
  return override_locale_holder.Get().value();
}

}  // namespace l10n_util

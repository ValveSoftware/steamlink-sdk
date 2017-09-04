// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_initializer_win.h"

#include <windows.h>

#include "printing/print_settings.h"

namespace printing {

namespace {

bool IsPrinterXPS(HDC hdc) {
  int device_type = ::GetDeviceCaps(hdc, TECHNOLOGY);
  if (device_type != DT_RASPRINTER)
    return false;

  const DWORD escape = GETTECHNOLOGY;
  const char* escape_ptr = reinterpret_cast<const char*>(&escape);
  int ret =
      ExtEscape(hdc, QUERYESCSUPPORT, sizeof(escape), escape_ptr, 0, nullptr);
  if (ret <= 0)
    return false;

  char buffer[256];
  memset(buffer, 0, sizeof(buffer));
  ret = ExtEscape(hdc, GETTECHNOLOGY, 0, nullptr, sizeof(buffer) - 1, buffer);
  if (ret <= 0)
    return false;

  static const char kXPSDriver[] = "http://schemas.microsoft.com/xps/2005/06";
  return strcmp(buffer, kXPSDriver) == 0;
}

}  // namespace

// static
void PrintSettingsInitializerWin::InitPrintSettings(
    HDC hdc,
    const DEVMODE& dev_mode,
    PrintSettings* print_settings) {
  DCHECK(hdc);
  DCHECK(print_settings);

  print_settings->SetOrientation(dev_mode.dmOrientation == DMORIENT_LANDSCAPE);

  int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
  print_settings->set_dpi(dpi);

  const int kAlphaCaps = SB_CONST_ALPHA | SB_PIXEL_ALPHA;
  print_settings->set_supports_alpha_blend(
    (GetDeviceCaps(hdc, SHADEBLENDCAPS) & kAlphaCaps) == kAlphaCaps);

  // No printer device is known to advertise different dpi in X and Y axis; even
  // the fax device using the 200x100 dpi setting. It's ought to break so many
  // applications that it's not even needed to care about. Blink doesn't support
  // different dpi settings in X and Y axis.
  DCHECK_EQ(dpi, GetDeviceCaps(hdc, LOGPIXELSY));

  DCHECK_EQ(GetDeviceCaps(hdc, SCALINGFACTORX), 0);
  DCHECK_EQ(GetDeviceCaps(hdc, SCALINGFACTORY), 0);

  // Initialize |page_setup_device_units_|.
  gfx::Size physical_size_device_units(GetDeviceCaps(hdc, PHYSICALWIDTH),
                                       GetDeviceCaps(hdc, PHYSICALHEIGHT));
  gfx::Rect printable_area_device_units(GetDeviceCaps(hdc, PHYSICALOFFSETX),
                                        GetDeviceCaps(hdc, PHYSICALOFFSETY),
                                        GetDeviceCaps(hdc, HORZRES),
                                        GetDeviceCaps(hdc, VERTRES));

  // Sanity check the printable_area: we've seen crashes caused by a printable
  // area rect of 0, 0, 0, 0, so it seems some drivers don't set it.
  if (printable_area_device_units.IsEmpty() ||
      !gfx::Rect(physical_size_device_units).Contains(
          printable_area_device_units)) {
    printable_area_device_units = gfx::Rect(physical_size_device_units);
  }
  DCHECK_EQ(print_settings->device_units_per_inch(), dpi);
  print_settings->SetPrinterPrintableArea(physical_size_device_units,
                                          printable_area_device_units,
                                          false);

  print_settings->set_printer_is_xps(IsPrinterXPS(hdc));
}

}  // namespace printing

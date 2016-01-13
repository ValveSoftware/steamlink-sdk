// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/dpi.h"

#include <windows.h>
#include "base/command_line.h"
#include "base/win/scoped_hdc.h"
#include "base/win/windows_version.h"
#include "base/win/registry.h"
#include "ui/gfx/display.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"

namespace {

int kDefaultDPIX = 96;
int kDefaultDPIY = 96;

bool force_highdpi_for_testing = false;

BOOL IsProcessDPIAwareWrapper() {
  typedef BOOL(WINAPI *IsProcessDPIAwarePtr)(VOID);
  IsProcessDPIAwarePtr is_process_dpi_aware_func =
      reinterpret_cast<IsProcessDPIAwarePtr>(
          GetProcAddress(GetModuleHandleA("user32.dll"), "IsProcessDPIAware"));
  if (is_process_dpi_aware_func)
    return is_process_dpi_aware_func();
  return FALSE;
}

float g_device_scale_factor = 0.0f;

float GetUnforcedDeviceScaleFactor() {
  // If the global device scale factor is initialized use it. This is to ensure
  // we use the same scale factor across all callsites. We don't use the
  // GetDeviceScaleFactor function here because it fires a DCHECK if the
  // g_device_scale_factor global is 0. 
  if (g_device_scale_factor)
    return g_device_scale_factor;
  return static_cast<float>(gfx::GetDPI().width()) /
      static_cast<float>(kDefaultDPIX);
}

// Duplicated from Win8.1 SDK ShellScalingApi.h
typedef enum PROCESS_DPI_AWARENESS {
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;

typedef enum MONITOR_DPI_TYPE {
    MDT_EFFECTIVE_DPI = 0,
    MDT_ANGULAR_DPI = 1,
    MDT_RAW_DPI = 2,
    MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;

// Win8.1 supports monitor-specific DPI scaling.
bool SetProcessDpiAwarenessWrapper(PROCESS_DPI_AWARENESS value) {
  typedef BOOL(WINAPI *SetProcessDpiAwarenessPtr)(PROCESS_DPI_AWARENESS);
  SetProcessDpiAwarenessPtr set_process_dpi_awareness_func =
      reinterpret_cast<SetProcessDpiAwarenessPtr>(
          GetProcAddress(GetModuleHandleA("user32.dll"),
                          "SetProcessDpiAwarenessInternal"));
  if (set_process_dpi_awareness_func) {
    HRESULT hr = set_process_dpi_awareness_func(value);
    if (SUCCEEDED(hr)) {
      VLOG(1) << "SetProcessDpiAwareness succeeded.";
      return true;
    } else if (hr == E_ACCESSDENIED) {
      LOG(ERROR) << "Access denied error from SetProcessDpiAwareness. "
          "Function called twice, or manifest was used.";
    }
  }
  return false;
}

// This function works for Windows Vista through Win8. Win8.1 must use
// SetProcessDpiAwareness[Wrapper]
BOOL SetProcessDPIAwareWrapper() {
  typedef BOOL(WINAPI *SetProcessDPIAwarePtr)(VOID);
  SetProcessDPIAwarePtr set_process_dpi_aware_func =
      reinterpret_cast<SetProcessDPIAwarePtr>(
      GetProcAddress(GetModuleHandleA("user32.dll"),
                      "SetProcessDPIAware"));
  return set_process_dpi_aware_func &&
    set_process_dpi_aware_func();
}

DWORD ReadRegistryValue(HKEY root,
                        const wchar_t* base_key,
                        const wchar_t* value_name,
                        DWORD default_value) {
  base::win::RegKey reg_key(HKEY_CURRENT_USER,
                            base_key,
                            KEY_QUERY_VALUE);
  DWORD value;
  if (reg_key.Valid() &&
      reg_key.ReadValueDW(value_name, &value) == ERROR_SUCCESS) {
    return value;
  }
  return default_value;
}

}  // namespace

namespace gfx {

void InitDeviceScaleFactor(float scale) {
  DCHECK_NE(0.0f, scale);
  g_device_scale_factor = scale;
}

Size GetDPI() {
  static int dpi_x = 0;
  static int dpi_y = 0;
  static bool should_initialize = true;

  if (should_initialize) {
    should_initialize = false;
    base::win::ScopedGetDC screen_dc(NULL);
    // This value is safe to cache for the life time of the app since the
    // user must logout to change the DPI setting. This value also applies
    // to all screens.
    dpi_x = GetDeviceCaps(screen_dc, LOGPIXELSX);
    dpi_y = GetDeviceCaps(screen_dc, LOGPIXELSY);
  }
  return Size(dpi_x, dpi_y);
}

float GetDPIScale() {
  if (IsHighDPIEnabled()) {
    if (GetDPI().width() <= 120) {
      // 120 logical pixels is 125% scale. We do this to maintain previous
      // (non-DPI-aware) behavior where only the font size was boosted.
      return 1.0;
    }
    return gfx::Display::HasForceDeviceScaleFactor() ?
        gfx::Display::GetForcedDeviceScaleFactor() :
        GetUnforcedDeviceScaleFactor();
  }
  return 1.0;
}

void ForceHighDPISupportForTesting(float scale) {
  g_device_scale_factor = scale;
}

bool IsHighDPIEnabled() {
  // Flag stored in HKEY_CURRENT_USER\SOFTWARE\\Google\\Chrome\\Profile,
  // under the DWORD value high-dpi-support.
  // Default is disabled.
  static DWORD value = ReadRegistryValue(
      HKEY_CURRENT_USER, gfx::win::kRegistryProfilePath,
      gfx::win::kHighDPISupportW, TRUE);
  return value != 0;
}

bool IsInHighDPIMode() {
  return GetDPIScale() > 1.0;
}

void EnableHighDPISupport() {
  if (IsHighDPIEnabled() &&
      !SetProcessDpiAwarenessWrapper(PROCESS_SYSTEM_DPI_AWARE)) {
    SetProcessDPIAwareWrapper();
  }
}

namespace win {

GFX_EXPORT const wchar_t kRegistryProfilePath[] =
    L"Software\\Google\\Chrome\\Profile";
GFX_EXPORT const wchar_t kHighDPISupportW[] = L"high-dpi-support";

float GetDeviceScaleFactor() {
  DCHECK_NE(0.0f, g_device_scale_factor);
  return g_device_scale_factor;
}

Point ScreenToDIPPoint(const Point& pixel_point) {
  return ToFlooredPoint(ScalePoint(pixel_point,
      1.0f / GetDeviceScaleFactor()));
}

Point DIPToScreenPoint(const Point& dip_point) {
  return ToFlooredPoint(ScalePoint(dip_point, GetDeviceScaleFactor()));
}

Rect ScreenToDIPRect(const Rect& pixel_bounds) {
  // TODO(kevers): Switch to non-deprecated method for float to int conversions.
  return ToFlooredRectDeprecated(
      ScaleRect(pixel_bounds, 1.0f / GetDeviceScaleFactor()));
}

Rect DIPToScreenRect(const Rect& dip_bounds) {
  // We scale the origin by the scale factor and round up via ceil. This
  // ensures that we get the original logical origin back when we scale down.
  // We round the size down after scaling. It may be better to round this up
  // on the same lines as the origin.
  // TODO(ananta)
  // Investigate if rounding size up on the same lines as origin is workable.
  return gfx::Rect(
      gfx::ToCeiledPoint(gfx::ScalePoint(
          dip_bounds.origin(), GetDeviceScaleFactor())),
      gfx::ToFlooredSize(gfx::ScaleSize(
          dip_bounds.size(), GetDeviceScaleFactor())));
}

Size ScreenToDIPSize(const Size& size_in_pixels) {
  return ToFlooredSize(
      ScaleSize(size_in_pixels, 1.0f / GetDeviceScaleFactor()));
}

Size DIPToScreenSize(const Size& dip_size) {
  return ToFlooredSize(ScaleSize(dip_size, GetDeviceScaleFactor()));
}

int GetSystemMetricsInDIP(int metric) {
  return static_cast<int>(GetSystemMetrics(metric) /
      GetDeviceScaleFactor() + 0.5);
}

bool IsDeviceScaleFactorSet() {
  return g_device_scale_factor != 0.0f;
}

}  // namespace win
}  // namespace gfx

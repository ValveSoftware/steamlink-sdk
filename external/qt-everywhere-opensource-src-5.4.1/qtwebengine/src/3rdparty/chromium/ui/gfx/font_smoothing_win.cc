// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_smoothing_win.h"

#include "base/memory/singleton.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace {

// Helper class to cache font smoothing settings and listen for notifications
// to re-query them from the system.
class CachedFontSmoothingSettings : public gfx::SingletonHwnd::Observer {
 public:
  static CachedFontSmoothingSettings* GetInstance();

  // Returns the cached Windows font smoothing settings. Queries the settings
  // via Windows APIs and begins listening for changes when called for the
  // first time.
  void GetFontSmoothingSettings(bool* smoothing_enabled,
                                bool* cleartype_enabled);

 private:
  friend struct DefaultSingletonTraits<CachedFontSmoothingSettings>;

  CachedFontSmoothingSettings();
  virtual ~CachedFontSmoothingSettings();

  // Listener for WM_SETTINGCHANGE notifications.
  virtual void OnWndProc(HWND hwnd,
                         UINT message,
                         WPARAM wparam,
                         LPARAM lparam) OVERRIDE;

  // Queries the font settings from the system.
  void QueryFontSettings();

  // Indicates whether the SingletonHwnd::Observer has been registered.
  bool observer_added_;

  // Indicates whether |smoothing_enabled_| and |cleartype_enabled_| are valid
  // or need to be re-queried from the system.
  bool need_to_query_settings_;

  // Indicates that font smoothing is enabled.
  bool smoothing_enabled_;

  // Indicates that the ClearType font smoothing is enabled.
  bool cleartype_enabled_;

  DISALLOW_COPY_AND_ASSIGN(CachedFontSmoothingSettings);
};

// static
CachedFontSmoothingSettings* CachedFontSmoothingSettings::GetInstance() {
  return Singleton<CachedFontSmoothingSettings>::get();
}

void CachedFontSmoothingSettings::GetFontSmoothingSettings(
    bool* smoothing_enabled,
    bool* cleartype_enabled) {
  // If cached settings are stale, query them from the OS.
  if (need_to_query_settings_) {
    QueryFontSettings();
    need_to_query_settings_ = false;
  }
  if (!observer_added_) {
    gfx::SingletonHwnd::GetInstance()->AddObserver(this);
    observer_added_ = true;
  }
  *smoothing_enabled = smoothing_enabled_;
  *cleartype_enabled = cleartype_enabled_;
}

CachedFontSmoothingSettings::CachedFontSmoothingSettings()
    : observer_added_(false),
      need_to_query_settings_(true),
      smoothing_enabled_(false),
      cleartype_enabled_(false) {
}

CachedFontSmoothingSettings::~CachedFontSmoothingSettings() {
  // Can't remove the SingletonHwnd observer here since SingletonHwnd may have
  // been destroyed already (both singletons).
}

void CachedFontSmoothingSettings::OnWndProc(HWND hwnd,
                                            UINT message,
                                            WPARAM wparam,
                                            LPARAM lparam) {
  if (message == WM_SETTINGCHANGE)
    need_to_query_settings_ = true;
}

void CachedFontSmoothingSettings::QueryFontSettings() {
  smoothing_enabled_ = false;
  cleartype_enabled_ = false;

  BOOL enabled = false;
  if (SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &enabled, 0) && enabled) {
    smoothing_enabled_ = true;

    UINT smooth_type = 0;
    if (SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &smooth_type, 0))
      cleartype_enabled_ = (smooth_type == FE_FONTSMOOTHINGCLEARTYPE);
  }
}

}  // namespace

namespace gfx {

void GetCachedFontSmoothingSettings(bool* smoothing_enabled,
                                    bool* cleartype_enabled) {
  CachedFontSmoothingSettings::GetInstance()->GetFontSmoothingSettings(
      smoothing_enabled,
      cleartype_enabled);
}

}  // namespace gfx

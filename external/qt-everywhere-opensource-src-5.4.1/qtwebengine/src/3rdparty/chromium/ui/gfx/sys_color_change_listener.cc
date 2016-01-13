// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/sys_color_change_listener.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "ui/gfx/color_utils.h"

#if defined(OS_WIN)
#include "ui/gfx/win/singleton_hwnd.h"
#endif

namespace gfx {

namespace {

bool g_is_inverted_color_scheme = false;
bool g_is_inverted_color_scheme_initialized = false;

void UpdateInvertedColorScheme() {
#if defined(OS_WIN)
  int foreground_luminance = color_utils::GetLuminanceForColor(
      color_utils::GetSysSkColor(COLOR_WINDOWTEXT));
  int background_luminance = color_utils::GetLuminanceForColor(
      color_utils::GetSysSkColor(COLOR_WINDOW));
  HIGHCONTRAST high_contrast = {0};
  high_contrast.cbSize = sizeof(HIGHCONTRAST);
  g_is_inverted_color_scheme =
      SystemParametersInfo(SPI_GETHIGHCONTRAST, 0, &high_contrast, 0) &&
      ((high_contrast.dwFlags & HCF_HIGHCONTRASTON) != 0) &&
      foreground_luminance > background_luminance;
  g_is_inverted_color_scheme_initialized = true;
#endif
}

}  // namespace

bool IsInvertedColorScheme() {
  if (!g_is_inverted_color_scheme_initialized)
    UpdateInvertedColorScheme();
  return g_is_inverted_color_scheme;
}

#if defined(OS_WIN)
class SysColorChangeObserver : public gfx::SingletonHwnd::Observer {
 public:
  static SysColorChangeObserver* GetInstance();

  void AddListener(SysColorChangeListener* listener);
  void RemoveListener(SysColorChangeListener* listener);

 private:
  friend struct DefaultSingletonTraits<SysColorChangeObserver>;

  SysColorChangeObserver();
  virtual ~SysColorChangeObserver();

  virtual void OnWndProc(HWND hwnd,
                         UINT message,
                         WPARAM wparam,
                         LPARAM lparam) OVERRIDE;

  ObserverList<SysColorChangeListener> listeners_;
};

// static
SysColorChangeObserver* SysColorChangeObserver::GetInstance() {
  return Singleton<SysColorChangeObserver>::get();
}

SysColorChangeObserver::SysColorChangeObserver() {
  gfx::SingletonHwnd::GetInstance()->AddObserver(this);
}

SysColorChangeObserver::~SysColorChangeObserver() {
  gfx::SingletonHwnd::GetInstance()->RemoveObserver(this);
}

void SysColorChangeObserver::AddListener(SysColorChangeListener* listener) {
  listeners_.AddObserver(listener);
}

void SysColorChangeObserver::RemoveListener(SysColorChangeListener* listener) {
  listeners_.RemoveObserver(listener);
}

void SysColorChangeObserver::OnWndProc(HWND hwnd,
                                       UINT message,
                                       WPARAM wparam,
                                       LPARAM lparam) {
  if (message == WM_SYSCOLORCHANGE ||
      (message == WM_SETTINGCHANGE && wparam == SPI_SETHIGHCONTRAST)) {
    UpdateInvertedColorScheme();
    FOR_EACH_OBSERVER(SysColorChangeListener, listeners_, OnSysColorChange());
  }
}
#endif

ScopedSysColorChangeListener::ScopedSysColorChangeListener(
    SysColorChangeListener* listener)
    : listener_(listener) {
#if defined(OS_WIN)
  SysColorChangeObserver::GetInstance()->AddListener(listener_);
#endif
}

ScopedSysColorChangeListener::~ScopedSysColorChangeListener() {
#if defined(OS_WIN)
  SysColorChangeObserver::GetInstance()->RemoveListener(listener_);
#endif
}

}  // namespace gfx

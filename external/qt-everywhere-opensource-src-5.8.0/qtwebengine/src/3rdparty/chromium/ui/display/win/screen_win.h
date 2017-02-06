// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_SCREEN_WIN_H_
#define UI_DISPLAY_WIN_SCREEN_WIN_H_

#include <windows.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/display/display_change_notifier.h"
#include "ui/display/display_export.h"
#include "ui/display/screen.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"

namespace gfx {
class Display;
class Point;
class Rect;
class Size;
}   // namespace gfx

namespace display {
namespace win {

class DisplayInfo;
class ScreenWinDisplay;

class DISPLAY_EXPORT ScreenWin : public display::Screen {
 public:
  ScreenWin();
  ~ScreenWin() override;

  // Converts a screen physical point to a screen DIP point.
  // The DPI scale is performed relative to the display containing the physical
  // point.
  static gfx::Point ScreenToDIPPoint(const gfx::Point& pixel_point);

  // Converts a screen DIP point to a screen physical point.
  // The DPI scale is performed relative to the display containing the DIP
  // point.
  static gfx::Point DIPToScreenPoint(const gfx::Point& dip_point);

  // Converts a client physical point relative to |hwnd| to a client DIP point.
  // The DPI scale is performed relative to |hwnd| using an origin of (0, 0).
  static gfx::Point ClientToDIPPoint(HWND hwnd, const gfx::Point& client_point);

  // Converts a client DIP point relative to |hwnd| to a client physical point.
  // The DPI scale is performed relative to |hwnd| using an origin of (0, 0).
  static gfx::Point DIPToClientPoint(HWND hwnd, const gfx::Point& dip_point);

  // WARNING: There is no right way to scale sizes and rects.
  // Sometimes you may need the enclosing rect (which favors transformations
  // that stretch the bounds towards integral values) or the enclosed rect
  // (transformations that shrink the bounds towards integral values).
  // This implementation favors the enclosing rect.
  //
  // Understand which you need before blindly assuming this is the right way.

  // Converts a screen physical rect to a screen DIP rect.
  // The DPI scale is performed relative to the display nearest to |hwnd|.
  // If |hwnd| is null, scaling will be performed to the display nearest to
  // |pixel_bounds|.
  static gfx::Rect ScreenToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds);

  // Converts a screen DIP rect to a screen physical rect.
  // The DPI scale is performed relative to the display nearest to |hwnd|.
  // If |hwnd| is null, scaling will be performed to the display nearest to
  // |dip_bounds|.
  static gfx::Rect DIPToScreenRect(HWND hwnd, const gfx::Rect& dip_bounds);

  // Converts a client physical rect to a client DIP rect.
  // The DPI scale is performed relative to |hwnd| using an origin of (0, 0).
  static gfx::Rect ClientToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds);

  // Converts a client DIP rect to a client physical rect.
  // The DPI scale is performed relative to |hwnd| using an origin of (0, 0).
  static gfx::Rect DIPToClientRect(HWND hwnd, const gfx::Rect& dip_bounds);

  // Converts a physical size to a DIP size.
  // The DPI scale is performed relative to the display nearest to |hwnd|.
  static gfx::Size ScreenToDIPSize(HWND hwnd, const gfx::Size& size_in_pixels);

  // Converts a DIP size to a physical size.
  // The DPI scale is performed relative to the display nearest to |hwnd|.
  static gfx::Size DIPToScreenSize(HWND hwnd, const gfx::Size& dip_size);

  // Returns the result of GetSystemMetrics for |metric| scaled to |hwnd|'s DPI.
  static int GetSystemMetricsForHwnd(HWND hwnd, int metric);

  // Returns the HWND associated with the NativeView.
  virtual HWND GetHWNDFromNativeView(gfx::NativeView window) const;

  // Returns the NativeView associated with the HWND.
  virtual gfx::NativeWindow GetNativeWindowFromHWND(HWND hwnd) const;

 protected:
  // display::Screen:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  int GetNumDisplays() const override;
  std::vector<display::Display> GetAllDisplays() const override;
  display::Display GetDisplayNearestWindow(
      gfx::NativeView window) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  display::Display GetPrimaryDisplay() const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;
  gfx::Rect ScreenToDIPRectInWindow(
      gfx::NativeView view, const gfx::Rect& screen_rect) const override;
  gfx::Rect DIPToScreenRectInWindow(
      gfx::NativeView view, const gfx::Rect& dip_rect) const override;

  void UpdateFromDisplayInfos(const std::vector<DisplayInfo>& display_infos);

  // Virtual to support mocking by unit tests.
  virtual void Initialize();
  virtual MONITORINFOEX MonitorInfoFromScreenPoint(
      const gfx::Point& screen_point) const;
  virtual MONITORINFOEX MonitorInfoFromScreenRect(const gfx::Rect& screen_rect)
      const;
  virtual MONITORINFOEX MonitorInfoFromWindow(HWND hwnd, DWORD default_options)
      const;
  virtual HWND GetRootWindow(HWND hwnd) const;
  virtual int GetSystemMetrics(int metric) const;

 private:
  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  // Returns the ScreenWinDisplay closest to or enclosing |hwnd|.
  ScreenWinDisplay GetScreenWinDisplayNearestHWND(HWND hwnd) const;

  // Returns the ScreenWinDisplay closest to or enclosing |screen_rect|.
  ScreenWinDisplay GetScreenWinDisplayNearestScreenRect(
      const gfx::Rect& screen_rect) const;

  // Returns the ScreenWinDisplay closest to or enclosing |screen_point|.
  ScreenWinDisplay GetScreenWinDisplayNearestScreenPoint(
      const gfx::Point& screen_point) const;

  // Returns the ScreenWinDisplay closest to or enclosing |dip_point|.
  ScreenWinDisplay GetScreenWinDisplayNearestDIPPoint(
      const gfx::Point& dip_point) const;

  // Returns the ScreenWinDisplay closest to or enclosing |dip_rect|.
  ScreenWinDisplay GetScreenWinDisplayNearestDIPRect(
      const gfx::Rect& dip_rect) const;

  // Returns the ScreenWinDisplay corresponding to the primary monitor.
  ScreenWinDisplay GetPrimaryScreenWinDisplay() const;

  ScreenWinDisplay GetScreenWinDisplay(const MONITORINFOEX& monitor_info) const;

  static float GetScaleFactorForHWND(HWND hwnd);

  // Returns the result of calling |getter| with |value| on the global
  // ScreenWin if it exists, otherwise return the default ScreenWinDisplay.
  template <typename Getter, typename GetterType>
  static ScreenWinDisplay GetScreenWinDisplayVia(Getter getter,
                                                 GetterType value);

  // Helper implementing the DisplayObserver handling.
  DisplayChangeNotifier change_notifier_;

  std::unique_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;

  // Current list of ScreenWinDisplays.
  std::vector<ScreenWinDisplay> screen_win_displays_;

  DISALLOW_COPY_AND_ASSIGN(ScreenWin);
};

}  // namespace win
}  // namespace display

#endif  // UI_DISPLAY_WIN_SCREEN_WIN_H_

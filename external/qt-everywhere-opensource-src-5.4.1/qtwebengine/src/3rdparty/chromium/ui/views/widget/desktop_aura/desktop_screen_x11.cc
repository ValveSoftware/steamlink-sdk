// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_x11.h"

#include <X11/extensions/Xrandr.h>
#include <X11/Xlib.h>

// It clashes with out RootWindow.
#undef RootWindow

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/layout.h"
#include "ui/display/util/display_util.h"
#include "ui/display/util/x11/edid_parser_x11.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/display.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"
#include "ui/views/widget/desktop_aura/x11_topmost_window_finder.h"

namespace {

// The delay to perform configuration after RRNotify.  See the comment
// in |Dispatch()|.
const int64 kConfigureDelayMs = 500;

// TODO(oshima): Consider using gtk-xft-dpi instead.
float GetDeviceScaleFactor(int screen_pixels, int screen_mm) {
  const int kCSSDefaultDPI = 96;
  const float kInchInMm = 25.4f;

  float screen_inches = screen_mm / kInchInMm;
  float screen_dpi = screen_pixels / screen_inches;
  float scale = screen_dpi / kCSSDefaultDPI;

  return ui::GetScaleForScaleFactor(ui::GetSupportedScaleFactor(scale));
}

std::vector<gfx::Display> GetFallbackDisplayList() {
  ::XDisplay* display = gfx::GetXDisplay();
  ::Screen* screen = DefaultScreenOfDisplay(display);
  int width = WidthOfScreen(screen);
  int height = HeightOfScreen(screen);
  gfx::Size physical_size(WidthMMOfScreen(screen), HeightMMOfScreen(screen));

  gfx::Rect bounds_in_pixels(0, 0, width, height);
  gfx::Display gfx_display(0, bounds_in_pixels);
  if (!gfx::Display::HasForceDeviceScaleFactor() &&
      !ui::IsDisplaySizeBlackListed(physical_size)) {
    float device_scale_factor = GetDeviceScaleFactor(
        width, physical_size.width());
    DCHECK_LE(1.0f, device_scale_factor);
    gfx_display.SetScaleAndBounds(device_scale_factor, bounds_in_pixels);
  }

  return std::vector<gfx::Display>(1, gfx_display);
}

}  // namespace

namespace views {

////////////////////////////////////////////////////////////////////////////////
// DesktopScreenX11, public:

DesktopScreenX11::DesktopScreenX11()
    : xdisplay_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      has_xrandr_(false),
      xrandr_event_base_(0) {
  // We only support 1.3+. There were library changes before this and we should
  // use the new interface instead of the 1.2 one.
  int randr_version_major = 0;
  int randr_version_minor = 0;
  has_xrandr_ = XRRQueryVersion(
        xdisplay_, &randr_version_major, &randr_version_minor) &&
      randr_version_major == 1 &&
      randr_version_minor >= 3;

  if (has_xrandr_) {
    int error_base_ignored = 0;
    XRRQueryExtension(xdisplay_, &xrandr_event_base_, &error_base_ignored);

    if (ui::PlatformEventSource::GetInstance())
      ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
    XRRSelectInput(xdisplay_,
                   x_root_window_,
                   RRScreenChangeNotifyMask |
                   RROutputChangeNotifyMask |
                   RRCrtcChangeNotifyMask);

    displays_ = BuildDisplaysFromXRandRInfo();
  } else {
    displays_ = GetFallbackDisplayList();
  }
}

DesktopScreenX11::~DesktopScreenX11() {
  if (has_xrandr_ && ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

void DesktopScreenX11::ProcessDisplayChange(
    const std::vector<gfx::Display>& incoming) {
  std::vector<gfx::Display> old_displays = displays_;
  displays_ = incoming;

  typedef std::vector<gfx::Display>::const_iterator DisplayIt;
  std::vector<gfx::Display>::const_iterator old_it = old_displays.begin();
  for (; old_it != old_displays.end(); ++old_it) {
    bool found = false;
    for (std::vector<gfx::Display>::const_iterator new_it =
             displays_.begin(); new_it != displays_.end(); ++new_it) {
      if (old_it->id() == new_it->id()) {
        found = true;
        break;
      }
    }

    if (!found) {
      FOR_EACH_OBSERVER(gfx::DisplayObserver, observer_list_,
                        OnDisplayRemoved(*old_it));
    }
  }

  std::vector<gfx::Display>::const_iterator new_it = displays_.begin();
  for (; new_it != displays_.end(); ++new_it) {
    bool found = false;
    for (std::vector<gfx::Display>::const_iterator old_it =
         old_displays.begin(); old_it != old_displays.end(); ++old_it) {
      if (new_it->id() != old_it->id())
        continue;

      uint32_t metrics = gfx::DisplayObserver::DISPLAY_METRIC_NONE;

      if (new_it->bounds() != old_it->bounds())
        metrics |= gfx::DisplayObserver::DISPLAY_METRIC_BOUNDS;

      if (new_it->rotation() != old_it->rotation())
        metrics |= gfx::DisplayObserver::DISPLAY_METRIC_ROTATION;

      if (new_it->work_area() != old_it->work_area())
        metrics |= gfx::DisplayObserver::DISPLAY_METRIC_WORK_AREA;

      if (new_it->device_scale_factor() != old_it->device_scale_factor())
        metrics |= gfx::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;

      if (metrics != gfx::DisplayObserver::DISPLAY_METRIC_NONE) {
        FOR_EACH_OBSERVER(gfx::DisplayObserver,
                          observer_list_,
                          OnDisplayMetricsChanged(*new_it, metrics));
      }

      found = true;
      break;
    }

    if (!found) {
      FOR_EACH_OBSERVER(gfx::DisplayObserver, observer_list_,
                        OnDisplayAdded(*new_it));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// DesktopScreenX11, gfx::Screen implementation:

bool DesktopScreenX11::IsDIPEnabled() {
  return true;
}

gfx::Point DesktopScreenX11::GetCursorScreenPoint() {
  TRACE_EVENT0("views", "DesktopScreenX11::GetCursorScreenPoint()");

  XDisplay* display = gfx::GetXDisplay();

  ::Window root, child;
  int root_x, root_y, win_x, win_y;
  unsigned int mask;
  XQueryPointer(display,
                DefaultRootWindow(display),
                &root,
                &child,
                &root_x,
                &root_y,
                &win_x,
                &win_y,
                &mask);

  return gfx::Point(root_x, root_y);
}

gfx::NativeWindow DesktopScreenX11::GetWindowUnderCursor() {
  return GetWindowAtScreenPoint(GetCursorScreenPoint());
}

gfx::NativeWindow DesktopScreenX11::GetWindowAtScreenPoint(
    const gfx::Point& point) {
  X11TopmostWindowFinder finder;
  return finder.FindLocalProcessWindowAt(point, std::set<aura::Window*>());
}

int DesktopScreenX11::GetNumDisplays() const {
  return displays_.size();
}

std::vector<gfx::Display> DesktopScreenX11::GetAllDisplays() const {
  return displays_;
}

gfx::Display DesktopScreenX11::GetDisplayNearestWindow(
    gfx::NativeView window) const {
  // Getting screen bounds here safely is hard.
  //
  // You'd think we'd be able to just call window->GetBoundsInScreen(), but we
  // can't because |window| (and the associated WindowEventDispatcher*) can be
  // partially initialized at this point; WindowEventDispatcher initializations
  // call through into GetDisplayNearestWindow(). But the X11 resources are
  // created before we create the aura::WindowEventDispatcher. So we ask what
  // the DRWHX11 believes the window bounds are instead of going through the
  // aura::Window's screen bounds.
  aura::WindowTreeHost* host = window->GetHost();
  if (host) {
    DesktopWindowTreeHostX11* rwh = DesktopWindowTreeHostX11::GetHostForXID(
        host->GetAcceleratedWidget());
    if (rwh)
      return GetDisplayMatching(rwh->GetX11RootWindowBounds());
  }

  return GetPrimaryDisplay();
}

gfx::Display DesktopScreenX11::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  for (std::vector<gfx::Display>::const_iterator it = displays_.begin();
       it != displays_.end(); ++it) {
    if (it->bounds().Contains(point))
      return *it;
  }

  return GetPrimaryDisplay();
}

gfx::Display DesktopScreenX11::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  int max_area = 0;
  const gfx::Display* matching = NULL;
  for (std::vector<gfx::Display>::const_iterator it = displays_.begin();
       it != displays_.end(); ++it) {
    gfx::Rect intersect = gfx::IntersectRects(it->bounds(), match_rect);
    int area = intersect.width() * intersect.height();
    if (area > max_area) {
      max_area = area;
      matching = &*it;
    }
  }
  // Fallback to the primary display if there is no matching display.
  return matching ? *matching : GetPrimaryDisplay();
}

gfx::Display DesktopScreenX11::GetPrimaryDisplay() const {
  return displays_.front();
}

void DesktopScreenX11::AddObserver(gfx::DisplayObserver* observer) {
  observer_list_.AddObserver(observer);
}

void DesktopScreenX11::RemoveObserver(gfx::DisplayObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool DesktopScreenX11::CanDispatchEvent(const ui::PlatformEvent& event) {
  return event->type - xrandr_event_base_ == RRScreenChangeNotify ||
         event->type - xrandr_event_base_ == RRNotify;
}

uint32_t DesktopScreenX11::DispatchEvent(const ui::PlatformEvent& event) {
  if (event->type - xrandr_event_base_ == RRScreenChangeNotify) {
    // Pass the event through to xlib.
    XRRUpdateConfiguration(event);
  } else if (event->type - xrandr_event_base_ == RRNotify) {
    // There's some sort of observer dispatch going on here, but I don't think
    // it's the screen's?
    if (configure_timer_.get() && configure_timer_->IsRunning()) {
      configure_timer_->Reset();
    } else {
      configure_timer_.reset(new base::OneShotTimer<DesktopScreenX11>());
      configure_timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kConfigureDelayMs),
          this,
          &DesktopScreenX11::ConfigureTimerFired);
    }
  } else {
    NOTREACHED();
  }

  return ui::POST_DISPATCH_NONE;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopScreenX11, private:

DesktopScreenX11::DesktopScreenX11(
    const std::vector<gfx::Display>& test_displays)
    : xdisplay_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      has_xrandr_(false),
      xrandr_event_base_(0),
      displays_(test_displays) {
}

std::vector<gfx::Display> DesktopScreenX11::BuildDisplaysFromXRandRInfo() {
  std::vector<gfx::Display> displays;
  XRRScreenResources* resources =
      XRRGetScreenResourcesCurrent(xdisplay_, x_root_window_);
  if (!resources) {
    LOG(ERROR) << "XRandR returned no displays. Falling back to Root Window.";
    return GetFallbackDisplayList();
  }

  bool has_work_area = false;
  gfx::Rect work_area;
  std::vector<int> value;
  if (ui::GetIntArrayProperty(x_root_window_, "_NET_WORKAREA", &value) &&
      value.size() >= 4) {
    work_area = gfx::Rect(value[0], value[1], value[2], value[3]);
    has_work_area = true;
  }

  float device_scale_factor = 1.0f;
  for (int i = 0; i < resources->noutput; ++i) {
    RROutput output_id = resources->outputs[i];
    XRROutputInfo* output_info =
        XRRGetOutputInfo(xdisplay_, resources, output_id);

    bool is_connected = (output_info->connection == RR_Connected);
    if (!is_connected) {
      XRRFreeOutputInfo(output_info);
      continue;
    }

    if (output_info->crtc) {
      XRRCrtcInfo *crtc = XRRGetCrtcInfo(xdisplay_,
                                         resources,
                                         output_info->crtc);

      int64 display_id = -1;
      if (!ui::GetDisplayId(output_id, static_cast<uint8>(i), &display_id)) {
        // It isn't ideal, but if we can't parse the EDID data, fallback on the
        // display number.
        display_id = i;
      }

      gfx::Rect crtc_bounds(crtc->x, crtc->y, crtc->width, crtc->height);
      gfx::Display display(display_id, crtc_bounds);

      if (!gfx::Display::HasForceDeviceScaleFactor()) {
        if (i == 0 && !ui::IsDisplaySizeBlackListed(
            gfx::Size(output_info->mm_width, output_info->mm_height))) {
          // As per display scale factor is not supported right now,
          // the primary display's scale factor is always used.
          device_scale_factor = GetDeviceScaleFactor(crtc->width,
                                                     output_info->mm_width);
          DCHECK_LE(1.0f, device_scale_factor);
        }
        display.SetScaleAndBounds(device_scale_factor, crtc_bounds);
      }

      if (has_work_area) {
        gfx::Rect intersection = crtc_bounds;
        intersection.Intersect(work_area);
        display.set_work_area(intersection);
      }

      switch (crtc->rotation) {
        case RR_Rotate_0:
          display.set_rotation(gfx::Display::ROTATE_0);
          break;
        case RR_Rotate_90:
          display.set_rotation(gfx::Display::ROTATE_90);
          break;
        case RR_Rotate_180:
          display.set_rotation(gfx::Display::ROTATE_180);
          break;
        case RR_Rotate_270:
          display.set_rotation(gfx::Display::ROTATE_270);
          break;
      }

      displays.push_back(display);

      XRRFreeCrtcInfo(crtc);
    }

    XRRFreeOutputInfo(output_info);
  }

  XRRFreeScreenResources(resources);

  if (displays.empty())
    return GetFallbackDisplayList();

  return displays;
}

void DesktopScreenX11::ConfigureTimerFired() {
  std::vector<gfx::Display> new_displays = BuildDisplaysFromXRandRInfo();
  ProcessDisplayChange(new_displays);
}

////////////////////////////////////////////////////////////////////////////////

gfx::Screen* CreateDesktopScreen() {
  return new DesktopScreenX11;
}

}  // namespace views

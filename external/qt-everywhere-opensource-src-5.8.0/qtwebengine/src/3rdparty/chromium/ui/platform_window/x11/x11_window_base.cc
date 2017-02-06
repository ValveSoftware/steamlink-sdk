// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/x11_window_base.h"

#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

namespace {

const char* kAtomsToCache[] = {"UTF8_STRING",  "WM_DELETE_WINDOW",
                               "_NET_WM_NAME", "_NET_WM_PID",
                               "_NET_WM_PING", NULL};

bool g_override_redirect = false;

XID FindXEventTarget(const XEvent& xev) {
  XID target = xev.xany.window;
  if (xev.type == GenericEvent)
    target = static_cast<XIDeviceEvent*>(xev.xcookie.data)->event;
  return target;
}

}  // namespace

X11WindowBase::X11WindowBase(PlatformWindowDelegate* delegate)
    : delegate_(delegate),
      xdisplay_(gfx::GetXDisplay()),
      xwindow_(None),
      xroot_window_(DefaultRootWindow(xdisplay_)),
      atom_cache_(xdisplay_, kAtomsToCache) {
  DCHECK(delegate_);
  TouchFactory::SetTouchDeviceListFromCommandLine();
}

X11WindowBase::~X11WindowBase() {
  Destroy();
}

void X11WindowBase::Destroy() {
  if (xwindow_ == None)
    return;

  // Stop processing events.
  XID xwindow = xwindow_;
  XDisplay* xdisplay = xdisplay_;
  xwindow_ = None;
  delegate_->OnClosed();
  // |this| might be deleted because of the above call.

  XDestroyWindow(xdisplay, xwindow);
}

void X11WindowBase::Create() {
  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.background_pixmap = None;
  swa.bit_gravity = NorthWestGravity;
  swa.override_redirect = g_override_redirect;
  xwindow_ = XCreateWindow(
      xdisplay_, xroot_window_, requested_bounds_.x(), requested_bounds_.y(),
      requested_bounds_.width(), requested_bounds_.height(),
      0,               // border width
      CopyFromParent,  // depth
      InputOutput,
      CopyFromParent,  // visual
      CWBackPixmap | CWBitGravity | CWOverrideRedirect, &swa);

  // Setup XInput event mask.
  long event_mask = ButtonPressMask | ButtonReleaseMask | FocusChangeMask |
                    KeyPressMask | KeyReleaseMask | EnterWindowMask |
                    LeaveWindowMask | ExposureMask | VisibilityChangeMask |
                    StructureNotifyMask | PropertyChangeMask |
                    PointerMotionMask;
  XSelectInput(xdisplay_, xwindow_, event_mask);

  // Setup XInput2 event mask.
  unsigned char mask[XIMaskLen(XI_LASTEVENT)];
  memset(mask, 0, sizeof(mask));

  XISetMask(mask, XI_TouchBegin);
  XISetMask(mask, XI_TouchUpdate);
  XISetMask(mask, XI_TouchEnd);
  XISetMask(mask, XI_ButtonPress);
  XISetMask(mask, XI_ButtonRelease);
  XISetMask(mask, XI_Motion);
  XISetMask(mask, XI_KeyPress);
  XISetMask(mask, XI_KeyRelease);
  XISetMask(mask, XI_HierarchyChanged);

  XIEventMask evmask;
  evmask.deviceid = XIAllDevices;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;
  XISelectEvents(xdisplay_, xwindow_, &evmask, 1);
  XFlush(xdisplay_);

  ::Atom protocols[2];
  protocols[0] = atom_cache_.GetAtom("WM_DELETE_WINDOW");
  protocols[1] = atom_cache_.GetAtom("_NET_WM_PING");
  XSetWMProtocols(xdisplay_, xwindow_, protocols, 2);

  // We need a WM_CLIENT_MACHINE and WM_LOCALE_NAME value so we integrate with
  // the desktop environment.
  XSetWMProperties(xdisplay_, xwindow_, NULL, NULL, NULL, 0, NULL, NULL, NULL);

  // Likewise, the X server needs to know this window's pid so it knows which
  // program to kill if the window hangs.
  // XChangeProperty() expects "pid" to be long.
  static_assert(sizeof(long) >= sizeof(pid_t),
                "pid_t should not be larger than long");
  long pid = getpid();
  XChangeProperty(xdisplay_, xwindow_, atom_cache_.GetAtom("_NET_WM_PID"),
                  XA_CARDINAL, 32, PropModeReplace,
                  reinterpret_cast<unsigned char*>(&pid), 1);
  // Before we map the window, set size hints. Otherwise, some window managers
  // will ignore toplevel XMoveWindow commands.
  XSizeHints size_hints;
  size_hints.flags = PPosition | PWinGravity;
  size_hints.x = requested_bounds_.x();
  size_hints.y = requested_bounds_.y();
  // Set StaticGravity so that the window position is not affected by the
  // frame width when running with window manager.
  size_hints.win_gravity = StaticGravity;
  XSetWMNormalHints(xdisplay_, xwindow_, &size_hints);

  // TODO(sky): provide real scale factor.
  delegate_->OnAcceleratedWidgetAvailable(xwindow_, 1.f);
}

void X11WindowBase::Show() {
  if (window_mapped_)
    return;
  if (xwindow_ == None)
    Create();

  XMapWindow(xdisplay_, xwindow_);

  // We now block until our window is mapped. Some X11 APIs will crash and
  // burn if passed |xwindow_| before the window is mapped, and XMapWindow is
  // asynchronous.
  if (X11EventSource::GetInstance())
    X11EventSource::GetInstance()->BlockUntilWindowMapped(xwindow_);
  window_mapped_ = true;
}

void X11WindowBase::Hide() {
  if (!window_mapped_)
    return;
  XWithdrawWindow(xdisplay_, xwindow_, 0);
  window_mapped_ = false;
}

void X11WindowBase::Close() {
  Destroy();
}

void X11WindowBase::SetBounds(const gfx::Rect& bounds) {
  requested_bounds_ = bounds;
  if (!window_mapped_ || bounds == confirmed_bounds_)
    return;
  XWindowChanges changes = {0};
  unsigned value_mask = CWX | CWY | CWWidth | CWHeight;
  changes.x = bounds.x();
  changes.y = bounds.y();
  changes.width = bounds.width();
  changes.height = bounds.height();
  XConfigureWindow(xdisplay_, xwindow_, value_mask, &changes);
}

gfx::Rect X11WindowBase::GetBounds() {
  return confirmed_bounds_;
}

void X11WindowBase::SetTitle(const base::string16& title) {
  if (window_title_ == title)
    return;
  window_title_ = title;
  std::string utf8str = base::UTF16ToUTF8(title);
  XChangeProperty(xdisplay_, xwindow_, atom_cache_.GetAtom("_NET_WM_NAME"),
                  atom_cache_.GetAtom("UTF8_STRING"), 8, PropModeReplace,
                  reinterpret_cast<const unsigned char*>(utf8str.c_str()),
                  utf8str.size());
  XTextProperty xtp;
  char* c_utf8_str = const_cast<char*>(utf8str.c_str());
  if (Xutf8TextListToTextProperty(xdisplay_, &c_utf8_str, 1, XUTF8StringStyle,
                                  &xtp) == Success) {
    XSetWMName(xdisplay_, xwindow_, &xtp);
    XFree(xtp.value);
  }
}

void X11WindowBase::SetCapture() {}

void X11WindowBase::ReleaseCapture() {}

void X11WindowBase::ToggleFullscreen() {}

void X11WindowBase::Maximize() {}

void X11WindowBase::Minimize() {}

void X11WindowBase::Restore() {}

void X11WindowBase::MoveCursorTo(const gfx::Point& location) {
  XWarpPointer(xdisplay_, None, xroot_window_, 0, 0, 0, 0,
               confirmed_bounds_.x() + location.x(),
               confirmed_bounds_.y() + location.y());
}

void X11WindowBase::ConfineCursorToBounds(const gfx::Rect& bounds) {}

PlatformImeController* X11WindowBase::GetPlatformImeController() {
  return nullptr;
}

bool X11WindowBase::IsEventForXWindow(const XEvent& xev) const {
  return xwindow_ != None && FindXEventTarget(xev) == xwindow_;
}

void X11WindowBase::ProcessXWindowEvent(XEvent* xev) {
  switch (xev->type) {
    case Expose: {
      gfx::Rect damage_rect(xev->xexpose.x, xev->xexpose.y, xev->xexpose.width,
                            xev->xexpose.height);
      delegate_->OnDamageRect(damage_rect);
      break;
    }

    case FocusOut:
      if (xev->xfocus.mode != NotifyGrab)
        delegate_->OnLostCapture();
      break;

    case ConfigureNotify: {
      DCHECK_EQ(xwindow_, xev->xconfigure.event);
      DCHECK_EQ(xwindow_, xev->xconfigure.window);
      // It's possible that the X window may be resized by some other means than
      // from within aura (e.g. the X window manager can change the size). Make
      // sure the root window size is maintained properly.
      int translated_x_in_pixels = xev->xconfigure.x;
      int translated_y_in_pixels = xev->xconfigure.y;
      if (!xev->xconfigure.send_event && !xev->xconfigure.override_redirect) {
        Window unused;
        XTranslateCoordinates(xdisplay_, xwindow_, xroot_window_, 0, 0,
                              &translated_x_in_pixels, &translated_y_in_pixels,
                              &unused);
      }
      gfx::Rect bounds(translated_x_in_pixels, translated_y_in_pixels,
                       xev->xconfigure.width, xev->xconfigure.height);
      if (confirmed_bounds_ != bounds) {
        confirmed_bounds_ = bounds;
        delegate_->OnBoundsChanged(confirmed_bounds_);
      }
      break;
    }

    case ClientMessage: {
      Atom message = static_cast<Atom>(xev->xclient.data.l[0]);
      if (message == atom_cache_.GetAtom("WM_DELETE_WINDOW")) {
        delegate_->OnCloseRequest();
      } else if (message == atom_cache_.GetAtom("_NET_WM_PING")) {
        XEvent reply_event = *xev;
        reply_event.xclient.window = xroot_window_;

        XSendEvent(xdisplay_, reply_event.xclient.window, False,
                   SubstructureRedirectMask | SubstructureNotifyMask,
                   &reply_event);
        XFlush(xdisplay_);
      }
      break;
    }
  }
}

namespace test {

void SetUseOverrideRedirectWindowByDefault(bool override_redirect) {
  g_override_redirect = override_redirect;
}

}  // namespace test
}  // namespace ui

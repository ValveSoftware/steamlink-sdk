// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/x11_desktop_handler.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include "base/message_loop/message_loop.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/x/x11_foreign_window_manager.h"
#include "ui/base/x/x11_menu_list.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11_error_tracker.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"

namespace {

const char* kAtomsToCache[] = {
  "_NET_ACTIVE_WINDOW",
  NULL
};

// Our global instance. Deleted when our Env() is deleted.
views::X11DesktopHandler* g_handler = NULL;

}  // namespace

namespace views {

// static
X11DesktopHandler* X11DesktopHandler::get() {
  if (!g_handler)
    g_handler = new X11DesktopHandler;

  return g_handler;
}

X11DesktopHandler::X11DesktopHandler()
    : xdisplay_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      x_active_window_(None),
      wm_user_time_ms_(CurrentTime),
      current_window_(None),
      current_window_active_state_(NOT_ACTIVE),
      atom_cache_(xdisplay_, kAtomsToCache),
      wm_supports_active_window_(false) {
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  aura::Env::GetInstance()->AddObserver(this);

  XWindowAttributes attr;
  XGetWindowAttributes(xdisplay_, x_root_window_, &attr);
  XSelectInput(xdisplay_, x_root_window_,
               attr.your_event_mask | PropertyChangeMask |
               StructureNotifyMask | SubstructureNotifyMask);

  if (ui::GuessWindowManager() == ui::WM_WMII) {
    // wmii says that it supports _NET_ACTIVE_WINDOW but does not.
    // https://code.google.com/p/wmii/issues/detail?id=266
    wm_supports_active_window_ = false;
  } else {
    wm_supports_active_window_ =
        ui::WmSupportsHint(atom_cache_.GetAtom("_NET_ACTIVE_WINDOW"));
  }
}

X11DesktopHandler::~X11DesktopHandler() {
  aura::Env::GetInstance()->RemoveObserver(this);
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

void X11DesktopHandler::ActivateWindow(::Window window) {
  if ((current_window_ == None || current_window_ == window) &&
      current_window_active_state_ == NOT_ACTIVE) {
    // |window| is most likely still active wrt to the X server. Undo the
    // changes made in DeactivateWindow().
    OnActiveWindowChanged(window, ACTIVE);

    // Go through the regular activation path such that calling
    // DeactivateWindow() and ActivateWindow() immediately afterwards results
    // in an active X window.
  }

  if (wm_supports_active_window_) {
    DCHECK_EQ(gfx::GetXDisplay(), xdisplay_);

    // If the window is not already active, send a hint to activate it
    if (x_active_window_ != window) {
      if (wm_user_time_ms_ == CurrentTime) {
        set_wm_user_time_ms(
            ui::X11EventSource::GetInstance()->UpdateLastSeenServerTime());
      }
      XEvent xclient;
      memset(&xclient, 0, sizeof(xclient));
      xclient.type = ClientMessage;
      xclient.xclient.window = window;
      xclient.xclient.message_type = atom_cache_.GetAtom("_NET_ACTIVE_WINDOW");
      xclient.xclient.format = 32;
      xclient.xclient.data.l[0] = 1;  // Specified we are an app.
      xclient.xclient.data.l[1] = wm_user_time_ms_;
      xclient.xclient.data.l[2] = None;
      xclient.xclient.data.l[3] = 0;
      xclient.xclient.data.l[4] = 0;

      XSendEvent(xdisplay_, x_root_window_, False,
                 SubstructureRedirectMask | SubstructureNotifyMask,
                 &xclient);
    } else {
      OnActiveWindowChanged(window, ACTIVE);
    }
  } else {
    XRaiseWindow(xdisplay_, window);
    // Directly ask the X server to give focus to the window. Note
    // that the call will raise an X error if the window is not
    // mapped.
    XSetInputFocus(xdisplay_, window, RevertToParent, CurrentTime);

    OnActiveWindowChanged(window, ACTIVE);
  }
}

void X11DesktopHandler::set_wm_user_time_ms(Time time_ms) {
  if (time_ms != CurrentTime) {
    int64_t event_time_64 = time_ms;
    int64_t time_difference = wm_user_time_ms_ - event_time_64;
    // Ignore timestamps that go backwards. However, X server time is a 32-bit
    // millisecond counter, so if the time goes backwards by more than half the
    // range of the 32-bit counter, treat it as a rollover.
    if (time_difference < 0 || time_difference > (UINT32_MAX >> 1))
      wm_user_time_ms_ = time_ms;
  }
}

void X11DesktopHandler::DeactivateWindow(::Window window) {
  if (!IsActiveWindow(window))
    return;

  XLowerWindow(xdisplay_, window);

  // Per ICCCM: http://tronche.com/gui/x/icccm/sec-4.html#s-4.1.7
  // "Clients should not give up the input focus of their own volition.
  // They should ignore input that they receive instead."
  //
  // There is nothing else that we can do. Pretend that we have been
  // deactivated and ignore keyboard input in DesktopWindowTreeHostX11.
  OnActiveWindowChanged(window, NOT_ACTIVE);
}

bool X11DesktopHandler::IsActiveWindow(::Window window) const {
  return window == current_window_ && current_window_active_state_ == ACTIVE;
}

void X11DesktopHandler::ProcessXEvent(XEvent* event) {
  // Ignore focus events that are being sent only because the pointer is over
  // our window, even if the input focus is in a different window.
  if (event->xfocus.detail == NotifyPointer)
    return;

  switch (event->type) {
    case FocusIn:
      if (current_window_ != event->xfocus.window)
        OnActiveWindowChanged(event->xfocus.window, ACTIVE);
      break;
    case FocusOut:
      if (current_window_ == event->xfocus.window)
        OnActiveWindowChanged(None, NOT_ACTIVE);
      break;
    default:
      NOTREACHED();
  }
}

bool X11DesktopHandler::CanDispatchEvent(const ui::PlatformEvent& event) {
  return event->type == CreateNotify || event->type == DestroyNotify ||
         (event->type == PropertyNotify &&
          event->xproperty.window == x_root_window_);
}

uint32_t X11DesktopHandler::DispatchEvent(const ui::PlatformEvent& event) {
  switch (event->type) {
    case PropertyNotify: {
      // Check for a change to the active window.
      CHECK_EQ(x_root_window_, event->xproperty.window);
      ::Atom active_window_atom = atom_cache_.GetAtom("_NET_ACTIVE_WINDOW");
      if (event->xproperty.atom == active_window_atom) {
        ::Window window;
        if (ui::GetXIDProperty(x_root_window_, "_NET_ACTIVE_WINDOW", &window) &&
            window) {
          x_active_window_ = window;
          OnActiveWindowChanged(window, ACTIVE);
        } else {
          x_active_window_ = None;
        }
      }
      break;
    }

    case CreateNotify:
      OnWindowCreatedOrDestroyed(event->type, event->xcreatewindow.window);
      break;
    case DestroyNotify:
      OnWindowCreatedOrDestroyed(event->type, event->xdestroywindow.window);
      // If the current active window is being destroyed, reset our tracker.
      if (x_active_window_ == event->xdestroywindow.window) {
        x_active_window_ = None;
      }
      break;
    default:
      NOTREACHED();
  }

  return ui::POST_DISPATCH_NONE;
}

void X11DesktopHandler::OnWindowInitialized(aura::Window* window) {
}

void X11DesktopHandler::OnWillDestroyEnv() {
  g_handler = NULL;
  delete this;
}

void X11DesktopHandler::OnActiveWindowChanged(::Window xid,
                                              ActiveState active_state) {
  if (current_window_ == xid && current_window_active_state_ == active_state)
    return;

  if (current_window_active_state_ == ACTIVE) {
    DesktopWindowTreeHostX11* old_host =
        views::DesktopWindowTreeHostX11::GetHostForXID(current_window_);
    if (old_host)
      old_host->HandleNativeWidgetActivationChanged(false);
  }

  // Update the current window ID to effectively change the active widget.
  current_window_ = xid;
  current_window_active_state_ = active_state;

  if (active_state == ACTIVE) {
    DesktopWindowTreeHostX11* new_host =
        views::DesktopWindowTreeHostX11::GetHostForXID(xid);
    if (new_host)
      new_host->HandleNativeWidgetActivationChanged(true);
  }
}

void X11DesktopHandler::OnWindowCreatedOrDestroyed(int event_type,
                                                   XID window) {
  // Menus created by Chrome can be drag and drop targets. Since they are
  // direct children of the screen root window and have override_redirect
  // we cannot use regular _NET_CLIENT_LIST_STACKING property to find them
  // and use a separate cache to keep track of them.
  // TODO(varkha): Implement caching of all top level X windows and their
  // coordinates and stacking order to eliminate repeated calls to the X server
  // during mouse movement, drag and shaping events.
  if (event_type == CreateNotify) {
    // The window might be destroyed if the message pump did not get a chance to
    // run but we can safely ignore the X error.
    gfx::X11ErrorTracker error_tracker;
    ui::XMenuList::GetInstance()->MaybeRegisterMenu(window);
  } else {
    ui::XMenuList::GetInstance()->MaybeUnregisterMenu(window);
  }

  if (event_type == DestroyNotify) {
    // Notify the XForeignWindowManager that |window| has been destroyed.
    ui::XForeignWindowManager::GetInstance()->OnWindowDestroyed(window);
  }
}

}  // namespace views

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/x11_desktop_handler.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include "base/message_loop/message_loop.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/x/x11_menu_list.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11_error_tracker.h"
#include "ui/views/ime/input_method.h"
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
      wm_user_time_ms_(0),
      current_window_(None),
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

  ::Window active_window;
  wm_supports_active_window_ =
    ui::GetXIDProperty(x_root_window_, "_NET_ACTIVE_WINDOW", &active_window) &&
    active_window;
}

X11DesktopHandler::~X11DesktopHandler() {
  aura::Env::GetInstance()->RemoveObserver(this);
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

void X11DesktopHandler::ActivateWindow(::Window window) {
  if (wm_supports_active_window_) {
    DCHECK_EQ(gfx::GetXDisplay(), xdisplay_);

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
    XRaiseWindow(xdisplay_, window);

    // XRaiseWindow will not give input focus to the window. We now need to ask
    // the X server to do that. Note that the call will raise an X error if the
    // window is not mapped.
    XSetInputFocus(xdisplay_, window, RevertToParent, CurrentTime);

    OnActiveWindowChanged(window);
  }
}

bool X11DesktopHandler::IsActiveWindow(::Window window) const {
  return window == current_window_;
}

void X11DesktopHandler::ProcessXEvent(XEvent* event) {
  switch (event->type) {
    case FocusIn:
      if (current_window_ != event->xfocus.window)
        OnActiveWindowChanged(event->xfocus.window);
      break;
    case FocusOut:
      if (current_window_ == event->xfocus.window)
        OnActiveWindowChanged(None);
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
          OnActiveWindowChanged(window);
        }
      }
      break;
    }

    // Menus created by Chrome can be drag and drop targets. Since they are
    // direct children of the screen root window and have override_redirect
    // we cannot use regular _NET_CLIENT_LIST_STACKING property to find them
    // and use a separate cache to keep track of them.
    // TODO(varkha): Implement caching of all top level X windows and their
    // coordinates and stacking order to eliminate repeated calls to X server
    // during mouse movement, drag and shaping events.
    case CreateNotify: {
      // The window might be destroyed if the message pump haven't gotten a
      // chance to run but we can safely ignore the X error.
      gfx::X11ErrorTracker error_tracker;
      XCreateWindowEvent *xcwe = &event->xcreatewindow;
      ui::XMenuList::GetInstance()->MaybeRegisterMenu(xcwe->window);
      break;
    }
    case DestroyNotify: {
      XDestroyWindowEvent *xdwe = &event->xdestroywindow;
      ui::XMenuList::GetInstance()->MaybeUnregisterMenu(xdwe->window);
      break;
    }
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

void X11DesktopHandler::OnActiveWindowChanged(::Window xid) {
  if (current_window_ == xid)
    return;
  DesktopWindowTreeHostX11* old_host =
      views::DesktopWindowTreeHostX11::GetHostForXID(current_window_);
  if (old_host)
    old_host->HandleNativeWidgetActivationChanged(false);

  // Update the current window ID to effectively change the active widget.
  current_window_ = xid;

  DesktopWindowTreeHostX11* new_host =
      views::DesktopWindowTreeHostX11::GetHostForXID(xid);
  if (new_host)
    new_host->HandleNativeWidgetActivationChanged(true);
}

}  // namespace views

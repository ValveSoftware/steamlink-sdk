// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/x11/x11_event_source.h"

#include <glib.h>
#include <X11/Xlib.h>

namespace ui {

namespace {

struct GLibX11Source : public GSource {
  // Note: The GLibX11Source is created and destroyed by GLib. So its
  // constructor/destructor may or may not get called.
  XDisplay* display;
  GPollFD* poll_fd;
};

gboolean XSourcePrepare(GSource* source, gint* timeout_ms) {
  GLibX11Source* gxsource = static_cast<GLibX11Source*>(source);
  if (XPending(gxsource->display))
    *timeout_ms = 0;
  else
    *timeout_ms = -1;
  return FALSE;
}

gboolean XSourceCheck(GSource* source) {
  GLibX11Source* gxsource = static_cast<GLibX11Source*>(source);
  return XPending(gxsource->display);
}

gboolean XSourceDispatch(GSource* source,
                         GSourceFunc unused_func,
                         gpointer data) {
  X11EventSource* x11_source = static_cast<X11EventSource*>(data);
  x11_source->DispatchXEvents();
  return TRUE;
}

GSourceFuncs XSourceFuncs = {
  XSourcePrepare,
  XSourceCheck,
  XSourceDispatch,
  NULL
};

class X11EventSourceGlib : public X11EventSource {
 public:
  explicit X11EventSourceGlib(XDisplay* display)
      : X11EventSource(display),
        x_source_(NULL) {
    InitXSource(ConnectionNumber(display));
  }

  virtual ~X11EventSourceGlib() {
    g_source_destroy(x_source_);
    g_source_unref(x_source_);
  }

 private:
  void InitXSource(int fd) {
    CHECK(!x_source_);
    CHECK(display()) << "Unable to get connection to X server";

    x_poll_.reset(new GPollFD());
    x_poll_->fd = fd;
    x_poll_->events = G_IO_IN;
    x_poll_->revents = 0;

    GLibX11Source* glib_x_source = static_cast<GLibX11Source*>
        (g_source_new(&XSourceFuncs, sizeof(GLibX11Source)));
    glib_x_source->display = display();
    glib_x_source->poll_fd = x_poll_.get();

    x_source_ = glib_x_source;
    g_source_add_poll(x_source_, x_poll_.get());
    g_source_set_can_recurse(x_source_, TRUE);
    g_source_set_callback(x_source_, NULL, this, NULL);
    g_source_attach(x_source_, g_main_context_default());
  }

  // The GLib event source for X events.
  GSource* x_source_;

  // The poll attached to |x_source_|.
  scoped_ptr<GPollFD> x_poll_;

  DISALLOW_COPY_AND_ASSIGN(X11EventSourceGlib);
};

}  // namespace

scoped_ptr<PlatformEventSource> PlatformEventSource::CreateDefault() {
  return scoped_ptr<PlatformEventSource>(
      new X11EventSourceGlib(gfx::GetXDisplay()));
}

}  // namespace ui

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines utility functions for X11 (Linux only). This code has been
// ported from XCB since we can't use XCB on Ubuntu while its 32-bit support
// remains woefully incomplete.

#include "ui/base/x/x11_util.h"

#include <ctype.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <list>
#include <map>
#include <utility>
#include <vector>

#include <X11/extensions/shape.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xcursor/Xcursor.h>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_byteorder.h"
#include "base/threading/thread.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPostConfig.h"
#include "ui/base/x/x11_menu_list.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/x/device_data_manager.h"
#include "ui/events/x/touch_factory_x11.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/point.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/x/x11_error_tracker.h"

#if defined(OS_FREEBSD)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

namespace ui {

namespace {

int DefaultX11ErrorHandler(XDisplay* d, XErrorEvent* e) {
  if (base::MessageLoop::current()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&LogErrorEventDescription, d, *e));
  } else {
    LOG(ERROR)
        << "X error received: "
        << "serial " << e->serial << ", "
        << "error_code " << static_cast<int>(e->error_code) << ", "
        << "request_code " << static_cast<int>(e->request_code) << ", "
        << "minor_code " << static_cast<int>(e->minor_code);
  }
  return 0;
}

int DefaultX11IOErrorHandler(XDisplay* d) {
  // If there's an IO error it likely means the X server has gone away
  LOG(ERROR) << "X IO error received (X server probably went away)";
  _exit(1);
}

// Note: The caller should free the resulting value data.
bool GetProperty(XID window, const std::string& property_name, long max_length,
                 Atom* type, int* format, unsigned long* num_items,
                 unsigned char** property) {
  Atom property_atom = GetAtom(property_name.c_str());
  unsigned long remaining_bytes = 0;
  return XGetWindowProperty(gfx::GetXDisplay(),
                            window,
                            property_atom,
                            0,          // offset into property data to read
                            max_length, // max length to get
                            False,      // deleted
                            AnyPropertyType,
                            type,
                            format,
                            num_items,
                            &remaining_bytes,
                            property);
}

// A process wide singleton that manages the usage of X cursors.
class XCursorCache {
 public:
  XCursorCache() {}
  ~XCursorCache() {
    Clear();
  }

  ::Cursor GetCursor(int cursor_shape) {
    // Lookup cursor by attempting to insert a null value, which avoids
    // a second pass through the map after a cache miss.
    std::pair<std::map<int, ::Cursor>::iterator, bool> it = cache_.insert(
        std::make_pair(cursor_shape, 0));
    if (it.second) {
      XDisplay* display = gfx::GetXDisplay();
      it.first->second = XCreateFontCursor(display, cursor_shape);
    }
    return it.first->second;
  }

  void Clear() {
    XDisplay* display = gfx::GetXDisplay();
    for (std::map<int, ::Cursor>::iterator it =
        cache_.begin(); it != cache_.end(); ++it) {
      XFreeCursor(display, it->second);
    }
    cache_.clear();
  }

 private:
  // Maps X11 font cursor shapes to Cursor IDs.
  std::map<int, ::Cursor> cache_;

  DISALLOW_COPY_AND_ASSIGN(XCursorCache);
};

XCursorCache* cursor_cache = NULL;

// A process wide singleton cache for custom X cursors.
class XCustomCursorCache {
 public:
  static XCustomCursorCache* GetInstance() {
    return Singleton<XCustomCursorCache>::get();
  }

  ::Cursor InstallCustomCursor(XcursorImage* image) {
    XCustomCursor* custom_cursor = new XCustomCursor(image);
    ::Cursor xcursor = custom_cursor->cursor();
    cache_[xcursor] = custom_cursor;
    return xcursor;
  }

  void Ref(::Cursor cursor) {
    cache_[cursor]->Ref();
  }

  void Unref(::Cursor cursor) {
    if (cache_[cursor]->Unref())
      cache_.erase(cursor);
  }

  void Clear() {
    cache_.clear();
  }

  const XcursorImage* GetXcursorImage(::Cursor cursor) const {
    return cache_.find(cursor)->second->image();
  }

 private:
  friend struct DefaultSingletonTraits<XCustomCursorCache>;

  class XCustomCursor {
   public:
    // This takes ownership of the image.
    XCustomCursor(XcursorImage* image)
        : image_(image),
          ref_(1) {
      cursor_ = XcursorImageLoadCursor(gfx::GetXDisplay(), image);
    }

    ~XCustomCursor() {
      XcursorImageDestroy(image_);
      XFreeCursor(gfx::GetXDisplay(), cursor_);
    }

    ::Cursor cursor() const { return cursor_; }

    void Ref() {
      ++ref_;
    }

    // Returns true if the cursor was destroyed because of the unref.
    bool Unref() {
      if (--ref_ == 0) {
        delete this;
        return true;
      }
      return false;
    }

    const XcursorImage* image() const {
      return image_;
    };

   private:
    XcursorImage* image_;
    int ref_;
    ::Cursor cursor_;

    DISALLOW_COPY_AND_ASSIGN(XCustomCursor);
  };

  XCustomCursorCache() {}
  ~XCustomCursorCache() {
    Clear();
  }

  std::map< ::Cursor, XCustomCursor*> cache_;
  DISALLOW_COPY_AND_ASSIGN(XCustomCursorCache);
};

}  // namespace

bool IsXInput2Available() {
  return DeviceDataManager::GetInstance()->IsXInput2Available();
}

static SharedMemorySupport DoQuerySharedMemorySupport(XDisplay* dpy) {
  int dummy;
  Bool pixmaps_supported;
  // Query the server's support for XSHM.
  if (!XShmQueryVersion(dpy, &dummy, &dummy, &pixmaps_supported))
    return SHARED_MEMORY_NONE;

#if defined(OS_FREEBSD)
  // On FreeBSD we can't access the shared memory after it was marked for
  // deletion, unless this behaviour is explicitly enabled by the user.
  // In case it's not enabled disable shared memory support.
  int allow_removed;
  size_t length = sizeof(allow_removed);

  if ((sysctlbyname("kern.ipc.shm_allow_removed", &allow_removed, &length,
      NULL, 0) < 0) || allow_removed < 1) {
    return SHARED_MEMORY_NONE;
  }
#endif

  // Next we probe to see if shared memory will really work
  int shmkey = shmget(IPC_PRIVATE, 1, 0600);
  if (shmkey == -1) {
    LOG(WARNING) << "Failed to get shared memory segment.";
    return SHARED_MEMORY_NONE;
  } else {
    VLOG(1) << "Got shared memory segment " << shmkey;
  }

  void* address = shmat(shmkey, NULL, 0);
  // Mark the shared memory region for deletion
  shmctl(shmkey, IPC_RMID, NULL);

  XShmSegmentInfo shminfo;
  memset(&shminfo, 0, sizeof(shminfo));
  shminfo.shmid = shmkey;

  gfx::X11ErrorTracker err_tracker;
  bool result = XShmAttach(dpy, &shminfo);
  if (result)
    VLOG(1) << "X got shared memory segment " << shmkey;
  else
    LOG(WARNING) << "X failed to attach to shared memory segment " << shmkey;
  if (err_tracker.FoundNewError())
    result = false;
  shmdt(address);
  if (!result) {
    LOG(WARNING) << "X failed to attach to shared memory segment " << shmkey;
    return SHARED_MEMORY_NONE;
  }

  VLOG(1) << "X attached to shared memory segment " << shmkey;

  XShmDetach(dpy, &shminfo);
  return pixmaps_supported ? SHARED_MEMORY_PIXMAP : SHARED_MEMORY_PUTIMAGE;
}

SharedMemorySupport QuerySharedMemorySupport(XDisplay* dpy) {
  static SharedMemorySupport shared_memory_support = SHARED_MEMORY_NONE;
  static bool shared_memory_support_cached = false;

  if (shared_memory_support_cached)
    return shared_memory_support;

  shared_memory_support = DoQuerySharedMemorySupport(dpy);
  shared_memory_support_cached = true;

  return shared_memory_support;
}

bool QueryRenderSupport(Display* dpy) {
  int dummy;
  // We don't care about the version of Xrender since all the features which
  // we use are included in every version.
  static bool render_supported = XRenderQueryExtension(dpy, &dummy, &dummy);

  return render_supported;
}

::Cursor GetXCursor(int cursor_shape) {
  if (!cursor_cache)
    cursor_cache = new XCursorCache;
  return cursor_cache->GetCursor(cursor_shape);
}

::Cursor CreateReffedCustomXCursor(XcursorImage* image) {
  return XCustomCursorCache::GetInstance()->InstallCustomCursor(image);
}

void RefCustomXCursor(::Cursor cursor) {
  XCustomCursorCache::GetInstance()->Ref(cursor);
}

void UnrefCustomXCursor(::Cursor cursor) {
  XCustomCursorCache::GetInstance()->Unref(cursor);
}

XcursorImage* SkBitmapToXcursorImage(const SkBitmap* cursor_image,
                                     const gfx::Point& hotspot) {
  DCHECK(cursor_image->colorType() == kPMColor_SkColorType);
  gfx::Point hotspot_point = hotspot;
  SkBitmap scaled;

  // X11 seems to have issues with cursors when images get larger than 64
  // pixels. So rescale the image if necessary.
  const float kMaxPixel = 64.f;
  bool needs_scale = false;
  if (cursor_image->width() > kMaxPixel || cursor_image->height() > kMaxPixel) {
    float scale = 1.f;
    if (cursor_image->width() > cursor_image->height())
      scale = kMaxPixel / cursor_image->width();
    else
      scale = kMaxPixel / cursor_image->height();

    scaled = skia::ImageOperations::Resize(*cursor_image,
        skia::ImageOperations::RESIZE_BETTER,
        static_cast<int>(cursor_image->width() * scale),
        static_cast<int>(cursor_image->height() * scale));
    hotspot_point = gfx::ToFlooredPoint(gfx::ScalePoint(hotspot, scale));
    needs_scale = true;
  }

  const SkBitmap* bitmap = needs_scale ? &scaled : cursor_image;
  XcursorImage* image = XcursorImageCreate(bitmap->width(), bitmap->height());
  image->xhot = std::min(bitmap->width() - 1, hotspot_point.x());
  image->yhot = std::min(bitmap->height() - 1, hotspot_point.y());

  if (bitmap->width() && bitmap->height()) {
    bitmap->lockPixels();
    // The |bitmap| contains ARGB image, so just copy it.
    memcpy(image->pixels,
           bitmap->getPixels(),
           bitmap->width() * bitmap->height() * 4);
    bitmap->unlockPixels();
  }

  return image;
}


int CoalescePendingMotionEvents(const XEvent* xev,
                                XEvent* last_event) {
  XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev->xcookie.data);
  int num_coalesced = 0;
  XDisplay* display = xev->xany.display;
  int event_type = xev->xgeneric.evtype;

  DCHECK(event_type == XI_Motion || event_type == XI_TouchUpdate);

  while (XPending(display)) {
    XEvent next_event;
    XPeekEvent(display, &next_event);

    // If we can't get the cookie, abort the check.
    if (!XGetEventData(next_event.xgeneric.display, &next_event.xcookie))
      return num_coalesced;

    // If this isn't from a valid device, throw the event away, as
    // that's what the message pump would do. Device events come in pairs
    // with one from the master and one from the slave so there will
    // always be at least one pending.
    if (!ui::TouchFactory::GetInstance()->ShouldProcessXI2Event(&next_event)) {
      XFreeEventData(display, &next_event.xcookie);
      XNextEvent(display, &next_event);
      continue;
    }

    if (next_event.type == GenericEvent &&
        next_event.xgeneric.evtype == event_type &&
        !ui::DeviceDataManager::GetInstance()->IsCMTGestureEvent(
            &next_event)) {
      XIDeviceEvent* next_xievent =
          static_cast<XIDeviceEvent*>(next_event.xcookie.data);
      // Confirm that the motion event is targeted at the same window
      // and that no buttons or modifiers have changed.
      if (xievent->event == next_xievent->event &&
          xievent->child == next_xievent->child &&
          xievent->detail == next_xievent->detail &&
          xievent->buttons.mask_len == next_xievent->buttons.mask_len &&
          (memcmp(xievent->buttons.mask,
                  next_xievent->buttons.mask,
                  xievent->buttons.mask_len) == 0) &&
          xievent->mods.base == next_xievent->mods.base &&
          xievent->mods.latched == next_xievent->mods.latched &&
          xievent->mods.locked == next_xievent->mods.locked &&
          xievent->mods.effective == next_xievent->mods.effective) {
        XFreeEventData(display, &next_event.xcookie);
        // Free the previous cookie.
        if (num_coalesced > 0)
          XFreeEventData(display, &last_event->xcookie);
        // Get the event and its cookie data.
        XNextEvent(display, last_event);
        XGetEventData(display, &last_event->xcookie);
        ++num_coalesced;
        continue;
      }
    }
    // This isn't an event we want so free its cookie data.
    XFreeEventData(display, &next_event.xcookie);
    break;
  }

  if (event_type == XI_Motion && num_coalesced > 0) {
    base::TimeDelta delta = ui::EventTimeFromNative(last_event) -
        ui::EventTimeFromNative(const_cast<XEvent*>(xev));
    UMA_HISTOGRAM_COUNTS_10000("Event.CoalescedCount.Mouse", num_coalesced);
    UMA_HISTOGRAM_TIMES("Event.CoalescedLatency.Mouse", delta);
  }
  return num_coalesced;
}

void HideHostCursor() {
  CR_DEFINE_STATIC_LOCAL(XScopedCursor, invisible_cursor,
                         (CreateInvisibleCursor(), gfx::GetXDisplay()));
  XDefineCursor(gfx::GetXDisplay(), DefaultRootWindow(gfx::GetXDisplay()),
                invisible_cursor.get());
}

::Cursor CreateInvisibleCursor() {
  XDisplay* xdisplay = gfx::GetXDisplay();
  ::Cursor invisible_cursor;
  char nodata[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  XColor black;
  black.red = black.green = black.blue = 0;
  Pixmap blank = XCreateBitmapFromData(xdisplay,
                                       DefaultRootWindow(xdisplay),
                                       nodata, 8, 8);
  invisible_cursor = XCreatePixmapCursor(xdisplay, blank, blank,
                                         &black, &black, 0, 0);
  XFreePixmap(xdisplay, blank);
  return invisible_cursor;
}

void SetUseOSWindowFrame(XID window, bool use_os_window_frame) {
  // This data structure represents additional hints that we send to the window
  // manager and has a direct lineage back to Motif, which defined this de facto
  // standard. This struct doesn't seem 64-bit safe though, but it's what GDK
  // does.
  typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
  } MotifWmHints;

  MotifWmHints motif_hints;
  memset(&motif_hints, 0, sizeof(motif_hints));
  // Signals that the reader of the _MOTIF_WM_HINTS property should pay
  // attention to the value of |decorations|.
  motif_hints.flags = (1L << 1);
  motif_hints.decorations = use_os_window_frame ? 1 : 0;

  ::Atom hint_atom = GetAtom("_MOTIF_WM_HINTS");
  XChangeProperty(gfx::GetXDisplay(),
                  window,
                  hint_atom,
                  hint_atom,
                  32,
                  PropModeReplace,
                  reinterpret_cast<unsigned char*>(&motif_hints),
                  sizeof(MotifWmHints)/sizeof(long));
}

bool IsShapeExtensionAvailable() {
  int dummy;
  static bool is_shape_available =
      XShapeQueryExtension(gfx::GetXDisplay(), &dummy, &dummy);
  return is_shape_available;
}

XID GetX11RootWindow() {
  return DefaultRootWindow(gfx::GetXDisplay());
}

bool GetCurrentDesktop(int* desktop) {
  return GetIntProperty(GetX11RootWindow(), "_NET_CURRENT_DESKTOP", desktop);
}

void SetHideTitlebarWhenMaximizedProperty(XID window,
                                          HideTitlebarWhenMaximized property) {
  // XChangeProperty() expects "hide" to be long.
  unsigned long hide = property;
  XChangeProperty(gfx::GetXDisplay(),
      window,
      GetAtom("_GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED"),
      XA_CARDINAL,
      32,  // size in bits
      PropModeReplace,
      reinterpret_cast<unsigned char*>(&hide),
      1);
}

void ClearX11DefaultRootWindow() {
  XDisplay* display = gfx::GetXDisplay();
  XID root_window = GetX11RootWindow();
  gfx::Rect root_bounds;
  if (!GetWindowRect(root_window, &root_bounds)) {
    LOG(ERROR) << "Failed to get the bounds of the X11 root window";
    return;
  }

  XGCValues gc_values = {0};
  gc_values.foreground = BlackPixel(display, DefaultScreen(display));
  GC gc = XCreateGC(display, root_window, GCForeground, &gc_values);
  XFillRectangle(display, root_window, gc,
                 root_bounds.x(),
                 root_bounds.y(),
                 root_bounds.width(),
                 root_bounds.height());
  XFreeGC(display, gc);
}

bool IsWindowVisible(XID window) {
  TRACE_EVENT0("ui", "IsWindowVisible");

  XWindowAttributes win_attributes;
  if (!XGetWindowAttributes(gfx::GetXDisplay(), window, &win_attributes))
    return false;
  if (win_attributes.map_state != IsViewable)
    return false;

  // Minimized windows are not visible.
  std::vector<Atom> wm_states;
  if (GetAtomArrayProperty(window, "_NET_WM_STATE", &wm_states)) {
    Atom hidden_atom = GetAtom("_NET_WM_STATE_HIDDEN");
    if (std::find(wm_states.begin(), wm_states.end(), hidden_atom) !=
            wm_states.end()) {
      return false;
    }
  }

  // Some compositing window managers (notably kwin) do not actually unmap
  // windows on desktop switch, so we also must check the current desktop.
  int window_desktop, current_desktop;
  return (!GetWindowDesktop(window, &window_desktop) ||
          !GetCurrentDesktop(&current_desktop) ||
          window_desktop == kAllDesktops ||
          window_desktop == current_desktop);
}

bool GetWindowRect(XID window, gfx::Rect* rect) {
  Window root, child;
  int x, y;
  unsigned int width, height;
  unsigned int border_width, depth;

  if (!XGetGeometry(gfx::GetXDisplay(), window, &root, &x, &y,
                    &width, &height, &border_width, &depth))
    return false;

  if (!XTranslateCoordinates(gfx::GetXDisplay(), window, root,
                             0, 0, &x, &y, &child))
    return false;

  *rect = gfx::Rect(x, y, width, height);

  std::vector<int> insets;
  if (GetIntArrayProperty(window, "_NET_FRAME_EXTENTS", &insets) &&
      insets.size() == 4) {
    rect->Inset(-insets[0], -insets[2], -insets[1], -insets[3]);
  }
  // Not all window managers support _NET_FRAME_EXTENTS so return true even if
  // requesting the property fails.

  return true;
}


bool WindowContainsPoint(XID window, gfx::Point screen_loc) {
  TRACE_EVENT0("ui", "WindowContainsPoint");

  gfx::Rect window_rect;
  if (!GetWindowRect(window, &window_rect))
    return false;

  if (!window_rect.Contains(screen_loc))
    return false;

  if (!IsShapeExtensionAvailable())
    return true;

  // According to http://www.x.org/releases/X11R7.6/doc/libXext/shapelib.html,
  // if an X display supports the shape extension the bounds of a window are
  // defined as the intersection of the window bounds and the interior
  // rectangles. This means to determine if a point is inside a window for the
  // purpose of input handling we have to check the rectangles in the ShapeInput
  // list.
  // According to http://www.x.org/releases/current/doc/xextproto/shape.html,
  // we need to also respect the ShapeBounding rectangles.
  // The effective input region of a window is defined to be the intersection
  // of the client input region with both the default input region and the
  // client bounding region. Any portion of the client input region that is not
  // included in both the default input region and the client bounding region
  // will not be included in the effective input region on the screen.
  int rectangle_kind[] = {ShapeInput, ShapeBounding};
  for (size_t kind_index = 0;
       kind_index < arraysize(rectangle_kind);
       kind_index++) {
    int dummy;
    int shape_rects_size = 0;
    XRectangle* shape_rects = XShapeGetRectangles(gfx::GetXDisplay(),
                                                  window,
                                                  rectangle_kind[kind_index],
                                                  &shape_rects_size,
                                                  &dummy);
    if (!shape_rects) {
      // The shape is empty. This can occur when |window| is minimized.
      DCHECK_EQ(0, shape_rects_size);
      return false;
    }
    bool is_in_shape_rects = false;
    for (int i = 0; i < shape_rects_size; ++i) {
      // The ShapeInput and ShapeBounding rects are to be in window space, so we
      // have to translate by the window_rect's offset to map to screen space.
      gfx::Rect shape_rect =
          gfx::Rect(shape_rects[i].x + window_rect.x(),
                    shape_rects[i].y + window_rect.y(),
                    shape_rects[i].width, shape_rects[i].height);
      if (shape_rect.Contains(screen_loc)) {
        is_in_shape_rects = true;
        break;
      }
    }
    XFree(shape_rects);
    if (!is_in_shape_rects)
      return false;
  }
  return true;
}


bool PropertyExists(XID window, const std::string& property_name) {
  Atom type = None;
  int format = 0;  // size in bits of each item in 'property'
  unsigned long num_items = 0;
  unsigned char* property = NULL;

  int result = GetProperty(window, property_name, 1,
                           &type, &format, &num_items, &property);
  if (result != Success)
    return false;

  XFree(property);
  return num_items > 0;
}

bool GetRawBytesOfProperty(XID window,
                           Atom property,
                           scoped_refptr<base::RefCountedMemory>* out_data,
                           size_t* out_data_bytes,
                           size_t* out_data_items,
                           Atom* out_type) {
  // Retrieve the data from our window.
  unsigned long nitems = 0;
  unsigned long nbytes = 0;
  Atom prop_type = None;
  int prop_format = 0;
  unsigned char* property_data = NULL;
  if (XGetWindowProperty(gfx::GetXDisplay(), window, property,
                         0, 0x1FFFFFFF /* MAXINT32 / 4 */, False,
                         AnyPropertyType, &prop_type, &prop_format,
                         &nitems, &nbytes, &property_data) != Success) {
    return false;
  }

  if (prop_type == None)
    return false;

  size_t bytes = 0;
  // So even though we should theoretically have nbytes (and we can't
  // pass NULL there), we need to manually calculate the byte length here
  // because nbytes always returns zero.
  switch (prop_format) {
    case 8:
      bytes = nitems;
      break;
    case 16:
      bytes = sizeof(short) * nitems;
      break;
    case 32:
      bytes = sizeof(long) * nitems;
      break;
    default:
      NOTREACHED();
      break;
  }

  if (out_data_bytes)
    *out_data_bytes = bytes;

  if (out_data)
    *out_data = new XRefcountedMemory(property_data, bytes);
  else
    XFree(property_data);

  if (out_data_items)
    *out_data_items = nitems;

  if (out_type)
    *out_type = prop_type;

  return true;
}

bool GetIntProperty(XID window, const std::string& property_name, int* value) {
  Atom type = None;
  int format = 0;  // size in bits of each item in 'property'
  unsigned long num_items = 0;
  unsigned char* property = NULL;

  int result = GetProperty(window, property_name, 1,
                           &type, &format, &num_items, &property);
  if (result != Success)
    return false;

  if (format != 32 || num_items != 1) {
    XFree(property);
    return false;
  }

  *value = static_cast<int>(*(reinterpret_cast<long*>(property)));
  XFree(property);
  return true;
}

bool GetXIDProperty(XID window, const std::string& property_name, XID* value) {
  Atom type = None;
  int format = 0;  // size in bits of each item in 'property'
  unsigned long num_items = 0;
  unsigned char* property = NULL;

  int result = GetProperty(window, property_name, 1,
                           &type, &format, &num_items, &property);
  if (result != Success)
    return false;

  if (format != 32 || num_items != 1) {
    XFree(property);
    return false;
  }

  *value = *(reinterpret_cast<XID*>(property));
  XFree(property);
  return true;
}

bool GetIntArrayProperty(XID window,
                         const std::string& property_name,
                         std::vector<int>* value) {
  Atom type = None;
  int format = 0;  // size in bits of each item in 'property'
  unsigned long num_items = 0;
  unsigned char* properties = NULL;

  int result = GetProperty(window, property_name,
                           (~0L), // (all of them)
                           &type, &format, &num_items, &properties);
  if (result != Success)
    return false;

  if (format != 32) {
    XFree(properties);
    return false;
  }

  long* int_properties = reinterpret_cast<long*>(properties);
  value->clear();
  for (unsigned long i = 0; i < num_items; ++i) {
    value->push_back(static_cast<int>(int_properties[i]));
  }
  XFree(properties);
  return true;
}

bool GetAtomArrayProperty(XID window,
                          const std::string& property_name,
                          std::vector<Atom>* value) {
  Atom type = None;
  int format = 0;  // size in bits of each item in 'property'
  unsigned long num_items = 0;
  unsigned char* properties = NULL;

  int result = GetProperty(window, property_name,
                           (~0L), // (all of them)
                           &type, &format, &num_items, &properties);
  if (result != Success)
    return false;

  if (type != XA_ATOM) {
    XFree(properties);
    return false;
  }

  Atom* atom_properties = reinterpret_cast<Atom*>(properties);
  value->clear();
  value->insert(value->begin(), atom_properties, atom_properties + num_items);
  XFree(properties);
  return true;
}

bool GetStringProperty(
    XID window, const std::string& property_name, std::string* value) {
  Atom type = None;
  int format = 0;  // size in bits of each item in 'property'
  unsigned long num_items = 0;
  unsigned char* property = NULL;

  int result = GetProperty(window, property_name, 1024,
                           &type, &format, &num_items, &property);
  if (result != Success)
    return false;

  if (format != 8) {
    XFree(property);
    return false;
  }

  value->assign(reinterpret_cast<char*>(property), num_items);
  XFree(property);
  return true;
}

bool SetIntProperty(XID window,
                    const std::string& name,
                    const std::string& type,
                    int value) {
  std::vector<int> values(1, value);
  return SetIntArrayProperty(window, name, type, values);
}

bool SetIntArrayProperty(XID window,
                         const std::string& name,
                         const std::string& type,
                         const std::vector<int>& value) {
  DCHECK(!value.empty());
  Atom name_atom = GetAtom(name.c_str());
  Atom type_atom = GetAtom(type.c_str());

  // XChangeProperty() expects values of type 32 to be longs.
  scoped_ptr<long[]> data(new long[value.size()]);
  for (size_t i = 0; i < value.size(); ++i)
    data[i] = value[i];

  gfx::X11ErrorTracker err_tracker;
  XChangeProperty(gfx::GetXDisplay(),
                  window,
                  name_atom,
                  type_atom,
                  32,  // size in bits of items in 'value'
                  PropModeReplace,
                  reinterpret_cast<const unsigned char*>(data.get()),
                  value.size());  // num items
  return !err_tracker.FoundNewError();
}

bool SetAtomProperty(XID window,
                     const std::string& name,
                     const std::string& type,
                     Atom value) {
  std::vector<Atom> values(1, value);
  return SetAtomArrayProperty(window, name, type, values);
}

bool SetAtomArrayProperty(XID window,
                          const std::string& name,
                          const std::string& type,
                          const std::vector<Atom>& value) {
  DCHECK(!value.empty());
  Atom name_atom = GetAtom(name.c_str());
  Atom type_atom = GetAtom(type.c_str());

  // XChangeProperty() expects values of type 32 to be longs.
  scoped_ptr<Atom[]> data(new Atom[value.size()]);
  for (size_t i = 0; i < value.size(); ++i)
    data[i] = value[i];

  gfx::X11ErrorTracker err_tracker;
  XChangeProperty(gfx::GetXDisplay(),
                  window,
                  name_atom,
                  type_atom,
                  32,  // size in bits of items in 'value'
                  PropModeReplace,
                  reinterpret_cast<const unsigned char*>(data.get()),
                  value.size());  // num items
  return !err_tracker.FoundNewError();
}

bool SetStringProperty(XID window,
                       Atom property,
                       Atom type,
                       const std::string& value) {
  gfx::X11ErrorTracker err_tracker;
  XChangeProperty(gfx::GetXDisplay(),
                  window,
                  property,
                  type,
                  8,
                  PropModeReplace,
                  reinterpret_cast<const unsigned char*>(value.c_str()),
                  value.size());
  return !err_tracker.FoundNewError();
}

Atom GetAtom(const char* name) {
  // TODO(derat): Cache atoms to avoid round-trips to the server.
  return XInternAtom(gfx::GetXDisplay(), name, false);
}

void SetWindowClassHint(XDisplay* display,
                        XID window,
                        const std::string& res_name,
                        const std::string& res_class) {
  XClassHint class_hints;
  // const_cast is safe because XSetClassHint does not modify the strings.
  // Just to be safe, the res_name and res_class parameters are local copies,
  // not const references.
  class_hints.res_name = const_cast<char*>(res_name.c_str());
  class_hints.res_class = const_cast<char*>(res_class.c_str());
  XSetClassHint(display, window, &class_hints);
}

void SetWindowRole(XDisplay* display, XID window, const std::string& role) {
  if (role.empty()) {
    XDeleteProperty(display, window, GetAtom("WM_WINDOW_ROLE"));
  } else {
    char* role_c = const_cast<char*>(role.c_str());
    XChangeProperty(display, window, GetAtom("WM_WINDOW_ROLE"), XA_STRING, 8,
                    PropModeReplace,
                    reinterpret_cast<unsigned char*>(role_c),
                    role.size());
  }
}

bool GetCustomFramePrefDefault() {
  // Ideally, we'd use the custom frame by default and just fall back on using
  // system decorations for the few (?) tiling window managers where the custom
  // frame doesn't make sense (e.g. awesome, ion3, ratpoison, xmonad, etc.) or
  // other WMs where it has issues (e.g. Fluxbox -- see issue 19130).  The EWMH
  // _NET_SUPPORTING_WM property makes it easy to look up a name for the current
  // WM, but at least some of the WMs in the latter group don't set it.
  // Instead, we default to using system decorations for all WMs and
  // special-case the ones where the custom frame should be used.
  ui::WindowManagerName wm_type = GuessWindowManager();
  return (wm_type == WM_BLACKBOX ||
          wm_type == WM_COMPIZ ||
          wm_type == WM_ENLIGHTENMENT ||
          wm_type == WM_METACITY ||
          wm_type == WM_MUFFIN ||
          wm_type == WM_MUTTER ||
          wm_type == WM_OPENBOX ||
          wm_type == WM_XFWM4);
}

bool GetWindowDesktop(XID window, int* desktop) {
  return GetIntProperty(window, "_NET_WM_DESKTOP", desktop);
}

std::string GetX11ErrorString(XDisplay* display, int err) {
  char buffer[256];
  XGetErrorText(display, err, buffer, arraysize(buffer));
  return buffer;
}

// Returns true if |window| is a named window.
bool IsWindowNamed(XID window) {
  XTextProperty prop;
  if (!XGetWMName(gfx::GetXDisplay(), window, &prop) || !prop.value)
    return false;

  XFree(prop.value);
  return true;
}

bool EnumerateChildren(EnumerateWindowsDelegate* delegate, XID window,
                       const int max_depth, int depth) {
  if (depth > max_depth)
    return false;

  std::vector<XID> windows;
  std::vector<XID>::iterator iter;
  if (depth == 0) {
    XMenuList::GetInstance()->InsertMenuWindowXIDs(&windows);
    // Enumerate the menus first.
    for (iter = windows.begin(); iter != windows.end(); iter++) {
      if (delegate->ShouldStopIterating(*iter))
        return true;
    }
    windows.clear();
  }

  XID root, parent, *children;
  unsigned int num_children;
  int status = XQueryTree(gfx::GetXDisplay(), window, &root, &parent, &children,
                          &num_children);
  if (status == 0)
    return false;

  for (int i = static_cast<int>(num_children) - 1; i >= 0; i--)
    windows.push_back(children[i]);

  XFree(children);

  // XQueryTree returns the children of |window| in bottom-to-top order, so
  // reverse-iterate the list to check the windows from top-to-bottom.
  for (iter = windows.begin(); iter != windows.end(); iter++) {
    if (IsWindowNamed(*iter) && delegate->ShouldStopIterating(*iter))
      return true;
  }

  // If we're at this point, we didn't find the window we're looking for at the
  // current level, so we need to recurse to the next level.  We use a second
  // loop because the recursion and call to XQueryTree are expensive and is only
  // needed for a small number of cases.
  if (++depth <= max_depth) {
    for (iter = windows.begin(); iter != windows.end(); iter++) {
      if (EnumerateChildren(delegate, *iter, max_depth, depth))
        return true;
    }
  }

  return false;
}

bool EnumerateAllWindows(EnumerateWindowsDelegate* delegate, int max_depth) {
  XID root = GetX11RootWindow();
  return EnumerateChildren(delegate, root, max_depth, 0);
}

void EnumerateTopLevelWindows(ui::EnumerateWindowsDelegate* delegate) {
  std::vector<XID> stack;
  if (!ui::GetXWindowStack(ui::GetX11RootWindow(), &stack)) {
    // Window Manager doesn't support _NET_CLIENT_LIST_STACKING, so fall back
    // to old school enumeration of all X windows.  Some WMs parent 'top-level'
    // windows in unnamed actual top-level windows (ion WM), so extend the
    // search depth to all children of top-level windows.
    const int kMaxSearchDepth = 1;
    ui::EnumerateAllWindows(delegate, kMaxSearchDepth);
    return;
  }
  XMenuList::GetInstance()->InsertMenuWindowXIDs(&stack);

  std::vector<XID>::iterator iter;
  for (iter = stack.begin(); iter != stack.end(); iter++) {
    if (delegate->ShouldStopIterating(*iter))
      return;
  }
}

bool GetXWindowStack(Window window, std::vector<XID>* windows) {
  windows->clear();

  Atom type;
  int format;
  unsigned long count;
  unsigned char *data = NULL;
  if (GetProperty(window,
                  "_NET_CLIENT_LIST_STACKING",
                  ~0L,
                  &type,
                  &format,
                  &count,
                  &data) != Success) {
    return false;
  }

  bool result = false;
  if (type == XA_WINDOW && format == 32 && data && count > 0) {
    result = true;
    XID* stack = reinterpret_cast<XID*>(data);
    for (long i = static_cast<long>(count) - 1; i >= 0; i--)
      windows->push_back(stack[i]);
  }

  if (data)
    XFree(data);

  return result;
}

bool CopyAreaToCanvas(XID drawable,
                      gfx::Rect source_bounds,
                      gfx::Point dest_offset,
                      gfx::Canvas* canvas) {
  ui::XScopedImage scoped_image(
      XGetImage(gfx::GetXDisplay(), drawable,
                source_bounds.x(), source_bounds.y(),
                source_bounds.width(), source_bounds.height(),
                AllPlanes, ZPixmap));
  XImage* image = scoped_image.get();
  if (!image) {
    LOG(ERROR) << "XGetImage failed";
    return false;
  }

  if (image->bits_per_pixel == 32) {
    if ((0xff << SK_R32_SHIFT) != image->red_mask ||
        (0xff << SK_G32_SHIFT) != image->green_mask ||
        (0xff << SK_B32_SHIFT) != image->blue_mask) {
      LOG(WARNING) << "XImage and Skia byte orders differ";
      return false;
    }

    // Set the alpha channel before copying to the canvas.  Otherwise, areas of
    // the framebuffer that were cleared by ply-image rather than being obscured
    // by an image during boot may end up transparent.
    // TODO(derat|marcheu): Remove this if/when ply-image has been updated to
    // set the framebuffer's alpha channel regardless of whether the device
    // claims to support alpha or not.
    for (int i = 0; i < image->width * image->height * 4; i += 4)
      image->data[i + 3] = 0xff;

    SkBitmap bitmap;
    bitmap.installPixels(SkImageInfo::MakeN32Premul(image->width,
                                                    image->height),
                         image->data, image->bytes_per_line);
    gfx::ImageSkia image_skia;
    gfx::ImageSkiaRep image_rep(bitmap, canvas->image_scale());
    image_skia.AddRepresentation(image_rep);
    canvas->DrawImageInt(image_skia, dest_offset.x(), dest_offset.y());
  } else {
    NOTIMPLEMENTED() << "Unsupported bits-per-pixel " << image->bits_per_pixel;
    return false;
  }

  return true;
}

bool GetWindowManagerName(std::string* wm_name) {
  DCHECK(wm_name);
  int wm_window = 0;
  if (!GetIntProperty(GetX11RootWindow(),
                      "_NET_SUPPORTING_WM_CHECK",
                      &wm_window)) {
    return false;
  }

  // It's possible that a window manager started earlier in this X session left
  // a stale _NET_SUPPORTING_WM_CHECK property when it was replaced by a
  // non-EWMH window manager, so we trap errors in the following requests to
  // avoid crashes (issue 23860).

  // EWMH requires the supporting-WM window to also have a
  // _NET_SUPPORTING_WM_CHECK property pointing to itself (to avoid a stale
  // property referencing an ID that's been recycled for another window), so we
  // check that too.
  gfx::X11ErrorTracker err_tracker;
  int wm_window_property = 0;
  bool result = GetIntProperty(
      wm_window, "_NET_SUPPORTING_WM_CHECK", &wm_window_property);
  if (err_tracker.FoundNewError() || !result ||
      wm_window_property != wm_window) {
    return false;
  }

  result = GetStringProperty(
      static_cast<XID>(wm_window), "_NET_WM_NAME", wm_name);
  return !err_tracker.FoundNewError() && result;
}

WindowManagerName GuessWindowManager() {
  std::string name;
  if (GetWindowManagerName(&name)) {
    // These names are taken from the WMs' source code.
    if (name == "Blackbox")
      return WM_BLACKBOX;
    if (name == "chromeos-wm")
      return WM_CHROME_OS;
    if (name == "Compiz" || name == "compiz")
      return WM_COMPIZ;
    if (name == "e16")
      return WM_ENLIGHTENMENT;
    if (StartsWithASCII(name, "IceWM", true))
      return WM_ICE_WM;
    if (name == "KWin")
      return WM_KWIN;
    if (name == "Metacity")
      return WM_METACITY;
    if (name == "Mutter (Muffin)")
      return WM_MUFFIN;
    if (name == "GNOME Shell")
      return WM_MUTTER; // GNOME Shell uses Mutter
    if (name == "Mutter")
      return WM_MUTTER;
    if (name == "Openbox")
      return WM_OPENBOX;
    if (name == "Xfwm4")
      return WM_XFWM4;
  }
  return WM_UNKNOWN;
}

void SetDefaultX11ErrorHandlers() {
  SetX11ErrorHandlers(NULL, NULL);
}

bool IsX11WindowFullScreen(XID window) {
  // If _NET_WM_STATE_FULLSCREEN is in _NET_SUPPORTED, use the presence or
  // absence of _NET_WM_STATE_FULLSCREEN in _NET_WM_STATE to determine
  // whether we're fullscreen.
  Atom fullscreen_atom = GetAtom("_NET_WM_STATE_FULLSCREEN");
  if (WmSupportsHint(fullscreen_atom)) {
    std::vector<Atom> atom_properties;
    if (GetAtomArrayProperty(window,
                             "_NET_WM_STATE",
                             &atom_properties)) {
      return std::find(atom_properties.begin(),
                       atom_properties.end(),
                       fullscreen_atom) !=
          atom_properties.end();
    }
  }

  gfx::Rect window_rect;
  if (!ui::GetWindowRect(window, &window_rect))
    return false;

  // We can't use gfx::Screen here because we don't have an aura::Window. So
  // instead just look at the size of the default display.
  //
  // TODO(erg): Actually doing this correctly would require pulling out xrandr,
  // which we don't even do in the desktop screen yet.
  ::XDisplay* display = gfx::GetXDisplay();
  ::Screen* screen = DefaultScreenOfDisplay(display);
  int width = WidthOfScreen(screen);
  int height = HeightOfScreen(screen);
  return window_rect.size() == gfx::Size(width, height);
}

bool WmSupportsHint(Atom atom) {
  std::vector<Atom> supported_atoms;
  if (!GetAtomArrayProperty(GetX11RootWindow(),
                            "_NET_SUPPORTED",
                            &supported_atoms)) {
    return false;
  }

  return std::find(supported_atoms.begin(), supported_atoms.end(), atom) !=
      supported_atoms.end();
}

const unsigned char* XRefcountedMemory::front() const {
  return x11_data_;
}

size_t XRefcountedMemory::size() const {
  return length_;
}

XRefcountedMemory::~XRefcountedMemory() {
  XFree(x11_data_);
}

XScopedString::~XScopedString() {
  XFree(string_);
}

XScopedImage::~XScopedImage() {
  reset(NULL);
}

void XScopedImage::reset(XImage* image) {
  if (image_ == image)
    return;
  if (image_)
    XDestroyImage(image_);
  image_ = image;
}

XScopedCursor::XScopedCursor(::Cursor cursor, XDisplay* display)
    : cursor_(cursor),
      display_(display) {
}

XScopedCursor::~XScopedCursor() {
  reset(0U);
}

::Cursor XScopedCursor::get() const {
  return cursor_;
}

void XScopedCursor::reset(::Cursor cursor) {
  if (cursor_)
    XFreeCursor(display_, cursor_);
  cursor_ = cursor;
}

namespace test {

void ResetXCursorCache() {
  delete cursor_cache;
  cursor_cache = NULL;
}

const XcursorImage* GetCachedXcursorImage(::Cursor cursor) {
  return XCustomCursorCache::GetInstance()->GetXcursorImage(cursor);
}
}

// ----------------------------------------------------------------------------
// These functions are declared in x11_util_internal.h because they require
// XLib.h to be included, and it conflicts with many other headers.
XRenderPictFormat* GetRenderARGB32Format(XDisplay* dpy) {
  static XRenderPictFormat* pictformat = NULL;
  if (pictformat)
    return pictformat;

  // First look for a 32-bit format which ignores the alpha value
  XRenderPictFormat templ;
  templ.depth = 32;
  templ.type = PictTypeDirect;
  templ.direct.red = 16;
  templ.direct.green = 8;
  templ.direct.blue = 0;
  templ.direct.redMask = 0xff;
  templ.direct.greenMask = 0xff;
  templ.direct.blueMask = 0xff;
  templ.direct.alphaMask = 0;

  static const unsigned long kMask =
    PictFormatType | PictFormatDepth |
    PictFormatRed | PictFormatRedMask |
    PictFormatGreen | PictFormatGreenMask |
    PictFormatBlue | PictFormatBlueMask |
    PictFormatAlphaMask;

  pictformat = XRenderFindFormat(dpy, kMask, &templ, 0 /* first result */);

  if (!pictformat) {
    // Not all X servers support xRGB32 formats. However, the XRENDER spec says
    // that they must support an ARGB32 format, so we can always return that.
    pictformat = XRenderFindStandardFormat(dpy, PictStandardARGB32);
    CHECK(pictformat) << "XRENDER ARGB32 not supported.";
  }

  return pictformat;
}

void SetX11ErrorHandlers(XErrorHandler error_handler,
                         XIOErrorHandler io_error_handler) {
  XSetErrorHandler(error_handler ? error_handler : DefaultX11ErrorHandler);
  XSetIOErrorHandler(
      io_error_handler ? io_error_handler : DefaultX11IOErrorHandler);
}

void LogErrorEventDescription(XDisplay* dpy,
                              const XErrorEvent& error_event) {
  char error_str[256];
  char request_str[256];

  XGetErrorText(dpy, error_event.error_code, error_str, sizeof(error_str));

  strncpy(request_str, "Unknown", sizeof(request_str));
  if (error_event.request_code < 128) {
    std::string num = base::UintToString(error_event.request_code);
    XGetErrorDatabaseText(
        dpy, "XRequest", num.c_str(), "Unknown", request_str,
        sizeof(request_str));
  } else {
    int num_ext;
    char** ext_list = XListExtensions(dpy, &num_ext);

    for (int i = 0; i < num_ext; i++) {
      int ext_code, first_event, first_error;
      XQueryExtension(dpy, ext_list[i], &ext_code, &first_event, &first_error);
      if (error_event.request_code == ext_code) {
        std::string msg = base::StringPrintf(
            "%s.%d", ext_list[i], error_event.minor_code);
        XGetErrorDatabaseText(
            dpy, "XRequest", msg.c_str(), "Unknown", request_str,
            sizeof(request_str));
        break;
      }
    }
    XFreeExtensionList(ext_list);
  }

  LOG(WARNING)
      << "X error received: "
      << "serial " << error_event.serial << ", "
      << "error_code " << static_cast<int>(error_event.error_code)
      << " (" << error_str << "), "
      << "request_code " << static_cast<int>(error_event.request_code) << ", "
      << "minor_code " << static_cast<int>(error_event.minor_code)
      << " (" << request_str << ")";
}

// ----------------------------------------------------------------------------
// End of x11_util_internal.h


}  // namespace ui

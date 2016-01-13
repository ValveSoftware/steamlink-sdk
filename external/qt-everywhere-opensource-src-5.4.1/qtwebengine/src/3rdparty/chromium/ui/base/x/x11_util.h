// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_UTIL_H_
#define UI_BASE_X_X11_UTIL_H_

// This file declares utility functions for X11 (Linux only).
//
// These functions do not require the Xlib headers to be included (which is why
// we use a void* for Visual*). The Xlib headers are highly polluting so we try
// hard to limit their spread into the rest of the code.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/event_types.h"
#include "base/memory/ref_counted_memory.h"
#include "ui/base/ui_base_export.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/point.h"
#include "ui/gfx/x/x11_types.h"

typedef unsigned long Atom;
typedef unsigned long XSharedMemoryId;  // ShmSeg in the X headers.
typedef unsigned long Cursor;
typedef struct _XcursorImage XcursorImage;
typedef union _XEvent XEvent;

namespace gfx {
class Canvas;
class Point;
class Rect;
}
class SkBitmap;

namespace ui {

// These functions use the default display and this /must/ be called from
// the UI thread. Thus, they don't support multiple displays.

// These functions cache their results ---------------------------------

// Returns true if the system supports XINPUT2.
UI_BASE_EXPORT bool IsXInput2Available();

// X shared memory comes in three flavors:
// 1) No SHM support,
// 2) SHM putimage,
// 3) SHM pixmaps + putimage.
enum SharedMemorySupport {
  SHARED_MEMORY_NONE,
  SHARED_MEMORY_PUTIMAGE,
  SHARED_MEMORY_PIXMAP
};
// Return the shared memory type of our X connection.
UI_BASE_EXPORT SharedMemorySupport QuerySharedMemorySupport(XDisplay* dpy);

// Return true iff the display supports Xrender
UI_BASE_EXPORT bool QueryRenderSupport(XDisplay* dpy);

// Returns an X11 Cursor, sharable across the process.
// |cursor_shape| is an X font cursor shape, see XCreateFontCursor().
UI_BASE_EXPORT ::Cursor GetXCursor(int cursor_shape);

// Creates a custom X cursor from the image. This takes ownership of image. The
// caller must not free/modify the image. The refcount of the newly created
// cursor is set to 1.
UI_BASE_EXPORT ::Cursor CreateReffedCustomXCursor(XcursorImage* image);

// Increases the refcount of the custom cursor.
UI_BASE_EXPORT void RefCustomXCursor(::Cursor cursor);

// Decreases the refcount of the custom cursor, and destroys it if it reaches 0.
UI_BASE_EXPORT void UnrefCustomXCursor(::Cursor cursor);

// Creates a XcursorImage and copies the SkBitmap |bitmap| on it. |bitmap|
// should be non-null. Caller owns the returned object.
UI_BASE_EXPORT XcursorImage* SkBitmapToXcursorImage(const SkBitmap* bitmap,
                                                    const gfx::Point& hotspot);

// Coalesce all pending motion events (touch or mouse) that are at the top of
// the queue, and return the number eliminated, storing the last one in
// |last_event|.
UI_BASE_EXPORT int CoalescePendingMotionEvents(const XEvent* xev,
                                               XEvent* last_event);

// Hides the host cursor.
UI_BASE_EXPORT void HideHostCursor();

// Returns an invisible cursor.
UI_BASE_EXPORT ::Cursor CreateInvisibleCursor();

// Sets whether |window| should use the OS window frame.
UI_BASE_EXPORT void SetUseOSWindowFrame(XID window, bool use_os_window_frame);

// These functions do not cache their results --------------------------

// Returns true if the shape extension is supported.
UI_BASE_EXPORT bool IsShapeExtensionAvailable();

// Get the X window id for the default root window
UI_BASE_EXPORT XID GetX11RootWindow();

// Returns the user's current desktop.
UI_BASE_EXPORT bool GetCurrentDesktop(int* desktop);

enum HideTitlebarWhenMaximized {
  SHOW_TITLEBAR_WHEN_MAXIMIZED = 0,
  HIDE_TITLEBAR_WHEN_MAXIMIZED = 1,
};
// Sets _GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED on |window|.
UI_BASE_EXPORT void SetHideTitlebarWhenMaximizedProperty(
    XID window,
    HideTitlebarWhenMaximized property);

// Clears all regions of X11's default root window by filling black pixels.
UI_BASE_EXPORT void ClearX11DefaultRootWindow();

// Returns true if |window| is visible.
UI_BASE_EXPORT bool IsWindowVisible(XID window);

// Returns the bounds of |window|.
UI_BASE_EXPORT bool GetWindowRect(XID window, gfx::Rect* rect);

// Returns true if |window| contains the point |screen_loc|.
UI_BASE_EXPORT bool WindowContainsPoint(XID window, gfx::Point screen_loc);

// Return true if |window| has any property with |property_name|.
UI_BASE_EXPORT bool PropertyExists(XID window,
                                   const std::string& property_name);

// Returns the raw bytes from a property with minimal
// interpretation. |out_data| should be freed by XFree() after use.
UI_BASE_EXPORT bool GetRawBytesOfProperty(
    XID window,
    Atom property,
    scoped_refptr<base::RefCountedMemory>* out_data,
    size_t* out_data_bytes,
    size_t* out_data_items,
    Atom* out_type);

// Get the value of an int, int array, atom array or string property.  On
// success, true is returned and the value is stored in |value|.
//
// TODO(erg): Once we remove the gtk port and are 100% aura, all of these
// should accept an Atom instead of a string.
UI_BASE_EXPORT bool GetIntProperty(XID window,
                                   const std::string& property_name,
                                   int* value);
UI_BASE_EXPORT bool GetXIDProperty(XID window,
                                   const std::string& property_name,
                                   XID* value);
UI_BASE_EXPORT bool GetIntArrayProperty(XID window,
                                        const std::string& property_name,
                                        std::vector<int>* value);
UI_BASE_EXPORT bool GetAtomArrayProperty(XID window,
                                         const std::string& property_name,
                                         std::vector<Atom>* value);
UI_BASE_EXPORT bool GetStringProperty(XID window,
                                      const std::string& property_name,
                                      std::string* value);

// These setters all make round trips.
UI_BASE_EXPORT bool SetIntProperty(XID window,
                                   const std::string& name,
                                   const std::string& type,
                                   int value);
UI_BASE_EXPORT bool SetIntArrayProperty(XID window,
                                        const std::string& name,
                                        const std::string& type,
                                        const std::vector<int>& value);
UI_BASE_EXPORT bool SetAtomProperty(XID window,
                                    const std::string& name,
                                    const std::string& type,
                                    Atom value);
UI_BASE_EXPORT bool SetAtomArrayProperty(XID window,
                                         const std::string& name,
                                         const std::string& type,
                                         const std::vector<Atom>& value);
UI_BASE_EXPORT bool SetStringProperty(XID window,
                                      Atom property,
                                      Atom type,
                                      const std::string& value);

// Gets the X atom for default display corresponding to atom_name.
UI_BASE_EXPORT Atom GetAtom(const char* atom_name);

// Sets the WM_CLASS attribute for a given X11 window.
UI_BASE_EXPORT void SetWindowClassHint(XDisplay* display,
                                       XID window,
                                       const std::string& res_name,
                                       const std::string& res_class);

// Sets the WM_WINDOW_ROLE attribute for a given X11 window.
UI_BASE_EXPORT void SetWindowRole(XDisplay* display,
                                  XID window,
                                  const std::string& role);

// Determine whether we should default to native decorations or the custom
// frame based on the currently-running window manager.
UI_BASE_EXPORT bool GetCustomFramePrefDefault();

static const int kAllDesktops = -1;
// Queries the desktop |window| is on, kAllDesktops if sticky. Returns false if
// property not found.
bool GetWindowDesktop(XID window, int* desktop);

// Translates an X11 error code into a printable string.
UI_BASE_EXPORT std::string GetX11ErrorString(XDisplay* display, int err);

// Implementers of this interface receive a notification for every X window of
// the main display.
class EnumerateWindowsDelegate {
 public:
  // |xid| is the X Window ID of the enumerated window.  Return true to stop
  // further iteration.
  virtual bool ShouldStopIterating(XID xid) = 0;

 protected:
  virtual ~EnumerateWindowsDelegate() {}
};

// Enumerates all windows in the current display.  Will recurse into child
// windows up to a depth of |max_depth|.
UI_BASE_EXPORT bool EnumerateAllWindows(EnumerateWindowsDelegate* delegate,
                                        int max_depth);

// Enumerates the top-level windows of the current display.
UI_BASE_EXPORT void EnumerateTopLevelWindows(
    ui::EnumerateWindowsDelegate* delegate);

// Returns all children windows of a given window in top-to-bottom stacking
// order.
UI_BASE_EXPORT bool GetXWindowStack(XID window, std::vector<XID>* windows);

// Copies |source_bounds| from |drawable| to |canvas| at offset |dest_offset|.
// |source_bounds| is in physical pixels, while |dest_offset| is relative to
// the canvas's scale. Note that this function is slow since it uses
// XGetImage() to copy the data from the X server to this process before
// copying it to |canvas|.
UI_BASE_EXPORT bool CopyAreaToCanvas(XID drawable,
                                     gfx::Rect source_bounds,
                                     gfx::Point dest_offset,
                                     gfx::Canvas* canvas);

enum WindowManagerName {
  WM_UNKNOWN,
  WM_BLACKBOX,
  WM_CHROME_OS,
  WM_COMPIZ,
  WM_ENLIGHTENMENT,
  WM_ICE_WM,
  WM_KWIN,
  WM_METACITY,
  WM_MUFFIN,
  WM_MUTTER,
  WM_OPENBOX,
  WM_XFWM4,
};
// Attempts to guess the window maager. Returns WM_UNKNOWN if we can't
// determine it for one reason or another.
UI_BASE_EXPORT WindowManagerName GuessWindowManager();

// Enable the default X error handlers. These will log the error and abort
// the process if called. Use SetX11ErrorHandlers() from x11_util_internal.h
// to set your own error handlers.
UI_BASE_EXPORT void SetDefaultX11ErrorHandlers();

// Returns true if a given window is in full-screen mode.
UI_BASE_EXPORT bool IsX11WindowFullScreen(XID window);

// Returns true if the window manager supports the given hint.
UI_BASE_EXPORT bool WmSupportsHint(Atom atom);

// Manages a piece of X11 allocated memory as a RefCountedMemory segment. This
// object takes ownership over the passed in memory and will free it with the
// X11 allocator when done.
class UI_BASE_EXPORT XRefcountedMemory : public base::RefCountedMemory {
 public:
  XRefcountedMemory(unsigned char* x11_data, size_t length)
      : x11_data_(length ? x11_data : NULL), length_(length) {}

  // Overridden from RefCountedMemory:
  virtual const unsigned char* front() const OVERRIDE;
  virtual size_t size() const OVERRIDE;

 private:
  virtual ~XRefcountedMemory();

  unsigned char* x11_data_;
  size_t length_;

  DISALLOW_COPY_AND_ASSIGN(XRefcountedMemory);
};

// Keeps track of a string returned by an X function (e.g. XGetAtomName) and
// makes sure it's XFree'd.
class UI_BASE_EXPORT XScopedString {
 public:
  explicit XScopedString(char* str) : string_(str) {}
  ~XScopedString();

  const char* string() const { return string_; }

 private:
  char* string_;

  DISALLOW_COPY_AND_ASSIGN(XScopedString);
};

// Keeps track of an image returned by an X function (e.g. XGetImage) and
// makes sure it's XDestroyImage'd.
class UI_BASE_EXPORT XScopedImage {
 public:
  explicit XScopedImage(XImage* image) : image_(image) {}
  ~XScopedImage();

  XImage* get() const { return image_; }

  XImage* operator->() const { return image_; }

  void reset(XImage* image);

 private:
  XImage* image_;

  DISALLOW_COPY_AND_ASSIGN(XScopedImage);
};

// Keeps track of a cursor returned by an X function and makes sure it's
// XFreeCursor'd.
class UI_BASE_EXPORT XScopedCursor {
 public:
  // Keeps track of |cursor| created with |display|.
  XScopedCursor(::Cursor cursor, XDisplay* display);
  ~XScopedCursor();

  ::Cursor get() const;
  void reset(::Cursor cursor);

 private:
  ::Cursor cursor_;
  XDisplay* display_;

  DISALLOW_COPY_AND_ASSIGN(XScopedCursor);
};

namespace test {
// Resets the cache used by GetXCursor(). Only useful for tests that may delete
// the display.
UI_BASE_EXPORT void ResetXCursorCache();

// Returns the cached XcursorImage for |cursor|.
UI_BASE_EXPORT const XcursorImage* GetCachedXcursorImage(::Cursor cursor);

}  // namespace test

}  // namespace ui

#endif  // UI_BASE_X_X11_UTIL_H_

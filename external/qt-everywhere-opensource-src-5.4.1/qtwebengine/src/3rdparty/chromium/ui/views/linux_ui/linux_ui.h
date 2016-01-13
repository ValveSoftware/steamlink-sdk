// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LINUX_UI_LINUX_UI_H_
#define UI_VIEWS_LINUX_UI_LINUX_UI_H_

#include <string>

#include "base/callback.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#include "ui/events/linux/text_edit_key_bindings_delegate_auralinux.h"
#include "ui/gfx/linux_font_delegate.h"
#include "ui/shell_dialogs/linux_shell_dialog.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/linux_ui/status_icon_linux.h"
#include "ui/views/views_export.h"

// The main entrypoint into Linux toolkit specific code. GTK code should only
// be executed behind this interface.

namespace aura {
class Window;
}

namespace gfx {
class Image;
}

namespace ui {
class NativeTheme;
}

namespace views {
class Border;
class LabelButton;
class LabelButtonBorder;
class View;
class WindowButtonOrderObserver;

// Adapter class with targets to render like different toolkits. Set by any
// project that wants to do linux desktop native rendering.
//
// TODO(erg): We're hardcoding GTK2, when we'll need to have backends for (at
// minimum) GTK2 and GTK3. LinuxUI::instance() should actually be a very
// complex method that pokes around with dlopen against a libuigtk2.so, a
// liuigtk3.so, etc.
class VIEWS_EXPORT LinuxUI : public ui::LinuxInputMethodContextFactory,
                             public gfx::LinuxFontDelegate,
                             public ui::LinuxShellDialog,
                             public ui::TextEditKeyBindingsDelegateAuraLinux {
 public:
  // Describes the window management actions that could be taken in response to
  // a middle click in the non client area.
  enum NonClientMiddleClickAction {
    MIDDLE_CLICK_ACTION_NONE,
    MIDDLE_CLICK_ACTION_LOWER,
    MIDDLE_CLICK_ACTION_MINIMIZE,
    MIDDLE_CLICK_ACTION_TOGGLE_MAXIMIZE
  };

  typedef base::Callback<ui::NativeTheme*(aura::Window* window)>
      NativeThemeGetter;

  virtual ~LinuxUI() {}

  // Sets the dynamically loaded singleton that draws the desktop native UI.
  static void SetInstance(LinuxUI* instance);

  // Returns a LinuxUI instance for the toolkit used in the user's desktop
  // environment.
  //
  // Can return NULL, in case no toolkit has been set. (For example, if we're
  // running with the "--ash" flag.)
  static LinuxUI* instance();

  virtual void Initialize() = 0;

  // Returns a themed image per theme_provider.h
  virtual gfx::Image GetThemeImageNamed(int id) const = 0;
  virtual bool GetColor(int id, SkColor* color) const = 0;
  virtual bool HasCustomImage(int id) const = 0;

  // Returns the preferences that we pass to WebKit.
  virtual SkColor GetFocusRingColor() const = 0;
  virtual SkColor GetThumbActiveColor() const = 0;
  virtual SkColor GetThumbInactiveColor() const = 0;
  virtual SkColor GetTrackColor() const = 0;
  virtual SkColor GetActiveSelectionBgColor() const = 0;
  virtual SkColor GetActiveSelectionFgColor() const = 0;
  virtual SkColor GetInactiveSelectionBgColor() const = 0;
  virtual SkColor GetInactiveSelectionFgColor() const = 0;
  virtual double GetCursorBlinkInterval() const = 0;

  // Returns a NativeTheme that will provide system colors and draw system
  // style widgets.
  virtual ui::NativeTheme* GetNativeTheme(aura::Window* window) const = 0;

  // Used to set an override NativeTheme.
  virtual void SetNativeThemeOverride(const NativeThemeGetter& callback) = 0;

  // Returns whether we should be using the native theme provided by this
  // object by default.
  virtual bool GetDefaultUsesSystemTheme() const = 0;

  // Sets visual properties in the desktop environment related to download
  // progress, if available.
  virtual void SetDownloadCount(int count) const = 0;
  virtual void SetProgressFraction(float percentage) const = 0;

  // Checks for platform support for status icons.
  virtual bool IsStatusIconSupported() const = 0;

  // Create a native status icon.
  virtual scoped_ptr<StatusIconLinux> CreateLinuxStatusIcon(
      const gfx::ImageSkia& image,
      const base::string16& tool_tip) const = 0;

  // Returns the icon for a given content type from the icon theme.
  // TODO(davidben): Add an observer for the theme changing, so we can drop the
  // caches.
  virtual gfx::Image GetIconForContentType(
      const std::string& content_type, int size) const = 0;

  // Builds a Border which paints the native button style.
  virtual scoped_ptr<Border> CreateNativeBorder(
      views::LabelButton* owning_button,
      scoped_ptr<views::LabelButtonBorder> border) = 0;

  // Notifies the observer about changes about how window buttons should be
  // laid out. If the order is anything other than the default min,max,close on
  // the right, will immediately send a button change event to the observer.
  virtual void AddWindowButtonOrderObserver(
      WindowButtonOrderObserver* observer) = 0;

  // Removes the observer from the LinuxUI's list.
  virtual void RemoveWindowButtonOrderObserver(
      WindowButtonOrderObserver* observer) = 0;

  // Determines whether the user's window manager is Unity.
  virtual bool UnityIsRunning() = 0;

  // What action we should take when the user middle clicks on non-client
  // area. The default is lowering the window.
  virtual NonClientMiddleClickAction GetNonClientMiddleClickAction() = 0;

  // Notifies the window manager that start up has completed.
  // Normally Chromium opens a new window on startup and GTK does this
  // automatically. In case Chromium does not open a new window on startup,
  // e.g. an existing browser window already exists, this should be called.
  virtual void NotifyWindowManagerStartupComplete() = 0;
};

}  // namespace views

#endif  // UI_VIEWS_LINUX_UI_LINUX_UI_H_

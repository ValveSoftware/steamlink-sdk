// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_CONTENT_KEYBOARD_UI_CONTENT_H_
#define UI_KEYBOARD_CONTENT_KEYBOARD_UI_CONTENT_H_

#include <memory>

#include "base/macros.h"
#include "content/public/common/media_stream_request.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/keyboard/keyboard_export.h"
#include "ui/keyboard/keyboard_ui.h"

namespace aura {
class Window;
}
namespace content {
class BrowserContext;
class SiteInstance;
class WebContents;
}
namespace gfx {
class Rect;
}
namespace ui {
class InputMethod;
}
namespace wm {
class Shadow;
}

namespace keyboard {

class KeyboardController;
class WindowBoundsChangeObserver;

// An implementation of KeyboardUI that uses a content::WebContents to implement
//the keyboard.
class KEYBOARD_EXPORT KeyboardUIContent : public KeyboardUI,
                                          public aura::WindowObserver {
 public:
  class TestApi {
   public:
    explicit TestApi(KeyboardUIContent* ui) : ui_(ui) {}

    const content::WebContents* keyboard_contents() {
      return ui_->keyboard_contents_.get();
    }

   private:
    KeyboardUIContent* ui_;

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  explicit KeyboardUIContent(content::BrowserContext* context);
  ~KeyboardUIContent() override;

  // Requests the audio input from microphone for speech input.
  virtual void RequestAudioInput(content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) = 0;

  // Loads system virtual keyboard. Noop if the current virtual keyboard is
  // system virtual keyboard.
  virtual void LoadSystemKeyboard();

  // Called when a window being observed changes bounds, to update its insets.
  void UpdateInsetsForWindow(aura::Window* window);

  // Overridden from KeyboardUI:
  aura::Window* GetKeyboardWindow() override;
  bool HasKeyboardWindow() const override;
  bool ShouldWindowOverscroll(aura::Window* window) const override;
  void ReloadKeyboardIfNeeded() override;
  void InitInsets(const gfx::Rect& new_bounds) override;
  void ResetInsets() override;

 protected:
  // The implementation can choose to setup the WebContents before the virtual
  // keyboard page is loaded (e.g. install a WebContentsObserver).
  // SetupWebContents() is called right after creating the WebContents, before
  // loading the keyboard page.
  virtual void SetupWebContents(content::WebContents* contents);

  // aura::WindowObserver overrides:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroyed(aura::Window* window) override;

  content::BrowserContext* browser_context() { return browser_context_; }

  const aura::Window* GetKeyboardRootWindow() const;

 private:
  friend class TestApi;

  // Loads the web contents for the given |url|.
  void LoadContents(const GURL& url);

  // Gets the virtual keyboard URL (either the default URL or IME override URL).
  const GURL& GetVirtualKeyboardUrl();

  // Determines whether a particular window should have insets for overscroll.
  bool ShouldEnableInsets(aura::Window* window);

  // Adds an observer for tracking changes to a window size or
  // position while the keyboard is displayed. Any window repositioning
  // invalidates insets for overscrolling.
  void AddBoundsChangedObserver(aura::Window* window);

  // The BrowserContext to use for creating the WebContents hosting the
  // keyboard.
  content::BrowserContext* browser_context_;

  const GURL default_url_;

  std::unique_ptr<content::WebContents> keyboard_contents_;
  std::unique_ptr<wm::Shadow> shadow_;

  std::unique_ptr<WindowBoundsChangeObserver> window_bounds_observer_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardUIContent);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_CONTENT_KEYBOARD_UI_CONTENT_H_

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_CONTROLLER_PROXY_H_
#define UI_KEYBOARD_KEYBOARD_CONTROLLER_PROXY_H_

#include "base/memory/scoped_ptr.h"
#include "content/public/common/media_stream_request.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/keyboard/keyboard_export.h"

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

// A proxy used by the KeyboardController to get access to the virtual
// keyboard window.
class KEYBOARD_EXPORT KeyboardControllerProxy : public aura::WindowObserver {
 public:
  class TestApi {
   public:
    explicit TestApi(KeyboardControllerProxy* proxy) : proxy_(proxy) {}

    const content::WebContents* keyboard_contents() {
      return proxy_->keyboard_contents_.get();
    }

   private:
    KeyboardControllerProxy* proxy_;

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  KeyboardControllerProxy();
  virtual ~KeyboardControllerProxy();

  // Gets the virtual keyboard window.  Ownership of the returned Window remains
  // with the proxy.
  virtual aura::Window* GetKeyboardWindow();

  // Whether the keyboard window is created. The keyboard window is tied to a
  // WebContent so we can just check if the WebContent is created or not.
  virtual bool HasKeyboardWindow() const;

  // Gets the InputMethod that will provide notifications about changes in the
  // text input context.
  virtual ui::InputMethod* GetInputMethod() = 0;

  // Requests the audio input from microphone for speech input.
  virtual void RequestAudioInput(content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) = 0;

  // Shows the container window of the keyboard. The default implementation
  // simply shows the container. An overridden implementation can set up
  // necessary animation, or delay the visibility change as it desires.
  virtual void ShowKeyboardContainer(aura::Window* container);

  // Hides the container window of the keyboard. The default implementation
  // simply hides the container. An overridden implementation can set up
  // necesasry animation, or delay the visibility change as it desires.
  virtual void HideKeyboardContainer(aura::Window* container);

  // Updates the type of the focused text input box.
  virtual void SetUpdateInputType(ui::TextInputType type);

  // Ensures caret in current work area (not occluded by virtual keyboard
  // window).
  virtual void EnsureCaretInWorkArea();

  // Loads system virtual keyboard. Noop if the current virtual keyboard is
  // system virtual keyboard.
  virtual void LoadSystemKeyboard();

  // Reloads virtual keyboard URL if the current keyboard's web content URL is
  // different. The URL can be different if user switch from password field to
  // any other type input field.
  // At password field, the system virtual keyboard is forced to load even if
  // the current IME provides a customized virtual keyboard. This is needed to
  // prevent IME virtual keyboard logging user's password. Once user switch to
  // other input fields, the virtual keyboard should switch back to the IME
  // provided keyboard, or keep using the system virtual keyboard if IME doesn't
  // provide one.
  virtual void ReloadKeyboardIfNeeded();

 protected:
  // Gets the BrowserContext to use for creating the WebContents hosting the
  // keyboard.
  virtual content::BrowserContext* GetBrowserContext() = 0;

  // The implementation can choose to setup the WebContents before the virtual
  // keyboard page is loaded (e.g. install a WebContentsObserver).
  // SetupWebContents() is called right after creating the WebContents, before
  // loading the keyboard page.
  virtual void SetupWebContents(content::WebContents* contents);

  // aura::WindowObserver overrides:
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

 private:
  friend class TestApi;

  // Loads the web contents for the given |url|.
  void LoadContents(const GURL& url);

  // Gets the virtual keyboard URL (either the default URL or IME override URL).
  const GURL& GetVirtualKeyboardUrl();

  const GURL default_url_;

  scoped_ptr<content::WebContents> keyboard_contents_;
  scoped_ptr<wm::Shadow> shadow_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerProxy);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_CONTROLLER_PROXY_H_

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_controller_proxy.h"

#include "base/command_line.h"
#include "base/values.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/bindings_policy.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/keyboard/keyboard_constants.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/wm/core/shadow.h"

namespace {

// The WebContentsDelegate for the keyboard.
// The delegate deletes itself when the keyboard is destroyed.
class KeyboardContentsDelegate : public content::WebContentsDelegate,
                                 public content::WebContentsObserver {
 public:
  KeyboardContentsDelegate(keyboard::KeyboardControllerProxy* proxy)
      : proxy_(proxy) {}
  virtual ~KeyboardContentsDelegate() {}

 private:
  // Overridden from content::WebContentsDelegate:
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE {
    source->GetController().LoadURL(
        params.url, params.referrer, params.transition, params.extra_headers);
    Observe(source);
    return source;
  }

  virtual bool IsPopupOrPanel(
      const content::WebContents* source) const OVERRIDE {
    return true;
  }

  virtual void MoveContents(content::WebContents* source,
                            const gfx::Rect& pos) OVERRIDE {
    aura::Window* keyboard = proxy_->GetKeyboardWindow();
    // keyboard window must have been added to keyboard container window at this
    // point. Otherwise, wrong keyboard bounds is used and may cause problem as
    // described in crbug.com/367788.
    DCHECK(keyboard->parent());
    gfx::Rect bounds = keyboard->bounds();
    int new_height = pos.height();
    bounds.set_y(bounds.y() + bounds.height() - new_height);
    bounds.set_height(new_height);
    keyboard->SetBounds(bounds);
  }

  // Overridden from content::WebContentsDelegate:
  virtual void RequestMediaAccessPermission(content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE {
    proxy_->RequestAudioInput(web_contents, request, callback);
  }

  // Overridden from content::WebContentsObserver:
  virtual void WebContentsDestroyed() OVERRIDE {
    delete this;
  }

  keyboard::KeyboardControllerProxy* proxy_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardContentsDelegate);
};

}  // namespace

namespace keyboard {

KeyboardControllerProxy::KeyboardControllerProxy()
    : default_url_(kKeyboardURL) {
}

KeyboardControllerProxy::~KeyboardControllerProxy() {
}

const GURL& KeyboardControllerProxy::GetVirtualKeyboardUrl() {
  if (keyboard::IsInputViewEnabled()) {
    const GURL& override_url = GetOverrideContentUrl();
    return override_url.is_valid() ? override_url : default_url_;
  } else {
    return default_url_;
  }
}

void KeyboardControllerProxy::LoadContents(const GURL& url) {
  if (keyboard_contents_) {
    content::OpenURLParams params(
        url,
        content::Referrer(),
        SINGLETON_TAB,
        content::PAGE_TRANSITION_AUTO_TOPLEVEL,
        false);
    keyboard_contents_->OpenURL(params);
  }
}

aura::Window* KeyboardControllerProxy::GetKeyboardWindow() {
  if (!keyboard_contents_) {
    content::BrowserContext* context = GetBrowserContext();
    keyboard_contents_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(context,
            content::SiteInstance::CreateForURL(context,
                                                GetVirtualKeyboardUrl()))));
    keyboard_contents_->SetDelegate(new KeyboardContentsDelegate(this));
    SetupWebContents(keyboard_contents_.get());
    LoadContents(GetVirtualKeyboardUrl());
    keyboard_contents_->GetNativeView()->AddObserver(this);
  }

  return keyboard_contents_->GetNativeView();
}

bool KeyboardControllerProxy::HasKeyboardWindow() const {
  return keyboard_contents_;
}

void KeyboardControllerProxy::ShowKeyboardContainer(aura::Window* container) {
  GetKeyboardWindow()->Show();
  container->Show();
}

void KeyboardControllerProxy::HideKeyboardContainer(aura::Window* container) {
  container->Hide();
  GetKeyboardWindow()->Hide();
}

void KeyboardControllerProxy::SetUpdateInputType(ui::TextInputType type) {
}

void KeyboardControllerProxy::EnsureCaretInWorkArea() {
  if (GetInputMethod()->GetTextInputClient()) {
    aura::Window* keyboard_window = GetKeyboardWindow();
    aura::Window* root_window = keyboard_window->GetRootWindow();
    gfx::Rect available_bounds = root_window->bounds();
    gfx::Rect keyboard_bounds = keyboard_window->bounds();
    available_bounds.set_height(available_bounds.height() -
        keyboard_bounds.height());
    GetInputMethod()->GetTextInputClient()->EnsureCaretInRect(available_bounds);
  }
}

void KeyboardControllerProxy::LoadSystemKeyboard() {
  DCHECK(keyboard_contents_);
  if (keyboard_contents_->GetURL() != default_url_) {
    // TODO(bshe): The height of system virtual keyboard and IME virtual
    // keyboard may different. The height needs to be restored too.
    LoadContents(default_url_);
  }
}

void KeyboardControllerProxy::ReloadKeyboardIfNeeded() {
  DCHECK(keyboard_contents_);
  if (keyboard_contents_->GetURL() != GetVirtualKeyboardUrl()) {
    LoadContents(GetVirtualKeyboardUrl());
  }
}

void KeyboardControllerProxy::SetupWebContents(content::WebContents* contents) {
}

void KeyboardControllerProxy::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  if (!shadow_) {
    shadow_.reset(new wm::Shadow());
    shadow_->Init(wm::Shadow::STYLE_ACTIVE);
    shadow_->layer()->SetVisible(true);
    DCHECK(keyboard_contents_->GetNativeView()->parent());
    keyboard_contents_->GetNativeView()->parent()->layer()->Add(
        shadow_->layer());
  }

  shadow_->SetContentBounds(new_bounds);
}

void KeyboardControllerProxy::OnWindowDestroyed(aura::Window* window) {
  window->RemoveObserver(this);
}

}  // namespace keyboard

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_IME_ARC_IME_SERVICE_H_
#define COMPONENTS_ARC_IME_ARC_IME_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "components/arc/arc_service.h"
#include "components/arc/ime/arc_ime_bridge.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_observer.h"

namespace aura {
class Window;
}

namespace ui {
class InputMethod;
}

namespace arc {

class ArcBridgeService;

// This class implements ui::TextInputClient and makes ARC windows behave
// as a text input target in Chrome OS environment.
class ArcImeService : public ArcService,
                      public ArcImeBridge::Delegate,
                      public aura::EnvObserver,
                      public aura::WindowObserver,
                      public aura::client::FocusChangeObserver,
                      public keyboard::KeyboardControllerObserver,
                      public ui::TextInputClient {
 public:
  explicit ArcImeService(ArcBridgeService* bridge_service);
  ~ArcImeService() override;

  // Injects the custom IPC bridge object for testing purpose only.
  void SetImeBridgeForTesting(std::unique_ptr<ArcImeBridge> test_ime_bridge);

  // Injects the custom IME for testing purpose only.
  void SetInputMethodForTesting(ui::InputMethod* test_input_method);

  // Overridden from aura::EnvObserver:
  void OnWindowInitialized(aura::Window* new_window) override;

  // Overridden from aura::WindowObserver:
  void OnWindowAddedToRootWindow(aura::Window* window) override;

  // Overridden from aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Overridden from ArcImeBridge::Delegate:
  void OnTextInputTypeChanged(ui::TextInputType type) override;
  void OnCursorRectChanged(const gfx::Rect& rect) override;
  void OnCancelComposition() override;
  void ShowImeIfNeeded() override;

  // Overridden from keyboard::KeyboardControllerObserver.
  void OnKeyboardBoundsChanging(const gfx::Rect& rect) override;

  // Overridden from ui::TextInputClient:
  void SetCompositionText(const ui::CompositionText& composition) override;
  void ConfirmCompositionText() override;
  void ClearCompositionText() override;
  void InsertText(const base::string16& text) override;
  void InsertChar(const ui::KeyEvent& event) override;
  ui::TextInputType GetTextInputType() const override;
  gfx::Rect GetCaretBounds() const override;

  // Overridden from ui::TextInputClient (with default implementation):
  // TODO(kinaba): Support each of these methods to the extent possible in
  // Android input method API.
  ui::TextInputMode GetTextInputMode() const override;
  base::i18n::TextDirection GetTextDirection() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  bool GetCompositionCharacterBounds(uint32_t index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool GetSelectionRange(gfx::Range* range) const override;
  bool SetSelectionRange(const gfx::Range& range) override;
  bool DeleteRange(const gfx::Range& range) override;
  bool GetTextFromRange(const gfx::Range& range,
                        base::string16* text) const override;
  void OnInputMethodChanged() override {}
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override;
  void EnsureCaretInRect(const gfx::Rect& rect) override {}
  bool IsTextEditCommandEnabled(ui::TextEditCommand command) const override;
  void SetTextEditCommandForNextKeyEvent(ui::TextEditCommand command) override {
  }

 private:
  ui::InputMethod* GetInputMethod();

  std::unique_ptr<ArcImeBridge> ime_bridge_;
  ui::TextInputType ime_type_;
  gfx::Rect cursor_rect_;
  bool has_composition_text_;

  aura::WindowTracker observing_root_windows_;
  aura::WindowTracker arc_windows_;
  aura::WindowTracker focused_arc_window_;

  keyboard::KeyboardController* keyboard_controller_;

  ui::InputMethod* test_input_method_;

  DISALLOW_COPY_AND_ASSIGN(ArcImeService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_IME_ARC_IME_SERVICE_H_

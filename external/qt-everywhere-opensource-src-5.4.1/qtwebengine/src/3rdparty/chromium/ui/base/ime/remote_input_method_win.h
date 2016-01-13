// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_REMOTE_INPUT_METHOD_WIN_H_
#define UI_BASE_IME_REMOTE_INPUT_METHOD_WIN_H_

#include <Windows.h>

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "ui/base/ui_base_export.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
namespace internal {
class InputMethodDelegate;
class RemoteInputMethodDelegateWin;
}  // namespace internal

class InputMethod;
struct CompositionText;

// RemoteInputMethodWin is a special implementation of ui::InputMethod that
// works as a proxy of an IME handler running in the metro_driver process.
// RemoteInputMethodWin works as follows.
// - Any action to RemoteInputMethodWin should be delegated to the
//   metro_driver process via RemoteInputMethodDelegateWin.
// - Data retrieval from RemoteInputMethodPrivateWin is implemented with
//   data cache. Whenever the IME state in the metro_driver process is changed,
//   RemoteWindowTreeHostWin, which receives IPCs from metro_driver process,
//   will call RemoteInputMethodPrivateWin::OnCandidatePopupChanged and/or
//   RemoteInputMethodPrivateWin::OnInputSourceChanged accordingly so that
//   the state cache should be updated.
// - Some IPC messages that represent actions to TextInputClient should be
//   delegated to RemoteInputMethodPrivateWin so that RemoteInputMethodWin can
//   work as a real proxy.

// Returns true if |widget| requires RemoteInputMethodWin.
bool IsRemoteInputMethodWinRequired(gfx::AcceleratedWidget widget);

// Returns the public interface of RemoteInputMethodWin.
// Caveats: Currently only one instance of RemoteInputMethodWin is able to run
// at the same time.
UI_BASE_EXPORT scoped_ptr<InputMethod> CreateRemoteInputMethodWin(
    internal::InputMethodDelegate* delegate);

// Private interface of RemoteInputMethodWin.
class UI_BASE_EXPORT RemoteInputMethodPrivateWin {
 public:
  RemoteInputMethodPrivateWin();

  // Returns the private interface of RemoteInputMethodWin when and only when
  // |input_method| is instanciated via CreateRemoteInputMethodWin. Caller does
  // not take the ownership of the returned object.
  // As you might notice, this is yet another reinplementation of dynamic_cast
  // or IUnknown::QueryInterface.
  static RemoteInputMethodPrivateWin* Get(InputMethod* input_method);

  // Installs RemoteInputMethodDelegateWin delegate. Set NULL to |delegate| to
  // unregister.
  virtual void SetRemoteDelegate(
      internal::RemoteInputMethodDelegateWin* delegate) = 0;

  // Updates internal cache so that subsequent calls of
  // RemoteInputMethodWin::IsCandidatePopupOpen can return the correct value
  // based on remote IME activities in the metro_driver process.
  virtual void OnCandidatePopupChanged(bool visible) = 0;

  // Updates internal cache so that subsequent calls of
  // RemoteInputMethodWin::GetInputLocale can return the correct values based on
  // remote IME activities in the metro_driver process.
  virtual void OnInputSourceChanged(LANGID langid, bool is_ime) = 0;

  // Handles composition-update events occurred in the metro_driver process.
  // Caveats: This method is designed to be used only with
  // metro_driver::TextService. In other words, there is no garantee that this
  // method works a wrapper to call ui::TextInputClient::SetCompositionText.
  virtual void OnCompositionChanged(
      const CompositionText& composition_text) = 0;

  // Handles text-commit events occurred in the metro_driver process.
  // Caveats: This method is designed to be used only with
  // metro_driver::TextService. In other words, there is no garantee that this
  // method works a wrapper to call ui::TextInputClient::InsertText. In fact,
  // this method may call ui::TextInputClient::InsertChar when the text input
  // type of the focused text input client is TEXT_INPUT_TYPE_NONE.
  virtual void OnTextCommitted(const base::string16& text) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteInputMethodPrivateWin);
};

}  // namespace ui

#endif  // UI_BASE_IME_REMOTE_INPUT_METHOD_WIN_H_

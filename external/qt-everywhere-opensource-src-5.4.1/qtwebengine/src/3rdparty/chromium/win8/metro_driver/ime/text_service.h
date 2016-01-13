// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_METRO_DRIVER_IME_TEXT_SERVICE_H_
#define WIN8_METRO_DRIVER_IME_TEXT_SERVICE_H_

#include <Windows.h>

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace metro_viewer {
struct CharacterBounds;
}

namespace metro_driver {

class TextServiceDelegate;

// An interface to manage a virtual text store with which an IME communicates.
class TextService {
 public:
  virtual ~TextService() {}

  // Cancels on-going composition. Does nothing if there is no composition.
  virtual void CancelComposition() = 0;

  // Updates document type with |input_scopes| and caret/composition position
  // with |character_bounds|.  An empty |input_scopes| indicates that IMEs
  // should be disabled until non-empty |input_scopes| is specified.
  // Note: |input_scopes| is defined as std::vector<int32> here rather than
  // std::vector<InputScope> because the wire format of IPC message
  // MetroViewerHostMsg_ImeTextInputClientUpdated uses std::vector<int32> to
  // avoid dependency on <InputScope.h> header.
  virtual void OnDocumentChanged(
      const std::vector<int32>& input_scopes,
      const std::vector<metro_viewer::CharacterBounds>& character_bounds) = 0;

  // Must be called whenever the attached window gains keyboard focus.
  virtual void OnWindowActivated() = 0;
};

// Returns an instance of TextService that works together with
// |text_store_delegate| as if it was an text area owned by |window_handle|.
scoped_ptr<TextService>
CreateTextService(TextServiceDelegate* delegate, HWND window_handle);

}  // namespace metro_driver

#endif  // WIN8_METRO_DRIVER_IME_TEXT_SERVICE_H_

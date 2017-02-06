// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_CHROMEOS_H_

#include <map>
#include <string>
#include <vector>

#include "chrome/browser/extensions/api/input_ime/input_ime_event_router_base.h"
#include "chrome/common/extensions/api/input_ime/input_components_handler.h"
#include "extensions/browser/extension_function.h"

namespace chromeos {

class InputMethodEngine;

}  // namespace chromeos

namespace extensions {

class InputImeClearCompositionFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.clearComposition",
                             INPUT_IME_CLEARCOMPOSITION)

 protected:
  ~InputImeClearCompositionFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class InputImeSetCandidateWindowPropertiesFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setCandidateWindowProperties",
                             INPUT_IME_SETCANDIDATEWINDOWPROPERTIES)

 protected:
  ~InputImeSetCandidateWindowPropertiesFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class InputImeSetCandidatesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setCandidates", INPUT_IME_SETCANDIDATES)

 protected:
  ~InputImeSetCandidatesFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class InputImeSetCursorPositionFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setCursorPosition",
                             INPUT_IME_SETCURSORPOSITION)

 protected:
  ~InputImeSetCursorPositionFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class InputImeSetMenuItemsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setMenuItems", INPUT_IME_SETMENUITEMS)

 protected:
  ~InputImeSetMenuItemsFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class InputImeUpdateMenuItemsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.updateMenuItems",
                             INPUT_IME_UPDATEMENUITEMS)

 protected:
  ~InputImeUpdateMenuItemsFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class InputImeDeleteSurroundingTextFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.deleteSurroundingText",
                             INPUT_IME_DELETESURROUNDINGTEXT)
 protected:
  ~InputImeDeleteSurroundingTextFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class InputImeHideInputViewFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.hideInputView",
                             INPUT_IME_HIDEINPUTVIEW)

 protected:
  ~InputImeHideInputViewFunction() override {}

  // ExtensionFunction:
  bool RunAsync() override;
};

class InputMethodPrivateNotifyImeMenuItemActivatedFunction
    : public UIThreadExtensionFunction {
 public:
  InputMethodPrivateNotifyImeMenuItemActivatedFunction() {}

 protected:
  ~InputMethodPrivateNotifyImeMenuItemActivatedFunction() override {}

  // UIThreadExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.notifyImeMenuItemActivated",
                             INPUTMETHODPRIVATE_NOTIFYIMEMENUITEMACTIVATED)
  DISALLOW_COPY_AND_ASSIGN(
      InputMethodPrivateNotifyImeMenuItemActivatedFunction);
};

class InputImeEventRouter : public InputImeEventRouterBase {
 public:
  explicit InputImeEventRouter(Profile* profile);
  ~InputImeEventRouter() override;

  bool RegisterImeExtension(
      const std::string& extension_id,
      const std::vector<extensions::InputComponentInfo>& input_components);
  void UnregisterAllImes(const std::string& extension_id);

  chromeos::InputMethodEngine* GetEngine(const std::string& extension_id,
                                         const std::string& component_id);
  input_method::InputMethodEngineBase* GetActiveEngine(
      const std::string& extension_id) override;

 private:
  // The engine map from extension_id to an engine.
  std::map<std::string, chromeos::InputMethodEngine*> engine_map_;

  DISALLOW_COPY_AND_ASSIGN(InputImeEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_CHROMEOS_H_

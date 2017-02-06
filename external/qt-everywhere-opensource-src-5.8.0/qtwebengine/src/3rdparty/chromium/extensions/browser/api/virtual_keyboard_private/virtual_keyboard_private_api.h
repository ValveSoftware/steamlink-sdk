// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_VIRTUAL_KEYBOARD_PRIVATE_VIRTUAL_KEYBOARD_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_VIRTUAL_KEYBOARD_PRIVATE_VIRTUAL_KEYBOARD_PRIVATE_API_H_

#include "base/compiler_specific.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class VirtualKeyboardPrivateInsertTextFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.insertText",
                             VIRTUALKEYBOARDPRIVATE_INSERTTEXT);

 protected:
  ~VirtualKeyboardPrivateInsertTextFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class VirtualKeyboardPrivateSendKeyEventFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.sendKeyEvent",
                             VIRTUALKEYBOARDPRIVATE_SENDKEYEVENT);

 protected:
  ~VirtualKeyboardPrivateSendKeyEventFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class VirtualKeyboardPrivateHideKeyboardFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.hideKeyboard",
                             VIRTUALKEYBOARDPRIVATE_HIDEKEYBOARD);

 protected:
  ~VirtualKeyboardPrivateHideKeyboardFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class VirtualKeyboardPrivateSetHotrodKeyboardFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.setHotrodKeyboard",
                             VIRTUALKEYBOARDPRIVATE_SETHOTRODKEYBOARD);

 protected:
  ~VirtualKeyboardPrivateSetHotrodKeyboardFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class VirtualKeyboardPrivateLockKeyboardFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.lockKeyboard",
                             VIRTUALKEYBOARDPRIVATE_LOCKKEYBOARD);

 protected:
  ~VirtualKeyboardPrivateLockKeyboardFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class VirtualKeyboardPrivateKeyboardLoadedFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.keyboardLoaded",
                             VIRTUALKEYBOARDPRIVATE_KEYBOARDLOADED);

 protected:
  ~VirtualKeyboardPrivateKeyboardLoadedFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class VirtualKeyboardPrivateGetKeyboardConfigFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.getKeyboardConfig",
                             VIRTUALKEYBOARDPRIVATE_GETKEYBOARDCONFIG);

 protected:
  ~VirtualKeyboardPrivateGetKeyboardConfigFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class VirtualKeyboardPrivateOpenSettingsFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.openSettings",
                             VIRTUALKEYBOARDPRIVATE_OPENSETTINGS);

 protected:
  ~VirtualKeyboardPrivateOpenSettingsFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class VirtualKeyboardPrivateSetModeFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.setMode",
                             VIRTUALKEYBOARDPRIVATE_SETMODE);

 protected:
  ~VirtualKeyboardPrivateSetModeFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class VirtualKeyboardPrivateSetKeyboardStateFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.setKeyboardState",
                             VIRTUALKEYBOARDPRIVATE_SETKEYBOARDSTATE);

 protected:
  ~VirtualKeyboardPrivateSetKeyboardStateFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class VirtualKeyboardDelegate;

class VirtualKeyboardAPI : public BrowserContextKeyedAPI {
 public:
  explicit VirtualKeyboardAPI(content::BrowserContext* context);
  ~VirtualKeyboardAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<VirtualKeyboardAPI>*
  GetFactoryInstance();

  VirtualKeyboardDelegate* delegate() { return delegate_.get(); }

 private:
  friend class BrowserContextKeyedAPIFactory<VirtualKeyboardAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "VirtualKeyboardAPI"; }

  // Require accces to delegate while incognito or during login.
  static const bool kServiceHasOwnInstanceInIncognito = true;

  std::unique_ptr<VirtualKeyboardDelegate> delegate_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_VIRTUAL_KEYBOARD_PRIVATE_VIRTUAL_KEYBOARD_PRIVATE_API_H_

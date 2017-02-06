// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"

#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/strings/string16.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/common/api/virtual_keyboard_private.h"
#include "ui/events/event.h"

namespace SetMode = extensions::api::virtual_keyboard_private::SetMode;
namespace SetRequestedKeyboardState =
    extensions::api::virtual_keyboard_private::SetKeyboardState;

namespace extensions {

namespace {

const char kNotYetImplementedError[] =
    "API is not implemented on this platform.";
const char kVirtualKeyboardNotEnabled[] =
    "The virtual keyboard is not enabled.";

typedef BrowserContextKeyedAPIFactory<VirtualKeyboardAPI> factory;

VirtualKeyboardDelegate* GetDelegate(SyncExtensionFunction* f) {
  VirtualKeyboardAPI* api = factory::Get(f->browser_context());
  DCHECK(api);
  return api ? api->delegate() : nullptr;
}

}  // namespace

bool VirtualKeyboardPrivateInsertTextFunction::RunSync() {
  VirtualKeyboardDelegate* delegate = GetDelegate(this);
  if (delegate) {
    base::string16 text;
    EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &text));
    return delegate->InsertText(text);
  }
  error_ = kNotYetImplementedError;
  return false;
}

bool VirtualKeyboardPrivateSendKeyEventFunction::RunSync() {
  VirtualKeyboardDelegate* delegate = GetDelegate(this);
  if (delegate) {
    base::Value* options_value = nullptr;
    base::DictionaryValue* params = nullptr;
    std::string type;
    int char_value;
    int key_code;
    std::string key_name;
    int modifiers;

    EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &options_value));
    EXTENSION_FUNCTION_VALIDATE(options_value->GetAsDictionary(&params));
    EXTENSION_FUNCTION_VALIDATE(params->GetString("type", &type));
    EXTENSION_FUNCTION_VALIDATE(params->GetInteger("charValue", &char_value));
    EXTENSION_FUNCTION_VALIDATE(params->GetInteger("keyCode", &key_code));
    EXTENSION_FUNCTION_VALIDATE(params->GetString("keyName", &key_name));
    EXTENSION_FUNCTION_VALIDATE(params->GetInteger("modifiers", &modifiers));
    return delegate->SendKeyEvent(
        type, char_value, key_code, key_name, modifiers);
  }
  error_ = kNotYetImplementedError;
  return false;
}

bool VirtualKeyboardPrivateHideKeyboardFunction::RunSync() {
  VirtualKeyboardDelegate* delegate = GetDelegate(this);
  if (delegate)
    return delegate->HideKeyboard();
  error_ = kNotYetImplementedError;
  return false;
}

bool VirtualKeyboardPrivateSetHotrodKeyboardFunction::RunSync() {
  VirtualKeyboardDelegate* delegate = GetDelegate(this);
  if (delegate) {
    bool enable;
    EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &enable));
    delegate->SetHotrodKeyboard(enable);
    return true;
  }
  error_ = kNotYetImplementedError;
  return false;
}

bool VirtualKeyboardPrivateLockKeyboardFunction::RunSync() {
  VirtualKeyboardDelegate* delegate = GetDelegate(this);
  if (delegate) {
    bool lock;
    EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &lock));
    return delegate->LockKeyboard(lock);
  }
  error_ = kNotYetImplementedError;
  return false;
}

bool VirtualKeyboardPrivateKeyboardLoadedFunction::RunSync() {
  VirtualKeyboardDelegate* delegate = GetDelegate(this);
  if (delegate)
    return delegate->OnKeyboardLoaded();
  error_ = kNotYetImplementedError;
  return false;
}

bool VirtualKeyboardPrivateGetKeyboardConfigFunction::RunSync() {
  VirtualKeyboardDelegate* delegate = GetDelegate(this);
  if (delegate) {
    std::unique_ptr<base::DictionaryValue> results(new base::DictionaryValue());
    if (delegate->GetKeyboardConfig(results.get())) {
      SetResult(std::move(results));
      return true;
    }
  }
  return false;
}

bool VirtualKeyboardPrivateOpenSettingsFunction::RunSync() {
  VirtualKeyboardDelegate* delegate = GetDelegate(this);
  if (delegate) {
    if (delegate->IsLanguageSettingsEnabled())
      return delegate->ShowLanguageSettings();
    return false;
  }
  error_ = kNotYetImplementedError;
  return false;
}

bool VirtualKeyboardPrivateSetModeFunction::RunSync() {
  VirtualKeyboardDelegate* delegate = GetDelegate(this);
  if (delegate) {
    std::unique_ptr<SetMode::Params> params = SetMode::Params::Create(*args_);
    EXTENSION_FUNCTION_VALIDATE(params);
    if (!delegate->SetVirtualKeyboardMode(params->mode)) {
      error_ = kVirtualKeyboardNotEnabled;
      return false;
    } else {
      return true;
    }
  }
  error_ = kNotYetImplementedError;
  return false;
}

bool VirtualKeyboardPrivateSetKeyboardStateFunction::RunSync() {
  VirtualKeyboardDelegate* delegate = GetDelegate(this);
  if (delegate) {
    std::unique_ptr<SetRequestedKeyboardState::Params> params =
        SetRequestedKeyboardState::Params::Create(*args_);
    EXTENSION_FUNCTION_VALIDATE(params);
    if (!delegate->SetRequestedKeyboardState(params->state)) {
      error_ = kVirtualKeyboardNotEnabled;
      return false;
    } else {
      return true;
    }
  }
  error_ = kNotYetImplementedError;
  return false;
}

VirtualKeyboardAPI::VirtualKeyboardAPI(content::BrowserContext* context) {
  delegate_ =
      extensions::ExtensionsAPIClient::Get()->CreateVirtualKeyboardDelegate();
}

VirtualKeyboardAPI::~VirtualKeyboardAPI() {
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<VirtualKeyboardAPI>>
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<VirtualKeyboardAPI>*
VirtualKeyboardAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

}  // namespace extensions

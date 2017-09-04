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

namespace extensions {

namespace {

const char kNotYetImplementedError[] =
    "API is not implemented on this platform.";
const char kVirtualKeyboardNotEnabled[] =
    "The virtual keyboard is not enabled.";
const char kUnknownError[] = "Unknown error.";

namespace keyboard = api::virtual_keyboard_private;

}  // namespace

bool VirtualKeyboardPrivateFunction::PreRunValidation(std::string* error) {
  if (!UIThreadExtensionFunction::PreRunValidation(error))
    return false;

  VirtualKeyboardAPI* api =
      BrowserContextKeyedAPIFactory<VirtualKeyboardAPI>::Get(browser_context());
  DCHECK(api);
  delegate_ = api->delegate();
  if (!delegate_) {
    *error = kNotYetImplementedError;
    return false;
  }
  return true;
}

VirtualKeyboardPrivateFunction::~VirtualKeyboardPrivateFunction() {}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateInsertTextFunction::Run() {
  base::string16 text;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &text));
  if (!delegate()->InsertText(text))
    return RespondNow(Error(kUnknownError));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSendKeyEventFunction::Run() {
  std::unique_ptr<keyboard::SendKeyEvent::Params> params(
      keyboard::SendKeyEvent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(params->key_event.modifiers);
  const keyboard::VirtualKeyboardEvent& event = params->key_event;
  if (!delegate()->SendKeyEvent(keyboard::ToString(event.type),
                                event.char_value, event.key_code,
                                event.key_name, *event.modifiers)) {
    return RespondNow(Error(kUnknownError));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateHideKeyboardFunction::Run() {
  if (!delegate()->HideKeyboard())
    return RespondNow(Error(kUnknownError));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetHotrodKeyboardFunction::Run() {
  bool enable = false;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &enable));
  delegate()->SetHotrodKeyboard(enable);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateLockKeyboardFunction::Run() {
  bool lock = false;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &lock));
  if (!delegate()->LockKeyboard(lock))
    return RespondNow(Error(kUnknownError));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateKeyboardLoadedFunction::Run() {
  if (!delegate()->OnKeyboardLoaded())
    return RespondNow(Error(kUnknownError));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateGetKeyboardConfigFunction::Run() {
  std::unique_ptr<base::DictionaryValue> results(new base::DictionaryValue());
  if (!delegate()->GetKeyboardConfig(results.get()))
    return RespondNow(Error(kUnknownError));
  return RespondNow(OneArgument(std::move(results)));
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateOpenSettingsFunction::Run() {
  if (!delegate()->IsLanguageSettingsEnabled() ||
      !delegate()->ShowLanguageSettings()) {
    return RespondNow(Error(kUnknownError));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction VirtualKeyboardPrivateSetModeFunction::Run() {
  std::unique_ptr<keyboard::SetMode::Params> params =
      keyboard::SetMode::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!delegate()->SetVirtualKeyboardMode(params->mode))
    return RespondNow(Error(kVirtualKeyboardNotEnabled));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetKeyboardStateFunction::Run() {
  std::unique_ptr<keyboard::SetKeyboardState::Params> params =
      keyboard::SetKeyboardState::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!delegate()->SetRequestedKeyboardState(params->state))
    return RespondNow(Error(kVirtualKeyboardNotEnabled));
  return RespondNow(NoArguments());
}

VirtualKeyboardAPI::VirtualKeyboardAPI(content::BrowserContext* context) {
  delegate_ = ExtensionsAPIClient::Get()->CreateVirtualKeyboardDelegate();
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

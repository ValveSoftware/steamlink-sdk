// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_view/chrome_web_view_internal_api.h"

#include "chrome/browser/extensions/api/context_menus/context_menus_api.h"
#include "chrome/browser/extensions/api/context_menus/context_menus_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/chrome_web_view_internal.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/error_utils.h"

namespace helpers = extensions::context_menus_api_helpers;
namespace webview = extensions::api::chrome_web_view_internal;

namespace extensions {

// TODO(lazyboy): Add checks similar to
// WebViewInternalExtensionFunction::RunAsyncSafe(WebViewGuest*).
bool ChromeWebViewInternalContextMenusCreateFunction::RunAsync() {
  std::unique_ptr<webview::ContextMenusCreate::Params> params(
      webview::ContextMenusCreate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  MenuItem::Id id(
      Profile::FromBrowserContext(browser_context())->IsOffTheRecord(),
      MenuItem::ExtensionKey(
          extension_id(),
          GetSenderWebContents()->GetRenderProcessHost()->GetID(),
          params->instance_id));

  if (params->create_properties.id.get()) {
    id.string_uid = *params->create_properties.id;
  } else {
    // The Generated Id is added by web_view_internal_custom_bindings.js.
    base::DictionaryValue* properties = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &properties));
    EXTENSION_FUNCTION_VALIDATE(
        properties->GetInteger(helpers::kGeneratedIdKey, &id.uid));
  }

  bool success = extensions::context_menus_api_helpers::CreateMenuItem(
      params->create_properties,
      Profile::FromBrowserContext(browser_context()),
      extension(),
      id,
      &error_);

  SendResponse(success);
  return success;
}

bool ChromeWebViewInternalContextMenusUpdateFunction::RunAsync() {
  std::unique_ptr<webview::ContextMenusUpdate::Params> params(
      webview::ContextMenusUpdate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Profile* profile = Profile::FromBrowserContext(browser_context());
  MenuItem::Id item_id(
      profile->IsOffTheRecord(),
      MenuItem::ExtensionKey(
          extension_id(),
          GetSenderWebContents()->GetRenderProcessHost()->GetID(),
          params->instance_id));

  if (params->id.as_string)
    item_id.string_uid = *params->id.as_string;
  else if (params->id.as_integer)
    item_id.uid = *params->id.as_integer;
  else
    NOTREACHED();

  bool success = extensions::context_menus_api_helpers::UpdateMenuItem(
      params->update_properties, profile, extension(), item_id, &error_);
  SendResponse(success);
  return success;
}

bool ChromeWebViewInternalContextMenusRemoveFunction::RunAsync() {
  std::unique_ptr<webview::ContextMenusRemove::Params> params(
      webview::ContextMenusRemove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  MenuManager* menu_manager =
      MenuManager::Get(Profile::FromBrowserContext(browser_context()));

  MenuItem::Id id(
      Profile::FromBrowserContext(browser_context())->IsOffTheRecord(),
      MenuItem::ExtensionKey(
          extension_id(),
          GetSenderWebContents()->GetRenderProcessHost()->GetID(),
          params->instance_id));

  if (params->menu_item_id.as_string) {
    id.string_uid = *params->menu_item_id.as_string;
  } else if (params->menu_item_id.as_integer) {
    id.uid = *params->menu_item_id.as_integer;
  } else {
    NOTREACHED();
  }

  bool success = true;
  MenuItem* item = menu_manager->GetItemById(id);
  // Ensure one <webview> can't remove another's menu items.
  if (!item || item->id().extension_key != id.extension_key) {
    error_ = ErrorUtils::FormatErrorMessage(
        context_menus_api_helpers::kCannotFindItemError,
        context_menus_api_helpers::GetIDString(id));
    success = false;
  } else if (!menu_manager->RemoveContextMenuItem(id)) {
    success = false;
  }

  SendResponse(success);
  return success;
}

bool ChromeWebViewInternalContextMenusRemoveAllFunction::RunAsync() {
  std::unique_ptr<webview::ContextMenusRemoveAll::Params> params(
      webview::ContextMenusRemoveAll::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  MenuManager* menu_manager =
      MenuManager::Get(Profile::FromBrowserContext(browser_context()));
  menu_manager->RemoveAllContextItems(MenuItem::ExtensionKey(
      extension_id(),
      GetSenderWebContents()->GetRenderProcessHost()->GetID(),
      params->instance_id));

  SendResponse(true);
  return true;
}

ChromeWebViewInternalShowContextMenuFunction::
    ChromeWebViewInternalShowContextMenuFunction() {
}

ChromeWebViewInternalShowContextMenuFunction::
    ~ChromeWebViewInternalShowContextMenuFunction() {
}

ExtensionFunction::ResponseAction
ChromeWebViewInternalShowContextMenuFunction::Run() {
  std::unique_ptr<webview::ShowContextMenu::Params> params(
      webview::ShowContextMenu::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // TODO(lazyboy): Actually implement filtering menu items.
  guest_->ShowContextMenu(params->request_id);
  return RespondNow(NoArguments());
}

}  // namespace extensions

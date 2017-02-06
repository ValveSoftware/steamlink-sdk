// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/context_menus/context_menus_api.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/context_menus/context_menus_api_helpers.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/url_pattern_set.h"

using extensions::ErrorUtils;
namespace helpers = extensions::context_menus_api_helpers;

namespace {

const char kIdRequiredError[] = "Extensions using event pages must pass an "
    "id parameter to chrome.contextMenus.create";

}  // namespace

namespace extensions {

namespace Create = api::context_menus::Create;
namespace Remove = api::context_menus::Remove;
namespace Update = api::context_menus::Update;

bool ContextMenusCreateFunction::RunSync() {
  MenuItem::Id id(GetProfile()->IsOffTheRecord(),
                  MenuItem::ExtensionKey(extension_id()));
  std::unique_ptr<Create::Params> params(Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->create_properties.id.get()) {
    id.string_uid = *params->create_properties.id;
  } else {
    if (BackgroundInfo::HasLazyBackgroundPage(extension())) {
      error_ = kIdRequiredError;
      return false;
    }

    // The Generated Id is added by context_menus_custom_bindings.js.
    base::DictionaryValue* properties = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &properties));
    EXTENSION_FUNCTION_VALIDATE(
        properties->GetInteger(helpers::kGeneratedIdKey, &id.uid));
  }

  return helpers::CreateMenuItem(
      params->create_properties, GetProfile(), extension(), id, &error_);
}

bool ContextMenusUpdateFunction::RunSync() {
  MenuItem::Id item_id(GetProfile()->IsOffTheRecord(),
                       MenuItem::ExtensionKey(extension_id()));
  std::unique_ptr<Update::Params> params(Update::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->id.as_string)
    item_id.string_uid = *params->id.as_string;
  else if (params->id.as_integer)
    item_id.uid = *params->id.as_integer;
  else
    NOTREACHED();

  return helpers::UpdateMenuItem(
      params->update_properties, GetProfile(), extension(), item_id, &error_);
}

bool ContextMenusRemoveFunction::RunSync() {
  std::unique_ptr<Remove::Params> params(Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  MenuManager* manager = MenuManager::Get(GetProfile());

  MenuItem::Id id(GetProfile()->IsOffTheRecord(),
                  MenuItem::ExtensionKey(extension_id()));
  if (params->menu_item_id.as_string)
    id.string_uid = *params->menu_item_id.as_string;
  else if (params->menu_item_id.as_integer)
    id.uid = *params->menu_item_id.as_integer;
  else
    NOTREACHED();

  MenuItem* item = manager->GetItemById(id);
  // Ensure one extension can't remove another's menu items.
  if (!item || item->extension_id() != extension_id()) {
    error_ = ErrorUtils::FormatErrorMessage(
        helpers::kCannotFindItemError, helpers::GetIDString(id));
    return false;
  }

  if (!manager->RemoveContextMenuItem(id))
    return false;
  manager->WriteToStorage(extension(), id.extension_key);
  return true;
}

bool ContextMenusRemoveAllFunction::RunSync() {
  MenuManager* manager = MenuManager::Get(GetProfile());
  manager->RemoveAllContextItems(MenuItem::ExtensionKey(extension()->id()));
  manager->WriteToStorage(extension(),
                          MenuItem::ExtensionKey(extension()->id()));
  return true;
}

}  // namespace extensions

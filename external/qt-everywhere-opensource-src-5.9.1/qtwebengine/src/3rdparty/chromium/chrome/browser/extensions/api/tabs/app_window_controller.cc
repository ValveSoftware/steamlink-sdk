// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/app_window_controller.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/tabs/app_base_window.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/extension.h"

namespace extensions {

AppWindowController::AppWindowController(
    AppWindow* app_window,
    std::unique_ptr<AppBaseWindow> base_window,
    Profile* profile)
    : WindowController(base_window.get(), profile),
      app_window_(app_window),
      base_window_(std::move(base_window)) {
  WindowControllerList::GetInstance()->AddExtensionWindow(this);
}

AppWindowController::~AppWindowController() {
  WindowControllerList::GetInstance()->RemoveExtensionWindow(this);
}

int AppWindowController::GetWindowId() const {
  return static_cast<int>(app_window_->session_id().id());
}

std::string AppWindowController::GetWindowTypeText() const {
  if (app_window_->window_type_is_panel())
    return tabs_constants::kWindowTypeValuePanel;
  return tabs_constants::kWindowTypeValueApp;
}

std::unique_ptr<base::DictionaryValue>
AppWindowController::CreateWindowValueWithTabs(
    const Extension* extension) const {
  std::unique_ptr<base::DictionaryValue> result = CreateWindowValue();

  std::unique_ptr<base::DictionaryValue> tab_value =
      CreateTabObject(extension, 0)->ToValue();
  if (!tab_value)
    return result;

  base::ListValue* tab_list = new base::ListValue();
  tab_list->Append(std::move(tab_value));
  result->Set(tabs_constants::kTabsKey, tab_list);

  return result;
}

std::unique_ptr<api::tabs::Tab> AppWindowController::CreateTabObject(
    const extensions::Extension* extension,
    int tab_index) const {
  if (tab_index > 0)
    return nullptr;

  content::WebContents* web_contents = app_window_->web_contents();
  if (!web_contents)
    return nullptr;

  std::unique_ptr<api::tabs::Tab> tab_object(new api::tabs::Tab);
  tab_object->id.reset(new int(SessionTabHelper::IdForTab(web_contents)));
  tab_object->index = 0;
  tab_object->window_id =
      SessionTabHelper::IdForWindowContainingTab(web_contents);
  tab_object->url.reset(new std::string(web_contents->GetURL().spec()));
  tab_object->status.reset(new std::string(
      ExtensionTabUtil::GetTabStatusText(web_contents->IsLoading())));
  tab_object->active = app_window_->GetBaseWindow()->IsActive();
  tab_object->selected = true;
  tab_object->highlighted = true;
  tab_object->pinned = false;
  tab_object->title.reset(
      new std::string(base::UTF16ToUTF8(web_contents->GetTitle())));
  tab_object->incognito = app_window_->GetBaseWindow()->IsActive();
  gfx::Rect bounds = app_window_->GetBaseWindow()->GetBounds();
  tab_object->width.reset(new int(bounds.width()));
  tab_object->height.reset(new int(bounds.height()));

  const Extension* ext = app_window_->GetExtension();
  if (ext) {
    std::string icon_str(chrome::kChromeUIFaviconURL);
    icon_str.append(app_window_->GetExtension()->url().spec());
    tab_object->fav_icon_url.reset(new std::string(icon_str));
  }

  return tab_object;
}

bool AppWindowController::CanClose(Reason* reason) const {
  return true;
}

void AppWindowController::SetFullscreenMode(bool is_fullscreen,
                                            const GURL& extension_url) const {
  // Do nothing. Panels cannot be fullscreen.
  if (app_window_->window_type_is_panel())
    return;
  // TODO(llandwerlin): should we prevent changes in fullscreen mode
  // when the fullscreen state is FULLSCREEN_TYPE_FORCED?
  app_window_->SetFullscreen(AppWindow::FULLSCREEN_TYPE_WINDOW_API,
                             is_fullscreen);
}

Browser* AppWindowController::GetBrowser() const {
  return nullptr;
}

bool AppWindowController::IsVisibleToExtension(
    const Extension* extension) const {
  DCHECK(extension);
  return extension->id() == app_window_->extension_id();
}

}  // namespace extensions

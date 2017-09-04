// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/launcher_page/launcher_page_api.h"

#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/common/extensions/api/launcher_page.h"
#include "content/public/browser/web_contents.h"
#include "ui/app_list/app_list_model.h"

namespace extensions {

static base::LazyInstance<BrowserContextKeyedAPIFactory<LauncherPageAPI>>
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<LauncherPageAPI>*
LauncherPageAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

LauncherPageAPI::LauncherPageAPI(content::BrowserContext* context)
    : service_(app_list::AppListSyncableServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context))) {
}

LauncherPageAPI::~LauncherPageAPI() {
}

app_list::AppListSyncableService* LauncherPageAPI::GetService() const {
  return service_;
}

LauncherPagePushSubpageFunction::LauncherPagePushSubpageFunction() {
}

ExtensionFunction::ResponseAction LauncherPagePushSubpageFunction::Run() {
  app_list::AppListSyncableService* service =
      LauncherPageAPI::GetFactoryInstance()
          ->Get(browser_context())
          ->GetService();
  app_list::AppListModel* model = service->GetModel();
  model->PushCustomLauncherPageSubpage();

  return RespondNow(NoArguments());
}

LauncherPageShowFunction::LauncherPageShowFunction() {
}

ExtensionFunction::ResponseAction LauncherPageShowFunction::Run() {
  AppListService::Get()->ShowForCustomLauncherPage(
      Profile::FromBrowserContext(browser_context()));

  return RespondNow(NoArguments());
}

LauncherPageHideFunction::LauncherPageHideFunction() {
}

ExtensionFunction::ResponseAction LauncherPageHideFunction::Run() {
  AppListService::Get()->HideCustomLauncherPage();

  return RespondNow(NoArguments());
}

LauncherPageSetEnabledFunction::LauncherPageSetEnabledFunction() {
}

ExtensionFunction::ResponseAction LauncherPageSetEnabledFunction::Run() {
  std::unique_ptr<api::launcher_page::SetEnabled::Params> params(
      api::launcher_page::SetEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  app_list::AppListSyncableService* service =
      LauncherPageAPI::GetFactoryInstance()
          ->Get(browser_context())
          ->GetService();
  app_list::AppListModel* model = service->GetModel();
  model->SetCustomLauncherPageEnabled(params->enabled);

  return RespondNow(NoArguments());
}

}  // namespace extensions

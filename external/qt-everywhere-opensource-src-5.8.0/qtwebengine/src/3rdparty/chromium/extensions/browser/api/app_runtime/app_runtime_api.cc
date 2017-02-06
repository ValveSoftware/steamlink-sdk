// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/app_runtime/app_runtime_api.h"

#include <stddef.h>

#include <utility>

#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "base/values.h"
#include "extensions/browser/entry_info.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/granted_file_entry.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/constants.h"
#include "extensions/common/feature_switch.h"
#include "url/gurl.h"

using content::BrowserContext;

namespace extensions {

namespace app_runtime = api::app_runtime;

namespace {

void DispatchOnEmbedRequestedEventImpl(
    const std::string& extension_id,
    std::unique_ptr<base::DictionaryValue> app_embedding_request_data,
    content::BrowserContext* context) {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Append(std::move(app_embedding_request_data));
  std::unique_ptr<Event> event(
      new Event(events::APP_RUNTIME_ON_EMBED_REQUESTED,
                app_runtime::OnEmbedRequested::kEventName, std::move(args)));
  event->restrict_to_browser_context = context;
  EventRouter::Get(context)
      ->DispatchEventWithLazyListener(extension_id, std::move(event));

  ExtensionPrefs::Get(context)
      ->SetLastLaunchTime(extension_id, base::Time::Now());
}

void DispatchOnLaunchedEventImpl(
    const std::string& extension_id,
    app_runtime::LaunchSource source,
    std::unique_ptr<base::DictionaryValue> launch_data,
    BrowserContext* context) {
  UMA_HISTOGRAM_ENUMERATION(
      "Extensions.AppLaunchSource", source, NUM_APP_LAUNCH_SOURCES);

  // "Forced app mode" is true for Chrome OS kiosk mode.
  launch_data->SetBoolean(
      "isKioskSession",
      ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode());

  launch_data->SetBoolean(
      "isPublicSession",
      ExtensionsBrowserClient::Get()->IsLoggedInAsPublicAccount());

  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Append(std::move(launch_data));
  std::unique_ptr<Event> event(new Event(events::APP_RUNTIME_ON_LAUNCHED,
                                         app_runtime::OnLaunched::kEventName,
                                         std::move(args)));
  event->restrict_to_browser_context = context;
  EventRouter::Get(context)
      ->DispatchEventWithLazyListener(extension_id, std::move(event));
  ExtensionPrefs::Get(context)
      ->SetLastLaunchTime(extension_id, base::Time::Now());
}

app_runtime::LaunchSource getLaunchSourceEnum(
    extensions::AppLaunchSource source) {
  switch (source) {
    case extensions::SOURCE_APP_LAUNCHER:
      return app_runtime::LAUNCH_SOURCE_APP_LAUNCHER;
    case extensions::SOURCE_NEW_TAB_PAGE:
      return app_runtime::LAUNCH_SOURCE_NEW_TAB_PAGE;
    case extensions::SOURCE_RELOAD:
      return app_runtime::LAUNCH_SOURCE_RELOAD;
    case extensions::SOURCE_RESTART:
      return app_runtime::LAUNCH_SOURCE_RESTART;
    case extensions::SOURCE_LOAD_AND_LAUNCH:
      return app_runtime::LAUNCH_SOURCE_LOAD_AND_LAUNCH;
    case extensions::SOURCE_COMMAND_LINE:
      return app_runtime::LAUNCH_SOURCE_COMMAND_LINE;
    case extensions::SOURCE_FILE_HANDLER:
      return app_runtime::LAUNCH_SOURCE_FILE_HANDLER;
    case extensions::SOURCE_URL_HANDLER:
      return app_runtime::LAUNCH_SOURCE_URL_HANDLER;
    case extensions::SOURCE_SYSTEM_TRAY:
      return app_runtime::LAUNCH_SOURCE_SYSTEM_TRAY;
    case extensions::SOURCE_ABOUT_PAGE:
      return app_runtime::LAUNCH_SOURCE_ABOUT_PAGE;
    case extensions::SOURCE_KEYBOARD:
      return app_runtime::LAUNCH_SOURCE_KEYBOARD;
    case extensions::SOURCE_EXTENSIONS_PAGE:
      return app_runtime::LAUNCH_SOURCE_EXTENSIONS_PAGE;
    case extensions::SOURCE_MANAGEMENT_API:
      return app_runtime::LAUNCH_SOURCE_MANAGEMENT_API;
    case extensions::SOURCE_EPHEMERAL_APP_DEPRECATED:
      return app_runtime::LAUNCH_SOURCE_EPHEMERAL_APP;
    case extensions::SOURCE_BACKGROUND:
      return app_runtime::LAUNCH_SOURCE_BACKGROUND;
    case extensions::SOURCE_KIOSK:
      return app_runtime::LAUNCH_SOURCE_KIOSK;
    case extensions::SOURCE_CHROME_INTERNAL:
      return app_runtime::LAUNCH_SOURCE_CHROME_INTERNAL;
    case extensions::SOURCE_TEST:
      return app_runtime::LAUNCH_SOURCE_TEST;
    case extensions::SOURCE_INSTALLED_NOTIFICATION:
      return app_runtime::LAUNCH_SOURCE_INSTALLED_NOTIFICATION;

    default:
      return app_runtime::LAUNCH_SOURCE_NONE;
  }
}

}  // namespace

// static
void AppRuntimeEventRouter::DispatchOnEmbedRequestedEvent(
    content::BrowserContext* context,
    std::unique_ptr<base::DictionaryValue> embed_app_data,
    const Extension* extension) {
  DispatchOnEmbedRequestedEventImpl(extension->id(), std::move(embed_app_data),
                                    context);
}

// static
void AppRuntimeEventRouter::DispatchOnLaunchedEvent(
    BrowserContext* context,
    const Extension* extension,
    extensions::AppLaunchSource source) {
  app_runtime::LaunchData launch_data;

  app_runtime::LaunchSource source_enum = getLaunchSourceEnum(source);
  if (extensions::FeatureSwitch::trace_app_source()->IsEnabled()) {
    launch_data.source = source_enum;
  }
  DispatchOnLaunchedEventImpl(extension->id(), source_enum,
                              launch_data.ToValue(), context);
}

// static
void AppRuntimeEventRouter::DispatchOnRestartedEvent(
    BrowserContext* context,
    const Extension* extension) {
  std::unique_ptr<base::ListValue> arguments(new base::ListValue());
  std::unique_ptr<Event> event(new Event(events::APP_RUNTIME_ON_RESTARTED,
                                         app_runtime::OnRestarted::kEventName,
                                         std::move(arguments)));
  event->restrict_to_browser_context = context;
  EventRouter::Get(context)
      ->DispatchEventToExtension(extension->id(), std::move(event));
}

// static
void AppRuntimeEventRouter::DispatchOnLaunchedEventWithFileEntries(
    BrowserContext* context,
    const Extension* extension,
    const std::string& handler_id,
    const std::vector<EntryInfo>& entries,
    const std::vector<GrantedFileEntry>& file_entries) {
  // TODO(sergeygs): Use the same way of creating an event (using the generated
  // boilerplate) as below in DispatchOnLaunchedEventWithUrl.
  std::unique_ptr<base::DictionaryValue> launch_data(new base::DictionaryValue);
  launch_data->SetString("id", handler_id);

  app_runtime::LaunchSource source_enum =
      app_runtime::LAUNCH_SOURCE_FILE_HANDLER;
  if (extensions::FeatureSwitch::trace_app_source()->IsEnabled()) {
    launch_data->SetString("source", app_runtime::ToString(source_enum));
  }

  std::unique_ptr<base::ListValue> items(new base::ListValue);
  DCHECK(file_entries.size() == entries.size());
  for (size_t i = 0; i < file_entries.size(); ++i) {
    std::unique_ptr<base::DictionaryValue> launch_item(
        new base::DictionaryValue);

    launch_item->SetString("fileSystemId", file_entries[i].filesystem_id);
    launch_item->SetString("baseName", file_entries[i].registered_name);
    launch_item->SetString("mimeType", entries[i].mime_type);
    launch_item->SetString("entryId", file_entries[i].id);
    launch_item->SetBoolean("isDirectory", entries[i].is_directory);
    items->Append(std::move(launch_item));
  }
  launch_data->Set("items", items.release());
  DispatchOnLaunchedEventImpl(extension->id(), source_enum,
                              std::move(launch_data), context);
}

// static
void AppRuntimeEventRouter::DispatchOnLaunchedEventWithUrl(
    BrowserContext* context,
    const Extension* extension,
    const std::string& handler_id,
    const GURL& url,
    const GURL& referrer_url) {
  app_runtime::LaunchData launch_data;
  app_runtime::LaunchSource source_enum =
      app_runtime::LAUNCH_SOURCE_URL_HANDLER;
  launch_data.id.reset(new std::string(handler_id));
  launch_data.url.reset(new std::string(url.spec()));
  launch_data.referrer_url.reset(new std::string(referrer_url.spec()));
  if (extensions::FeatureSwitch::trace_app_source()->IsEnabled()) {
    launch_data.source = source_enum;
  }
  DispatchOnLaunchedEventImpl(extension->id(), source_enum,
                              launch_data.ToValue(), context);
}

}  // namespace extensions

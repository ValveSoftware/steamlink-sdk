// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/webui/popular_sites_internals_message_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "components/ntp_tiles/popular_sites.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/webui/popular_sites_internals_message_handler_client.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_fixer.h"
#include "url/gurl.h"

namespace {

std::string ReadFileToString(const base::FilePath& path) {
  std::string result;
  if (!base::ReadFileToString(path, &result))
    result.clear();
  return result;
}

}  // namespace

namespace ntp_tiles {

PopularSitesInternalsMessageHandlerClient::
    PopularSitesInternalsMessageHandlerClient() = default;
PopularSitesInternalsMessageHandlerClient::
    ~PopularSitesInternalsMessageHandlerClient() = default;

PopularSitesInternalsMessageHandler::PopularSitesInternalsMessageHandler(
    PopularSitesInternalsMessageHandlerClient* web_ui)
    : web_ui_(web_ui), weak_ptr_factory_(this) {}

PopularSitesInternalsMessageHandler::~PopularSitesInternalsMessageHandler() =
    default;

void PopularSitesInternalsMessageHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(
      "registerForEvents",
      base::Bind(&PopularSitesInternalsMessageHandler::HandleRegisterForEvents,
                 base::Unretained(this)));

  web_ui_->RegisterMessageCallback(
      "update", base::Bind(&PopularSitesInternalsMessageHandler::HandleUpdate,
                           base::Unretained(this)));

  web_ui_->RegisterMessageCallback(
      "viewJson",
      base::Bind(&PopularSitesInternalsMessageHandler::HandleViewJson,
                 base::Unretained(this)));
}

void PopularSitesInternalsMessageHandler::HandleRegisterForEvents(
    const base::ListValue* args) {
  DCHECK(args->empty());

  SendOverrides();

  popular_sites_ = web_ui_->MakePopularSites();
  popular_sites_->StartFetch(
      false,
      base::Bind(&PopularSitesInternalsMessageHandler::OnPopularSitesAvailable,
                 base::Unretained(this), false));
}

void PopularSitesInternalsMessageHandler::HandleUpdate(
    const base::ListValue* args) {
  DCHECK_EQ(3u, args->GetSize());

  PrefService* prefs = web_ui_->GetPrefs();

  std::string url;
  args->GetString(0, &url);
  if (url.empty())
    prefs->ClearPref(ntp_tiles::prefs::kPopularSitesOverrideURL);
  else
    prefs->SetString(ntp_tiles::prefs::kPopularSitesOverrideURL,
                     url_formatter::FixupURL(url, std::string()).spec());

  std::string country;
  args->GetString(1, &country);
  if (country.empty())
    prefs->ClearPref(ntp_tiles::prefs::kPopularSitesOverrideCountry);
  else
    prefs->SetString(ntp_tiles::prefs::kPopularSitesOverrideCountry, country);

  std::string version;
  args->GetString(2, &version);
  if (version.empty())
    prefs->ClearPref(ntp_tiles::prefs::kPopularSitesOverrideVersion);
  else
    prefs->SetString(ntp_tiles::prefs::kPopularSitesOverrideVersion, version);

  popular_sites_ = web_ui_->MakePopularSites();
  popular_sites_->StartFetch(
      true,
      base::Bind(&PopularSitesInternalsMessageHandler::OnPopularSitesAvailable,
                 base::Unretained(this), true));
}

void PopularSitesInternalsMessageHandler::HandleViewJson(
    const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());

  const base::FilePath& path = popular_sites_->local_path();
  base::PostTaskAndReplyWithResult(
      web_ui_->GetBlockingPool()
          ->GetTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN)
          .get(),
      FROM_HERE, base::Bind(&ReadFileToString, path),
      base::Bind(&PopularSitesInternalsMessageHandler::SendJson,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PopularSitesInternalsMessageHandler::SendOverrides() {
  PrefService* prefs = web_ui_->GetPrefs();
  std::string url =
      prefs->GetString(ntp_tiles::prefs::kPopularSitesOverrideURL);
  std::string country =
      prefs->GetString(ntp_tiles::prefs::kPopularSitesOverrideCountry);
  std::string version =
      prefs->GetString(ntp_tiles::prefs::kPopularSitesOverrideVersion);
  web_ui_->CallJavascriptFunction(
      "chrome.popular_sites_internals.receiveOverrides", base::StringValue(url),
      base::StringValue(country), base::StringValue(version));
}

void PopularSitesInternalsMessageHandler::SendDownloadResult(bool success) {
  base::StringValue result(success ? "Success" : "Fail");
  web_ui_->CallJavascriptFunction(
      "chrome.popular_sites_internals.receiveDownloadResult", result);
}

void PopularSitesInternalsMessageHandler::SendSites() {
  auto sites_list = base::MakeUnique<base::ListValue>();
  for (const PopularSites::Site& site : popular_sites_->sites()) {
    auto entry = base::MakeUnique<base::DictionaryValue>();
    entry->SetString("title", site.title);
    entry->SetString("url", site.url.spec());
    sites_list->Append(std::move(entry));
  }

  base::DictionaryValue result;
  result.Set("sites", std::move(sites_list));
  result.SetString("url", popular_sites_->LastURL().spec());
  web_ui_->CallJavascriptFunction("chrome.popular_sites_internals.receiveSites",
                                  result);
}

void PopularSitesInternalsMessageHandler::SendJson(const std::string& json) {
  web_ui_->CallJavascriptFunction("chrome.popular_sites_internals.receiveJson",
                                  base::StringValue(json));
}

void PopularSitesInternalsMessageHandler::OnPopularSitesAvailable(
    bool explicit_request,
    bool success) {
  if (explicit_request)
    SendDownloadResult(success);
  SendSites();
}

}  // namespace ntp_tiles

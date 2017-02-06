// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/popular_sites.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/google/core/browser/google_util.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/switches.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/safe_json/safe_json_parser.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"

using net::URLFetcher;
using variations::VariationsService;

namespace ntp_tiles {

namespace {

const char kPopularSitesURLFormat[] =
    "https://www.gstatic.com/chrome/ntp/suggested_sites_%s_%s.json";
const char kPopularSitesDefaultCountryCode[] = "DEFAULT";
const char kPopularSitesDefaultVersion[] = "5";
const char kPopularSitesLocalFilename[] = "suggested_sites.json";
const int kPopularSitesRedownloadIntervalHours = 24;

const char kPopularSitesLastDownloadPref[] = "popular_sites_last_download";
const char kPopularSitesCountryPref[] = "popular_sites_country";
const char kPopularSitesVersionPref[] = "popular_sites_version";
const char kPopularSitesURLPref[] = "popular_sites_url";

GURL GetPopularSitesURL(const std::string& country,
                        const std::string& version) {
  return GURL(base::StringPrintf(kPopularSitesURLFormat, country.c_str(),
                                 version.c_str()));
}

// Extract the country from the default search engine if the default search
// engine is Google.
std::string GetDefaultSearchEngineCountryCode(
    const TemplateURLService* template_url_service) {
  DCHECK(template_url_service);

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(
          ntp_tiles::switches::kEnableNTPSearchEngineCountryDetection))
    return std::string();

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  // It's possible to not have a default provider in the case that the default
  // search engine is defined by policy.
  if (default_provider) {
    bool is_google_search_engine =
        default_provider->GetEngineType(
            template_url_service->search_terms_data()) ==
        SearchEngineType::SEARCH_ENGINE_GOOGLE;

    if (is_google_search_engine) {
      GURL search_url = default_provider->GenerateSearchURL(
          template_url_service->search_terms_data());
      return google_util::GetGoogleCountryCode(search_url);
    }
  }

  return std::string();
}

std::string GetVariationCountry() {
  return variations::GetVariationParamValue(kPopularSitesFieldTrialName,
                                            "country");
}

std::string GetVariationVersion() {
  return variations::GetVariationParamValue(kPopularSitesFieldTrialName,
                                            "version");
}

// Determine the country code to use. In order of precedence:
// - The explicit "override country" pref set by the user.
// - The country code from the field trial config (variation parameter).
// - The Google country code if Google is the default search engine (and the
//   "--enable-ntp-search-engine-country-detection" switch is present).
// - The country provided by the VariationsService.
// - A default fallback.
std::string GetCountryToUse(const PrefService* prefs,
                            const TemplateURLService* template_url_service,
                            VariationsService* variations_service) {
  std::string country_code =
      prefs->GetString(ntp_tiles::prefs::kPopularSitesOverrideCountry);

  if (country_code.empty())
    country_code = GetVariationCountry();

  if (country_code.empty())
    country_code = GetDefaultSearchEngineCountryCode(template_url_service);

  if (country_code.empty() && variations_service)
    country_code = variations_service->GetStoredPermanentCountry();

  if (country_code.empty())
    country_code = kPopularSitesDefaultCountryCode;

  return base::ToUpperASCII(country_code);
}

// Determine the version to use. In order of precedence:
// - The explicit "override version" pref set by the user.
// - The version from the field trial config (variation parameter).
// - A default fallback.
std::string GetVersionToUse(const PrefService* prefs) {
  std::string version =
      prefs->GetString(ntp_tiles::prefs::kPopularSitesOverrideVersion);

  if (version.empty())
    version = GetVariationVersion();

  if (version.empty())
    version = kPopularSitesDefaultVersion;

  return version;
}

// Must run on the blocking thread pool.
bool WriteJsonToFile(const base::FilePath& local_path,
                     const base::Value* json) {
  std::string json_string;
  return base::JSONWriter::Write(*json, &json_string) &&
         base::ImportantFileWriter::WriteFileAtomically(local_path,
                                                        json_string);
}

}  // namespace

PopularSites::Site::Site(const base::string16& title,
                         const GURL& url,
                         const GURL& favicon_url,
                         const GURL& large_icon_url,
                         const GURL& thumbnail_url)
    : title(title),
      url(url),
      favicon_url(favicon_url),
      large_icon_url(large_icon_url),
      thumbnail_url(thumbnail_url) {}

PopularSites::Site::Site(const Site& other) = default;

PopularSites::Site::~Site() {}

PopularSites::PopularSites(
    const scoped_refptr<base::SequencedWorkerPool>& blocking_pool,
    PrefService* prefs,
    const TemplateURLService* template_url_service,
    VariationsService* variations_service,
    net::URLRequestContextGetter* download_context,
    const base::FilePath& directory,
    bool force_download,
    const FinishedCallback& callback)
    : callback_(callback),
      is_fallback_(false),
      local_path_(directory.empty()
                      ? base::FilePath()
                      : directory.AppendASCII(kPopularSitesLocalFilename)),
      prefs_(prefs),
      download_context_(download_context),
      blocking_runner_(blocking_pool->GetTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN)),
      weak_ptr_factory_(this) {
  const base::Time last_download_time = base::Time::FromInternalValue(
      prefs_->GetInt64(kPopularSitesLastDownloadPref));
  const base::TimeDelta time_since_last_download =
      base::Time::Now() - last_download_time;
  const base::TimeDelta redownload_interval =
      base::TimeDelta::FromHours(kPopularSitesRedownloadIntervalHours);
  const bool download_time_is_future = base::Time::Now() < last_download_time;

  const std::string country =
      GetCountryToUse(prefs, template_url_service, variations_service);
  const std::string version = GetVersionToUse(prefs);

  const GURL override_url =
      GURL(prefs->GetString(ntp_tiles::prefs::kPopularSitesOverrideURL));
  pending_url_ = override_url.is_valid() ? override_url
                                         : GetPopularSitesURL(country, version);
  const bool url_changed =
      pending_url_.spec() != prefs_->GetString(kPopularSitesURLPref);

  // No valid path to save to. Immediately post failure.
  if (local_path_.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback_, false));
    return;
  }

  // Download forced, or we need to download a new file.
  if (force_download || download_time_is_future ||
      (time_since_last_download > redownload_interval) || url_changed) {
    FetchPopularSites();
    return;
  }

  std::unique_ptr<std::string> file_data(new std::string);
  std::string* file_data_ptr = file_data.get();
  base::PostTaskAndReplyWithResult(
      blocking_runner_.get(), FROM_HERE,
      base::Bind(&base::ReadFileToString, local_path_, file_data_ptr),
      base::Bind(&PopularSites::OnReadFileDone, weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(file_data))));
}

PopularSites::~PopularSites() {}

GURL PopularSites::LastURL() const {
  return GURL(prefs_->GetString(kPopularSitesURLPref));
}

// static
void PopularSites::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterStringPref(ntp_tiles::prefs::kPopularSitesOverrideURL,
                                 std::string());
  user_prefs->RegisterStringPref(ntp_tiles::prefs::kPopularSitesOverrideCountry,
                                 std::string());
  user_prefs->RegisterStringPref(ntp_tiles::prefs::kPopularSitesOverrideVersion,
                                 std::string());

  user_prefs->RegisterInt64Pref(kPopularSitesLastDownloadPref, 0);
  user_prefs->RegisterStringPref(kPopularSitesURLPref, std::string());

  // TODO(sfiera): remove these obsolete preferences.
  user_prefs->RegisterStringPref(kPopularSitesCountryPref, std::string());
  user_prefs->RegisterStringPref(kPopularSitesVersionPref, std::string());
}

void PopularSites::OnReadFileDone(std::unique_ptr<std::string> data,
                                  bool success) {
  if (success) {
    auto json = base::JSONReader::Read(*data, base::JSON_ALLOW_TRAILING_COMMAS);
    if (json) {
      ParseSiteList(std::move(json));
    } else {
      OnJsonParseFailed("previously-fetched JSON was no longer parseable");
    }
  } else {
    // File didn't exist, or couldn't be read for some other reason.
    FetchPopularSites();
  }
}

void PopularSites::FetchPopularSites() {
  fetcher_ = URLFetcher::Create(pending_url_, URLFetcher::GET, this);
  fetcher_->SetRequestContext(download_context_);
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                         net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher_->SetAutomaticallyRetryOnNetworkChanges(1);
  fetcher_->Start();
}

void PopularSites::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(fetcher_.get(), source);
  std::unique_ptr<net::URLFetcher> free_fetcher = std::move(fetcher_);

  std::string json_string;
  if (!(source->GetStatus().is_success() &&
        source->GetResponseCode() == net::HTTP_OK &&
        source->GetResponseAsString(&json_string))) {
    OnDownloadFailed();
    return;
  }

  safe_json::SafeJsonParser::Parse(
      json_string,
      base::Bind(&PopularSites::OnJsonParsed, weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&PopularSites::OnJsonParseFailed,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PopularSites::OnJsonParsed(std::unique_ptr<base::Value> json) {
  const base::Value* json_ptr = json.get();
  base::PostTaskAndReplyWithResult(
      blocking_runner_.get(), FROM_HERE,
      base::Bind(&WriteJsonToFile, local_path_, json_ptr),
      base::Bind(&PopularSites::OnFileWriteDone, weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(json))));
}

void PopularSites::OnJsonParseFailed(const std::string& error_message) {
  DLOG(WARNING) << "JSON parsing failed: " << error_message;
  OnDownloadFailed();
}

void PopularSites::OnFileWriteDone(std::unique_ptr<base::Value> json,
                                   bool success) {
  if (success) {
    prefs_->SetInt64(kPopularSitesLastDownloadPref,
                     base::Time::Now().ToInternalValue());
    prefs_->SetString(kPopularSitesURLPref, pending_url_.spec());
    ParseSiteList(std::move(json));
  } else {
    DLOG(WARNING) << "Could not write file to "
                  << local_path_.LossyDisplayName();
    OnDownloadFailed();
  }
}

void PopularSites::ParseSiteList(std::unique_ptr<base::Value> json) {
  base::ListValue* list = nullptr;
  if (!json || !json->GetAsList(&list)) {
    DLOG(WARNING) << "JSON is not a list";
    sites_.clear();
    callback_.Run(false);
    return;
  }

  std::vector<PopularSites::Site> sites;
  for (size_t i = 0; i < list->GetSize(); i++) {
    base::DictionaryValue* item;
    if (!list->GetDictionary(i, &item))
      continue;
    base::string16 title;
    std::string url;
    if (!item->GetString("title", &title) || !item->GetString("url", &url))
      continue;
    std::string favicon_url;
    item->GetString("favicon_url", &favicon_url);
    std::string thumbnail_url;
    item->GetString("thumbnail_url", &thumbnail_url);
    std::string large_icon_url;
    item->GetString("large_icon_url", &large_icon_url);

    sites.push_back(PopularSites::Site(title, GURL(url), GURL(favicon_url),
                                       GURL(large_icon_url),
                                       GURL(thumbnail_url)));
  }

  sites_.swap(sites);
  callback_.Run(true);
}

void PopularSites::OnDownloadFailed() {
  if (!is_fallback_) {
    DLOG(WARNING) << "Download country site list failed";
    is_fallback_ = true;
    pending_url_ = GetPopularSitesURL(kPopularSitesDefaultCountryCode,
                                      kPopularSitesDefaultVersion);
    FetchPopularSites();
  } else {
    DLOG(WARNING) << "Download fallback site list failed";
    callback_.Run(false);
  }
}

}  // namespace ntp_tiles

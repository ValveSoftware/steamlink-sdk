// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_POPULAR_SITES_H_
#define COMPONENTS_NTP_TILES_POPULAR_SITES_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace net {
class URLRequestContextGetter;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace variations {
class VariationsService;
}

class PrefService;
class TemplateURLService;

namespace ntp_tiles {

using ParseJSONCallback = base::Callback<void(
    const std::string& unsafe_json,
    const base::Callback<void(std::unique_ptr<base::Value>)>& success_callback,
    const base::Callback<void(const std::string&)>& error_callback)>;

// Downloads and provides a list of suggested popular sites, for display on
// the NTP when there are not enough personalized tiles. Caches the downloaded
// file on disk to avoid re-downloading on every startup.
class PopularSites : public net::URLFetcherDelegate {
 public:
  struct Site {
    Site(const base::string16& title,
         const GURL& url,
         const GURL& favicon_url,
         const GURL& large_icon_url,
         const GURL& thumbnail_url);
    Site(const Site& other);
    ~Site();

    base::string16 title;
    GURL url;
    GURL favicon_url;
    GURL large_icon_url;
    GURL thumbnail_url;
  };

  using FinishedCallback = base::Callback<void(bool /* success */)>;

  PopularSites(const scoped_refptr<base::SequencedWorkerPool>& blocking_pool,
               PrefService* prefs,
               const TemplateURLService* template_url_service,
               variations::VariationsService* variations_service,
               net::URLRequestContextGetter* download_context,
               const base::FilePath& directory,
               ParseJSONCallback parse_json);

  // Starts the process of retrieving popular sites. When they are available,
  // invokes |callback| with the result, on the same thread as the caller. Never
  // invokes |callback| before returning control to the caller, even if the
  // result is immediately known.
  //
  // Set |force_download| to enforce re-downloading the popular sites file, even
  // if it already exists on disk.
  //
  // Must be called at most once on a given PopularSites object.
  void StartFetch(bool force_download, const FinishedCallback& callback);

  ~PopularSites() override;

  const std::vector<Site>& sites() const { return sites_; }

  // The URL of the file that was last downloaded.
  GURL LastURL() const;

  const base::FilePath& local_path() const { return local_path_; }

  // Register preferences used by this class.
  static void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* user_prefs);

 private:
  void OnReadFileDone(std::unique_ptr<std::string> data, bool success);

  // Fetch the popular sites at the given URL, overwriting any file that already
  // exists.
  void FetchPopularSites();

  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  void OnJsonParsed(std::unique_ptr<base::Value> json);
  void OnJsonParseFailed(const std::string& error_message);
  void OnFileWriteDone(std::unique_ptr<base::Value> json, bool success);
  void ParseSiteList(std::unique_ptr<base::Value> json);
  void OnDownloadFailed();

  // Parameters set from constructor.
  scoped_refptr<base::TaskRunner> const blocking_runner_;
  PrefService* const prefs_;
  const TemplateURLService* const template_url_service_;
  variations::VariationsService* const variations_;
  net::URLRequestContextGetter* const download_context_;
  base::FilePath const local_path_;
  ParseJSONCallback parse_json_;

  // Set by StartFetch() and called after fetch completes.
  FinishedCallback callback_;

  std::unique_ptr<net::URLFetcher> fetcher_;
  bool is_fallback_;
  std::vector<Site> sites_;
  GURL pending_url_;

  base::WeakPtrFactory<PopularSites> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PopularSites);
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_POPULAR_SITES_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/popular_sites.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_parser.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::IsEmpty;

namespace ntp_tiles {
namespace {

const char kTitle[] = "title";
const char kUrl[] = "url";
const char kLargeIconUrl[] = "large_icon_url";
const char kFaviconUrl[] = "favicon_url";

using TestPopularSite = std::map<std::string, std::string>;
using TestPopularSiteVector = std::vector<TestPopularSite>;

::testing::Matcher<const base::string16&> Str16Eq(const std::string& s) {
  return ::testing::Eq(base::UTF8ToUTF16(s));
}

::testing::Matcher<const GURL&> URLEq(const std::string& s) {
  return ::testing::Eq(GURL(s));
}

// Copied from iOS code. Perhaps should be in a shared location.
class JsonUnsafeParser {
 public:
  using SuccessCallback = base::Callback<void(std::unique_ptr<base::Value>)>;
  using ErrorCallback = base::Callback<void(const std::string&)>;

  // As with SafeJsonParser, runs either success_callback or error_callback on
  // the calling thread, but not before the call returns.
  static void Parse(const std::string& unsafe_json,
                    const SuccessCallback& success_callback,
                    const ErrorCallback& error_callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(
            [](const std::string& unsafe_json,
               const SuccessCallback& success_callback,
               const ErrorCallback& error_callback) {
              std::string error_msg;
              int error_line, error_column;
              std::unique_ptr<base::Value> value =
                  base::JSONReader::ReadAndReturnError(
                      unsafe_json, base::JSON_ALLOW_TRAILING_COMMAS, nullptr,
                      &error_msg, &error_line, &error_column);
              if (value) {
                success_callback.Run(std::move(value));
              } else {
                error_callback.Run(base::StringPrintf(
                    "%s (%d:%d)", error_msg.c_str(), error_line, error_column));
              }
            },
            unsafe_json, success_callback, error_callback));
  }

  JsonUnsafeParser() = delete;
};

class PopularSitesTest : public ::testing::Test {
 protected:
  PopularSitesTest()
      : kWikipedia{
            {kTitle, "Wikipedia, fhta Ph'nglui mglw'nafh"},
            {kUrl, "https://zz.m.wikipedia.org/"},
            {kLargeIconUrl, "https://zz.m.wikipedia.org/wikipedia.png"},
        },
        kYouTube{
            {kTitle, "YouTube"},
            {kUrl, "https://m.youtube.com/"},
            {kLargeIconUrl, "https://s.ytimg.com/apple-touch-icon.png"},
        },
        kChromium{
            {kTitle, "The Chromium Project"},
            {kUrl, "https://www.chromium.org/"},
            {kFaviconUrl, "https://www.chromium.org/favicon.ico"},
        },
        worker_pool_owner_(2, "PopularSitesTest."),
        url_fetcher_factory_(nullptr) {
    PopularSites::RegisterProfilePrefs(prefs_.registry());
    CHECK(scoped_cache_dir_.CreateUniqueTempDir());
    cache_dir_ = scoped_cache_dir_.GetPath();
  }

  void SetCountryAndVersion(const std::string& country,
                            const std::string& version) {
    prefs_.SetString(prefs::kPopularSitesOverrideCountry, country);
    prefs_.SetString(prefs::kPopularSitesOverrideVersion, version);
  }

  void RespondWithJSON(const std::string& url,
                       const TestPopularSiteVector& sites) {
    base::ListValue sites_value;
    for (const TestPopularSite& site : sites) {
      auto site_value = base::MakeUnique<base::DictionaryValue>();
      for (const std::pair<std::string, std::string>& kv : site) {
        site_value->SetString(kv.first, kv.second);
      }
      sites_value.Append(std::move(site_value));
    }
    std::string sites_string;
    base::JSONWriter::Write(sites_value, &sites_string);
    url_fetcher_factory_.SetFakeResponse(GURL(url), sites_string, net::HTTP_OK,
                                         net::URLRequestStatus::SUCCESS);
  }

  void RespondWithData(const std::string& url, const std::string& data) {
    url_fetcher_factory_.SetFakeResponse(GURL(url), data, net::HTTP_OK,
                                         net::URLRequestStatus::SUCCESS);
  }

  void RespondWith404(const std::string& url) {
    url_fetcher_factory_.SetFakeResponse(GURL(url), "404", net::HTTP_NOT_FOUND,
                                         net::URLRequestStatus::SUCCESS);
  }

  bool FetchPopularSites(bool force_download,
                         std::vector<PopularSites::Site>* sites) {
    scoped_refptr<net::TestURLRequestContextGetter> url_request_context(
        new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get()));
    PopularSites popular_sites(worker_pool_owner_.pool().get(), &prefs_,
                               /*template_url_service=*/nullptr,
                               /*variations_service=*/nullptr,
                               url_request_context.get(), cache_dir_,
                               base::Bind(JsonUnsafeParser::Parse));

    base::RunLoop loop;
    bool save_success = false;
    popular_sites.StartFetch(
        force_download,
        base::Bind(
            [](bool* save_success, base::RunLoop* loop, bool success) {
              *save_success = success;
              loop->Quit();
            },
            &save_success, &loop));
    loop.Run();
    *sites = popular_sites.sites();
    return save_success;
  }

  const TestPopularSite kWikipedia;
  const TestPopularSite kYouTube;
  const TestPopularSite kChromium;

  base::MessageLoopForUI ui_loop_;
  base::SequencedWorkerPoolOwner worker_pool_owner_;
  base::ScopedTempDir scoped_cache_dir_;
  base::FilePath cache_dir_;
  user_prefs::TestingPrefServiceSyncable prefs_;
  net::FakeURLFetcherFactory url_fetcher_factory_;
};

TEST_F(PopularSitesTest, Basic) {
  SetCountryAndVersion("ZZ", "9");
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kWikipedia});

  std::vector<PopularSites::Site> sites;
  EXPECT_TRUE(FetchPopularSites(/*force_download=*/false, &sites));

  ASSERT_THAT(sites.size(), Eq(1u));
  EXPECT_THAT(sites[0].title, Str16Eq("Wikipedia, fhta Ph'nglui mglw'nafh"));
  EXPECT_THAT(sites[0].url, URLEq("https://zz.m.wikipedia.org/"));
  EXPECT_THAT(sites[0].large_icon_url,
              URLEq("https://zz.m.wikipedia.org/wikipedia.png"));
  EXPECT_THAT(sites[0].favicon_url, URLEq(""));
}

TEST_F(PopularSitesTest, Fallback) {
  SetCountryAndVersion("ZZ", "9");
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json");
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_DEFAULT_5.json",
      {kYouTube, kChromium});

  std::vector<PopularSites::Site> sites;
  EXPECT_TRUE(FetchPopularSites(/*force_download=*/false, &sites));

  ASSERT_THAT(sites.size(), Eq(2u));
  EXPECT_THAT(sites[0].title, Str16Eq("YouTube"));
  EXPECT_THAT(sites[0].url, URLEq("https://m.youtube.com/"));
  EXPECT_THAT(sites[0].large_icon_url,
              URLEq("https://s.ytimg.com/apple-touch-icon.png"));
  EXPECT_THAT(sites[0].favicon_url, URLEq(""));
  EXPECT_THAT(sites[1].title, Str16Eq("The Chromium Project"));
  EXPECT_THAT(sites[1].url, URLEq("https://www.chromium.org/"));
  EXPECT_THAT(sites[1].large_icon_url, URLEq(""));
  EXPECT_THAT(sites[1].favicon_url,
              URLEq("https://www.chromium.org/favicon.ico"));
}

TEST_F(PopularSitesTest, Failure) {
  SetCountryAndVersion("ZZ", "9");
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json");
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_DEFAULT_5.json");

  std::vector<PopularSites::Site> sites;
  EXPECT_FALSE(FetchPopularSites(/*force_download=*/false, &sites));
  ASSERT_THAT(sites, IsEmpty());
}

TEST_F(PopularSitesTest, FailsWithoutFetchIfNoCacheDir) {
  SetCountryAndVersion("ZZ", "9");
  std::vector<PopularSites::Site> sites;
  cache_dir_ = base::FilePath();  // Override with invalid file path.
  EXPECT_FALSE(FetchPopularSites(/*force_download=*/false, &sites));
}

TEST_F(PopularSitesTest, UsesCachedFile) {
  SetCountryAndVersion("ZZ", "9");
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kWikipedia});

  // First request succeeds and gets cached.
  std::vector<PopularSites::Site> sites;
  EXPECT_TRUE(FetchPopularSites(/*force_download=*/false, &sites));

  // File disappears from server, but we don't need it because it's cached.
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json");
  EXPECT_TRUE(FetchPopularSites(/*force_download=*/false, &sites));
  EXPECT_THAT(sites[0].url, URLEq("https://zz.m.wikipedia.org/"));
}

TEST_F(PopularSitesTest, CachesEmptyFile) {
  SetCountryAndVersion("ZZ", "9");
  RespondWithData(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json", "[]");
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_DEFAULT_5.json",
      {kWikipedia});

  // First request succeeds and caches empty suggestions list (no fallback).
  std::vector<PopularSites::Site> sites;
  EXPECT_TRUE(FetchPopularSites(/*force_download=*/false, &sites));
  EXPECT_THAT(sites, IsEmpty());

  // File appears on server, but we continue to use our cached empty file.
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kWikipedia});
  EXPECT_TRUE(FetchPopularSites(/*force_download=*/false, &sites));
  EXPECT_THAT(sites, IsEmpty());
}

TEST_F(PopularSitesTest, DoesntUseCachedFileIfDownloadForced) {
  SetCountryAndVersion("ZZ", "9");
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kWikipedia});

  // First request succeeds and gets cached.
  std::vector<PopularSites::Site> sites;
  EXPECT_TRUE(FetchPopularSites(/*force_download=*/true, &sites));
  EXPECT_THAT(sites[0].url, URLEq("https://zz.m.wikipedia.org/"));

  // File disappears from server. Download is forced, so we get the new file.
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kChromium});
  EXPECT_TRUE(FetchPopularSites(/*force_download=*/true, &sites));
  EXPECT_THAT(sites[0].url, URLEq("https://www.chromium.org/"));
}

TEST_F(PopularSitesTest, RefetchesAfterCountryMoved) {
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kWikipedia});
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZX_9.json",
      {kChromium});

  std::vector<PopularSites::Site> sites;

  // First request (in ZZ) saves Wikipedia.
  SetCountryAndVersion("ZZ", "9");
  EXPECT_TRUE(FetchPopularSites(/*force_download=*/false, &sites));
  EXPECT_THAT(sites[0].url, URLEq("https://zz.m.wikipedia.org/"));

  // Second request (now in ZX) saves Chromium.
  SetCountryAndVersion("ZX", "9");
  EXPECT_TRUE(FetchPopularSites(/*force_download=*/false, &sites));
  EXPECT_THAT(sites[0].url, URLEq("https://www.chromium.org/"));
}

TEST_F(PopularSitesTest, DoesntCacheInvalidFile) {
  SetCountryAndVersion("ZZ", "9");
  RespondWithData(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      "ceci n'est pas un json");
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_DEFAULT_5.json");

  // First request falls back and gets nothing there either.
  std::vector<PopularSites::Site> sites;
  EXPECT_FALSE(FetchPopularSites(/*force_download=*/false, &sites));

  // Second request refetches ZZ_9, which now has data.
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kChromium});
  EXPECT_TRUE(FetchPopularSites(/*force_download=*/false, &sites));
  ASSERT_THAT(sites.size(), Eq(1u));
  EXPECT_THAT(sites[0].url, URLEq("https://www.chromium.org/"));
}

TEST_F(PopularSitesTest, RefetchesAfterFallback) {
  SetCountryAndVersion("ZZ", "9");
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json");
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_DEFAULT_5.json",
      {kWikipedia});

  // First request falls back.
  std::vector<PopularSites::Site> sites;
  EXPECT_TRUE(FetchPopularSites(/*force_download=*/false, &sites));
  ASSERT_THAT(sites.size(), Eq(1u));
  EXPECT_THAT(sites[0].url, URLEq("https://zz.m.wikipedia.org/"));

  // Second request refetches ZZ_9, which now has data.
  RespondWithJSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_9.json",
      {kChromium});
  EXPECT_TRUE(FetchPopularSites(/*force_download=*/false, &sites));
  ASSERT_THAT(sites.size(), Eq(1u));
  EXPECT_THAT(sites[0].url, URLEq("https://www.chromium.org/"));
}

}  // namespace
}  // namespace ntp_tiles

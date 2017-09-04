// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/logo_tracker.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/search_provider_logos/switches.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace search_provider_logos {

namespace {

const int64_t kMaxDownloadBytes = 1024 * 1024;

// Returns whether the metadata for the cached logo indicates that the logo is
// OK to show, i.e. it's not expired or it's allowed to be shown temporarily
// after expiration.
bool IsLogoOkToShow(const LogoMetadata& metadata, base::Time now) {
  base::TimeDelta offset =
      base::TimeDelta::FromMilliseconds(kMaxTimeToLiveMS * 3 / 2);
  base::Time distant_past = now - offset;
  base::Time distant_future = now + offset;
  // Sanity check so logos aren't accidentally cached forever.
  if (metadata.expiration_time < distant_past ||
      metadata.expiration_time > distant_future) {
    return false;
  }
  return metadata.can_show_after_expiration || metadata.expiration_time >= now;
}

// Reads the logo from the cache and returns it. Returns NULL if the cache is
// empty, corrupt, expired, or doesn't apply to the current logo URL.
std::unique_ptr<EncodedLogo> GetLogoFromCacheOnFileThread(LogoCache* logo_cache,
                                                          const GURL& logo_url,
                                                          base::Time now) {
  const LogoMetadata* metadata = logo_cache->GetCachedLogoMetadata();
  if (!metadata)
    return nullptr;

  if (metadata->source_url != logo_url.spec() ||
      !IsLogoOkToShow(*metadata, now)) {
    logo_cache->SetCachedLogo(NULL);
    return nullptr;
  }

  return logo_cache->GetCachedLogo();
}

void DeleteLogoCacheOnFileThread(LogoCache* logo_cache) {
  delete logo_cache;
}

}  // namespace

LogoTracker::LogoTracker(
    base::FilePath cached_logo_directory,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    scoped_refptr<base::TaskRunner> background_task_runner,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    std::unique_ptr<LogoDelegate> delegate)
    : is_idle_(true),
      is_cached_logo_valid_(false),
      logo_delegate_(std::move(delegate)),
      logo_cache_(new LogoCache(cached_logo_directory)),
      clock_(new base::DefaultClock()),
      file_task_runner_(file_task_runner),
      background_task_runner_(background_task_runner),
      request_context_getter_(request_context_getter),
      weak_ptr_factory_(this) {}

LogoTracker::~LogoTracker() {
  ReturnToIdle(kDownloadOutcomeNotTracked);
  file_task_runner_->PostTask(
      FROM_HERE, base::Bind(&DeleteLogoCacheOnFileThread, logo_cache_));
  logo_cache_ = NULL;
}

void LogoTracker::SetServerAPI(
    const GURL& logo_url,
    const ParseLogoResponse& parse_logo_response_func,
    const AppendQueryparamsToLogoURL& append_queryparams_func,
    bool wants_cta,
    bool transparent) {
  if (logo_url == logo_url_)
    return;

  ReturnToIdle(kDownloadOutcomeNotTracked);

  logo_url_ = logo_url;
  parse_logo_response_func_ = parse_logo_response_func;
  append_queryparams_func_ = append_queryparams_func;
  wants_cta_ = wants_cta;
  transparent_ = transparent;
}

void LogoTracker::GetLogo(LogoObserver* observer) {
  DCHECK(!logo_url_.is_empty());
  logo_observers_.AddObserver(observer);

  if (is_idle_) {
    is_idle_ = false;
    base::PostTaskAndReplyWithResult(
        file_task_runner_.get(),
        FROM_HERE,
        base::Bind(&GetLogoFromCacheOnFileThread,
                   logo_cache_,
                   logo_url_,
                   clock_->Now()),
        base::Bind(&LogoTracker::OnCachedLogoRead,
                   weak_ptr_factory_.GetWeakPtr()));
  } else if (is_cached_logo_valid_) {
    observer->OnLogoAvailable(cached_logo_.get(), true);
  }
}

void LogoTracker::RemoveObserver(LogoObserver* observer) {
  logo_observers_.RemoveObserver(observer);
}

void LogoTracker::SetLogoCacheForTests(std::unique_ptr<LogoCache> cache) {
  DCHECK(cache);
  file_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DeleteLogoCacheOnFileThread, logo_cache_));
  logo_cache_ = cache.release();
}

void LogoTracker::SetClockForTests(std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

void LogoTracker::ReturnToIdle(int outcome) {
  if (outcome != kDownloadOutcomeNotTracked) {
    UMA_HISTOGRAM_ENUMERATION("NewTabPage.LogoDownloadOutcome", outcome,
                              DOWNLOAD_OUTCOME_COUNT);
  }
  // Cancel the current asynchronous operation, if any.
  fetcher_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Reset state.
  is_idle_ = true;
  cached_logo_.reset();
  is_cached_logo_valid_ = false;

  // Clear obsevers.
  for (auto& observer : logo_observers_)
    observer.OnObserverRemoved();
  logo_observers_.Clear();
}

void LogoTracker::OnCachedLogoRead(std::unique_ptr<EncodedLogo> cached_logo) {
  DCHECK(!is_idle_);

  if (cached_logo) {
    logo_delegate_->DecodeUntrustedImage(
        cached_logo->encoded_image,
        base::Bind(&LogoTracker::OnCachedLogoAvailable,
                   weak_ptr_factory_.GetWeakPtr(),
                   cached_logo->metadata));
  } else {
    OnCachedLogoAvailable(LogoMetadata(), SkBitmap());
  }
}

void LogoTracker::OnCachedLogoAvailable(const LogoMetadata& metadata,
                                        const SkBitmap& image) {
  DCHECK(!is_idle_);

  if (!image.isNull()) {
    cached_logo_.reset(new Logo());
    cached_logo_->metadata = metadata;
    cached_logo_->image = image;
  }
  is_cached_logo_valid_ = true;
  Logo* logo = cached_logo_.get();
  for (auto& observer : logo_observers_)
    observer.OnLogoAvailable(logo, true);
  FetchLogo();
}

void LogoTracker::SetCachedLogo(std::unique_ptr<EncodedLogo> logo) {
  file_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LogoCache::SetCachedLogo,
                 base::Unretained(logo_cache_),
                 base::Owned(logo.release())));
}

void LogoTracker::SetCachedMetadata(const LogoMetadata& metadata) {
  file_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&LogoCache::UpdateCachedLogoMetadata,
                                         base::Unretained(logo_cache_),
                                         metadata));
}

void LogoTracker::FetchLogo() {
  DCHECK(!fetcher_);
  DCHECK(!is_idle_);

  std::string fingerprint;
  if (cached_logo_ && !cached_logo_->metadata.fingerprint.empty() &&
      cached_logo_->metadata.expiration_time >= clock_->Now()) {
    fingerprint = cached_logo_->metadata.fingerprint;
  }
  GURL url;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kGoogleDoodleUrl)) {
    url = GURL(command_line->GetSwitchValueASCII(switches::kGoogleDoodleUrl));
  } else {
    url = append_queryparams_func_.Run(
        logo_url_, fingerprint, wants_cta_, transparent_);
  }

  fetcher_ = net::URLFetcher::Create(url, net::URLFetcher::GET, this);
  fetcher_->SetRequestContext(request_context_getter_.get());
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher_.get(),
      data_use_measurement::DataUseUserData::SEARCH_PROVIDER_LOGOS);
  fetcher_->Start();
  logo_download_start_time_ = base::TimeTicks::Now();
}

void LogoTracker::OnFreshLogoParsed(bool* parsing_failed,
                                    std::unique_ptr<EncodedLogo> logo) {
  DCHECK(!is_idle_);

  if (logo)
    logo->metadata.source_url = logo_url_.spec();

  if (!logo || !logo->encoded_image.get()) {
    OnFreshLogoAvailable(std::move(logo), *parsing_failed, SkBitmap());
  } else {
    // Store the value of logo->encoded_image for use below. This ensures that
    // logo->encoded_image is evaulated before base::Passed(&logo), which sets
    // logo to NULL.
    scoped_refptr<base::RefCountedString> encoded_image = logo->encoded_image;
    logo_delegate_->DecodeUntrustedImage(
        encoded_image,
        base::Bind(&LogoTracker::OnFreshLogoAvailable,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(&logo),
                   *parsing_failed));
  }
}

void LogoTracker::OnFreshLogoAvailable(
    std::unique_ptr<EncodedLogo> encoded_logo,
    bool parsing_failed,
    const SkBitmap& image) {
  DCHECK(!is_idle_);

  int download_outcome = kDownloadOutcomeNotTracked;

  if (encoded_logo && !encoded_logo->encoded_image.get() && cached_logo_ &&
      !encoded_logo->metadata.fingerprint.empty() &&
      encoded_logo->metadata.fingerprint ==
          cached_logo_->metadata.fingerprint) {
    // The cached logo was revalidated, i.e. its fingerprint was verified.
    // mime_type isn't sent when revalidating, so copy it from the cached logo.
    encoded_logo->metadata.mime_type = cached_logo_->metadata.mime_type;
    SetCachedMetadata(encoded_logo->metadata);
    download_outcome = DOWNLOAD_OUTCOME_LOGO_REVALIDATED;
  } else if (encoded_logo && image.isNull()) {
    // Image decoding failed. Do nothing.
    download_outcome = DOWNLOAD_OUTCOME_DECODING_FAILED;
  } else {
    std::unique_ptr<Logo> logo;
    // Check if the server returned a valid, non-empty response.
    if (encoded_logo) {
      DCHECK(!image.isNull());
      logo.reset(new Logo());
      logo->metadata = encoded_logo->metadata;
      logo->image = image;
    }

    if (logo) {
      download_outcome = DOWNLOAD_OUTCOME_NEW_LOGO_SUCCESS;
    } else {
      if (parsing_failed)
        download_outcome = DOWNLOAD_OUTCOME_PARSING_FAILED;
      else
        download_outcome = DOWNLOAD_OUTCOME_NO_LOGO_TODAY;
    }

    // Notify observers if a new logo was fetched, or if the new logo is NULL
    // but the cached logo was non-NULL.
    if (logo || cached_logo_) {
      for (auto& observer : logo_observers_)
        observer.OnLogoAvailable(logo.get(), false);
      SetCachedLogo(std::move(encoded_logo));
    }
  }

  DCHECK_NE(kDownloadOutcomeNotTracked, download_outcome);
  ReturnToIdle(download_outcome);
}

void LogoTracker::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(!is_idle_);
  std::unique_ptr<net::URLFetcher> cleanup_fetcher(fetcher_.release());

  if (!source->GetStatus().is_success()) {
    ReturnToIdle(DOWNLOAD_OUTCOME_DOWNLOAD_FAILED);
    return;
  }

  int response_code = source->GetResponseCode();
  if (response_code != net::HTTP_OK &&
      response_code != net::URLFetcher::RESPONSE_CODE_INVALID) {
    // RESPONSE_CODE_INVALID is returned when fetching from a file: URL
    // (for testing). In all other cases we would have had a non-success status.
    ReturnToIdle(DOWNLOAD_OUTCOME_DOWNLOAD_FAILED);
    return;
  }

  UMA_HISTOGRAM_TIMES("NewTabPage.LogoDownloadTime",
                      base::TimeTicks::Now() - logo_download_start_time_);

  std::unique_ptr<std::string> response(new std::string());
  source->GetResponseAsString(response.get());
  base::Time response_time = clock_->Now();

  bool* parsing_failed = new bool(false);
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::Bind(parse_logo_response_func_, base::Passed(&response),
                 response_time, parsing_failed),
      base::Bind(&LogoTracker::OnFreshLogoParsed,
                 weak_ptr_factory_.GetWeakPtr(), base::Owned(parsing_failed)));
}

void LogoTracker::OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                             int64_t current,
                                             int64_t total,
                                             int64_t current_network_bytes) {
  if (total > kMaxDownloadBytes || current > kMaxDownloadBytes) {
    LOG(WARNING) << "Search provider logo exceeded download size limit";
    ReturnToIdle(DOWNLOAD_OUTCOME_DOWNLOAD_FAILED);
  }
}

}  // namespace search_provider_logos

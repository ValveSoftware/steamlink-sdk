// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_URL_FETCHER_DOWNLOADER_H_
#define COMPONENTS_UPDATE_CLIENT_URL_FETCHER_DOWNLOADER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/update_client/crx_downloader.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace update_client {

// Implements a CRX downloader in top of the URLFetcher.
class UrlFetcherDownloader : public CrxDownloader,
                             public net::URLFetcherDelegate {
 protected:
  friend class CrxDownloader;
  UrlFetcherDownloader(
      std::unique_ptr<CrxDownloader> successor,
      net::URLRequestContextGetter* context_getter,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~UrlFetcherDownloader() override;

 private:
  // Overrides for CrxDownloader.
  void DoStartDownload(const GURL& url) override;

  // Overrides for URLFetcherDelegate.
  void OnURLFetchComplete(const net::URLFetcher* source) override;
  void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                  int64_t current,
                                  int64_t total) override;
  std::unique_ptr<net::URLFetcher> url_fetcher_;
  net::URLRequestContextGetter* context_getter_;

  base::Time download_start_time_;

  int64_t downloaded_bytes_;
  int64_t total_bytes_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(UrlFetcherDownloader);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_URL_FETCHER_DOWNLOADER_H_

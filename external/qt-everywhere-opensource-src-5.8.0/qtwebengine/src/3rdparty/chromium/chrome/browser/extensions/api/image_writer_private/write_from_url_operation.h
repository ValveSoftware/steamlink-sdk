// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_WRITE_FROM_URL_OPERATION_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_WRITE_FROM_URL_OPERATION_H_

#include <stdint.h>

#include "chrome/browser/extensions/api/image_writer_private/operation.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

namespace extensions {
namespace image_writer {

class OperationManager;

// Encapsulates a write of an image accessed via URL.
class WriteFromUrlOperation : public Operation, public net::URLFetcherDelegate {
 public:
  WriteFromUrlOperation(base::WeakPtr<OperationManager> manager,
                        const ExtensionId& extension_id,
                        net::URLRequestContextGetter* request_context,
                        GURL url,
                        const std::string& hash,
                        const std::string& storage_unit_id);
  void StartImpl() override;

 protected:
  ~WriteFromUrlOperation() override;

  // Sets the image_path to the correct location to download to.
  void GetDownloadTarget(const base::Closure& continuation);

  // Downloads the |url| to the currently configured |image_path|.  Should not
  // be called without calling |GetDownloadTarget| first.
  void Download(const base::Closure& continuation);

  // Verifies the download matches |hash|.  If the hash is empty, this stage is
  // skipped.
  void VerifyDownload(const base::Closure& continuation);

 private:
  // Destroys the URLFetcher.  The URLFetcher needs to be destroyed on the same
  // thread it was created on.  The Operation may be deleted on the UI thread
  // and so we must first delete the URLFetcher on the FILE thread.
  void DestroyUrlFetcher();

  // URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;
  void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                  int64_t current,
                                  int64_t total) override;
  void OnURLFetchUploadProgress(const net::URLFetcher* source,
                                int64_t current,
                                int64_t total) override;

  void VerifyDownloadCompare(const base::Closure& continuation,
                             const std::string& download_hash);
  void VerifyDownloadComplete(const base::Closure& continuation);

  // Arguments
  net::URLRequestContextGetter* request_context_;
  GURL url_;
  const std::string hash_;

  // Local state
  std::unique_ptr<net::URLFetcher> url_fetcher_;
  base::Closure download_continuation_;
};

} // namespace image_writer
} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_WRITE_FROM_URL_OPERATION_H_

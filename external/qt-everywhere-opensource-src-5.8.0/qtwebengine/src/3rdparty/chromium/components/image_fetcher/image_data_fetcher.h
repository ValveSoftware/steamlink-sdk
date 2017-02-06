// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_IMAGE_DATA_FETCHER_H_
#define COMPONENTS_IMAGE_FETCHER_IMAGE_DATA_FETCHER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

namespace image_fetcher {

class ImageDataFetcher : public net::URLFetcherDelegate {
 public:
  using ImageDataFetcherCallback =
      base::Callback<void(const std::string& image_data)>;

  explicit ImageDataFetcher(
      net::URLRequestContextGetter* url_request_context_getter);
  ~ImageDataFetcher() override;

  // Fetches the raw image bytes from the given |image_url| and calls the given
  // |callback|. The callback is run even if fetching the URL fails. In case
  // of an error an empty string is passed to the callback.
  void FetchImageData(const GURL& image_url,
                      const ImageDataFetcherCallback& callback);

 private:
  struct ImageDataFetcherRequest;

  // Method inherited from URLFetcherDelegate
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // All active image url requests.
  std::map<const net::URLFetcher*, std::unique_ptr<ImageDataFetcherRequest>>
      pending_requests_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // The next ID to use for a newly created URLFetcher. Each URLFetcher gets an
  // id when it is created. The |url_fetcher_id_| is incremented by one for each
  // newly created URLFetcher. The URLFetcher ID can be used during testing to
  // get individual URLFetchers and modify their state. Outside of tests this ID
  // is not used.
  int next_url_fetcher_id_;

  DISALLOW_COPY_AND_ASSIGN(ImageDataFetcher);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_IMAGE_DATA_FETCHER_H_

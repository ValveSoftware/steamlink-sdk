// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/url_fetcher.h"

#include <utility>

#include "base/logging.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace headless {

void URLFetcher::ResultListener::OnFetchCompleteExtractHeaders(
    const GURL& final_url,
    int http_response_code,
    const char* response_data,
    size_t response_data_size) {
  size_t read_offset = 0;
  int header_size =
      net::HttpUtil::LocateEndOfHeaders(response_data, response_data_size);
  scoped_refptr<net::HttpResponseHeaders> response_headers;

  if (header_size == -1) {
    LOG(WARNING) << "Can't find headers in result for url: " << final_url;
    response_headers = new net::HttpResponseHeaders("");
  } else {
    response_headers = new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(response_data, header_size));
    read_offset = header_size;
  }

  CHECK_LE(read_offset, response_data_size);
  OnFetchComplete(final_url, http_response_code, std::move(response_headers),
                  response_data + read_offset,
                  response_data_size - read_offset);
}

}  // namespace headless

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "url/gurl.h"

namespace autofill {

namespace {

// A URLFetcherResponseWriter that writes into a provided buffer.
class UnownedStringWriter : public net::URLFetcherResponseWriter {
 public:
  UnownedStringWriter(std::string* data) : data_(data) {}
  virtual ~UnownedStringWriter() {}

  virtual int Initialize(const net::CompletionCallback& callback) override {
    data_->clear();
    return net::OK;
  }

  virtual int Write(net::IOBuffer* buffer,
                    int num_bytes,
                    const net::CompletionCallback& callback) override {
    data_->append(buffer->data(), num_bytes);
    return num_bytes;
  }

  virtual int Finish(const net::CompletionCallback& callback) override {
    return net::OK;
  }

 private:
  std::string* data_;  // weak reference.

  DISALLOW_COPY_AND_ASSIGN(UnownedStringWriter);
};

}  // namespace

ChromeMetadataSource::ChromeMetadataSource(
    const std::string& validation_data_url,
    net::URLRequestContextGetter* getter)
    : validation_data_url_(validation_data_url),
      getter_(getter) {}

ChromeMetadataSource::~ChromeMetadataSource() {
  STLDeleteValues(&requests_);
}

void ChromeMetadataSource::Get(const std::string& key,
                               const Callback& downloaded) const {
  const_cast<ChromeMetadataSource*>(this)->Download(key, downloaded);
}

void ChromeMetadataSource::OnURLFetchComplete(const net::URLFetcher* source) {
  std::map<const net::URLFetcher*, Request*>::iterator request =
      requests_.find(source);
  DCHECK(request != requests_.end());

  bool ok = source->GetResponseCode() == net::HTTP_OK;
  std::unique_ptr<std::string> data(new std::string());
  if (ok)
    data->swap(request->second->data);
  request->second->callback(ok, request->second->key, data.release());

  delete request->second;
  requests_.erase(request);
}

ChromeMetadataSource::Request::Request(const std::string& key,
                                       std::unique_ptr<net::URLFetcher> fetcher,
                                       const Callback& callback)
    : key(key), fetcher(std::move(fetcher)), callback(callback) {}

void ChromeMetadataSource::Download(const std::string& key,
                                    const Callback& downloaded) {
  GURL resource(validation_data_url_ + key);
  if (!resource.SchemeIsCryptographic()) {
    downloaded(false, key, NULL);
    return;
  }

  std::unique_ptr<net::URLFetcher> fetcher =
      net::URLFetcher::Create(resource, net::URLFetcher::GET, this);
  fetcher->SetLoadFlags(
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher->SetRequestContext(getter_);

  Request* request = new Request(key, std::move(fetcher), downloaded);
  request->fetcher->SaveResponseWithWriter(
      std::unique_ptr<net::URLFetcherResponseWriter>(
          new UnownedStringWriter(&request->data)));
  requests_[request->fetcher.get()] = request;
  request->fetcher->Start();
}

}  // namespace autofill

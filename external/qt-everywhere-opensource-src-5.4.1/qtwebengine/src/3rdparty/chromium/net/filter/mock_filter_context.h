// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FILTER_MOCK_FILTER_CONTEXT_H_
#define NET_FILTER_MOCK_FILTER_CONTEXT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "net/filter/filter.h"
#include "url/gurl.h"

namespace net {

class URLRequestContext;

class MockFilterContext : public FilterContext {
 public:
  MockFilterContext();
  virtual ~MockFilterContext();

  void SetMimeType(const std::string& mime_type) { mime_type_ = mime_type; }
  void SetURL(const GURL& gurl) { gurl_ = gurl; }
  void SetContentDisposition(const std::string& disposition) {
    content_disposition_ = disposition;
  }
  void SetRequestTime(const base::Time time) { request_time_ = time; }
  void SetCached(bool is_cached) { is_cached_content_ = is_cached; }
  void SetDownload(bool is_download) { is_download_ = is_download; }
  void SetResponseCode(int response_code) { response_code_ = response_code; }
  void SetSdchResponse(bool is_sdch_response) {
    is_sdch_response_ = is_sdch_response;
  }
  URLRequestContext* GetModifiableURLRequestContext() const {
    return context_.get();
  }

  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;

  // What URL was used to access this data?
  // Return false if gurl is not present.
  virtual bool GetURL(GURL* gurl) const OVERRIDE;

  // What Content-Disposition did the server supply for this data?
  // Return false if Content-Disposition was not present.
  virtual bool GetContentDisposition(std::string* disposition) const OVERRIDE;

  // What was this data requested from a server?
  virtual base::Time GetRequestTime() const OVERRIDE;

  // Is data supplied from cache, or fresh across the net?
  virtual bool IsCachedContent() const OVERRIDE;

  // Is this a download?
  virtual bool IsDownload() const OVERRIDE;

  // Was this data flagged as a response to a request with an SDCH dictionary?
  virtual bool IsSdchResponse() const OVERRIDE;

  // How many bytes were fed to filter(s) so far?
  virtual int64 GetByteReadCount() const OVERRIDE;

  virtual int GetResponseCode() const OVERRIDE;

  // The URLRequestContext associated with the request.
  virtual const URLRequestContext* GetURLRequestContext() const OVERRIDE;

  virtual void RecordPacketStats(StatisticSelector statistic) const OVERRIDE {}

 private:
  int buffer_size_;
  std::string mime_type_;
  std::string content_disposition_;
  GURL gurl_;
  base::Time request_time_;
  bool is_cached_content_;
  bool is_download_;
  bool is_sdch_response_;
  int response_code_;
  scoped_ptr<URLRequestContext> context_;

  DISALLOW_COPY_AND_ASSIGN(MockFilterContext);
};

}  // namespace net

#endif  // NET_FILTER_MOCK_FILTER_CONTEXT_H_

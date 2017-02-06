// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/document_loader.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "net/http/http_util.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"

namespace chrome_pdf {

namespace {

// If the headers have a byte-range response, writes the start and end
// positions and returns true if at least the start position was parsed.
// The end position will be set to 0 if it was not found or parsed from the
// response.
// Returns false if not even a start position could be parsed.
bool GetByteRange(const std::string& headers, uint32_t* start, uint32_t* end) {
  net::HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\n");
  while (it.GetNext()) {
    if (base::LowerCaseEqualsASCII(it.name(), "content-range")) {
      std::string range = it.values().c_str();
      if (base::StartsWith(range, "bytes",
                           base::CompareCase::INSENSITIVE_ASCII)) {
        range = range.substr(strlen("bytes"));
        std::string::size_type pos = range.find('-');
        std::string range_end;
        if (pos != std::string::npos)
          range_end = range.substr(pos + 1);
        base::TrimWhitespaceASCII(range, base::TRIM_LEADING, &range);
        base::TrimWhitespaceASCII(range_end, base::TRIM_LEADING, &range_end);
        *start = atoi(range.c_str());
        *end = atoi(range_end.c_str());
        return true;
      }
    }
  }
  return false;
}

// If the headers have a multi-part response, returns the boundary name.
// Otherwise returns an empty string.
std::string GetMultiPartBoundary(const std::string& headers) {
  net::HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\n");
  while (it.GetNext()) {
    if (base::LowerCaseEqualsASCII(it.name(), "content-type")) {
      std::string type = base::ToLowerASCII(it.values());
      if (base::StartsWith(type, "multipart/", base::CompareCase::SENSITIVE)) {
        const char* boundary = strstr(type.c_str(), "boundary=");
        if (!boundary) {
          NOTREACHED();
          break;
        }

        return std::string(boundary + 9);
      }
    }
  }
  return std::string();
}

bool IsValidContentType(const std::string& type) {
  return (base::EndsWith(type, "/pdf", base::CompareCase::INSENSITIVE_ASCII) ||
          base::EndsWith(type, ".pdf", base::CompareCase::INSENSITIVE_ASCII) ||
          base::EndsWith(type, "/x-pdf",
                         base::CompareCase::INSENSITIVE_ASCII) ||
          base::EndsWith(type, "/*", base::CompareCase::INSENSITIVE_ASCII) ||
          base::EndsWith(type, "/acrobat",
                         base::CompareCase::INSENSITIVE_ASCII) ||
          base::EndsWith(type, "/unknown",
                         base::CompareCase::INSENSITIVE_ASCII));
}

}  // namespace

DocumentLoader::Client::~Client() {
}

DocumentLoader::DocumentLoader(Client* client)
    : client_(client), partial_document_(false), request_pending_(false),
      current_pos_(0), current_chunk_size_(0), current_chunk_read_(0),
      document_size_(0), header_request_(true), is_multipart_(false) {
  loader_factory_.Initialize(this);
}

DocumentLoader::~DocumentLoader() {
}

bool DocumentLoader::Init(const pp::URLLoader& loader,
                          const std::string& url,
                          const std::string& headers) {
  DCHECK(url_.empty());
  url_ = url;
  loader_ = loader;

  std::string response_headers;
  if (!headers.empty()) {
    response_headers = headers;
  } else {
    pp::URLResponseInfo response = loader_.GetResponseInfo();
    pp::Var headers_var = response.GetHeaders();

    if (headers_var.is_string()) {
      response_headers = headers_var.AsString();
    }
  }

  bool accept_ranges_bytes = false;
  bool content_encoded = false;
  uint32_t content_length = 0;
  std::string type;
  std::string disposition;

  // This happens for PDFs not loaded from http(s) sources.
  if (response_headers == "Content-Type: text/plain") {
    if (!base::StartsWith(url, "http://",
                          base::CompareCase::INSENSITIVE_ASCII) &&
        !base::StartsWith(url, "https://",
                          base::CompareCase::INSENSITIVE_ASCII)) {
      type = "application/pdf";
    }
  }
  if (type.empty() && !response_headers.empty()) {
    net::HttpUtil::HeadersIterator it(response_headers.begin(),
                                      response_headers.end(), "\n");
    while (it.GetNext()) {
      if (base::LowerCaseEqualsASCII(it.name(), "content-length")) {
        content_length = atoi(it.values().c_str());
      } else if (base::LowerCaseEqualsASCII(it.name(), "accept-ranges")) {
        accept_ranges_bytes = base::LowerCaseEqualsASCII(it.values(), "bytes");
      } else if (base::LowerCaseEqualsASCII(it.name(), "content-encoding")) {
        content_encoded = true;
      } else if (base::LowerCaseEqualsASCII(it.name(), "content-type")) {
        type = it.values();
        size_t semi_colon_pos = type.find(';');
        if (semi_colon_pos != std::string::npos) {
          type = type.substr(0, semi_colon_pos);
        }
        TrimWhitespaceASCII(type, base::TRIM_ALL, &type);
      } else if (base::LowerCaseEqualsASCII(it.name(), "content-disposition")) {
        disposition = it.values();
      }
    }
  }
  if (!type.empty() && !IsValidContentType(type))
    return false;
  if (base::StartsWith(disposition, "attachment",
                       base::CompareCase::INSENSITIVE_ASCII))
    return false;

  if (content_length > 0)
    chunk_stream_.Preallocate(content_length);

  document_size_ = content_length;
  requests_count_ = 0;

  // Enable partial loading only if file size is above the threshold.
  // It will allow avoiding latency for multiple requests.
  if (content_length > kMinFileSize &&
      accept_ranges_bytes &&
      !content_encoded) {
    LoadPartialDocument();
  } else {
    LoadFullDocument();
  }
  return true;
}

void DocumentLoader::LoadPartialDocument() {
  // The current request is a full request (not a range request) so it starts at
  // 0 and ends at |document_size_|.
  current_chunk_size_ = document_size_;
  current_pos_ = 0;
  current_request_offset_ = 0;
  current_request_size_ = 0;
  current_request_extended_size_ = document_size_;
  request_pending_ = true;

  partial_document_ = true;
  header_request_ = true;
  ReadMore();
}

void DocumentLoader::LoadFullDocument() {
  partial_document_ = false;
  chunk_buffer_.clear();
  ReadMore();
}

bool DocumentLoader::IsDocumentComplete() const {
  if (document_size_ == 0)  // Document size unknown.
    return false;
  return IsDataAvailable(0, document_size_);
}

uint32_t DocumentLoader::GetAvailableData() const {
  if (document_size_ == 0) {  // If document size is unknown.
    return current_pos_;
  }

  std::vector<std::pair<size_t, size_t> > ranges;
  chunk_stream_.GetMissedRanges(0, document_size_, &ranges);
  uint32_t available = document_size_;
  for (const auto& range : ranges)
    available -= range.second;
  return available;
}

void DocumentLoader::ClearPendingRequests() {
  pending_requests_.erase(pending_requests_.begin(),
                          pending_requests_.end());
}

bool DocumentLoader::GetBlock(uint32_t position,
                              uint32_t size,
                              void* buf) const {
  return chunk_stream_.ReadData(position, size, buf);
}

bool DocumentLoader::IsDataAvailable(uint32_t position, uint32_t size) const {
  return chunk_stream_.IsRangeAvailable(position, size);
}

void DocumentLoader::RequestData(uint32_t position, uint32_t size) {
  DCHECK(partial_document_);

  // We have some artefact request from
  // PDFiumEngine::OnDocumentComplete() -> FPDFAvail_IsPageAvail after
  // document is complete.
  // We need this fix in PDFIum. Adding this as a work around.
  // Bug: http://code.google.com/p/chromium/issues/detail?id=79996
  // Test url:
  // http://www.icann.org/en/correspondence/holtzman-to-jeffrey-02mar11-en.pdf
  if (IsDocumentComplete())
    return;

  pending_requests_.push_back(std::pair<size_t, size_t>(position, size));
  DownloadPendingRequests();
}

void DocumentLoader::RemoveCompletedRanges() {
  // Split every request that has been partially downloaded already into smaller
  // requests.
  std::vector<std::pair<size_t, size_t> > ranges;
  auto it = pending_requests_.begin();
  while (it != pending_requests_.end()) {
    chunk_stream_.GetMissedRanges(it->first, it->second, &ranges);
    pending_requests_.insert(it, ranges.begin(), ranges.end());
    ranges.clear();
    pending_requests_.erase(it++);
  }
}

void DocumentLoader::DownloadPendingRequests() {
  if (request_pending_)
    return;

  uint32_t pos;
  uint32_t size;
  if (pending_requests_.empty()) {
    // If the document is not complete and we have no outstanding requests,
    // download what's left for as long as no other request gets added to
    // |pending_requests_|.
    pos = chunk_stream_.GetFirstMissingByte();
    if (pos >= document_size_) {
      // We're done downloading the document.
      return;
    }
    // Start with size 0, we'll set |current_request_extended_size_| to > 0.
    // This way this request will get cancelled as soon as the renderer wants
    // another portion of the document.
    size = 0;
  } else {
    RemoveCompletedRanges();

    pos = pending_requests_.front().first;
    size = pending_requests_.front().second;
    if (IsDataAvailable(pos, size)) {
      ReadComplete();
      return;
    }
  }

  size_t last_byte_before = chunk_stream_.GetFirstMissingByteInInterval(pos);
  if (size < kDefaultRequestSize) {
    // Try to extend before pos, up to size |kDefaultRequestSize|.
    if (pos + size - last_byte_before > kDefaultRequestSize) {
      pos += size - kDefaultRequestSize;
      size = kDefaultRequestSize;
    } else {
      size += pos - last_byte_before;
      pos = last_byte_before;
    }
  }
  if (pos - last_byte_before < kDefaultRequestSize) {
    // Don't leave a gap smaller than |kDefaultRequestSize|.
    size += pos - last_byte_before;
    pos = last_byte_before;
  }

  current_request_offset_ = pos;
  current_request_size_ = size;

  // Extend the request until the next downloaded byte or the end of the
  // document.
  size_t last_missing_byte =
      chunk_stream_.GetLastMissingByteInInterval(pos + size - 1);
  current_request_extended_size_ = last_missing_byte - pos + 1;

  request_pending_ = true;

  // Start downloading first pending request.
  loader_.Close();
  loader_ = client_->CreateURLLoader();
  pp::CompletionCallback callback =
      loader_factory_.NewCallback(&DocumentLoader::DidOpen);
  pp::URLRequestInfo request = GetRequest(pos, current_request_extended_size_);
  requests_count_++;
  int rv = loader_.Open(request, callback);
  if (rv != PP_OK_COMPLETIONPENDING)
    callback.Run(rv);
}

pp::URLRequestInfo DocumentLoader::GetRequest(uint32_t position,
                                              uint32_t size) const {
  pp::URLRequestInfo request(client_->GetPluginInstance());
  request.SetURL(url_);
  request.SetMethod("GET");
  request.SetFollowRedirects(false);
  request.SetCustomReferrerURL(url_);

  const size_t kBufSize = 100;
  char buf[kBufSize];
  // According to rfc2616, byte range specifies position of the first and last
  // bytes in the requested range inclusively. Therefore we should subtract 1
  // from the position + size, to get index of the last byte that needs to be
  // downloaded.
  base::snprintf(buf, kBufSize, "Range: bytes=%d-%d", position,
                 position + size - 1);
  pp::Var header(buf);
  request.SetHeaders(header);

  return request;
}

void DocumentLoader::DidOpen(int32_t result) {
  if (result != PP_OK) {
    NOTREACHED();
    return;
  }

  int32_t http_code = loader_.GetResponseInfo().GetStatusCode();
  if (http_code >= 400 && http_code < 500) {
    // Error accessing resource. 4xx error indicate subsequent requests
    // will fail too.
    // E.g. resource has been removed from the server while loading it.
    // https://code.google.com/p/chromium/issues/detail?id=414827
    return;
  }

  is_multipart_ = false;
  current_chunk_size_ = 0;
  current_chunk_read_ = 0;

  pp::Var headers_var = loader_.GetResponseInfo().GetHeaders();
  std::string headers;
  if (headers_var.is_string())
    headers = headers_var.AsString();

  std::string boundary = GetMultiPartBoundary(headers);
  if (!boundary.empty()) {
    // Leave position untouched for now, when we read the data we'll get it.
    is_multipart_ = true;
    multipart_boundary_ = boundary;
  } else {
    // Need to make sure that the server returned a byte-range, since it's
    // possible for a server to just ignore our byte-range request and just
    // return the entire document even if it supports byte-range requests.
    // i.e. sniff response to
    // http://www.act.org/compass/sample/pdf/geometry.pdf
    current_pos_ = 0;
    uint32_t start_pos, end_pos;
    if (GetByteRange(headers, &start_pos, &end_pos)) {
      current_pos_ = start_pos;
      if (end_pos && end_pos > start_pos)
        current_chunk_size_ = end_pos - start_pos + 1;
    } else {
      partial_document_ = false;
    }
  }

  ReadMore();
}

void DocumentLoader::ReadMore() {
  pp::CompletionCallback callback =
        loader_factory_.NewCallback(&DocumentLoader::DidRead);
  int rv = loader_.ReadResponseBody(buffer_, sizeof(buffer_), callback);
  if (rv != PP_OK_COMPLETIONPENDING)
    callback.Run(rv);
}

void DocumentLoader::DidRead(int32_t result) {
  if (result <= 0) {
    // If |result| == PP_OK, the document was loaded, otherwise an error was
    // encountered. Either way we want to stop processing the response. In the
    // case where an error occurred, the renderer will detect that we're missing
    // data and will display a message.
    ReadComplete();
    return;
  }

  char* start = buffer_;
  size_t length = result;
  if (is_multipart_ && result > 2) {
    for (int i = 2; i < result; ++i) {
      if ((buffer_[i - 1] == '\n' && buffer_[i - 2] == '\n') ||
          (i >= 4 && buffer_[i - 1] == '\n' && buffer_[i - 2] == '\r' &&
           buffer_[i - 3] == '\n' && buffer_[i - 4] == '\r')) {
        uint32_t start_pos, end_pos;
        if (GetByteRange(std::string(buffer_, i), &start_pos, &end_pos)) {
          current_pos_ = start_pos;
          start += i;
          length -= i;
          if (end_pos && end_pos > start_pos)
            current_chunk_size_ = end_pos - start_pos + 1;
        }
        break;
      }
    }

    // Reset this flag so we don't look inside the buffer in future calls of
    // DidRead for this response.  Note that this code DOES NOT handle multi-
    // part responses with more than one part (we don't issue them at the
    // moment, so they shouldn't arrive).
    is_multipart_ = false;
  }

  if (current_chunk_size_ && current_chunk_read_ + length > current_chunk_size_)
    length = current_chunk_size_ - current_chunk_read_;

  if (length) {
    if (document_size_ > 0) {
      chunk_stream_.WriteData(current_pos_, start, length);
    } else {
      // If we did not get content-length in the response, we can't
      // preallocate buffer for the entire document. Resizing array causing
      // memory fragmentation issues on the large files and OOM exceptions.
      // To fix this, we collect all chunks of the file to the list and
      // concatenate them together after request is complete.
      std::vector<unsigned char> buf(length);
      memcpy(buf.data(), start, length);
      chunk_buffer_.push_back(std::move(buf));
    }
    current_pos_ += length;
    current_chunk_read_ += length;
    client_->OnNewDataAvailable();
  }

  // Only call the renderer if we allow partial loading.
  if (!partial_document_) {
    ReadMore();
    return;
  }

  UpdateRendering();
  RemoveCompletedRanges();

  if (!pending_requests_.empty()) {
    // If there are pending requests and the current content we're downloading
    // doesn't satisfy any of these requests, cancel the current request to
    // fullfill those more important requests.
    bool satisfying_pending_request =
        SatisfyingRequest(current_request_offset_, current_request_size_);
    for (const auto& pending_request : pending_requests_) {
      if (SatisfyingRequest(pending_request.first, pending_request.second)) {
        satisfying_pending_request = true;
        break;
      }
    }
    // Cancel the request as it's not satisfying any request from the
    // renderer, unless the current request is finished in which case we let
    // it finish cleanly.
    if (!satisfying_pending_request &&
        current_pos_ <
            current_request_offset_ + current_request_extended_size_) {
      loader_.Close();
    }
  }

  ReadMore();
}

bool DocumentLoader::SatisfyingRequest(size_t offset, size_t size) const {
  return offset <= current_pos_ + kDefaultRequestSize &&
      current_pos_ < offset + size;
}

void DocumentLoader::ReadComplete() {
  if (!partial_document_) {
    if (document_size_ == 0) {
      // For the document with no 'content-length" specified we've collected all
      // the chunks already. Let's allocate final document buffer and copy them
      // over.
      chunk_stream_.Preallocate(current_pos_);
      uint32_t pos = 0;
      for (auto& chunk : chunk_buffer_) {
        chunk_stream_.WriteData(pos, chunk.data(), chunk.size());
        pos += chunk.size();
      }
      chunk_buffer_.clear();
    }
    document_size_ = current_pos_;
    client_->OnDocumentComplete();
    return;
  }

  request_pending_ = false;

  if (IsDocumentComplete()) {
    client_->OnDocumentComplete();
    return;
  }

  UpdateRendering();
  DownloadPendingRequests();
}

void DocumentLoader::UpdateRendering() {
  if (header_request_)
    client_->OnPartialDocumentLoaded();
  else
    client_->OnPendingRequestComplete();
  header_request_ = false;
}

}  // namespace chrome_pdf

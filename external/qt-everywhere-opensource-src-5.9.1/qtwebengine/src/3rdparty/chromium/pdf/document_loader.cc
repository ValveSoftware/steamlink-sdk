// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/document_loader.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_util.h"
#include "pdf/url_loader_wrapper.h"
#include "ppapi/c/pp_errors.h"
#include "ui/gfx/range/range.h"

namespace chrome_pdf {

namespace {

// The distance from last received chunk, when we wait requesting data, using
// current connection (like playing a cassette tape) and do not send new range
// request (like rewind a cassette tape, and continue playing after).
// Experimentally chosen value.
const int kChunkCloseDistance = 10;

// Return true if the HTTP response of |loader| is a successful one and loading
// should continue. 4xx error indicate subsequent requests will fail too.
// e.g. resource has been removed from the server while loading it. 301
// indicates a redirect was returned which won't be successful because we
// disable following redirects for PDF loading (we assume they are already
// resolved by the browser.
bool ResponseStatusSuccess(const URLLoaderWrapper* loader) {
  int32_t http_code = loader->GetStatusCode();
  return (http_code < 400 && http_code != 301) || http_code >= 500;
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

DocumentLoader::Chunk::Chunk() {}

DocumentLoader::Chunk::~Chunk() {}

void DocumentLoader::Chunk::Clear() {
  chunk_index = 0;
  data_size = 0;
  chunk_data.reset();
}

DocumentLoader::DocumentLoader(Client* client)
    : client_(client), loader_factory_(this) {}

DocumentLoader::~DocumentLoader() {
}

bool DocumentLoader::Init(std::unique_ptr<URLLoaderWrapper> loader,
                          const std::string& url) {
  DCHECK(url_.empty());
  DCHECK(!loader_);

  // Check that the initial response status is a valid one.
  if (!ResponseStatusSuccess(loader.get()))
    return false;

  std::string type = loader->GetContentType();

  // This happens for PDFs not loaded from http(s) sources.
  if (type == "text/plain") {
    if (!base::StartsWith(url, "http://",
                          base::CompareCase::INSENSITIVE_ASCII) &&
        !base::StartsWith(url, "https://",
                          base::CompareCase::INSENSITIVE_ASCII)) {
      type = "application/pdf";
    }
  }
  if (!type.empty() && !IsValidContentType(type))
    return false;

  if (base::StartsWith(loader->GetContentDisposition(), "attachment",
                       base::CompareCase::INSENSITIVE_ASCII))
    return false;

  url_ = url;
  loader_ = std::move(loader);

  if (!loader_->IsContentEncoded()) {
    chunk_stream_.set_eof_pos(std::max(0, loader_->GetContentLength()));
  }
  int64_t bytes_received = 0;
  int64_t total_bytes_to_be_received = 0;
  if (!chunk_stream_.eof_pos() &&
      loader_->GetDownloadProgress(&bytes_received,
                                   &total_bytes_to_be_received)) {
    chunk_stream_.set_eof_pos(
        std::max(0, static_cast<int>(total_bytes_to_be_received)));
  }

  SetPartialLoadingEnabled(
      partial_loading_enabled_ &&
      !base::StartsWith(url, "file://", base::CompareCase::INSENSITIVE_ASCII) &&
      loader_->IsAcceptRangesBytes() && !loader_->IsContentEncoded() &&
      GetDocumentSize());

  ReadMore();
  return true;
}

bool DocumentLoader::IsDocumentComplete() const {
  return chunk_stream_.IsComplete();
}

uint32_t DocumentLoader::GetDocumentSize() const {
  return chunk_stream_.eof_pos();
}

void DocumentLoader::ClearPendingRequests() {
  pending_requests_.Clear();
}

bool DocumentLoader::GetBlock(uint32_t position,
                              uint32_t size,
                              void* buf) const {
  base::CheckedNumeric<uint32_t> addition_result = position;
  addition_result += size;
  if (!addition_result.IsValid())
    return false;
  return chunk_stream_.ReadData(
      gfx::Range(position, addition_result.ValueOrDie()), buf);
}

bool DocumentLoader::IsDataAvailable(uint32_t position, uint32_t size) const {
  base::CheckedNumeric<uint32_t> addition_result = position;
  addition_result += size;
  if (!addition_result.IsValid())
    return false;
  return chunk_stream_.IsRangeAvailable(
      gfx::Range(position, addition_result.ValueOrDie()));
}

void DocumentLoader::RequestData(uint32_t position, uint32_t size) {
  if (!size || IsDataAvailable(position, size)) {
    return;
  }
  {
    // Check integer overflow.
    base::CheckedNumeric<uint32_t> addition_result = position;
    addition_result += size;
    if (!addition_result.IsValid())
      return;
  }

  if (GetDocumentSize() && (position + size > GetDocumentSize())) {
    return;
  }

  // We have some artefact request from
  // PDFiumEngine::OnDocumentComplete() -> FPDFAvail_IsPageAvail after
  // document is complete.
  // We need this fix in PDFIum. Adding this as a work around.
  // Bug: http://code.google.com/p/chromium/issues/detail?id=79996
  // Test url:
  // http://www.icann.org/en/correspondence/holtzman-to-jeffrey-02mar11-en.pdf
  if (!loader_)
    return;

  RangeSet requested_chunks(chunk_stream_.GetChunksRange(position, size));
  requested_chunks.Subtract(chunk_stream_.filled_chunks());
  if (requested_chunks.IsEmpty()) {
    NOTREACHED();
    return;
  }
  pending_requests_.Union(requested_chunks);
}

void DocumentLoader::SetPartialLoadingEnabled(bool enabled) {
  partial_loading_enabled_ = enabled;
  if (!enabled) {
    is_partial_loader_active_ = false;
  }
}

bool DocumentLoader::ShouldCancelLoading() const {
  if (!loader_)
    return true;
  if (!partial_loading_enabled_ || pending_requests_.IsEmpty())
    return false;
  const gfx::Range current_range(chunk_.chunk_index,
                                 chunk_.chunk_index + kChunkCloseDistance);
  return !pending_requests_.Intersects(current_range);
}

void DocumentLoader::ContinueDownload() {
  if (!ShouldCancelLoading())
    return ReadMore();
  DCHECK(partial_loading_enabled_);
  DCHECK(!IsDocumentComplete());
  DCHECK(GetDocumentSize());

  const uint32_t range_start =
      pending_requests_.IsEmpty() ? 0 : pending_requests_.First().start();
  RangeSet candidates_for_request(
      gfx::Range(range_start, chunk_stream_.total_chunks_count()));
  candidates_for_request.Subtract(chunk_stream_.filled_chunks());
  DCHECK(!candidates_for_request.IsEmpty());
  gfx::Range next_request = candidates_for_request.First();
  if (candidates_for_request.Size() == 1 &&
      next_request.length() < kChunkCloseDistance) {
    // We have only request at the end, try to enlarge it to improve back order
    // reading.
    const int additional_chunks_count =
        kChunkCloseDistance - next_request.length();
    int new_start = std::max(
        0, static_cast<int>(next_request.start()) - additional_chunks_count);
    candidates_for_request =
        RangeSet(gfx::Range(new_start, next_request.end()));
    candidates_for_request.Subtract(chunk_stream_.filled_chunks());
    next_request = candidates_for_request.Last();
  }

  loader_.reset();
  chunk_.Clear();
  if (!is_partial_loader_active_) {
    client_->CancelBrowserDownload();
    is_partial_loader_active_ = true;
  }

  const uint32_t start = next_request.start() * DataStream::kChunkSize;
  const uint32_t length =
      std::min(chunk_stream_.eof_pos() - start,
               next_request.length() * DataStream::kChunkSize);

  loader_ = client_->CreateURLLoader();

  loader_->OpenRange(
      url_, url_, start, length,
      loader_factory_.NewCallback(&DocumentLoader::DidOpenPartial));
}

void DocumentLoader::DidOpenPartial(int32_t result) {
  if (result != PP_OK) {
    return ReadComplete();
  }

  if (!ResponseStatusSuccess(loader_.get()))
    return ReadComplete();

  // Leave position untouched for multiparted responce for now, when we read the
  // data we'll get it.
  if (!loader_->IsMultipart()) {
    // Need to make sure that the server returned a byte-range, since it's
    // possible for a server to just ignore our byte-range request and just
    // return the entire document even if it supports byte-range requests.
    // i.e. sniff response to
    // http://www.act.org/compass/sample/pdf/geometry.pdf
    int start_pos = 0;
    int end_pos = 0;
    if (loader_->GetByteRange(&start_pos, &end_pos)) {
      if (start_pos % DataStream::kChunkSize != 0) {
        return ReadComplete();
      }
      DCHECK(!chunk_.chunk_data);
      chunk_.chunk_index = chunk_stream_.GetChunkIndex(start_pos);
    } else {
      SetPartialLoadingEnabled(false);
    }
    return ContinueDownload();
  }
  // Needs more data to calc chunk index.
  return ReadMore();
}

void DocumentLoader::ReadMore() {
  loader_->ReadResponseBody(
      buffer_, sizeof(buffer_),
      loader_factory_.NewCallback(&DocumentLoader::DidRead));
}

void DocumentLoader::DidRead(int32_t result) {
  if (result < 0) {
    // An error occurred.
    // The renderer will detect that we're missing data and will display a
    // message.
    return ReadComplete();
  }
  if (result == 0) {
    loader_.reset();
    if (!is_partial_loader_active_)
      return ReadComplete();
    return ContinueDownload();
  }
  if (loader_->IsMultipart()) {
    int start_pos = 0;
    int end_pos = 0;
    if (!loader_->GetByteRange(&start_pos, &end_pos)) {
      return ReadComplete();
    }
    DCHECK(!chunk_.chunk_data);
    chunk_.chunk_index = chunk_stream_.GetChunkIndex(start_pos);
  }
  if (!SaveChunkData(buffer_, result)) {
    return ReadMore();
  }
  if (IsDocumentComplete()) {
    return ReadComplete();
  }
  return ContinueDownload();
}

bool DocumentLoader::SaveChunkData(char* input, uint32_t input_size) {
  count_of_bytes_received_ += input_size;
  bool chunk_saved = false;
  bool loading_pending_request = pending_requests_.Contains(chunk_.chunk_index);
  while (input_size > 0) {
    if (chunk_.data_size == 0) {
      chunk_.chunk_data = base::MakeUnique<DataStream::ChunkData>();
    }
    const uint32_t new_chunk_data_len =
        std::min(DataStream::kChunkSize - chunk_.data_size, input_size);
    memcpy(chunk_.chunk_data->data() + chunk_.data_size, input,
           new_chunk_data_len);
    chunk_.data_size += new_chunk_data_len;
    if (chunk_.data_size == DataStream::kChunkSize ||
        chunk_stream_.eof_pos() ==
            chunk_.chunk_index * DataStream::kChunkSize + chunk_.data_size) {
      chunk_stream_.SetChunkData(chunk_.chunk_index,
                                 std::move(chunk_.chunk_data));
      pending_requests_.Subtract(
          gfx::Range(chunk_.chunk_index, chunk_.chunk_index + 1));
      chunk_.data_size = 0;
      ++(chunk_.chunk_index);
      chunk_saved = true;
    }

    input += new_chunk_data_len;
    input_size -= new_chunk_data_len;
  }

  if (IsDocumentComplete())
    return true;

  if (!chunk_saved)
    return false;

  if (loading_pending_request &&
      !pending_requests_.Contains(chunk_.chunk_index)) {
    client_->OnPendingRequestComplete();
  }
  client_->OnNewDataAvailable();
  return true;
}

void DocumentLoader::ReadComplete() {
  if (!GetDocumentSize()) {
    uint32_t eof =
        chunk_.chunk_index * DataStream::kChunkSize + chunk_.data_size;
    if (!chunk_stream_.filled_chunks().IsEmpty()) {
      eof = std::max(
          chunk_stream_.filled_chunks().Last().end() * DataStream::kChunkSize,
          eof);
    }
    chunk_stream_.set_eof_pos(eof);
    if (eof == chunk_.chunk_index * DataStream::kChunkSize + chunk_.data_size) {
      chunk_stream_.SetChunkData(chunk_.chunk_index,
                                 std::move(chunk_.chunk_data));
    }
  }
  loader_.reset();
  if (IsDocumentComplete()) {
    client_->OnDocumentComplete();
  } else {
    client_->OnDocumentCanceled();
  }
}

float DocumentLoader::GetProgress() const {
  if (!GetDocumentSize())
    return -1;
  if (IsDocumentComplete())
    return 1;
  return static_cast<float>(chunk_stream_.filled_chunks_count()) /
         chunk_stream_.total_chunks_count();
}

}  // namespace chrome_pdf

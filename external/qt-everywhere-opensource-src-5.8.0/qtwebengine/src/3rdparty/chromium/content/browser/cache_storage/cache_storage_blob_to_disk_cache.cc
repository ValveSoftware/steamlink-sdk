// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_blob_to_disk_cache.h"

#include <utility>

#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"

namespace content {

const int CacheStorageBlobToDiskCache::kBufferSize = 1024 * 512;

CacheStorageBlobToDiskCache::CacheStorageBlobToDiskCache()
    : cache_entry_offset_(0),
      disk_cache_body_index_(0),
      buffer_(new net::IOBufferWithSize(kBufferSize)),
      weak_ptr_factory_(this) {
}

CacheStorageBlobToDiskCache::~CacheStorageBlobToDiskCache() {
  if (blob_request_)
    request_context_getter_->RemoveObserver(this);
}

void CacheStorageBlobToDiskCache::StreamBlobToCache(
    disk_cache::ScopedEntryPtr entry,
    int disk_cache_body_index,
    net::URLRequestContextGetter* request_context_getter,
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle,
    const EntryAndBoolCallback& callback) {
  DCHECK(entry);
  DCHECK_LE(0, disk_cache_body_index);
  DCHECK(blob_data_handle);
  DCHECK(!blob_request_);
  DCHECK(request_context_getter);

  if (!request_context_getter->GetURLRequestContext()) {
    callback.Run(std::move(entry), false /* success */);
    return;
  }

  disk_cache_body_index_ = disk_cache_body_index;

  entry_ = std::move(entry);
  callback_ = callback;
  request_context_getter_ = request_context_getter;

  blob_request_ = storage::BlobProtocolHandler::CreateBlobRequest(
      std::move(blob_data_handle),
      request_context_getter->GetURLRequestContext(), this);
  request_context_getter_->AddObserver(this);
  blob_request_->Start();
}

void CacheStorageBlobToDiskCache::OnResponseStarted(net::URLRequest* request) {
  if (!request->status().is_success()) {
    RunCallbackAndRemoveObserver(false);
    return;
  }

  ReadFromBlob();
}

void CacheStorageBlobToDiskCache::OnReadCompleted(net::URLRequest* request,
                                                  int bytes_read) {
  if (!request->status().is_success()) {
    RunCallbackAndRemoveObserver(false);
    return;
  }

  if (bytes_read == 0) {
    RunCallbackAndRemoveObserver(true);
    return;
  }

  net::CompletionCallback cache_write_callback =
      base::Bind(&CacheStorageBlobToDiskCache::DidWriteDataToEntry,
                 weak_ptr_factory_.GetWeakPtr(), bytes_read);

  int rv = entry_->WriteData(disk_cache_body_index_, cache_entry_offset_,
                             buffer_.get(), bytes_read, cache_write_callback,
                             true /* truncate */);
  if (rv != net::ERR_IO_PENDING)
    cache_write_callback.Run(rv);
}

void CacheStorageBlobToDiskCache::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirect_info,
    bool* defer_redirect) {
  NOTREACHED();
}

void CacheStorageBlobToDiskCache::OnAuthRequired(
    net::URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  NOTREACHED();
}
void CacheStorageBlobToDiskCache::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  NOTREACHED();
}
void CacheStorageBlobToDiskCache::OnSSLCertificateError(
    net::URLRequest* request,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  NOTREACHED();
}
void CacheStorageBlobToDiskCache::OnBeforeNetworkStart(net::URLRequest* request,
                                                       bool* defer) {
  NOTREACHED();
}

void CacheStorageBlobToDiskCache::OnContextShuttingDown() {
  DCHECK(blob_request_);
  RunCallbackAndRemoveObserver(false);
}

void CacheStorageBlobToDiskCache::ReadFromBlob() {
  int bytes_read = 0;
  bool done = blob_request_->Read(buffer_.get(), buffer_->size(), &bytes_read);
  if (done)
    OnReadCompleted(blob_request_.get(), bytes_read);
}

void CacheStorageBlobToDiskCache::DidWriteDataToEntry(int expected_bytes,
                                                      int rv) {
  if (rv != expected_bytes) {
    RunCallbackAndRemoveObserver(false);
    return;
  }

  cache_entry_offset_ += rv;
  ReadFromBlob();
}

void CacheStorageBlobToDiskCache::RunCallbackAndRemoveObserver(bool success) {
  DCHECK(request_context_getter_);

  request_context_getter_->RemoveObserver(this);
  blob_request_.reset();
  callback_.Run(std::move(entry_), success);
}

}  // namespace content

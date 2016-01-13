// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/partial_data.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace net {

namespace {

// The headers that we have to process.
const char kLengthHeader[] = "Content-Length";
const char kRangeHeader[] = "Content-Range";
const int kDataStream = 1;

}  // namespace

// A core object that can be detached from the Partialdata object at destruction
// so that asynchronous operations cleanup can be performed.
class PartialData::Core {
 public:
  // Build a new core object. Lifetime management is automatic.
  static Core* CreateCore(PartialData* owner) {
    return new Core(owner);
  }

  // Wrapper for Entry::GetAvailableRange. If this method returns ERR_IO_PENDING
  // PartialData::GetAvailableRangeCompleted() will be invoked on the owner
  // object when finished (unless Cancel() is called first).
  int GetAvailableRange(disk_cache::Entry* entry, int64 offset, int len,
                        int64* start);

  // Cancels a pending operation. It is a mistake to call this method if there
  // is no operation in progress; in fact, there will be no object to do so.
  void Cancel();

 private:
  explicit Core(PartialData* owner);
  ~Core();

  // Pending io completion routine.
  void OnIOComplete(int result);

  PartialData* owner_;
  int64 start_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

PartialData::Core::Core(PartialData* owner)
    : owner_(owner), start_(0) {
  DCHECK(!owner_->core_);
  owner_->core_ = this;
}

PartialData::Core::~Core() {
  if (owner_)
    owner_->core_ = NULL;
}

void PartialData::Core::Cancel() {
  DCHECK(owner_);
  owner_ = NULL;
}

int PartialData::Core::GetAvailableRange(disk_cache::Entry* entry, int64 offset,
                                         int len, int64* start) {
  int rv = entry->GetAvailableRange(
      offset, len, &start_, base::Bind(&PartialData::Core::OnIOComplete,
                                       base::Unretained(this)));
  if (rv != net::ERR_IO_PENDING) {
    // The callback will not be invoked. Lets cleanup.
    *start = start_;
    delete this;
  }
  return rv;
}

void PartialData::Core::OnIOComplete(int result) {
  if (owner_)
    owner_->GetAvailableRangeCompleted(result, start_);
  delete this;
}

// -----------------------------------------------------------------------------

PartialData::PartialData()
    : range_present_(false),
      final_range_(false),
      sparse_entry_(true),
      truncated_(false),
      initial_validation_(false),
      core_(NULL) {
}

PartialData::~PartialData() {
  if (core_)
    core_->Cancel();
}

bool PartialData::Init(const HttpRequestHeaders& headers) {
  std::string range_header;
  if (!headers.GetHeader(HttpRequestHeaders::kRange, &range_header))
    return false;

  std::vector<HttpByteRange> ranges;
  if (!HttpUtil::ParseRangeHeader(range_header, &ranges) || ranges.size() != 1)
    return false;

  // We can handle this range request.
  byte_range_ = ranges[0];
  if (!byte_range_.IsValid())
    return false;

  resource_size_ = 0;
  current_range_start_ = byte_range_.first_byte_position();

  DVLOG(1) << "Range start: " << current_range_start_ << " end: " <<
               byte_range_.last_byte_position();
  return true;
}

void PartialData::SetHeaders(const HttpRequestHeaders& headers) {
  DCHECK(extra_headers_.IsEmpty());
  extra_headers_.CopyFrom(headers);
}

void PartialData::RestoreHeaders(HttpRequestHeaders* headers) const {
  DCHECK(current_range_start_ >= 0 || byte_range_.IsSuffixByteRange());
  int64 end = byte_range_.IsSuffixByteRange() ?
              byte_range_.suffix_length() : byte_range_.last_byte_position();

  headers->CopyFrom(extra_headers_);
  if (truncated_ || !byte_range_.IsValid())
    return;

  if (current_range_start_ < 0) {
    headers->SetHeader(HttpRequestHeaders::kRange,
                       HttpByteRange::Suffix(end).GetHeaderValue());
  } else {
    headers->SetHeader(HttpRequestHeaders::kRange,
                       HttpByteRange::Bounded(
                           current_range_start_, end).GetHeaderValue());
  }
}

int PartialData::ShouldValidateCache(disk_cache::Entry* entry,
                                     const CompletionCallback& callback) {
  DCHECK_GE(current_range_start_, 0);

  // Scan the disk cache for the first cached portion within this range.
  int len = GetNextRangeLen();
  if (!len)
    return 0;

  DVLOG(3) << "ShouldValidateCache len: " << len;

  if (sparse_entry_) {
    DCHECK(callback_.is_null());
    Core* core = Core::CreateCore(this);
    cached_min_len_ = core->GetAvailableRange(entry, current_range_start_, len,
                                              &cached_start_);

    if (cached_min_len_ == ERR_IO_PENDING) {
      callback_ = callback;
      return ERR_IO_PENDING;
    }
  } else if (!truncated_) {
    if (byte_range_.HasFirstBytePosition() &&
        byte_range_.first_byte_position() >= resource_size_) {
      // The caller should take care of this condition because we should have
      // failed IsRequestedRangeOK(), but it's better to be consistent here.
      len = 0;
    }
    cached_min_len_ = len;
    cached_start_ = current_range_start_;
  }

  if (cached_min_len_ < 0)
    return cached_min_len_;

  // Return a positive number to indicate success (versus error or finished).
  return 1;
}

void PartialData::PrepareCacheValidation(disk_cache::Entry* entry,
                                         HttpRequestHeaders* headers) {
  DCHECK_GE(current_range_start_, 0);
  DCHECK_GE(cached_min_len_, 0);

  int len = GetNextRangeLen();
  DCHECK_NE(0, len);
  range_present_ = false;

  headers->CopyFrom(extra_headers_);

  if (!cached_min_len_) {
    // We don't have anything else stored.
    final_range_ = true;
    cached_start_ =
        byte_range_.HasLastBytePosition() ? current_range_start_  + len : 0;
  }

  if (current_range_start_ == cached_start_) {
    // The data lives in the cache.
    range_present_ = true;
    if (len == cached_min_len_)
      final_range_ = true;
    headers->SetHeader(
        HttpRequestHeaders::kRange,
        net::HttpByteRange::Bounded(
            current_range_start_,
            cached_start_ + cached_min_len_ - 1).GetHeaderValue());
  } else {
    // This range is not in the cache.
    headers->SetHeader(
        HttpRequestHeaders::kRange,
        net::HttpByteRange::Bounded(
            current_range_start_, cached_start_ - 1).GetHeaderValue());
  }
}

bool PartialData::IsCurrentRangeCached() const {
  return range_present_;
}

bool PartialData::IsLastRange() const {
  return final_range_;
}

bool PartialData::UpdateFromStoredHeaders(const HttpResponseHeaders* headers,
                                          disk_cache::Entry* entry,
                                          bool truncated) {
  resource_size_ = 0;
  if (truncated) {
    DCHECK_EQ(headers->response_code(), 200);
    // We don't have the real length and the user may be trying to create a
    // sparse entry so let's not write to this entry.
    if (byte_range_.IsValid())
      return false;

    if (!headers->HasStrongValidators())
      return false;

    // Now we avoid resume if there is no content length, but that was not
    // always the case so double check here.
    int64 total_length = headers->GetContentLength();
    if (total_length <= 0)
      return false;

    truncated_ = true;
    initial_validation_ = true;
    sparse_entry_ = false;
    int current_len = entry->GetDataSize(kDataStream);
    byte_range_.set_first_byte_position(current_len);
    resource_size_ = total_length;
    current_range_start_ = current_len;
    cached_min_len_ = current_len;
    cached_start_ = current_len + 1;
    return true;
  }

  if (headers->response_code() != 206) {
    DCHECK(byte_range_.IsValid());
    sparse_entry_ = false;
    resource_size_ = entry->GetDataSize(kDataStream);
    DVLOG(2) << "UpdateFromStoredHeaders size: " << resource_size_;
    return true;
  }

  if (!headers->HasStrongValidators())
    return false;

  int64 length_value = headers->GetContentLength();
  if (length_value <= 0)
    return false;  // We must have stored the resource length.

  resource_size_ = length_value;

  // Make sure that this is really a sparse entry.
  return entry->CouldBeSparse();
}

void PartialData::SetRangeToStartDownload() {
  DCHECK(truncated_);
  DCHECK(!sparse_entry_);
  current_range_start_ = 0;
  cached_start_ = 0;
  initial_validation_ = false;
}

bool PartialData::IsRequestedRangeOK() {
  if (byte_range_.IsValid()) {
    if (!byte_range_.ComputeBounds(resource_size_))
      return false;
    if (truncated_)
      return true;

    if (current_range_start_ < 0)
      current_range_start_ = byte_range_.first_byte_position();
  } else {
    // This is not a range request but we have partial data stored.
    current_range_start_ = 0;
    byte_range_.set_last_byte_position(resource_size_ - 1);
  }

  bool rv = current_range_start_ >= 0;
  if (!rv)
    current_range_start_ = 0;

  return rv;
}

bool PartialData::ResponseHeadersOK(const HttpResponseHeaders* headers) {
  if (headers->response_code() == 304) {
    if (!byte_range_.IsValid() || truncated_)
      return true;

    // We must have a complete range here.
    return byte_range_.HasFirstBytePosition() &&
        byte_range_.HasLastBytePosition();
  }

  int64 start, end, total_length;
  if (!headers->GetContentRange(&start, &end, &total_length))
    return false;
  if (total_length <= 0)
    return false;

  DCHECK_EQ(headers->response_code(), 206);

  // A server should return a valid content length with a 206 (per the standard)
  // but relax the requirement because some servers don't do that.
  int64 content_length = headers->GetContentLength();
  if (content_length > 0 && content_length != end - start + 1)
    return false;

  if (!resource_size_) {
    // First response. Update our values with the ones provided by the server.
    resource_size_ = total_length;
    if (!byte_range_.HasFirstBytePosition()) {
      byte_range_.set_first_byte_position(start);
      current_range_start_ = start;
    }
    if (!byte_range_.HasLastBytePosition())
      byte_range_.set_last_byte_position(end);
  } else if (resource_size_ != total_length) {
    return false;
  }

  if (truncated_) {
    if (!byte_range_.HasLastBytePosition())
      byte_range_.set_last_byte_position(end);
  }

  if (start != current_range_start_)
    return false;

  if (byte_range_.IsValid() && end > byte_range_.last_byte_position())
    return false;

  return true;
}

// We are making multiple requests to complete the range requested by the user.
// Just assume that everything is fine and say that we are returning what was
// requested.
void PartialData::FixResponseHeaders(HttpResponseHeaders* headers,
                                     bool success) {
  if (truncated_)
    return;

  if (byte_range_.IsValid() && success) {
    headers->UpdateWithNewRange(byte_range_, resource_size_, !sparse_entry_);
    return;
  }

  headers->RemoveHeader(kLengthHeader);
  headers->RemoveHeader(kRangeHeader);

  if (byte_range_.IsValid()) {
    headers->ReplaceStatusLine("HTTP/1.1 416 Requested Range Not Satisfiable");
    headers->AddHeader(base::StringPrintf("%s: bytes 0-0/%" PRId64,
                                          kRangeHeader, resource_size_));
    headers->AddHeader(base::StringPrintf("%s: 0", kLengthHeader));
  } else {
    // TODO(rvargas): Is it safe to change the protocol version?
    headers->ReplaceStatusLine("HTTP/1.1 200 OK");
    DCHECK_NE(resource_size_, 0);
    headers->AddHeader(base::StringPrintf("%s: %" PRId64, kLengthHeader,
                                          resource_size_));
  }
}

void PartialData::FixContentLength(HttpResponseHeaders* headers) {
  headers->RemoveHeader(kLengthHeader);
  headers->AddHeader(base::StringPrintf("%s: %" PRId64, kLengthHeader,
                                        resource_size_));
}

int PartialData::CacheRead(
    disk_cache::Entry* entry, IOBuffer* data, int data_len,
    const net::CompletionCallback& callback) {
  int read_len = std::min(data_len, cached_min_len_);
  if (!read_len)
    return 0;

  int rv = 0;
  if (sparse_entry_) {
    rv = entry->ReadSparseData(current_range_start_, data, read_len,
                               callback);
  } else {
    if (current_range_start_ > kint32max)
      return ERR_INVALID_ARGUMENT;

    rv = entry->ReadData(kDataStream, static_cast<int>(current_range_start_),
                         data, read_len, callback);
  }
  return rv;
}

int PartialData::CacheWrite(
    disk_cache::Entry* entry, IOBuffer* data, int data_len,
    const net::CompletionCallback& callback) {
  DVLOG(3) << "To write: " << data_len;
  if (sparse_entry_) {
    return entry->WriteSparseData(
        current_range_start_, data, data_len, callback);
  } else  {
    if (current_range_start_ > kint32max)
      return ERR_INVALID_ARGUMENT;

    return entry->WriteData(kDataStream, static_cast<int>(current_range_start_),
                            data, data_len, callback, true);
  }
}

void PartialData::OnCacheReadCompleted(int result) {
  DVLOG(3) << "Read: " << result;
  if (result > 0) {
    current_range_start_ += result;
    cached_min_len_ -= result;
    DCHECK_GE(cached_min_len_, 0);
  }
}

void PartialData::OnNetworkReadCompleted(int result) {
  if (result > 0)
    current_range_start_ += result;
}

int PartialData::GetNextRangeLen() {
  int64 range_len =
      byte_range_.HasLastBytePosition() ?
      byte_range_.last_byte_position() - current_range_start_ + 1 :
      kint32max;
  if (range_len > kint32max)
    range_len = kint32max;
  return static_cast<int32>(range_len);
}

void PartialData::GetAvailableRangeCompleted(int result, int64 start) {
  DCHECK(!callback_.is_null());
  DCHECK_NE(ERR_IO_PENDING, result);

  cached_start_ = start;
  cached_min_len_ = result;
  if (result >= 0)
    result = 1;  // Return success, go ahead and validate the entry.

  CompletionCallback cb = callback_;
  callback_.Reset();
  cb.Run(result);
}

}  // namespace net

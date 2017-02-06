// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "media/blink/resource_multibuffer_data_provider.h"
#include "media/blink/url_index.h"

namespace media {

const int kBlockSizeShift = 15;  // 1<<15 == 32kb
const int kUrlMappingTimeoutSeconds = 300;

ResourceMultiBuffer::ResourceMultiBuffer(UrlData* url_data, int block_shift)
    : MultiBuffer(block_shift, url_data->url_index_->lru_),
      url_data_(url_data) {}

ResourceMultiBuffer::~ResourceMultiBuffer() {}

std::unique_ptr<MultiBuffer::DataProvider> ResourceMultiBuffer::CreateWriter(
    const MultiBufferBlockId& pos) {
  ResourceMultiBufferDataProvider* ret =
      new ResourceMultiBufferDataProvider(url_data_, pos);
  ret->Start();
  return std::unique_ptr<MultiBuffer::DataProvider>(ret);
}

bool ResourceMultiBuffer::RangeSupported() const {
  return url_data_->range_supported_;
}

void ResourceMultiBuffer::OnEmpty() {
  url_data_->OnEmpty();
}

UrlData::UrlData(const GURL& url,
                 CORSMode cors_mode,
                 const base::WeakPtr<UrlIndex>& url_index)
    : url_(url),
      have_data_origin_(false),
      cors_mode_(cors_mode),
      url_index_(url_index),
      length_(kPositionNotSpecified),
      range_supported_(false),
      cacheable_(false),
      last_used_(),
      multibuffer_(this, url_index_->block_shift_),
      frame_(url_index->frame()) {}

UrlData::~UrlData() {}

std::pair<GURL, UrlData::CORSMode> UrlData::key() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return std::make_pair(url(), cors_mode());
}

void UrlData::set_valid_until(base::Time valid_until) {
  DCHECK(thread_checker_.CalledOnValidThread());
  valid_until_ = valid_until;
}

void UrlData::MergeFrom(const scoped_refptr<UrlData>& other) {
  // We're merging from another UrlData that refers to the *same*
  // resource, so when we merge the metadata, we can use the most
  // optimistic values.
  if (ValidateDataOrigin(other->data_origin_)) {
    DCHECK(thread_checker_.CalledOnValidThread());
    valid_until_ = std::max(valid_until_, other->valid_until_);
    // set_length() will not override the length if already known.
    set_length(other->length_);
    cacheable_ |= other->cacheable_;
    range_supported_ |= other->range_supported_;
    if (last_modified_.is_null()) {
      last_modified_ = other->last_modified_;
    }
    multibuffer()->MergeFrom(other->multibuffer());
  }
}

void UrlData::set_cacheable(bool cacheable) {
  DCHECK(thread_checker_.CalledOnValidThread());
  cacheable_ = cacheable;
}

void UrlData::set_length(int64_t length) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (length != kPositionNotSpecified) {
    length_ = length;
  }
}

void UrlData::RedirectTo(const scoped_refptr<UrlData>& url_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Copy any cached data over to the new location.
  url_data->multibuffer()->MergeFrom(multibuffer());

  std::vector<RedirectCB> redirect_callbacks;
  redirect_callbacks.swap(redirect_callbacks_);
  for (const RedirectCB& cb : redirect_callbacks) {
    cb.Run(url_data);
  }
}

void UrlData::Fail() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Handled similar to a redirect.
  std::vector<RedirectCB> redirect_callbacks;
  redirect_callbacks.swap(redirect_callbacks_);
  for (const RedirectCB& cb : redirect_callbacks) {
    cb.Run(nullptr);
  }
}

void UrlData::OnRedirect(const RedirectCB& cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  redirect_callbacks_.push_back(cb);
}

void UrlData::Use() {
  DCHECK(thread_checker_.CalledOnValidThread());
  last_used_ = base::Time::Now();
}

bool UrlData::ValidateDataOrigin(const GURL& origin) {
  if (!have_data_origin_) {
    data_origin_ = origin;
    have_data_origin_ = true;
    return true;
  }
  if (cors_mode_ == UrlData::CORS_UNSPECIFIED) {
    return data_origin_ == origin;
  }
  // The actual cors checks is done in the net layer.
  return true;
}

void UrlData::OnEmpty() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&UrlIndex::RemoveUrlDataIfEmpty, url_index_,
                            scoped_refptr<UrlData>(this)));
}

bool UrlData::Valid() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::Time now = base::Time::Now();
  if (!range_supported_)
    return false;
  // When ranges are not supported, we cannot re-use cached data.
  if (valid_until_ > now)
    return true;
  if (now - last_used_ <
      base::TimeDelta::FromSeconds(kUrlMappingTimeoutSeconds))
    return true;
  return false;
}

void UrlData::set_last_modified(base::Time last_modified) {
  DCHECK(thread_checker_.CalledOnValidThread());
  last_modified_ = last_modified;
}

void UrlData::set_range_supported() {
  DCHECK(thread_checker_.CalledOnValidThread());
  range_supported_ = true;
}

ResourceMultiBuffer* UrlData::multibuffer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return &multibuffer_;
}

size_t UrlData::CachedSize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return multibuffer()->map().size();
}

UrlIndex::UrlIndex(blink::WebFrame* frame) : UrlIndex(frame, kBlockSizeShift) {}

UrlIndex::UrlIndex(blink::WebFrame* frame, int block_shift)
    : frame_(frame),
      lru_(new MultiBuffer::GlobalLRU(base::ThreadTaskRunnerHandle::Get())),
      block_shift_(block_shift),
      weak_factory_(this) {}

UrlIndex::~UrlIndex() {}

void UrlIndex::RemoveUrlDataIfEmpty(const scoped_refptr<UrlData>& url_data) {
  if (!url_data->multibuffer()->map().empty())
    return;

  auto i = by_url_.find(url_data->key());
  if (i != by_url_.end() && i->second == url_data)
    by_url_.erase(i);
}

scoped_refptr<UrlData> UrlIndex::GetByUrl(const GURL& gurl,
                                          UrlData::CORSMode cors_mode) {
  auto i = by_url_.find(std::make_pair(gurl, cors_mode));
  if (i != by_url_.end() && i->second->Valid()) {
    return i->second;
  }
  return NewUrlData(gurl, cors_mode);
}

scoped_refptr<UrlData> UrlIndex::NewUrlData(const GURL& url,
                                            UrlData::CORSMode cors_mode) {
  return new UrlData(url, cors_mode, weak_factory_.GetWeakPtr());
}

scoped_refptr<UrlData> UrlIndex::TryInsert(
    const scoped_refptr<UrlData>& url_data) {
  scoped_refptr<UrlData>* by_url_slot;
  bool urldata_valid = url_data->Valid();
  if (urldata_valid) {
    by_url_slot = &by_url_.insert(std::make_pair(url_data->key(), url_data))
                       .first->second;
  } else {
    std::map<UrlData::KeyType, scoped_refptr<UrlData>>::iterator iter;
    iter = by_url_.find(url_data->key());
    if (iter == by_url_.end())
      return url_data;
    by_url_slot = &iter->second;
  }
  if (*by_url_slot == url_data)
    return url_data;

  // TODO(hubbe): Support etag validation.
  if (!url_data->last_modified().is_null()) {
    if ((*by_url_slot)->last_modified() != url_data->last_modified()) {
      if (urldata_valid)
        *by_url_slot = url_data;
      return url_data;
    }
  }
  // Check if we should replace the in-cache url data with our url data.
  if (urldata_valid) {
    if ((!(*by_url_slot)->Valid() ||
         url_data->CachedSize() > (*by_url_slot)->CachedSize())) {
      *by_url_slot = url_data;
    } else {
      (*by_url_slot)->MergeFrom(url_data);
    }
  }
  return *by_url_slot;
}

}  // namespace media

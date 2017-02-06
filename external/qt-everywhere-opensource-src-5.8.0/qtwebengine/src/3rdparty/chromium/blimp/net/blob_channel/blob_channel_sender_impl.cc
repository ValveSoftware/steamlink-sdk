// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blob_channel/blob_channel_sender_impl.h"

#include "base/strings/string_number_conversions.h"
#include "blimp/common/blob_cache/blob_cache.h"
#include "blimp/common/blob_cache/id_util.h"

namespace blimp {

BlobChannelSenderImpl::BlobChannelSenderImpl(std::unique_ptr<BlobCache> cache,
                                             std::unique_ptr<Delegate> delegate)
    : cache_(std::move(cache)), delegate_(std::move(delegate)) {
  DCHECK(cache_);
  DCHECK(delegate_);
}

BlobChannelSenderImpl::~BlobChannelSenderImpl() {}

void BlobChannelSenderImpl::PutBlob(const BlobId& id, BlobDataPtr data) {
  DCHECK(data);
  DCHECK(!id.empty());

  if (cache_->Contains(id)) {
    return;
  }

  VLOG(2) << "Put blob: " << BlobIdToString(id);
  cache_->Put(id, data);
}

void BlobChannelSenderImpl::DeliverBlob(const BlobId& id) {
  if (!cache_->Contains(id)) {
    DLOG(FATAL) << "Attempted to push unknown blob: " << BlobIdToString(id);
    return;
  }

  if (receiver_cache_contents_.find(id) != receiver_cache_contents_.end()) {
    DVLOG(3) << "Suppressed redundant push: " << BlobIdToString(id);
    return;
  }
  receiver_cache_contents_.insert(id);

  VLOG(2) << "Deliver blob: " << BlobIdToString(id);
  delegate_->DeliverBlob(id, cache_->Get(id));
}

}  // namespace blimp

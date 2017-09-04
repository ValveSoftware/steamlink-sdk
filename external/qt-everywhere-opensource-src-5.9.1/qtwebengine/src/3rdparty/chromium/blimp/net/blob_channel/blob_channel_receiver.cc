// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blob_channel/blob_channel_receiver.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "blimp/common/blob_cache/blob_cache.h"
#include "blimp/common/blob_cache/id_util.h"

namespace blimp {
namespace {

// Takes incoming blobs from |delegate_| stores them in |cache_|, and provides
// callers a getter interface for accessing blobs from |cache_|.
class BLIMP_NET_EXPORT BlobChannelReceiverImpl : public BlobChannelReceiver {
 public:
  BlobChannelReceiverImpl(std::unique_ptr<BlobCache> cache,
                          std::unique_ptr<Delegate> delegate);
  ~BlobChannelReceiverImpl() override;

  // BlobChannelReceiver implementation.
  BlobDataPtr Get(const BlobId& id) override;
  void OnBlobReceived(const BlobId& id, BlobDataPtr data) override;

 private:
  std::unique_ptr<BlobCache> cache_;
  std::unique_ptr<Delegate> delegate_;

  // Guards against concurrent access to |cache_|.
  base::Lock cache_lock_;

  DISALLOW_COPY_AND_ASSIGN(BlobChannelReceiverImpl);
};

}  // namespace

// static
std::unique_ptr<BlobChannelReceiver> BlobChannelReceiver::Create(
    std::unique_ptr<BlobCache> cache,
    std::unique_ptr<Delegate> delegate) {
  return base::WrapUnique(
      new BlobChannelReceiverImpl(std::move(cache), std::move(delegate)));
}

BlobChannelReceiverImpl::BlobChannelReceiverImpl(
    std::unique_ptr<BlobCache> cache,
    std::unique_ptr<Delegate> delegate)
    : cache_(std::move(cache)), delegate_(std::move(delegate)) {
  DCHECK(cache_);

  delegate_->SetReceiver(this);
}

BlobChannelReceiverImpl::~BlobChannelReceiverImpl() {}

BlobDataPtr BlobChannelReceiverImpl::Get(const BlobId& id) {
  DVLOG(2) << "Get blob: " << BlobIdToString(id);

  base::AutoLock lock(cache_lock_);
  return cache_->Get(id);
}

void BlobChannelReceiverImpl::OnBlobReceived(const BlobId& id,
                                             BlobDataPtr data) {
  DVLOG(2) << "Blob received: " << BlobIdToString(id)
           << ", size: " << data->data.size();

  base::AutoLock lock(cache_lock_);
  cache_->Put(id, data);
}

}  // namespace blimp

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/mojo/blob_channel_service.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "blimp/net/blob_channel/blob_channel_sender.h"
#include "mojo/public/cpp/system/buffer.h"

namespace blimp {
namespace engine {
namespace {

std::vector<BlobChannelSender::CacheStateEntry>
GetCachedBlobIdsFromWeakPtr(base::WeakPtr<BlobChannelSender> sender) {
  if (sender) {
    return sender->GetCachedBlobIds();
  } else {
    return std::vector<BlobChannelSender::CacheStateEntry>();
  }
}

}  // namespace

BlobChannelService::BlobChannelService(
    base::WeakPtr<BlobChannelSender> blob_channel_sender,
    scoped_refptr<base::SingleThreadTaskRunner> blob_sender_task_runner)
    : blob_channel_sender_(blob_channel_sender),
      blob_sender_task_runner_(blob_sender_task_runner),
      weak_factory_(this) {
  DCHECK(blob_sender_task_runner_.get());
}

BlobChannelService::~BlobChannelService() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void BlobChannelService::GetCachedBlobIds(
    const BlobChannelService::GetCachedBlobIdsCallback& response_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "BlobChannel::GetCachedBlobIds called.";

  // Pull the list of blob IDs from the UI thread.
  base::PostTaskAndReplyWithResult(
      blob_sender_task_runner_.get(), FROM_HERE,
      base::Bind(&GetCachedBlobIdsFromWeakPtr, blob_channel_sender_),
      base::Bind(&BlobChannelService::OnGetCachedBlobsCompleted,
                 weak_factory_.GetWeakPtr(), response_callback));
}

void BlobChannelService::OnGetCachedBlobsCompleted(
    const BlobChannelService::GetCachedBlobIdsCallback& response_callback,
    const std::vector<BlobChannelSender::CacheStateEntry>& ids) {
  std::unordered_map<std::string, bool> cache_state;
  for (const auto& next_entry : ids) {
    cache_state[next_entry.id] = next_entry.was_delivered;
  }
  response_callback.Run(std::move(cache_state));
}

void BlobChannelService::PutBlob(const std::string& id,
                                 mojo::ScopedSharedBufferHandle data,
                                 uint32_t size) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Map |data| into the address space and copy out its contents.
  if (!data.is_valid()) {
    LOG(ERROR) << "Invalid data handle received from renderer process.";
    return;
  }

  if (size > kMaxBlobSizeBytes) {
    LOG(ERROR) << "Blob size too big: " << size;
    return;
  }

  mojo::ScopedSharedBufferMapping mapping = data->Map(size);
  CHECK(mapping) << "Failed to mmap region of " << size << " bytes.";

  scoped_refptr<BlobData> new_blob(new BlobData);
  new_blob->data.assign(reinterpret_cast<const char*>(mapping.get()), size);

  blob_sender_task_runner_->PostTask(FROM_HERE,
                         base::Bind(&BlobChannelSender::PutBlob,
                                    blob_channel_sender_,
                                    id,
                                    base::Passed(std::move(new_blob))));
}

void BlobChannelService::DeliverBlob(const std::string& id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  blob_sender_task_runner_->PostTask(FROM_HERE,
                         base::Bind(&BlobChannelSender::DeliverBlob,
                                    blob_channel_sender_,
                                    id));
}

void BlobChannelService::BindRequest(
    mojo::InterfaceRequest<mojom::BlobChannel> request) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bindings_.AddBinding(this, std::move(request));
}

}  // namespace engine
}  // namespace blimp

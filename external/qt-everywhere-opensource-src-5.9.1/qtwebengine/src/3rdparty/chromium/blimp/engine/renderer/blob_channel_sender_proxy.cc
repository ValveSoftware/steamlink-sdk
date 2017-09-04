// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/renderer/blob_channel_sender_proxy.h"

#include <unordered_map>
#include <utility>

#include "blimp/common/blob_cache/id_util.h"
#include "content/public/renderer/render_thread.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blimp {
namespace engine {
namespace {

mojom::BlobChannelPtr GetConnectedBlobChannel() {
  mojom::BlobChannelPtr blob_channel_ptr;
  content::RenderThread::Get()->GetRemoteInterfaces()->GetInterface(
      &blob_channel_ptr);
  CHECK(blob_channel_ptr) << "Could not connect to BlobChannel Mojo interface.";
  return blob_channel_ptr;
}

// Manages the creation and lifetime of Mojo shared memory buffers for blobs.
class SharedMemoryBlob {
 public:
  explicit SharedMemoryBlob(BlobDataPtr data);
  ~SharedMemoryBlob();

  mojo::ScopedSharedBufferHandle CreateRemoteHandle();

 private:
  mojo::ScopedSharedBufferHandle local_handle_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemoryBlob);
};

SharedMemoryBlob::SharedMemoryBlob(BlobDataPtr data) {
  DCHECK_GE(kMaxBlobSizeBytes, data->data.size());

  local_handle_ = mojo::SharedBufferHandle::Create(data->data.size());
  DCHECK(local_handle_.is_valid());

  mojo::ScopedSharedBufferMapping mapped =
      local_handle_->Map(data->data.size());
  DCHECK(mapped);
  memcpy(mapped.get(), data->data.data(), data->data.size());
}

SharedMemoryBlob::~SharedMemoryBlob() {}

mojo::ScopedSharedBufferHandle SharedMemoryBlob::CreateRemoteHandle() {
  mojo::ScopedSharedBufferHandle remote_handle =
      local_handle_->Clone(mojo::SharedBufferHandle::AccessMode::READ_ONLY);
  CHECK(remote_handle.is_valid())
      << "Mojo error when creating read-only buffer handle.";
  return remote_handle;
}

}  // namespace

BlobChannelSenderProxy::BlobChannelSenderProxy()
    : blob_channel_(GetConnectedBlobChannel()), weak_factory_(this) {
  blob_channel_->GetCachedBlobIds(
      base::Bind(&BlobChannelSenderProxy::OnGetCacheStateComplete,
                 weak_factory_.GetWeakPtr()));
}

BlobChannelSenderProxy::~BlobChannelSenderProxy() {}

BlobChannelSenderProxy::BlobChannelSenderProxy(
    mojom::BlobChannelPtr blob_channel)
    : blob_channel_(std::move(blob_channel)), weak_factory_(this) {}

// static
std::unique_ptr<BlobChannelSenderProxy> BlobChannelSenderProxy::CreateForTest(
    mojom::BlobChannelPtr blob_channel) {
  return base::WrapUnique(new BlobChannelSenderProxy(std::move(blob_channel)));
}

bool BlobChannelSenderProxy::IsInEngineCache(const std::string& id) const {
  return replication_state_.find(id) != replication_state_.end();
}

bool BlobChannelSenderProxy::IsInClientCache(const std::string& id) const {
  auto found = replication_state_.find(id);
  return found != replication_state_.end() && found->second;
}

void BlobChannelSenderProxy::PutBlob(const BlobId& id, BlobDataPtr data) {
  DCHECK(!IsInEngineCache(id));

  size_t size = data->data.size();
  CHECK(!data->data.empty()) << "Zero length blob sent: " << BlobIdToString(id);

  replication_state_[id] = false;
  std::unique_ptr<SharedMemoryBlob> shared_mem_blob(
      new SharedMemoryBlob(std::move(data)));
  blob_channel_->PutBlob(id, shared_mem_blob->CreateRemoteHandle(), size);
}

void BlobChannelSenderProxy::DeliverBlob(const std::string& id) {
  DCHECK(IsInEngineCache(id)) << "Attempted to deliver an invalid blob: "
                              << BlobIdToString(id);
  DCHECK(!IsInClientCache(id)) << "Blob is already in the remote cache:"
                               << BlobIdToString(id);

  // We assume that the client will have the blob if we push it.
  // TODO(kmarshall): Revisit this assumption when asynchronous blob transport
  // is supported.
  replication_state_[id] = true;

  blob_channel_->DeliverBlob(id);
}

std::vector<BlobChannelSender::CacheStateEntry>
BlobChannelSenderProxy::GetCachedBlobIds() const {
  NOTREACHED();
  return std::vector<BlobChannelSender::CacheStateEntry>();
}

void BlobChannelSenderProxy::OnGetCacheStateComplete(
    const std::unordered_map<std::string, bool>& cache_state) {
  VLOG(1) << "Received cache state from browser (" << cache_state.size()
          << " items)";
  replication_state_.clear();
  for (const auto& next_item : cache_state) {
    replication_state_[next_item.first] = next_item.second;
  }
}

}  // namespace engine
}  // namespace blimp

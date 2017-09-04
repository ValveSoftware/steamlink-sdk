// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_MOJO_BLOB_CHANNEL_SERVICE_H_
#define BLIMP_ENGINE_MOJO_BLOB_CHANNEL_SERVICE_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "blimp/engine/mojo/blob_channel.mojom.h"
#include "blimp/net/blob_channel/blob_channel_sender.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace blimp {
namespace engine {

// Service for processing BlobChannel requests from the renderer.
// Caller is responsible for executing all methods on the IO thread.
// The class may interact with objects on the UI thread by posting tasks to
// |ui_task_runner_|.
class BlobChannelService : public mojom::BlobChannel {
 public:
  // |blob_channel_sender|: The BlobChannelSender object which will receive
  // blobs and answer delivery status queries.
  // |blob_sender_task_runner|: The task runner that will process all
  // interactions with |blob_channel_sender|.
  BlobChannelService(
      base::WeakPtr<BlobChannelSender> blob_channel_sender,
      scoped_refptr<base::SingleThreadTaskRunner> blob_sender_task_runner);

  ~BlobChannelService() override;

  // Factory method called by Mojo.
  // Binds |this| to the connection specified by |request|.
  void BindRequest(mojo::InterfaceRequest<mojom::BlobChannel> request);

 private:
  std::vector<BlobChannelSender::CacheStateEntry> GetCachedBlobIdsOnUiThread();

  // Processes the return value from BlobChannelSender::GetCachedBlobIds().
  void OnGetCachedBlobsCompleted(
      const BlobChannelService::GetCachedBlobIdsCallback& response_callback,
      const std::vector<BlobChannelSender::CacheStateEntry>& ids);

  // BlobChannel implementation.
  void GetCachedBlobIds(
      const GetCachedBlobIdsCallback& response_callback) override;
  void PutBlob(const std::string& id,
               mojo::ScopedSharedBufferHandle data,
               uint32_t size) override;
  void DeliverBlob(const std::string& id) override;

  mojo::BindingSet<mojom::BlobChannel> bindings_;

  // Sender object which will receive the blobs passed over the Mojo service.
  base::WeakPtr<BlobChannelSender> blob_channel_sender_;

  // UI thread TaskRunner.
  scoped_refptr<base::SingleThreadTaskRunner> blob_sender_task_runner_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<BlobChannelService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlobChannelService);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_MOJO_BLOB_CHANNEL_SERVICE_H_

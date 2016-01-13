// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_update_controller.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource_provider.h"
#include "ui/gfx/frame_time.h"

namespace {

// Number of partial updates we allow.
const size_t kPartialTextureUpdatesMax = 12;

// Measured in seconds.
const double kUploaderBusyTickRate = 0.001;

// Number of blocking update intervals to allow.
const size_t kMaxBlockingUpdateIntervals = 4;

}  // namespace

namespace cc {

size_t ResourceUpdateController::MaxPartialTextureUpdates() {
  return kPartialTextureUpdatesMax;
}

size_t ResourceUpdateController::MaxFullUpdatesPerTick(
    ResourceProvider* resource_provider) {
  return resource_provider->EstimatedUploadsPerTick();
}

ResourceUpdateController::ResourceUpdateController(
    ResourceUpdateControllerClient* client,
    base::SingleThreadTaskRunner* task_runner,
    scoped_ptr<ResourceUpdateQueue> queue,
    ResourceProvider* resource_provider)
    : client_(client),
      queue_(queue.Pass()),
      resource_provider_(resource_provider),
      texture_updates_per_tick_(MaxFullUpdatesPerTick(resource_provider)),
      first_update_attempt_(true),
      task_runner_(task_runner),
      task_posted_(false),
      ready_to_finalize_(false),
      weak_factory_(this) {}

ResourceUpdateController::~ResourceUpdateController() {}

void ResourceUpdateController::PerformMoreUpdates(
    base::TimeTicks time_limit) {
  time_limit_ = time_limit;

  // Update already in progress or we are already done.
  if (task_posted_ || ready_to_finalize_)
    return;

  // Call UpdateMoreTexturesNow() directly unless it's the first update
  // attempt. This ensures that we empty the update queue in a finite
  // amount of time.
  if (!first_update_attempt_)
    UpdateMoreTexturesNow();

  // Post a 0-delay task when no updates were left. When it runs,
  // ReadyToFinalizeTextureUpdates() will be called.
  if (!UpdateMoreTexturesIfEnoughTimeRemaining()) {
    task_posted_ = true;
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ResourceUpdateController::OnTimerFired,
                   weak_factory_.GetWeakPtr()));
  }

  first_update_attempt_ = false;
}

void ResourceUpdateController::DiscardUploadsToEvictedResources() {
  queue_->ClearUploadsToEvictedResources();
}

void ResourceUpdateController::UpdateTexture(ResourceUpdate update) {
  update.bitmap->lockPixels();
  update.texture->SetPixels(
      resource_provider_,
      static_cast<const uint8_t*>(update.bitmap->getPixels()),
      update.content_rect,
      update.source_rect,
      update.dest_offset);
  update.bitmap->unlockPixels();
}

void ResourceUpdateController::Finalize() {
  while (queue_->FullUploadSize())
    UpdateTexture(queue_->TakeFirstFullUpload());

  while (queue_->PartialUploadSize())
    UpdateTexture(queue_->TakeFirstPartialUpload());

  resource_provider_->FlushUploads();
}

void ResourceUpdateController::OnTimerFired() {
  task_posted_ = false;
  if (!UpdateMoreTexturesIfEnoughTimeRemaining()) {
    ready_to_finalize_ = true;
    client_->ReadyToFinalizeTextureUpdates();
  }
}

base::TimeTicks ResourceUpdateController::UpdateMoreTexturesCompletionTime() {
  return resource_provider_->EstimatedUploadCompletionTime(
      texture_updates_per_tick_);
}

size_t ResourceUpdateController::UpdateMoreTexturesSize() const {
  return texture_updates_per_tick_;
}

size_t ResourceUpdateController::MaxBlockingUpdates() const {
  return UpdateMoreTexturesSize() * kMaxBlockingUpdateIntervals;
}

bool ResourceUpdateController::UpdateMoreTexturesIfEnoughTimeRemaining() {
  while (resource_provider_->NumBlockingUploads() < MaxBlockingUpdates()) {
    if (!queue_->FullUploadSize())
      return false;

    if (!time_limit_.is_null()) {
      base::TimeTicks completion_time = UpdateMoreTexturesCompletionTime();
      if (completion_time > time_limit_)
        return true;
    }

    UpdateMoreTexturesNow();
  }

  task_posted_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ResourceUpdateController::OnTimerFired,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kUploaderBusyTickRate * 1000));
  return true;
}

void ResourceUpdateController::UpdateMoreTexturesNow() {
  size_t uploads = std::min(
      queue_->FullUploadSize(), UpdateMoreTexturesSize());

  if (!uploads)
    return;

  while (queue_->FullUploadSize() && uploads--)
    UpdateTexture(queue_->TakeFirstFullUpload());

  resource_provider_->FlushUploads();
}

}  // namespace cc

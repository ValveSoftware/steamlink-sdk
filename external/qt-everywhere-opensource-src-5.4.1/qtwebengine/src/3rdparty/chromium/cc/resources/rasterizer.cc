// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/rasterizer.h"

#include <algorithm>

namespace cc {

RasterizerTask::RasterizerTask() : did_schedule_(false), did_complete_(false) {}

RasterizerTask::~RasterizerTask() {
  // Debugging CHECKs to help track down a use-after-free.
  CHECK(!did_schedule_);
  CHECK(!did_run_ || did_complete_);
}

ImageDecodeTask* RasterizerTask::AsImageDecodeTask() { return NULL; }

RasterTask* RasterizerTask::AsRasterTask() { return NULL; }

void RasterizerTask::WillSchedule() { DCHECK(!did_schedule_); }

void RasterizerTask::DidSchedule() {
  did_schedule_ = true;
  did_complete_ = false;
}

bool RasterizerTask::HasBeenScheduled() const { return did_schedule_; }

void RasterizerTask::WillComplete() { DCHECK(!did_complete_); }

void RasterizerTask::DidComplete() {
  DCHECK(did_schedule_);
  DCHECK(!did_complete_);
  did_schedule_ = false;
  did_complete_ = true;
}

bool RasterizerTask::HasCompleted() const { return did_complete_; }

ImageDecodeTask::ImageDecodeTask() {}

ImageDecodeTask::~ImageDecodeTask() {}

ImageDecodeTask* ImageDecodeTask::AsImageDecodeTask() { return this; }

RasterTask::RasterTask(const Resource* resource,
                       ImageDecodeTask::Vector* dependencies)
    : resource_(resource) {
  dependencies_.swap(*dependencies);
}

RasterTask::~RasterTask() {}

RasterTask* RasterTask::AsRasterTask() { return this; }

RasterTaskQueue::Item::Item(RasterTask* task, bool required_for_activation)
    : task(task), required_for_activation(required_for_activation) {}

RasterTaskQueue::Item::~Item() {}

RasterTaskQueue::RasterTaskQueue() : required_for_activation_count(0u) {}

RasterTaskQueue::~RasterTaskQueue() {}

void RasterTaskQueue::Swap(RasterTaskQueue* other) {
  items.swap(other->items);
  std::swap(required_for_activation_count,
            other->required_for_activation_count);
}

void RasterTaskQueue::Reset() {
  required_for_activation_count = 0u;
  items.clear();
}

}  // namespace cc

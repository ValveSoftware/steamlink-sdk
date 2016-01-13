// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_update_queue.h"

#include "cc/resources/prioritized_resource.h"

namespace cc {

ResourceUpdateQueue::ResourceUpdateQueue() {}

ResourceUpdateQueue::~ResourceUpdateQueue() {}

void ResourceUpdateQueue::AppendFullUpload(const ResourceUpdate& upload) {
  full_entries_.push_back(upload);
}

void ResourceUpdateQueue::AppendPartialUpload(const ResourceUpdate& upload) {
  partial_entries_.push_back(upload);
}

void ResourceUpdateQueue::ClearUploadsToEvictedResources() {
  ClearUploadsToEvictedResources(&full_entries_);
  ClearUploadsToEvictedResources(&partial_entries_);
}

void ResourceUpdateQueue::ClearUploadsToEvictedResources(
    std::deque<ResourceUpdate>* entry_queue) {
  std::deque<ResourceUpdate> temp;
  entry_queue->swap(temp);
  while (temp.size()) {
    ResourceUpdate upload = temp.front();
    temp.pop_front();
    if (!upload.texture->BackingResourceWasEvicted())
      entry_queue->push_back(upload);
  }
}

ResourceUpdate ResourceUpdateQueue::TakeFirstFullUpload() {
  ResourceUpdate first = full_entries_.front();
  full_entries_.pop_front();
  return first;
}

ResourceUpdate ResourceUpdateQueue::TakeFirstPartialUpload() {
  ResourceUpdate first = partial_entries_.front();
  partial_entries_.pop_front();
  return first;
}

bool ResourceUpdateQueue::HasMoreUpdates() const {
  return !full_entries_.empty() || !partial_entries_.empty();
}

}  // namespace cc

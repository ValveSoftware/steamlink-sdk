// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_UPDATE_QUEUE_H_
#define CC_RESOURCES_RESOURCE_UPDATE_QUEUE_H_

#include <deque>
#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "cc/resources/resource_update.h"

namespace cc {

class CC_EXPORT ResourceUpdateQueue {
 public:
  ResourceUpdateQueue();
  virtual ~ResourceUpdateQueue();

  void AppendFullUpload(const ResourceUpdate& upload);
  void AppendPartialUpload(const ResourceUpdate& upload);

  void ClearUploadsToEvictedResources();

  ResourceUpdate TakeFirstFullUpload();
  ResourceUpdate TakeFirstPartialUpload();

  size_t FullUploadSize() const { return full_entries_.size(); }
  size_t PartialUploadSize() const { return partial_entries_.size(); }

  bool HasMoreUpdates() const;

 private:
  void ClearUploadsToEvictedResources(std::deque<ResourceUpdate>* entry_queue);
  std::deque<ResourceUpdate> full_entries_;
  std::deque<ResourceUpdate> partial_entries_;

  DISALLOW_COPY_AND_ASSIGN(ResourceUpdateQueue);
};

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_UPDATE_QUEUE_H_

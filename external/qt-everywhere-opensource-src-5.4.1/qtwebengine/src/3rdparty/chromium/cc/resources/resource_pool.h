// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_POOL_H_
#define CC_RESOURCES_RESOURCE_POOL_H_

#include <list>

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/output/renderer.h"
#include "cc/resources/resource.h"
#include "cc/resources/resource_format.h"

namespace cc {
class ScopedResource;

class CC_EXPORT ResourcePool {
 public:
  static scoped_ptr<ResourcePool> Create(ResourceProvider* resource_provider,
                                         GLenum target,
                                         ResourceFormat format) {
    return make_scoped_ptr(new ResourcePool(resource_provider, target, format));
  }

  virtual ~ResourcePool();

  scoped_ptr<ScopedResource> AcquireResource(const gfx::Size& size);
  void ReleaseResource(scoped_ptr<ScopedResource>);

  void SetResourceUsageLimits(size_t max_memory_usage_bytes,
                              size_t max_unused_memory_usage_bytes,
                              size_t max_resource_count);

  void ReduceResourceUsage();
  void CheckBusyResources();

  size_t total_memory_usage_bytes() const { return memory_usage_bytes_; }
  size_t acquired_memory_usage_bytes() const {
    return memory_usage_bytes_ - unused_memory_usage_bytes_;
  }
  size_t total_resource_count() const { return resource_count_; }
  size_t acquired_resource_count() const {
    return resource_count_ - unused_resources_.size();
  }

  ResourceFormat resource_format() const { return format_; }

 protected:
  ResourcePool(ResourceProvider* resource_provider,
               GLenum target,
               ResourceFormat format);

  bool ResourceUsageTooHigh();

 private:
  void DidFinishUsingResource(ScopedResource* resource);

  ResourceProvider* resource_provider_;
  const GLenum target_;
  const ResourceFormat format_;
  size_t max_memory_usage_bytes_;
  size_t max_unused_memory_usage_bytes_;
  size_t max_resource_count_;
  size_t memory_usage_bytes_;
  size_t unused_memory_usage_bytes_;
  size_t resource_count_;

  typedef std::list<ScopedResource*> ResourceList;
  ResourceList unused_resources_;
  ResourceList busy_resources_;

  DISALLOW_COPY_AND_ASSIGN(ResourcePool);
};

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_POOL_H_

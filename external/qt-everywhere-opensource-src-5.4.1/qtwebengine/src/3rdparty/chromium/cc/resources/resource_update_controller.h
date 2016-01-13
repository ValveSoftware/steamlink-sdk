// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_UPDATE_CONTROLLER_H_
#define CC_RESOURCES_RESOURCE_UPDATE_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "cc/resources/resource_update_queue.h"

namespace base { class SingleThreadTaskRunner; }

namespace cc {

class ResourceProvider;

class ResourceUpdateControllerClient {
 public:
  virtual void ReadyToFinalizeTextureUpdates() = 0;

 protected:
  virtual ~ResourceUpdateControllerClient() {}
};

class CC_EXPORT ResourceUpdateController {
 public:
  static scoped_ptr<ResourceUpdateController> Create(
      ResourceUpdateControllerClient* client,
      base::SingleThreadTaskRunner* task_runner,
      scoped_ptr<ResourceUpdateQueue> queue,
      ResourceProvider* resource_provider) {
    return make_scoped_ptr(new ResourceUpdateController(
        client, task_runner, queue.Pass(), resource_provider));
  }
  static size_t MaxPartialTextureUpdates();

  virtual ~ResourceUpdateController();

  // Discard uploads to textures that were evicted on the impl thread.
  void DiscardUploadsToEvictedResources();

  void PerformMoreUpdates(base::TimeTicks time_limit);
  void Finalize();


  // Virtual for testing.
  virtual size_t UpdateMoreTexturesSize() const;
  virtual base::TimeTicks UpdateMoreTexturesCompletionTime();

 protected:
  ResourceUpdateController(ResourceUpdateControllerClient* client,
                           base::SingleThreadTaskRunner* task_runner,
                           scoped_ptr<ResourceUpdateQueue> queue,
                           ResourceProvider* resource_provider);

 private:
  static size_t MaxFullUpdatesPerTick(ResourceProvider* resource_provider);

  size_t MaxBlockingUpdates() const;

  void UpdateTexture(ResourceUpdate update);

  // This returns true when there were textures left to update.
  bool UpdateMoreTexturesIfEnoughTimeRemaining();
  void UpdateMoreTexturesNow();
  void OnTimerFired();

  ResourceUpdateControllerClient* client_;
  scoped_ptr<ResourceUpdateQueue> queue_;
  bool contents_textures_purged_;
  ResourceProvider* resource_provider_;
  base::TimeTicks time_limit_;
  size_t texture_updates_per_tick_;
  bool first_update_attempt_;
  base::SingleThreadTaskRunner* task_runner_;
  bool task_posted_;
  bool ready_to_finalize_;
  base::WeakPtrFactory<ResourceUpdateController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourceUpdateController);
};

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_UPDATE_CONTROLLER_H_

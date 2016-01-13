// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/resources/returned_resource.h"
#include "cc/resources/transferable_resource.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class DelegatedFrameResourceCollectionTest
    : public testing::Test,
      public DelegatedFrameResourceCollectionClient {
 protected:
  DelegatedFrameResourceCollectionTest() : resources_available_(false) {}

  virtual void SetUp() OVERRIDE { CreateResourceCollection(); }

  virtual void TearDown() OVERRIDE { DestroyResourceCollection(); }

  void CreateResourceCollection() {
    DCHECK(!resource_collection_);
    resource_collection_ = new DelegatedFrameResourceCollection;
    resource_collection_->SetClient(this);
  }

  void DestroyResourceCollection() {
    if (resource_collection_) {
      resource_collection_->SetClient(NULL);
      resource_collection_ = NULL;
    }
  }

  TransferableResourceArray CreateResourceArray() {
    TransferableResourceArray resources;
    TransferableResource resource;
    resource.id = 444;
    resources.push_back(resource);
    return resources;
  }

  virtual void UnusedResourcesAreAvailable() OVERRIDE {
    resources_available_ = true;
    resource_collection_->TakeUnusedResourcesForChildCompositor(
        &returned_resources_);
    if (!resources_available_closure_.is_null())
      resources_available_closure_.Run();
  }

  bool ReturnAndResetResourcesAvailable() {
    bool r = resources_available_;
    resources_available_ = false;
    return r;
  }

  scoped_refptr<DelegatedFrameResourceCollection> resource_collection_;
  bool resources_available_;
  ReturnedResourceArray returned_resources_;
  base::Closure resources_available_closure_;
};

// This checks that taking the return callback doesn't take extra refcounts,
// since it's sent to other threads.
TEST_F(DelegatedFrameResourceCollectionTest, NoRef) {
  // Start with one ref.
  EXPECT_TRUE(resource_collection_->HasOneRef());

  ReturnCallback return_callback =
      resource_collection_->GetReturnResourcesCallbackForImplThread();

  // Callback shouldn't take a ref since it's sent to other threads.
  EXPECT_TRUE(resource_collection_->HasOneRef());
}

void ReturnResourcesOnThread(ReturnCallback callback,
                             const ReturnedResourceArray& resources,
                             base::WaitableEvent* event) {
  callback.Run(resources);
  if (event)
    event->Wait();
}

// Tests that the ReturnCallback can run safely on threads even after the
// last references to the collection were dropped.
// Flaky: crbug.com/313441
TEST_F(DelegatedFrameResourceCollectionTest, Thread) {
  base::Thread thread("test thread");
  thread.Start();

  TransferableResourceArray resources = CreateResourceArray();
  resource_collection_->ReceivedResources(resources);
  resource_collection_->RefResources(resources);

  ReturnedResourceArray returned_resources;
  TransferableResource::ReturnResources(resources, &returned_resources);

  base::WaitableEvent event(false, false);

  {
    base::RunLoop run_loop;
    resources_available_closure_ = run_loop.QuitClosure();

    thread.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(
            &ReturnResourcesOnThread,
            resource_collection_->GetReturnResourcesCallbackForImplThread(),
            returned_resources,
            &event));

    run_loop.Run();
  }
  EXPECT_TRUE(ReturnAndResetResourcesAvailable());
  EXPECT_EQ(1u, returned_resources_.size());
  EXPECT_EQ(444u, returned_resources_[0].id);
  EXPECT_EQ(1, returned_resources_[0].count);
  returned_resources_.clear();

  // The event prevents the return resources callback from being deleted.
  // Destroy the last reference from this thread to the collection before
  // signaling the event, to ensure any reference taken by the callback, if any,
  // would be the last one.
  DestroyResourceCollection();
  event.Signal();

  CreateResourceCollection();
  resource_collection_->ReceivedResources(resources);
  resource_collection_->RefResources(resources);

  // Destroy the collection before we have a chance to run the return callback.
  ReturnCallback return_callback =
      resource_collection_->GetReturnResourcesCallbackForImplThread();
  resource_collection_->LoseAllResources();
  DestroyResourceCollection();

  EXPECT_TRUE(ReturnAndResetResourcesAvailable());
  EXPECT_EQ(1u, returned_resources_.size());
  EXPECT_EQ(444u, returned_resources_[0].id);
  EXPECT_EQ(1, returned_resources_[0].count);
  EXPECT_TRUE(returned_resources_[0].lost);
  returned_resources_.clear();

  base::WaitableEvent* null_event = NULL;
  thread.message_loop()->PostTask(FROM_HERE,
                                  base::Bind(&ReturnResourcesOnThread,
                                             return_callback,
                                             returned_resources,
                                             null_event));

  thread.Stop();
}

}  // namespace
}  // namespace cc

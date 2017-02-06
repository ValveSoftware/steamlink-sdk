// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/refcounted_keyed_service.h"

#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"

namespace impl {

// static
void RefcountedKeyedServiceTraits::Destruct(const RefcountedKeyedService* obj) {
  if (obj->task_runner_.get() != nullptr &&
      obj->task_runner_.get() != base::ThreadTaskRunnerHandle::Get()) {
    obj->task_runner_->DeleteSoon(FROM_HERE, obj);
  } else {
    delete obj;
  }
}

}  // namespace impl

RefcountedKeyedService::RefcountedKeyedService() : task_runner_(nullptr) {
}

RefcountedKeyedService::RefcountedKeyedService(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner) {
}

RefcountedKeyedService::~RefcountedKeyedService() {
}

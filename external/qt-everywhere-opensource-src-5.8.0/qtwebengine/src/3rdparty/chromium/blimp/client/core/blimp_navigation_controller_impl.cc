// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/blimp_navigation_controller_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/client/core/blimp_navigation_controller_delegate.h"

namespace blimp {
namespace client {

BlimpNavigationControllerImpl::BlimpNavigationControllerImpl(
    BlimpNavigationControllerDelegate* delegate)
    : delegate_(delegate), weak_ptr_factory_(this) {}

BlimpNavigationControllerImpl::~BlimpNavigationControllerImpl() = default;

void BlimpNavigationControllerImpl::LoadURL(const GURL& url) {
  current_url_ = url;
  // Temporary trick to ensure that the delegate is not invoked before this
  // method has finished executing. This enables tests to test the
  // asynchronous nature of the API.
  // TODO(shaktisahu): Remove this after integration with NavigationFeature.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&BlimpNavigationControllerImpl::NotifyDelegateURLLoaded,
                 weak_ptr_factory_.GetWeakPtr(), url));
}

const GURL& BlimpNavigationControllerImpl::GetURL() {
  return current_url_;
}

void BlimpNavigationControllerImpl::NotifyDelegateURLLoaded(const GURL& url) {
  delegate_->NotifyURLLoaded(url);
}

}  // namespace client
}  // namespace blimp

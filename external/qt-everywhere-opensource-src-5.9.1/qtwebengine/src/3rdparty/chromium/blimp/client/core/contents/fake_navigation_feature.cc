// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <blimp/client/core/contents/fake_navigation_feature.h>
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_task_runner_handle.h"
#include "url/gurl.h"

namespace blimp {
namespace client {

FakeNavigationFeature::FakeNavigationFeature() {}

FakeNavigationFeature::~FakeNavigationFeature() {}

void FakeNavigationFeature::NavigateToUrlText(int tab_id,
                                              const std::string& url_text) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FakeNavigationFeature::NotifyDelegateURLLoaded,
                            base::Unretained(this), tab_id, url_text));
}

void FakeNavigationFeature::NotifyDelegateURLLoaded(
    int tab_id,
    const std::string& url_text) {
  NavigationFeatureDelegate* delegate = FindDelegate(tab_id);
  delegate->OnUrlChanged(tab_id, GURL(url_text));
}

}  // namespace client
}  // namespace blimp

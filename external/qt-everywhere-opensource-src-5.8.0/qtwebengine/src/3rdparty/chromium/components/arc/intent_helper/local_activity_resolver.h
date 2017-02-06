// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_LOCAL_ACTIVITY_RESOLVER_H_
#define COMPONENTS_ARC_INTENT_HELPER_LOCAL_ACTIVITY_RESOLVER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/intent_helper/intent_filter.h"
#include "mojo/public/cpp/bindings/binding.h"

class GURL;

namespace arc {

class LocalActivityResolver : public base::RefCounted<LocalActivityResolver> {
 public:
  LocalActivityResolver();

  bool ShouldChromeHandleUrl(const GURL& url);
  void UpdateIntentFilters(mojo::Array<mojom::IntentFilterPtr> intent_filters);

 private:
  friend class base::RefCounted<LocalActivityResolver>;
  ~LocalActivityResolver();

  // List of intent filters from Android. Used to determine if Chrome should
  // handle a URL without handing off to Android.
  std::vector<IntentFilter> intent_filters_;
  DISALLOW_COPY_AND_ASSIGN(LocalActivityResolver);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_LOCAL_ACTIVITY_HELPER_H_

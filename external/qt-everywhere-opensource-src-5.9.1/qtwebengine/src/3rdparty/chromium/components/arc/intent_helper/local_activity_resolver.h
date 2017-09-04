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

class GURL;

namespace arc {

class LocalActivityResolver : public base::RefCounted<LocalActivityResolver> {
 public:
  LocalActivityResolver();

  // Returns true when |url| can only be handled by Chrome. Otherwise, which is
  // when there might be one or more ARC apps that can handle |url|, returns
  // false. This function synchronously checks the |url| without making any IPC
  // to ARC side. Note that this function only supports http and https. If url's
  // scheme is neither http nor https, the function immediately returns true
  // without checking the filters.
  bool ShouldChromeHandleUrl(const GURL& url);

  // Called when the list of intent filters on ARC side is updated.
  void UpdateIntentFilters(std::vector<mojom::IntentFilterPtr> intent_filters);

 private:
  friend class base::RefCounted<LocalActivityResolver>;
  ~LocalActivityResolver();

  // List of intent filters from Android. Used to determine if Chrome should
  // handle a URL without handing off to Android.
  std::vector<IntentFilter> intent_filters_;

  DISALLOW_COPY_AND_ASSIGN(LocalActivityResolver);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_LOCAL_ACTIVITY_RESOLVER_H_

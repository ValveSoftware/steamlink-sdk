// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_MOCK_NAVIGATION_FEATURE_DELEGATE_H_
#define BLIMP_CLIENT_FEATURE_MOCK_NAVIGATION_FEATURE_DELEGATE_H_

#include <string>

#include "blimp/client/feature/navigation_feature.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;
class SkBitmap;

namespace blimp {
namespace client {

class MockNavigationFeatureDelegate
    : public NavigationFeature::NavigationFeatureDelegate {
 public:
  MockNavigationFeatureDelegate();
  ~MockNavigationFeatureDelegate() override;

  MOCK_METHOD2(OnUrlChanged, void(int tab_id, const GURL& url));
  MOCK_METHOD2(OnFaviconChanged, void(int tab_id, const SkBitmap& favicon));
  MOCK_METHOD2(OnTitleChanged, void(int tab_id, const std::string& title));
  MOCK_METHOD2(OnLoadingChanged, void(int tab_id, bool loading));
  MOCK_METHOD2(OnPageLoadStatusUpdate, void(int tab_id, bool completed));
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_MOCK_NAVIGATION_FEATURE_DELEGATE_H_

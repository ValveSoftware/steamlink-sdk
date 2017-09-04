// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_FAKE_NAVIGATION_FEATURE_H_
#define BLIMP_CLIENT_CORE_CONTENTS_FAKE_NAVIGATION_FEATURE_H_

#include <string>

#include "blimp/client/core/contents/navigation_feature.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blimp {
namespace client {

class FakeNavigationFeature : public NavigationFeature {
 public:
  FakeNavigationFeature();
  ~FakeNavigationFeature();

  MOCK_METHOD1(Reload, void(int tab_id));
  MOCK_METHOD1(GoBack, void(int tab_id));
  MOCK_METHOD1(GoForward, void(int tab_id));

  void NavigateToUrlText(int tab_id, const std::string& url_text) override;

 private:
  // For mimicing asynchronous invocation of delegate from NavigateToUrlText.
  void NotifyDelegateURLLoaded(int tab_id, const std::string& url_text);

  DISALLOW_COPY_AND_ASSIGN(FakeNavigationFeature);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTENTS_FAKE_NAVIGATION_FEATURE_H_

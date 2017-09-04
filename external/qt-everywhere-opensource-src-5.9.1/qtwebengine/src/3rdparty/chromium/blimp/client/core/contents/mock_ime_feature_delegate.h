// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_MOCK_IME_FEATURE_DELEGATE_H_
#define BLIMP_CLIENT_CORE_CONTENTS_MOCK_IME_FEATURE_DELEGATE_H_

#include "blimp/client/core/contents/ime_feature.h"

#include <string>

#include "testing/gmock/include/gmock/gmock.h"

namespace blimp {
namespace client {

class MockImeFeatureDelegate : public client::ImeFeature::Delegate {
 public:
  MockImeFeatureDelegate();
  ~MockImeFeatureDelegate() override;

  MOCK_METHOD1(OnShowImeRequested,
               void(const ImeFeature::WebInputRequest& request));
  MOCK_METHOD0(OnHideImeRequested, void());
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTENTS_MOCK_IME_FEATURE_DELEGATE_H_

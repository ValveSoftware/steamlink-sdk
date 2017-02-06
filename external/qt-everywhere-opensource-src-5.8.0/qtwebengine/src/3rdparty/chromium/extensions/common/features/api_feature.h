// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_API_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_API_FEATURE_H_

#include <string>

#include "extensions/common/features/simple_feature.h"

namespace extensions {

class APIFeature : public SimpleFeature {
 public:
  APIFeature();
  ~APIFeature() override;

  // extensions::Feature:
  bool IsInternal() const override;

  std::string Parse(const base::DictionaryValue* value) override;

 private:
  bool internal_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_API_FEATURE_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_BASE_FEATURE_PROVIDER_H_
#define EXTENSIONS_COMMON_FEATURES_BASE_FEATURE_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/simple_feature.h"

namespace Base {
class DictionaryValue;
}

namespace extensions {

// Reads Features out of a simple JSON file description.
class BaseFeatureProvider : public FeatureProvider {
 public:
  typedef SimpleFeature*(*FeatureFactory)();

  // Creates a new BaseFeatureProvider. Pass null to |factory| to have the
  // provider create plain old Feature instances.
  BaseFeatureProvider(const base::DictionaryValue& root,
                      FeatureFactory factory);
  ~BaseFeatureProvider() override;

  // Gets the feature |feature_name|, if it exists.
  Feature* GetFeature(const std::string& feature_name) const override;
  Feature* GetParent(Feature* feature) const override;
  std::vector<Feature*> GetChildren(const Feature& parent) const override;

  const FeatureMap& GetAllFeatures() const override;

 private:
  std::map<std::string, std::unique_ptr<Feature>> features_;

  // Populated on first use.
  mutable std::vector<std::string> feature_names_;

  FeatureFactory factory_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_BASE_FEATURE_PROVIDER_H_

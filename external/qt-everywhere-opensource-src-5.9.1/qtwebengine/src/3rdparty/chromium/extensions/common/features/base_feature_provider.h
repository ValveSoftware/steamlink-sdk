// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_BASE_FEATURE_PROVIDER_H_
#define EXTENSIONS_COMMON_FEATURES_BASE_FEATURE_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "extensions/common/features/feature_provider.h"

namespace extensions {
class Feature;

// A FeatureProvider contains the mapping of all feature names specified in the
// _*_features.json files to the Feature classes. Look up a Feature by its name
// to determine whether or not it is available in a certain context.
// Subclasses implement the specific logic for how the features are populated;
// this class handles vending the features given the query.
// TODO(devlin): We could probably combine this and FeatureProvider, since both
// contain common functionality and neither are designed to be full
// implementations.
class BaseFeatureProvider : public FeatureProvider {
 public:
  ~BaseFeatureProvider() override;

  // Gets the feature |feature_name|, if it exists.
  Feature* GetFeature(const std::string& feature_name) const override;
  Feature* GetParent(Feature* feature) const override;
  std::vector<Feature*> GetChildren(const Feature& parent) const override;

  const FeatureMap& GetAllFeatures() const override;

 protected:
  BaseFeatureProvider();

  void AddFeature(base::StringPiece name, std::unique_ptr<Feature> feature);

  // Takes ownership. Used in preference to unique_ptr variant to reduce size
  // of generated code.
  void AddFeature(base::StringPiece name, Feature* feature);

 private:
  std::map<std::string, std::unique_ptr<Feature>> features_;

  DISALLOW_COPY_AND_ASSIGN(BaseFeatureProvider);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_BASE_FEATURE_PROVIDER_H_

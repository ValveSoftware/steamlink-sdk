// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/base_feature_provider.h"

#include <utility>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "extensions/common/features/feature.h"

namespace extensions {

BaseFeatureProvider::BaseFeatureProvider() {}
BaseFeatureProvider::~BaseFeatureProvider() {}

const FeatureMap& BaseFeatureProvider::GetAllFeatures() const {
  return features_;
}

Feature* BaseFeatureProvider::GetFeature(const std::string& name) const {
  FeatureMap::const_iterator iter = features_.find(name);
  if (iter != features_.end())
    return iter->second.get();
  else
    return nullptr;
}

Feature* BaseFeatureProvider::GetParent(Feature* feature) const {
  CHECK(feature);
  if (feature->no_parent())
    return nullptr;

  std::vector<std::string> split = base::SplitString(
      feature->name(), ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (split.size() < 2)
    return nullptr;
  split.pop_back();
  return GetFeature(base::JoinString(split, "."));
}

// Children of a given API are named starting with parent.name()+".", which
// means they'll be contiguous in the features_ std::map.
std::vector<Feature*> BaseFeatureProvider::GetChildren(const Feature& parent)
    const {
  std::string prefix = parent.name() + ".";
  const FeatureMap::const_iterator first_child = features_.lower_bound(prefix);

  // All children have names before (parent.name() + ('.'+1)).
  ++prefix.back();
  const FeatureMap::const_iterator after_children =
      features_.lower_bound(prefix);

  std::vector<Feature*> result;
  result.reserve(std::distance(first_child, after_children));
  for (FeatureMap::const_iterator it = first_child; it != after_children;
       ++it) {
    result.push_back(it->second.get());
  }
  return result;
}

void BaseFeatureProvider::AddFeature(base::StringPiece name,
                                     std::unique_ptr<Feature> feature) {
  features_[name.as_string()] = std::move(feature);
}

void BaseFeatureProvider::AddFeature(base::StringPiece name, Feature* feature) {
  features_[name.as_string()] = std::unique_ptr<Feature>(feature);
}

}  // namespace extensions

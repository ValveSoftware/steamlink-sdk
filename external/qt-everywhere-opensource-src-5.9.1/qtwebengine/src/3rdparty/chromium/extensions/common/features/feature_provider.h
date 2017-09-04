// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_FEATURE_PROVIDER_H_
#define EXTENSIONS_COMMON_FEATURES_FEATURE_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>


namespace extensions {

class Feature;

using FeatureMap = std::map<std::string, std::unique_ptr<Feature>>;

// Implemented by classes that can vend features.
class FeatureProvider {
 public:
  FeatureProvider() {}
  virtual ~FeatureProvider() {}

  //
  // Static helpers.
  //

  // Gets a FeatureProvider for a specific type, like "permission".
  static const FeatureProvider* GetByName(const std::string& name);

  // Directly access the common FeatureProvider types.
  // Each is equivalent to GetByName('featuretype').
  static const FeatureProvider* GetAPIFeatures();
  static const FeatureProvider* GetManifestFeatures();
  static const FeatureProvider* GetPermissionFeatures();
  static const FeatureProvider* GetBehaviorFeatures();

  // Directly get Features from the common FeatureProvider types.
  // Each is equivalent to GetByName('featuretype')->GetFeature(name).
  // NOTE: These functions may return |nullptr| in case corresponding JSON file
  // got corrupted.
  static const Feature* GetAPIFeature(const std::string& name);
  static const Feature* GetManifestFeature(const std::string& name);
  static const Feature* GetPermissionFeature(const std::string& name);
  static const Feature* GetBehaviorFeature(const std::string& name);

  //
  // Instance methods.
  //

  // Returns the feature with the specified name.
  virtual Feature* GetFeature(const std::string& name) const = 0;

  // Returns the parent feature of |feature|, or NULL if there isn't one.
  virtual Feature* GetParent(Feature* feature) const = 0;

  // Returns the features inside the |parent| namespace, recursively.
  virtual std::vector<Feature*> GetChildren(const Feature& parent) const = 0;

  // Returns a map containing all features described by this instance.
  virtual const FeatureMap& GetAllFeatures() const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_FEATURE_PROVIDER_H_

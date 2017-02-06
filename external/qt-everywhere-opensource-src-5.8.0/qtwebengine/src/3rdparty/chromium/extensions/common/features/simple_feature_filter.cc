// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/simple_feature_filter.h"

#include "extensions/common/features/simple_feature.h"

namespace extensions {

SimpleFeatureFilter::SimpleFeatureFilter(SimpleFeature* feature)
    : feature_(feature) {}

SimpleFeatureFilter::~SimpleFeatureFilter() {}

std::string SimpleFeatureFilter::Parse(const base::DictionaryValue* value) {
  return std::string();
}

Feature::Availability SimpleFeatureFilter::IsAvailableToContext(
    const Extension* extension,
    Feature::Context context,
    const GURL& url,
    Feature::Platform platform) const {
  return Feature::Availability(Feature::IS_AVAILABLE, std::string());
}

Feature::Availability SimpleFeatureFilter::IsAvailableToManifest(
    const std::string& extension_id,
    Manifest::Type type,
    Manifest::Location location,
    int manifest_version,
    Feature::Platform platform) const {
  return Feature::Availability(Feature::IS_AVAILABLE, std::string());
}

}  // namespace extensions

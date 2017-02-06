// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/complex_feature.h"

namespace extensions {

ComplexFeature::ComplexFeature(std::unique_ptr<FeatureList> features) {
  DCHECK_GT(features->size(), 0UL);
  features_.swap(*features);
  no_parent_ = features_[0]->no_parent();

#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  // Verify IsInternal and no_parent is consistent across all features.
  bool first_is_internal = features_[0]->IsInternal();
  for (FeatureList::const_iterator it = features_.begin() + 1;
       it != features_.end();
       ++it) {
    DCHECK(first_is_internal == (*it)->IsInternal())
        << "Complex feature must have consistent values of "
           "internal across all sub features.";
    DCHECK(no_parent_ == (*it)->no_parent())
        << "Complex feature must have consistent values of "
           "no_parent across all sub features.";
  }
#endif
}

ComplexFeature::~ComplexFeature() {
}

Feature::Availability ComplexFeature::IsAvailableToManifest(
    const std::string& extension_id,
    Manifest::Type type,
    Manifest::Location location,
    int manifest_version,
    Platform platform) const {
  Feature::Availability first_availability =
      features_[0]->IsAvailableToManifest(
          extension_id, type, location, manifest_version, platform);
  if (first_availability.is_available())
    return first_availability;

  for (FeatureList::const_iterator it = features_.begin() + 1;
       it != features_.end(); ++it) {
    Availability availability = (*it)->IsAvailableToManifest(
        extension_id, type, location, manifest_version, platform);
    if (availability.is_available())
      return availability;
  }
  // If none of the SimpleFeatures are available, we return the availability
  // info of the first SimpleFeature that was not available.
  return first_availability;
}

Feature::Availability ComplexFeature::IsAvailableToContext(
    const Extension* extension,
    Context context,
    const GURL& url,
    Platform platform) const {
  Feature::Availability first_availability =
      features_[0]->IsAvailableToContext(extension, context, url, platform);
  if (first_availability.is_available())
    return first_availability;

  for (FeatureList::const_iterator it = features_.begin() + 1;
       it != features_.end(); ++it) {
    Availability availability =
        (*it)->IsAvailableToContext(extension, context, url, platform);
    if (availability.is_available())
      return availability;
  }
  // If none of the SimpleFeatures are available, we return the availability
  // info of the first SimpleFeature that was not available.
  return first_availability;
}

bool ComplexFeature::IsIdInBlacklist(const std::string& extension_id) const {
  for (FeatureList::const_iterator it = features_.begin();
       it != features_.end();
       ++it) {
    if ((*it)->IsIdInBlacklist(extension_id))
      return true;
  }
  return false;
}

bool ComplexFeature::IsIdInWhitelist(const std::string& extension_id) const {
  for (FeatureList::const_iterator it = features_.begin();
       it != features_.end();
       ++it) {
    if ((*it)->IsIdInWhitelist(extension_id))
      return true;
  }
  return false;
}

bool ComplexFeature::IsInternal() const {
  // Constructor verifies that composed features are consistent, thus we can
  // return just the first feature's value.
  return features_[0]->IsInternal();
}

std::string ComplexFeature::GetAvailabilityMessage(AvailabilityResult result,
                                                   Manifest::Type type,
                                                   const GURL& url,
                                                   Context context) const {
  if (result == IS_AVAILABLE)
    return std::string();

  // TODO(justinlin): Form some kind of combined availabilities/messages from
  // SimpleFeatures.
  return features_[0]->GetAvailabilityMessage(result, type, url, context);
}

}  // namespace extensions

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_FILTER_H_
#define EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_FILTER_H_

#include <string>

#include "base/macros.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/manifest.h"

class GURL;

namespace base {
class DictionaryValue;
}

namespace extensions {

class SimpleFeature;

// A SimpleFeatureFilter can be attached to SimpleFeature objects to provide
// additional parsing and availability filtering behavior.
class SimpleFeatureFilter {
 public:
  explicit SimpleFeatureFilter(SimpleFeature* feature);
  virtual ~SimpleFeatureFilter();

  SimpleFeature* feature() const { return feature_; }

  // Parses any additional feature data that may be used by this filter.
  // Returns an error string on failure or the empty string on success.
  // The default implementation simply returns the empty string.
  virtual std::string Parse(const base::DictionaryValue* value);

  // Indicates whether or not the owning feature is available within a given
  // extensions context. The default implementation only affirms availability.
  virtual Feature::Availability IsAvailableToContext(
      const Extension* extension,
      Feature::Context context,
      const GURL& url,
      Feature::Platform platform) const;

  // Indicates whether or not the owning feature is available to a given
  // extension manifest. The default implementation only affirms availability.
  virtual Feature::Availability IsAvailableToManifest(
      const std::string& extension_id,
      Manifest::Type type,
      Manifest::Location location,
      int manifest_version,
      Feature::Platform platform) const;

 private:
  // The feature which owns this filter.
  SimpleFeature* feature_;

  DISALLOW_COPY_AND_ASSIGN(SimpleFeatureFilter);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_SIMPLE_FEATURE_FILTER_H_

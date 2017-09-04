// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/requirements_info.h"

#include <memory>

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

RequirementsInfo::RequirementsInfo(const Manifest* manifest)
    : webgl(false),
      npapi(false),
      window_shape(false) {
  // Before parsing requirements from the manifest, automatically default the
  // NPAPI plugin requirement based on whether it includes NPAPI plugins.
  const base::ListValue* list_value = NULL;
  npapi = manifest->GetList(keys::kPlugins, &list_value) &&
          !list_value->empty();
}

RequirementsInfo::~RequirementsInfo() {
}

// static
const RequirementsInfo& RequirementsInfo::GetRequirements(
    const Extension* extension) {
  RequirementsInfo* info = static_cast<RequirementsInfo*>(
      extension->GetManifestData(keys::kRequirements));

  // We should be guaranteed to have requirements, since they are parsed for all
  // extension types.
  CHECK(info);
  return *info;
}

RequirementsHandler::RequirementsHandler() {
}

RequirementsHandler::~RequirementsHandler() {
}

const std::vector<std::string> RequirementsHandler::PrerequisiteKeys() const {
  return SingleKey(keys::kPlugins);
}

const std::vector<std::string> RequirementsHandler::Keys() const {
  return SingleKey(keys::kRequirements);
}

bool RequirementsHandler::AlwaysParseForType(Manifest::Type type) const {
  return true;
}

bool RequirementsHandler::Parse(Extension* extension, base::string16* error) {
  std::unique_ptr<RequirementsInfo> requirements(
      new RequirementsInfo(extension->manifest()));

  if (!extension->manifest()->HasKey(keys::kRequirements)) {
    extension->SetManifestData(keys::kRequirements, requirements.release());
    return true;
  }

  const base::DictionaryValue* requirements_value = NULL;
  if (!extension->manifest()->GetDictionary(keys::kRequirements,
                                            &requirements_value)) {
    *error = base::ASCIIToUTF16(errors::kInvalidRequirements);
    return false;
  }

  for (base::DictionaryValue::Iterator iter(*requirements_value);
       !iter.IsAtEnd();
       iter.Advance()) {
    const base::DictionaryValue* requirement_value;
    if (!iter.value().GetAsDictionary(&requirement_value)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidRequirement, iter.key());
      return false;
    }

    if (iter.key() == "plugins") {
      for (base::DictionaryValue::Iterator plugin_iter(*requirement_value);
           !plugin_iter.IsAtEnd(); plugin_iter.Advance()) {
        bool plugin_required = false;
        if (!plugin_iter.value().GetAsBoolean(&plugin_required)) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidRequirement, iter.key());
          return false;
        }
        if (plugin_iter.key() == "npapi") {
          requirements->npapi = plugin_required;
        } else {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidRequirement, iter.key());
          return false;
        }
      }
    } else if (iter.key() == "3D") {
      const base::ListValue* features = NULL;
      if (!requirement_value->GetListWithoutPathExpansion("features",
                                                          &features) ||
          !features) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidRequirement, iter.key());
        return false;
      }

      for (base::ListValue::const_iterator feature_iter = features->begin();
           feature_iter != features->end(); ++feature_iter) {
        std::string feature;
        if ((*feature_iter)->GetAsString(&feature)) {
          if (feature == "webgl") {
            requirements->webgl = true;
          } else if (feature == "css3d") {
            // css3d is always available, so no check is needed, but no error is
            // generated.
          } else {
            *error = ErrorUtils::FormatErrorMessageUTF16(
                errors::kInvalidRequirement, iter.key());
            return false;
          }
        }
      }
    } else if (iter.key() == "window") {
      for (base::DictionaryValue::Iterator feature_iter(*requirement_value);
           !feature_iter.IsAtEnd(); feature_iter.Advance()) {
        bool feature_required = false;
        if (!feature_iter.value().GetAsBoolean(&feature_required)) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidRequirement, iter.key());
          return false;
        }
        if (feature_iter.key() == "shape") {
          requirements->window_shape = feature_required;
        } else {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidRequirement, iter.key());
          return false;
        }
      }
    } else {
      *error = base::ASCIIToUTF16(errors::kInvalidRequirements);
      return false;
    }
  }

  extension->SetManifestData(keys::kRequirements, requirements.release());
  return true;
}

}  // namespace extensions

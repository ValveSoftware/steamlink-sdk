// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_builder.h"

#include <utility>

#include "extensions/common/extension.h"

namespace extensions {

ExtensionBuilder::ExtensionBuilder()
    : location_(Manifest::UNPACKED),
      flags_(Extension::NO_FLAGS) {
}
ExtensionBuilder::~ExtensionBuilder() {}

ExtensionBuilder::ExtensionBuilder(ExtensionBuilder&& other)
    : path_(std::move(other.path_)),
      location_(other.location_),
      manifest_(std::move(other.manifest_)),
      flags_(other.flags_),
      id_(std::move(other.id_)) {}

ExtensionBuilder& ExtensionBuilder::operator=(ExtensionBuilder&& other) {
  path_ = std::move(other.path_);
  location_ = other.location_;
  manifest_ = std::move(other.manifest_);
  flags_ = other.flags_;
  id_ = std::move(other.id_);
  return *this;
}

scoped_refptr<Extension> ExtensionBuilder::Build() {
  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      path_,
      location_,
      *manifest_,
      flags_,
      id_,
      &error);
  CHECK_EQ("", error);
  return extension;
}

ExtensionBuilder& ExtensionBuilder::SetPath(const base::FilePath& path) {
  path_ = path;
  return *this;
}

ExtensionBuilder& ExtensionBuilder::SetLocation(Manifest::Location location) {
  location_ = location;
  return *this;
}

ExtensionBuilder& ExtensionBuilder::SetManifest(
    std::unique_ptr<base::DictionaryValue> manifest) {
  manifest_ = std::move(manifest);
  return *this;
}

ExtensionBuilder& ExtensionBuilder::MergeManifest(
    std::unique_ptr<base::DictionaryValue> manifest) {
  manifest_->MergeDictionary(manifest.get());
  return *this;
}

ExtensionBuilder& ExtensionBuilder::AddFlags(int init_from_value_flags) {
  flags_ |= init_from_value_flags;
  return *this;
}

ExtensionBuilder& ExtensionBuilder::SetID(const std::string& id) {
  id_ = id;
  return *this;
}

}  // namespace extensions

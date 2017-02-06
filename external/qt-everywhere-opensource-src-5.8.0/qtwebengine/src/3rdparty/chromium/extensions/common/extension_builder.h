// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_BUILDER_H_
#define EXTENSIONS_COMMON_EXTENSION_BUILDER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "extensions/common/manifest.h"
#include "extensions/common/value_builder.h"

namespace extensions {
class Extension;

// An easier way to create extensions than Extension::Create.  The
// constructor sets up some defaults which are customized using the
// methods.  The only method that must be called is SetManifest().
class ExtensionBuilder {
 public:
  ExtensionBuilder();
  ~ExtensionBuilder();

  // Move constructor and operator=.
  ExtensionBuilder(ExtensionBuilder&& other);
  ExtensionBuilder& operator=(ExtensionBuilder&& other);

  // Can only be called once, after which it's invalid to use the builder.
  // CHECKs that the extension was created successfully.
  scoped_refptr<Extension> Build();

  // Defaults to FilePath().
  ExtensionBuilder& SetPath(const base::FilePath& path);

  // Defaults to Manifest::UNPACKED.
  ExtensionBuilder& SetLocation(Manifest::Location location);

  ExtensionBuilder& SetManifest(
      std::unique_ptr<base::DictionaryValue> manifest);

  // Merge another manifest into the current manifest, with new keys taking
  // precedence.
  ExtensionBuilder& MergeManifest(
      std::unique_ptr<base::DictionaryValue> manifest);

  ExtensionBuilder& AddFlags(int init_from_value_flags);

  // Defaults to the default extension ID created in Extension::Create.
  ExtensionBuilder& SetID(const std::string& id);

 private:
  base::FilePath path_;
  Manifest::Location location_;
  std::unique_ptr<base::DictionaryValue> manifest_;
  int flags_;
  std::string id_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionBuilder);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EXTENSION_BUILDER_H_

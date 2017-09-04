// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_TEST_UTIL_H_
#define EXTENSIONS_COMMON_TEST_UTIL_H_

#include <string>

#include "base/memory/ref_counted.h"

namespace extensions {
class Extension;
class ExtensionBuilder;

namespace test_util {

// Adds an extension manifest to a builder.
ExtensionBuilder BuildExtension(ExtensionBuilder builder);

// Adds an app manifest to a builder.
ExtensionBuilder BuildApp(ExtensionBuilder builder);

// Creates an extension instance that can be attached to an ExtensionFunction
// before running it.
scoped_refptr<Extension> CreateEmptyExtension();

// Create an extension with a variable |id|, for tests that require multiple
// extensions side-by-side having distinct IDs.
scoped_refptr<Extension> CreateEmptyExtension(const std::string& id);

}  // namespace test_util
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_TEST_UTIL_H_

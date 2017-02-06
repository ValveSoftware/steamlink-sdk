// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_TEST_H_
#define EXTENSIONS_COMMON_MANIFEST_TEST_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class FilePath;
}

namespace extensions {

// Base class for tests that parse a manifest file.
class ManifestTest : public testing::Test {
 public:
  ManifestTest();
  ~ManifestTest() override;

 protected:
  // Helper class that simplifies creating methods that take either a filename
  // to a manifest or the manifest itself.
  class ManifestData {
   public:
    explicit ManifestData(const char* name);
    ManifestData(base::DictionaryValue* manifest, const char* name);
    explicit ManifestData(std::unique_ptr<base::DictionaryValue> manifest);
    explicit ManifestData(std::unique_ptr<base::DictionaryValue> manifest,
                          const char* name);
    // C++98 requires the copy constructor for a type to be visible if you
    // take a const-ref of a temporary for that type.  Since Manifest
    // contains a scoped_ptr, its implicit copy constructor is declared
    // Manifest(Manifest&) according to spec 12.8.5.  This breaks the first
    // requirement and thus you cannot use it with LoadAndExpectError() or
    // LoadAndExpectSuccess() easily.
    //
    // To get around this spec pedantry, we declare the copy constructor
    // explicitly.  It will never get invoked.
    ManifestData(const ManifestData& m);

    ~ManifestData();

    const std::string& name() const { return name_; };

    base::DictionaryValue* GetManifest(base::FilePath manifest_path,
                                       std::string* error) const;

   private:
    const std::string name_;
    mutable base::DictionaryValue* manifest_;
    mutable std::unique_ptr<base::DictionaryValue> manifest_holder_;
  };

  // Allows the test implementation to override a loaded test manifest's
  // extension ID. Useful for testing features behind a whitelist.
  virtual std::string GetTestExtensionID() const;

  // Returns the path in which to find test manifest data files, for example
  // extensions/test/data/manifest_tests.
  virtual base::FilePath GetTestDataDir();

  std::unique_ptr<base::DictionaryValue> LoadManifest(char const* manifest_name,
                                                      std::string* error);

  scoped_refptr<extensions::Extension> LoadExtension(
      const ManifestData& manifest,
      std::string* error,
      extensions::Manifest::Location location =
          extensions::Manifest::INTERNAL,
      int flags = extensions::Extension::NO_FLAGS);

  scoped_refptr<extensions::Extension> LoadAndExpectSuccess(
      const ManifestData& manifest,
      extensions::Manifest::Location location =
          extensions::Manifest::INTERNAL,
      int flags = extensions::Extension::NO_FLAGS);

  scoped_refptr<extensions::Extension> LoadAndExpectSuccess(
      char const* manifest_name,
      extensions::Manifest::Location location =
          extensions::Manifest::INTERNAL,
      int flags = extensions::Extension::NO_FLAGS);

  scoped_refptr<extensions::Extension> LoadAndExpectWarning(
      const ManifestData& manifest,
      const std::string& expected_error,
      extensions::Manifest::Location location =
          extensions::Manifest::INTERNAL,
      int flags = extensions::Extension::NO_FLAGS);

  scoped_refptr<extensions::Extension> LoadAndExpectWarning(
      char const* manifest_name,
      const std::string& expected_error,
      extensions::Manifest::Location location =
          extensions::Manifest::INTERNAL,
      int flags = extensions::Extension::NO_FLAGS);

  void VerifyExpectedError(extensions::Extension* extension,
                           const std::string& name,
                           const std::string& error,
                           const std::string& expected_error);

  void LoadAndExpectError(char const* manifest_name,
                          const std::string& expected_error,
                          extensions::Manifest::Location location =
                              extensions::Manifest::INTERNAL,
                          int flags = extensions::Extension::NO_FLAGS);

  void LoadAndExpectError(const ManifestData& manifest,
                          const std::string& expected_error,
                          extensions::Manifest::Location location =
                              extensions::Manifest::INTERNAL,
                          int flags = extensions::Extension::NO_FLAGS);

  void AddPattern(extensions::URLPatternSet* extent,
                  const std::string& pattern);

  // used to differentiate between calls to LoadAndExpectError,
  // LoadAndExpectWarning and LoadAndExpectSuccess via function RunTestcases.
  enum ExpectType {
    EXPECT_TYPE_ERROR,
    EXPECT_TYPE_WARNING,
    EXPECT_TYPE_SUCCESS
  };

  struct Testcase {
    const std::string manifest_filename_;
    std::string expected_error_; // only used for ExpectedError tests
    extensions::Manifest::Location location_;
    int flags_;

    Testcase(const std::string& manifest_filename,
             const std::string& expected_error,
             extensions::Manifest::Location location,
             int flags);

    Testcase(const std::string& manifest_filename,
             const std::string& expected_error);

    explicit Testcase(const std::string& manifest_filename);

    Testcase(const std::string& manifest_filename,
             extensions::Manifest::Location location,
             int flags);
  };

  void RunTestcases(const Testcase* testcases,
                    size_t num_testcases,
                    ExpectType type);

  void RunTestcase(const Testcase& testcase, ExpectType type);

  bool enable_apps_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ManifestTest);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_TEST_H_

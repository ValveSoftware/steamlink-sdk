// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "extensions/test/test_extensions_client.h"
#include "extensions/utility/unpacker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/zlib/google/zip.h"
#include "ui/base/l10n/l10n_util.h"

using base::ASCIIToUTF16;

namespace extensions {

namespace errors = manifest_errors;
namespace keys = manifest_keys;

class UnpackerTest : public testing::Test {
 public:
  ~UnpackerTest() override {
    VLOG(1) << "Deleting temp dir: " << temp_dir_.path().LossyDisplayName();
    VLOG(1) << temp_dir_.Delete();
  }

  void SetupUnpacker(const std::string& crx_name) {
    base::FilePath crx_path;
    ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &crx_path));
    crx_path = crx_path.AppendASCII("unpacker").AppendASCII(crx_name);
    ASSERT_TRUE(base::PathExists(crx_path)) << crx_path.value();

    // Try bots won't let us write into DIR_TEST_DATA, so we have to create
    // a temp folder to play in.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    base::FilePath unzipped_dir = temp_dir_.path().AppendASCII("unzipped");
    ASSERT_TRUE(zip::Unzip(crx_path, unzipped_dir))
        << "Failed to unzip " << crx_path.value() << " to "
        << unzipped_dir.value();

    unpacker_.reset(new Unpacker(temp_dir_.path(), unzipped_dir, std::string(),
                                 Manifest::INTERNAL, Extension::NO_FLAGS));
  }

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<Unpacker> unpacker_;
};

TEST_F(UnpackerTest, EmptyDefaultLocale) {
  SetupUnpacker("empty_default_locale.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kInvalidDefaultLocale),
            unpacker_->error_message());
}

TEST_F(UnpackerTest, HasDefaultLocaleMissingLocalesFolder) {
  SetupUnpacker("has_default_missing_locales.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kLocalesTreeMissing),
            unpacker_->error_message());
}

TEST_F(UnpackerTest, InvalidDefaultLocale) {
  SetupUnpacker("invalid_default_locale.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kInvalidDefaultLocale),
            unpacker_->error_message());
}

TEST_F(UnpackerTest, InvalidMessagesFile) {
  SetupUnpacker("invalid_messages_file.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_TRUE(base::MatchPattern(
      unpacker_->error_message(),
      ASCIIToUTF16(
          "*_locales?en_US?messages.json: Line: 2, column: 11,"
          " Syntax error.")))
      << unpacker_->error_message();
}

TEST_F(UnpackerTest, MissingDefaultData) {
  SetupUnpacker("missing_default_data.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kLocalesNoDefaultMessages),
            unpacker_->error_message());
}

TEST_F(UnpackerTest, MissingDefaultLocaleHasLocalesFolder) {
  SetupUnpacker("missing_default_has_locales.crx");
  const base::string16 kExpectedError =
      l10n_util::GetStringUTF16(
          IDS_EXTENSION_LOCALES_NO_DEFAULT_LOCALE_SPECIFIED);

  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(kExpectedError, unpacker_->error_message());
}

TEST_F(UnpackerTest, MissingMessagesFile) {
  SetupUnpacker("missing_messages_file.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_TRUE(
      base::MatchPattern(unpacker_->error_message(),
                         ASCIIToUTF16(errors::kLocalesMessagesFileMissing) +
                             ASCIIToUTF16("*_locales?en_US?messages.json")));
}

TEST_F(UnpackerTest, NoLocaleData) {
  SetupUnpacker("no_locale_data.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_EQ(ASCIIToUTF16(errors::kLocalesNoDefaultMessages),
            unpacker_->error_message());
}

TEST_F(UnpackerTest, GoodL10n) {
  SetupUnpacker("good_l10n.crx");
  EXPECT_TRUE(unpacker_->Run());
  EXPECT_TRUE(unpacker_->error_message().empty());
  ASSERT_EQ(2U, unpacker_->parsed_catalogs()->size());
}

TEST_F(UnpackerTest, NoL10n) {
  SetupUnpacker("no_l10n.crx");
  EXPECT_TRUE(unpacker_->Run());
  EXPECT_TRUE(unpacker_->error_message().empty());
  EXPECT_EQ(0U, unpacker_->parsed_catalogs()->size());
}

namespace {

// Inserts an illegal path into the browser images returned by
// TestExtensionsClient for any extension.
class IllegalImagePathInserter
    : public TestExtensionsClient::BrowserImagePathsFilter {
 public:
  IllegalImagePathInserter(TestExtensionsClient* client) : client_(client) {
    client_->AddBrowserImagePathsFilter(this);
  }

  virtual ~IllegalImagePathInserter() {
    client_->RemoveBrowserImagePathsFilter(this);
  }

  void Filter(const Extension* extension,
              std::set<base::FilePath>* paths) override {
    base::FilePath illegal_path =
        base::FilePath(base::FilePath::kParentDirectory)
            .AppendASCII(kTempExtensionName)
            .AppendASCII("product_logo_128.png");
    paths->insert(illegal_path);
  }

 private:
  TestExtensionsClient* client_;
};

}  // namespace

TEST_F(UnpackerTest, BadPathError) {
  const char kExpected[] = "Illegal path (absolute or relative with '..'): ";
  SetupUnpacker("good_package.crx");
  IllegalImagePathInserter inserter(
      static_cast<TestExtensionsClient*>(ExtensionsClient::Get()));

  EXPECT_FALSE(unpacker_->Run());
  EXPECT_TRUE(base::StartsWith(unpacker_->error_message(),
                               ASCIIToUTF16(kExpected),
                               base::CompareCase::INSENSITIVE_ASCII))
      << "Expected prefix: \"" << kExpected << "\", actual error: \""
      << unpacker_->error_message() << "\"";
}

TEST_F(UnpackerTest, ImageDecodingError) {
  const char kExpected[] = "Could not decode image: ";
  SetupUnpacker("bad_image.crx");
  EXPECT_FALSE(unpacker_->Run());
  EXPECT_TRUE(base::StartsWith(unpacker_->error_message(),
                               ASCIIToUTF16(kExpected),
                               base::CompareCase::INSENSITIVE_ASCII))
      << "Expected prefix: \"" << kExpected << "\", actual error: \""
      << unpacker_->error_message() << "\"";
}

}  // namespace extensions

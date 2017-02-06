// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/file_util.h"

#include <stddef.h>

#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "grit/extensions_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace extensions {

namespace {

scoped_refptr<Extension> LoadExtensionManifest(
    const base::DictionaryValue& manifest,
    const base::FilePath& manifest_dir,
    Manifest::Location location,
    int extra_flags,
    std::string* error) {
  scoped_refptr<Extension> extension =
      Extension::Create(manifest_dir, location, manifest, extra_flags, error);
  return extension;
}

scoped_refptr<Extension> LoadExtensionManifest(
    const std::string& manifest_value,
    const base::FilePath& manifest_dir,
    Manifest::Location location,
    int extra_flags,
    std::string* error) {
  JSONStringValueDeserializer deserializer(manifest_value);
  std::unique_ptr<base::Value> result = deserializer.Deserialize(NULL, error);
  if (!result.get())
    return NULL;
  CHECK_EQ(base::Value::TYPE_DICTIONARY, result->GetType());
  return LoadExtensionManifest(
      *base::DictionaryValue::From(std::move(result)).get(), manifest_dir,
      location, extra_flags, error);
}

}  // namespace

typedef testing::Test FileUtilTest;

TEST_F(FileUtilTest, InstallUninstallGarbageCollect) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // Create a source extension.
  std::string extension_id("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  std::string version("1.0");
  base::FilePath src = temp.path().AppendASCII(extension_id);
  ASSERT_TRUE(base::CreateDirectory(src));

  base::FilePath extension_content;
  base::CreateTemporaryFileInDir(src, &extension_content);
  ASSERT_TRUE(base::PathExists(extension_content));

  // Create a extensions tree.
  base::FilePath all_extensions = temp.path().AppendASCII("extensions");
  ASSERT_TRUE(base::CreateDirectory(all_extensions));

  // Install in empty directory. Should create parent directories as needed.
  base::FilePath version_1 =
      file_util::InstallExtension(src, extension_id, version, all_extensions);
  ASSERT_EQ(
      version_1.value(),
      all_extensions.AppendASCII(extension_id).AppendASCII("1.0_0").value());
  ASSERT_TRUE(base::DirectoryExists(version_1));
  ASSERT_TRUE(base::PathExists(version_1.Append(extension_content.BaseName())));

  // Should have moved the source.
  ASSERT_FALSE(base::DirectoryExists(src));

  // Install again. Should create a new one with different name.
  ASSERT_TRUE(base::CreateDirectory(src));
  base::FilePath version_2 =
      file_util::InstallExtension(src, extension_id, version, all_extensions);
  ASSERT_EQ(
      version_2.value(),
      all_extensions.AppendASCII(extension_id).AppendASCII("1.0_1").value());
  ASSERT_TRUE(base::DirectoryExists(version_2));

  // Should have moved the source.
  ASSERT_FALSE(base::DirectoryExists(src));

  // Install yet again. Should create a new one with a different name.
  ASSERT_TRUE(base::CreateDirectory(src));
  base::FilePath version_3 =
      file_util::InstallExtension(src, extension_id, version, all_extensions);
  ASSERT_EQ(
      version_3.value(),
      all_extensions.AppendASCII(extension_id).AppendASCII("1.0_2").value());
  ASSERT_TRUE(base::DirectoryExists(version_3));

  // Uninstall. Should remove entire extension subtree.
  file_util::UninstallExtension(all_extensions, extension_id);
  ASSERT_FALSE(base::DirectoryExists(version_1.DirName()));
  ASSERT_FALSE(base::DirectoryExists(version_2.DirName()));
  ASSERT_FALSE(base::DirectoryExists(version_3.DirName()));
  ASSERT_TRUE(base::DirectoryExists(all_extensions));
}

TEST_F(FileUtilTest, LoadExtensionWithValidLocales) {
  base::FilePath install_dir;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extension_with_locales");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      install_dir, Manifest::UNPACKED, Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get() != NULL);
  EXPECT_EQ("The first extension that I made.", extension->description());
}

TEST_F(FileUtilTest, LoadExtensionWithoutLocalesFolder) {
  base::FilePath install_dir;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extension_without_locales");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      install_dir, Manifest::UNPACKED, Extension::NO_FLAGS, &error));
  ASSERT_FALSE(extension.get() == NULL);
  EXPECT_TRUE(error.empty());
}

TEST_F(FileUtilTest, CheckIllegalFilenamesNoUnderscores) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.path().AppendASCII("some_dir");
  ASSERT_TRUE(base::CreateDirectory(src_path));

  std::string data = "{ \"name\": { \"message\": \"foobar\" } }";
  ASSERT_TRUE(base::WriteFile(
      src_path.AppendASCII("some_file.txt"), data.c_str(), data.length()));
  std::string error;
  EXPECT_TRUE(file_util::CheckForIllegalFilenames(temp.path(), &error));
}

TEST_F(FileUtilTest, CheckIllegalFilenamesOnlyReserved) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  const base::FilePath::CharType* folders[] = {
      extensions::kLocaleFolder, extensions::kPlatformSpecificFolder};

  for (size_t i = 0; i < arraysize(folders); i++) {
    base::FilePath src_path = temp.path().Append(folders[i]);
    ASSERT_TRUE(base::CreateDirectory(src_path));
  }

  std::string error;
  EXPECT_TRUE(file_util::CheckForIllegalFilenames(temp.path(), &error));
}

TEST_F(FileUtilTest, CheckIllegalFilenamesReservedAndIllegal) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.path().Append(extensions::kLocaleFolder);
  ASSERT_TRUE(base::CreateDirectory(src_path));

  src_path = temp.path().AppendASCII("_some_dir");
  ASSERT_TRUE(base::CreateDirectory(src_path));

  std::string error;
  EXPECT_FALSE(file_util::CheckForIllegalFilenames(temp.path(), &error));
}

// These tests do not work on Windows, because it is illegal to create a
// file/directory with a Windows reserved name. Because we cannot create a
// file that will cause the test to fail, let's skip the test.
#if !defined(OS_WIN)
TEST_F(FileUtilTest, CheckIllegalFilenamesDirectoryWindowsReserved) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.path().AppendASCII("aux");
  ASSERT_TRUE(base::CreateDirectory(src_path));

  std::string error;
  EXPECT_FALSE(
      file_util::CheckForWindowsReservedFilenames(temp.path(), &error));
}

TEST_F(FileUtilTest,
       CheckIllegalFilenamesWindowsReservedFilenameWithExtension) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.path().AppendASCII("some_dir");
  ASSERT_TRUE(base::CreateDirectory(src_path));

  std::string data = "{ \"name\": { \"message\": \"foobar\" } }";
  ASSERT_TRUE(base::WriteFile(src_path.AppendASCII("lpt1.txt"), data.c_str(),
                              data.length()));

  std::string error;
  EXPECT_FALSE(
      file_util::CheckForWindowsReservedFilenames(temp.path(), &error));
}
#endif

TEST_F(FileUtilTest, LoadExtensionGivesHelpfullErrorOnMissingManifest) {
  base::FilePath install_dir;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &install_dir));
  install_dir =
      install_dir.AppendASCII("file_util").AppendASCII("missing_manifest");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      install_dir, Manifest::UNPACKED, Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get() == NULL);
  ASSERT_FALSE(error.empty());
  ASSERT_STREQ("Manifest file is missing or unreadable.", error.c_str());
}

TEST_F(FileUtilTest, LoadExtensionGivesHelpfullErrorOnBadManifest) {
  base::FilePath install_dir;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &install_dir));
  install_dir =
      install_dir.AppendASCII("file_util").AppendASCII("bad_manifest");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      install_dir, Manifest::UNPACKED, Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get() == NULL);
  ASSERT_FALSE(error.empty());
  ASSERT_STREQ(
      "Manifest is not valid JSON.  "
      "Line: 2, column: 16, Syntax error.",
      error.c_str());
}

TEST_F(FileUtilTest, ValidateThemeUTF8) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // "aeo" with accents. Use http://0xcc.net/jsescape/ to decode them.
  std::string non_ascii_file = "\xC3\xA0\xC3\xA8\xC3\xB2.png";
  base::FilePath non_ascii_path =
      temp.path().Append(base::FilePath::FromUTF8Unsafe(non_ascii_file));
  base::WriteFile(non_ascii_path, "", 0);

  std::string kManifest = base::StringPrintf(
      "{ \"name\": \"Test\", \"version\": \"1.0\", "
      "  \"theme\": { \"images\": { \"theme_frame\": \"%s\" } }"
      "}",
      non_ascii_file.c_str());
  std::string error;
  scoped_refptr<Extension> extension = LoadExtensionManifest(
      kManifest, temp.path(), Manifest::UNPACKED, 0, &error);
  ASSERT_TRUE(extension.get()) << error;

  std::vector<extensions::InstallWarning> warnings;
  EXPECT_TRUE(file_util::ValidateExtension(extension.get(), &error, &warnings))
      << error;
  EXPECT_EQ(0U, warnings.size());
}

TEST_F(FileUtilTest, BackgroundScriptsMustExist) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetString("name", "test");
  value->SetString("version", "1");
  value->SetInteger("manifest_version", 1);

  base::ListValue* scripts = new base::ListValue();
  scripts->AppendString("foo.js");
  value->Set("background.scripts", scripts);

  std::string error;
  std::vector<extensions::InstallWarning> warnings;
  scoped_refptr<Extension> extension = LoadExtensionManifest(
      *value.get(), temp.path(), Manifest::UNPACKED, 0, &error);
  ASSERT_TRUE(extension.get()) << error;

  EXPECT_FALSE(
      file_util::ValidateExtension(extension.get(), &error, &warnings));
  EXPECT_EQ(
      l10n_util::GetStringFUTF8(IDS_EXTENSION_LOAD_BACKGROUND_SCRIPT_FAILED,
                                base::ASCIIToUTF16("foo.js")),
      error);
  EXPECT_EQ(0U, warnings.size());

  scripts->Clear();
  scripts->AppendString("http://google.com/foo.js");

  extension = LoadExtensionManifest(*value.get(), temp.path(),
                                    Manifest::UNPACKED, 0, &error);
  ASSERT_TRUE(extension.get()) << error;

  warnings.clear();
  EXPECT_FALSE(
      file_util::ValidateExtension(extension.get(), &error, &warnings));
  EXPECT_EQ(
      l10n_util::GetStringFUTF8(IDS_EXTENSION_LOAD_BACKGROUND_SCRIPT_FAILED,
                                base::ASCIIToUTF16("http://google.com/foo.js")),
      error);
  EXPECT_EQ(0U, warnings.size());
}

// Private key, generated by Chrome specifically for this test, and
// never used elsewhere.
const char private_key[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIICdgIBADANBgkqhkiG9w0BAQEFAASCAmAwggJcAgEAAoGBAKt02SR0FYaYy6fpW\n"
    "MAA+kU1BgK3d+OmmWfdr+JATIjhRkyeSF4lTd/71JQsyKqPzYkQPi3EeROWM+goTv\n"
    "EhJqq07q63BolpsFmlV+S4ny+sBA2B4aWwRYXlBWikdrQSA0mJMzvEHc6nKzBgXik\n"
    "QSVbyyBNAsxlDB9WaCxRVOpK3AgMBAAECgYBGvSPlrVtAOAQ2V8j9FqorKZA8SLPX\n"
    "IeJC/yzU3RB2nPMjI17aMOvrUHxJUhzMeh4jwabVvSzzDtKFozPGupW3xaI8sQdi2\n"
    "WWMTQIk/Q9HHDWoQ9qA6SwX2qWCc5SyjCKqVp78ye+000kqTJYjBsDgXeAlzKcx2B\n"
    "4GAAeWonDdkQJBANNb8wrqNWFn7DqyQTfELzcRTRnqQ/r1pdeJo6obzbnwGnlqe3t\n"
    "KhLjtJNIGrQg5iC0OVLWFuvPJs0t3z62A1ckCQQDPq2JZuwTwu5Pl4DJ0r9O1FdqN\n"
    "JgqPZyMptokCDQ3khLLGakIu+TqB9YtrzI69rJMSG2Egb+6McaDX+dh3XmR/AkB9t\n"
    "xJf6qDnmA2td/tMtTc0NOk8Qdg/fD8xbZ/YfYMnVoYYs9pQoilBaWRePDRNURMLYZ\n"
    "vHAI0Llmw7tj7jv17pAkEAz44uXRpjRKtllUIvi5pUENAHwDz+HvdpGH68jpU3hmb\n"
    "uOwrmnQYxaMReFV68Z2w9DcLZn07f7/R9Wn72z89CxwJAFsDoNaDes4h48bX7plct\n"
    "s9ACjmTwcCigZjN2K7AGv7ntCLF3DnV5dK0dTHNaAdD3SbY3jl29Rk2CwiURSX6Ee\n"
    "g==\n"
    "-----END PRIVATE KEY-----\n";

TEST_F(FileUtilTest, FindPrivateKeyFiles) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath src_path = temp.path().AppendASCII("some_dir");
  ASSERT_TRUE(base::CreateDirectory(src_path));

  ASSERT_TRUE(base::WriteFile(
      src_path.AppendASCII("a_key.pem"), private_key, arraysize(private_key)));
  ASSERT_TRUE(base::WriteFile(src_path.AppendASCII("second_key.pem"),
                              private_key,
                              arraysize(private_key)));
  // Shouldn't find a key with a different extension.
  ASSERT_TRUE(base::WriteFile(src_path.AppendASCII("key.diff_ext"),
                              private_key,
                              arraysize(private_key)));
  // Shouldn't find a key that isn't parsable.
  ASSERT_TRUE(base::WriteFile(src_path.AppendASCII("unparsable_key.pem"),
                              private_key,
                              arraysize(private_key) - 30));
  std::vector<base::FilePath> private_keys =
      file_util::FindPrivateKeyFiles(temp.path());
  EXPECT_EQ(2U, private_keys.size());
  EXPECT_THAT(private_keys,
              testing::Contains(src_path.AppendASCII("a_key.pem")));
  EXPECT_THAT(private_keys,
              testing::Contains(src_path.AppendASCII("second_key.pem")));
}

TEST_F(FileUtilTest, WarnOnPrivateKey) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  base::FilePath ext_path = temp.path().AppendASCII("ext_root");
  ASSERT_TRUE(base::CreateDirectory(ext_path));

  const char manifest[] =
      "{\n"
      "  \"name\": \"Test Extension\",\n"
      "  \"version\": \"1.0\",\n"
      "  \"manifest_version\": 2,\n"
      "  \"description\": \"The first extension that I made.\"\n"
      "}\n";
  ASSERT_TRUE(base::WriteFile(
      ext_path.AppendASCII("manifest.json"), manifest, strlen(manifest)));
  ASSERT_TRUE(base::WriteFile(
      ext_path.AppendASCII("a_key.pem"), private_key, strlen(private_key)));

  std::string error;
  scoped_refptr<Extension> extension(
      file_util::LoadExtension(ext_path,
                               "the_id",
                               Manifest::EXTERNAL_PREF,
                               Extension::NO_FLAGS,
                               &error));
  ASSERT_TRUE(extension.get()) << error;
  ASSERT_EQ(1u, extension->install_warnings().size());
  EXPECT_THAT(extension->install_warnings(),
              testing::ElementsAre(testing::Field(
                  &extensions::InstallWarning::message,
                  testing::ContainsRegex(
                      "extension includes the key file.*ext_root.a_key.pem"))));

  // Turn the warning into an error with ERROR_ON_PRIVATE_KEY.
  extension = file_util::LoadExtension(ext_path,
                                       "the_id",
                                       Manifest::EXTERNAL_PREF,
                                       Extension::ERROR_ON_PRIVATE_KEY,
                                       &error);
  EXPECT_FALSE(extension.get());
  EXPECT_THAT(error,
              testing::ContainsRegex(
                  "extension includes the key file.*ext_root.a_key.pem"));
}

// Try to install an extension with a zero-length icon file.
TEST_F(FileUtilTest, CheckZeroLengthAndMissingIconFile) {
  base::FilePath install_dir;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &install_dir));

  base::FilePath ext_dir =
      install_dir.AppendASCII("file_util").AppendASCII("bad_icon");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      ext_dir, Manifest::INTERNAL, Extension::NO_FLAGS, &error));
  ASSERT_FALSE(extension);
}

// Try to install an unpacked extension with a zero-length icon file.
TEST_F(FileUtilTest, CheckZeroLengthAndMissingIconFileUnpacked) {
  base::FilePath install_dir;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &install_dir));

  base::FilePath ext_dir =
      install_dir.AppendASCII("file_util").AppendASCII("bad_icon");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      ext_dir, Manifest::UNPACKED, Extension::NO_FLAGS, &error));
  EXPECT_FALSE(extension);
  EXPECT_EQ("Could not load extension icon 'missing-icon.png'.", error);
}

TEST_F(FileUtilTest, ExtensionURLToRelativeFilePath) {
#define URL_PREFIX "chrome-extension://extension-id/"
  struct TestCase {
    const char* url;
    const char* expected_relative_path;
  } test_cases[] = {
    {URL_PREFIX "simple.html", "simple.html"},
    {URL_PREFIX "directory/to/file.html", "directory/to/file.html"},
    {URL_PREFIX "escape%20spaces.html", "escape spaces.html"},
    {URL_PREFIX "%C3%9Cber.html",
     "\xC3\x9C"
     "ber.html"},
#if defined(OS_WIN)
    {URL_PREFIX "C%3A/simple.html", ""},
#endif
    {URL_PREFIX "////simple.html", "simple.html"},
    {URL_PREFIX "/simple.html", "simple.html"},
    {URL_PREFIX "\\simple.html", "simple.html"},
    {URL_PREFIX "\\\\foo\\simple.html", "foo/simple.html"},
    {URL_PREFIX "..%2f..%2fsimple.html", "..%2f..%2fsimple.html"},
  };
#undef URL_PREFIX

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    GURL url(test_cases[i].url);
    base::FilePath expected_path =
        base::FilePath::FromUTF8Unsafe(test_cases[i].expected_relative_path);
    base::FilePath actual_path =
        extensions::file_util::ExtensionURLToRelativeFilePath(url);
    EXPECT_FALSE(actual_path.IsAbsolute()) <<
      " For the path " << actual_path.value();
    EXPECT_EQ(expected_path.value(), actual_path.value()) <<
      " For the path " << url;
  }
}

TEST_F(FileUtilTest, ExtensionResourceURLToFilePath) {
  // Setup filesystem for testing.
  base::FilePath root_path;
  ASSERT_TRUE(base::CreateNewTempDirectory(
      base::FilePath::StringType(), &root_path));
  root_path = base::MakeAbsoluteFilePath(root_path);
  ASSERT_FALSE(root_path.empty());

  base::FilePath api_path = root_path.Append(FILE_PATH_LITERAL("apiname"));
  ASSERT_TRUE(base::CreateDirectory(api_path));

  const char data[] = "Test Data";
  base::FilePath resource_path = api_path.Append(FILE_PATH_LITERAL("test.js"));
  ASSERT_TRUE(base::WriteFile(resource_path, data, sizeof(data)));
  resource_path = api_path.Append(FILE_PATH_LITERAL("escape spaces.js"));
  ASSERT_TRUE(base::WriteFile(resource_path, data, sizeof(data)));
  resource_path = api_path.Append(FILE_PATH_LITERAL("escape spaces.js"));
  ASSERT_TRUE(base::WriteFile(resource_path, data, sizeof(data)));
  resource_path = api_path.Append(FILE_PATH_LITERAL("..%2f..%2fsimple.html"));
  ASSERT_TRUE(base::WriteFile(resource_path, data, sizeof(data)));

#ifdef FILE_PATH_USES_WIN_SEPARATORS
#define SEP "\\"
#else
#define SEP "/"
#endif
#define URL_PREFIX "chrome-extension-resource://"
  struct TestCase {
    const char* url;
    const base::FilePath::CharType* expected_path;
  } test_cases[] = {
      {URL_PREFIX "apiname/test.js", FILE_PATH_LITERAL("test.js")},
      {URL_PREFIX "/apiname/test.js", FILE_PATH_LITERAL("test.js")},
      // Test % escape
      {URL_PREFIX "apiname/%74%65st.js", FILE_PATH_LITERAL("test.js")},
      {URL_PREFIX "apiname/escape%20spaces.js",
       FILE_PATH_LITERAL("escape spaces.js")},
      // Test file does not exist.
      {URL_PREFIX "apiname/directory/to/file.js", NULL},
      // Test apiname/../../test.js
      {URL_PREFIX "apiname/../../test.js", FILE_PATH_LITERAL("test.js")},
      {URL_PREFIX "apiname/..%2F../test.js", NULL},
      {URL_PREFIX "apiname/f/../../../test.js", FILE_PATH_LITERAL("test.js")},
      {URL_PREFIX "apiname/f%2F..%2F..%2F../test.js", NULL},
      {URL_PREFIX "apiname/..%2f..%2fsimple.html",
       FILE_PATH_LITERAL("..%2f..%2fsimple.html")},
  };
#undef SEP
#undef URL_PREFIX

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    GURL url(test_cases[i].url);
    base::FilePath expected_path;
    if (test_cases[i].expected_path)
      expected_path = root_path.Append(FILE_PATH_LITERAL("apiname")).Append(
          test_cases[i].expected_path);
    base::FilePath actual_path =
        extensions::file_util::ExtensionResourceURLToFilePath(url, root_path);
    EXPECT_EQ(expected_path.value(), actual_path.value()) <<
      " For the path " << url;
  }
  // Remove temp files.
  ASSERT_TRUE(base::DeleteFile(root_path, true));
}

}  // namespace extensions

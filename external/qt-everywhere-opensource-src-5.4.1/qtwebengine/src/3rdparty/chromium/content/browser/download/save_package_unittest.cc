// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/download/save_package.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

#define FPL FILE_PATH_LITERAL
#if defined(OS_WIN)
#define HTML_EXTENSION ".htm"
// This second define is needed because MSVC is broken.
#define FPL_HTML_EXTENSION L".htm"
#else
#define HTML_EXTENSION ".html"
#define FPL_HTML_EXTENSION ".html"
#endif

namespace {

// This constant copied from save_package.cc.
#if defined(OS_WIN)
const uint32 kMaxFilePathLength = MAX_PATH - 1;
const uint32 kMaxFileNameLength = MAX_PATH - 1;
#elif defined(OS_POSIX)
const uint32 kMaxFilePathLength = PATH_MAX - 1;
const uint32 kMaxFileNameLength = NAME_MAX;
#endif

// Used to make long filenames.
std::string long_file_name(
    "EFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz01234567"
    "89ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz012345"
    "6789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz0123"
    "456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789a");

bool HasOrdinalNumber(const base::FilePath::StringType& filename) {
  base::FilePath::StringType::size_type r_paren_index =
      filename.rfind(FPL(')'));
  base::FilePath::StringType::size_type l_paren_index =
      filename.rfind(FPL('('));
  if (l_paren_index >= r_paren_index)
    return false;

  for (base::FilePath::StringType::size_type i = l_paren_index + 1;
       i != r_paren_index; ++i) {
    if (!IsAsciiDigit(filename[i]))
      return false;
  }

  return true;
}

}  // namespace

class SavePackageTest : public RenderViewHostImplTestHarness {
 public:
  bool GetGeneratedFilename(bool need_success_generate_filename,
                            const std::string& disposition,
                            const std::string& url,
                            bool need_htm_ext,
                            base::FilePath::StringType* generated_name) {
    SavePackage* save_package;
    if (need_success_generate_filename)
      save_package = save_package_success_.get();
    else
      save_package = save_package_fail_.get();
    return save_package->GenerateFileName(disposition, GURL(url), need_htm_ext,
                                          generated_name);
  }

  base::FilePath EnsureHtmlExtension(const base::FilePath& name) {
    return SavePackage::EnsureHtmlExtension(name);
  }

  base::FilePath EnsureMimeExtension(const base::FilePath& name,
                               const std::string& content_mime_type) {
    return SavePackage::EnsureMimeExtension(name, content_mime_type);
  }

  GURL GetUrlToBeSaved() {
    return save_package_success_->GetUrlToBeSaved();
  }

 protected:
  virtual void SetUp() {
    RenderViewHostImplTestHarness::SetUp();

    // Do the initialization in SetUp so contents() is initialized by
    // RenderViewHostImplTestHarness::SetUp.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    save_package_success_ = new SavePackage(contents(),
        temp_dir_.path().AppendASCII("testfile" HTML_EXTENSION),
        temp_dir_.path().AppendASCII("testfile_files"));

    // We need to construct a path that is *almost* kMaxFilePathLength long
    long_file_name.reserve(kMaxFilePathLength + long_file_name.length());
    while (long_file_name.length() < kMaxFilePathLength)
      long_file_name += long_file_name;
    long_file_name.resize(
        kMaxFilePathLength - 9 - temp_dir_.path().value().length());

    save_package_fail_ = new SavePackage(contents(),
        temp_dir_.path().AppendASCII(long_file_name + HTML_EXTENSION),
        temp_dir_.path().AppendASCII(long_file_name + "_files"));
  }

 private:
  // SavePackage for successfully generating file name.
  scoped_refptr<SavePackage> save_package_success_;
  // SavePackage for failed generating file name.
  scoped_refptr<SavePackage> save_package_fail_;

  base::ScopedTempDir temp_dir_;
};

static const struct {
  const char* disposition;
  const char* url;
  const base::FilePath::CharType* expected_name;
  bool need_htm_ext;
} kGeneratedFiles[] = {
  // We mainly focus on testing duplicated names here, since retrieving file
  // name from disposition and url has been tested in DownloadManagerTest.

  // No useful information in disposition or URL, use default.
  {"1.html", "http://www.savepage.com/",
    FPL("saved_resource") FPL_HTML_EXTENSION, true},

  // No duplicate occurs.
  {"filename=1.css", "http://www.savepage.com", FPL("1.css"), false},

  // No duplicate occurs.
  {"filename=1.js", "http://www.savepage.com", FPL("1.js"), false},

  // Append numbers for duplicated names.
  {"filename=1.css", "http://www.savepage.com", FPL("1(1).css"), false},

  // No duplicate occurs.
  {"filename=1(1).js", "http://www.savepage.com", FPL("1(1).js"), false},

  // Append numbers for duplicated names.
  {"filename=1.css", "http://www.savepage.com", FPL("1(2).css"), false},

  // Change number for duplicated names.
  {"filename=1(1).css", "http://www.savepage.com", FPL("1(3).css"), false},

  // No duplicate occurs.
  {"filename=1(11).css", "http://www.savepage.com", FPL("1(11).css"), false},

  // Test for case-insensitive file names.
  {"filename=readme.txt", "http://www.savepage.com",
                          FPL("readme.txt"), false},

  {"filename=readme.TXT", "http://www.savepage.com",
                          FPL("readme(1).TXT"), false},

  {"filename=READme.txt", "http://www.savepage.com",
                          FPL("readme(2).txt"), false},

  {"filename=Readme(1).txt", "http://www.savepage.com",
                          FPL("readme(3).txt"), false},
};

TEST_F(SavePackageTest, TestSuccessfullyGenerateSavePackageFilename) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kGeneratedFiles); ++i) {
    base::FilePath::StringType file_name;
    bool ok = GetGeneratedFilename(true,
                                   kGeneratedFiles[i].disposition,
                                   kGeneratedFiles[i].url,
                                   kGeneratedFiles[i].need_htm_ext,
                                   &file_name);
    ASSERT_TRUE(ok);
    EXPECT_EQ(kGeneratedFiles[i].expected_name, file_name);
  }
}

TEST_F(SavePackageTest, TestUnSuccessfullyGenerateSavePackageFilename) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kGeneratedFiles); ++i) {
    base::FilePath::StringType file_name;
    bool ok = GetGeneratedFilename(false,
                                   kGeneratedFiles[i].disposition,
                                   kGeneratedFiles[i].url,
                                   kGeneratedFiles[i].need_htm_ext,
                                   &file_name);
    ASSERT_FALSE(ok);
  }
}

// Crashing on Windows, see http://crbug.com/79365
#if defined(OS_WIN)
#define MAYBE_TestLongSavePackageFilename DISABLED_TestLongSavePackageFilename
#else
#define MAYBE_TestLongSavePackageFilename TestLongSavePackageFilename
#endif
TEST_F(SavePackageTest, MAYBE_TestLongSavePackageFilename) {
  const std::string base_url("http://www.google.com/");
  const std::string long_file = long_file_name + ".css";
  const std::string url = base_url + long_file;

  base::FilePath::StringType filename;
  // Test that the filename is successfully shortened to fit.
  ASSERT_TRUE(GetGeneratedFilename(true, std::string(), url, false, &filename));
  EXPECT_TRUE(filename.length() < long_file.length());
  EXPECT_FALSE(HasOrdinalNumber(filename));

  // Test that the filename is successfully shortened to fit, and gets an
  // an ordinal appended.
  ASSERT_TRUE(GetGeneratedFilename(true, std::string(), url, false, &filename));
  EXPECT_TRUE(filename.length() < long_file.length());
  EXPECT_TRUE(HasOrdinalNumber(filename));

  // Test that the filename is successfully shortened to fit, and gets a
  // different ordinal appended.
  base::FilePath::StringType filename2;
  ASSERT_TRUE(
      GetGeneratedFilename(true, std::string(), url, false, &filename2));
  EXPECT_TRUE(filename2.length() < long_file.length());
  EXPECT_TRUE(HasOrdinalNumber(filename2));
  EXPECT_NE(filename, filename2);
}

// Crashing on Windows, see http://crbug.com/79365
#if defined(OS_WIN)
#define MAYBE_TestLongSafePureFilename DISABLED_TestLongSafePureFilename
#else
#define MAYBE_TestLongSafePureFilename TestLongSafePureFilename
#endif
TEST_F(SavePackageTest, MAYBE_TestLongSafePureFilename) {
  const base::FilePath save_dir(FPL("test_dir"));
  const base::FilePath::StringType ext(FPL_HTML_EXTENSION);
  base::FilePath::StringType filename =
#if defined(OS_WIN)
      base::ASCIIToWide(long_file_name);
#else
      long_file_name;
#endif

  // Test that the filename + extension doesn't exceed kMaxFileNameLength
  uint32 max_path = SavePackage::GetMaxPathLengthForDirectory(save_dir);
  ASSERT_TRUE(SavePackage::GetSafePureFileName(save_dir, ext, max_path,
                                               &filename));
  EXPECT_TRUE(filename.length() <= kMaxFileNameLength-ext.length());
}

static const struct {
  const base::FilePath::CharType* page_title;
  const base::FilePath::CharType* expected_name;
} kExtensionTestCases[] = {
  // Extension is preserved if it is already proper for HTML.
  {FPL("filename.html"), FPL("filename.html")},
  {FPL("filename.HTML"), FPL("filename.HTML")},
  {FPL("filename.XHTML"), FPL("filename.XHTML")},
  {FPL("filename.xhtml"), FPL("filename.xhtml")},
  {FPL("filename.htm"), FPL("filename.htm")},
  // ".htm" is added if the extension is improper for HTML.
  {FPL("hello.world"), FPL("hello.world") FPL_HTML_EXTENSION},
  {FPL("hello.txt"), FPL("hello.txt") FPL_HTML_EXTENSION},
  {FPL("is.html.good"), FPL("is.html.good") FPL_HTML_EXTENSION},
  // ".htm" is added if the name doesn't have an extension.
  {FPL("helloworld"), FPL("helloworld") FPL_HTML_EXTENSION},
  {FPL("helloworld."), FPL("helloworld.") FPL_HTML_EXTENSION},
};

// Crashing on Windows, see http://crbug.com/79365
#if defined(OS_WIN)
#define MAYBE_TestEnsureHtmlExtension DISABLED_TestEnsureHtmlExtension
#else
#define MAYBE_TestEnsureHtmlExtension TestEnsureHtmlExtension
#endif
TEST_F(SavePackageTest, MAYBE_TestEnsureHtmlExtension) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kExtensionTestCases); ++i) {
    base::FilePath original = base::FilePath(kExtensionTestCases[i].page_title);
    base::FilePath expected =
        base::FilePath(kExtensionTestCases[i].expected_name);
    base::FilePath actual = EnsureHtmlExtension(original);
    EXPECT_EQ(expected.value(), actual.value()) << "Failed for page title: " <<
        kExtensionTestCases[i].page_title;
  }
}

// Crashing on Windows, see http://crbug.com/79365
#if defined(OS_WIN)
#define MAYBE_TestEnsureMimeExtension DISABLED_TestEnsureMimeExtension
#else
#define MAYBE_TestEnsureMimeExtension TestEnsureMimeExtension
#endif
TEST_F(SavePackageTest, MAYBE_TestEnsureMimeExtension) {
  static const struct {
    const base::FilePath::CharType* page_title;
    const base::FilePath::CharType* expected_name;
    const char* contents_mime_type;
  } kExtensionTests[] = {
    { FPL("filename.html"), FPL("filename.html"), "text/html" },
    { FPL("filename.htm"), FPL("filename.htm"), "text/html" },
    { FPL("filename.xhtml"), FPL("filename.xhtml"), "text/html" },
#if defined(OS_WIN)
    { FPL("filename"), FPL("filename.htm"), "text/html" },
#else  // defined(OS_WIN)
    { FPL("filename"), FPL("filename.html"), "text/html" },
#endif  // defined(OS_WIN)
    { FPL("filename.html"), FPL("filename.html"), "text/xml" },
    { FPL("filename.xml"), FPL("filename.xml"), "text/xml" },
    { FPL("filename"), FPL("filename.xml"), "text/xml" },
    { FPL("filename.xhtml"), FPL("filename.xhtml"),
      "application/xhtml+xml" },
    { FPL("filename.html"), FPL("filename.html"),
      "application/xhtml+xml" },
    { FPL("filename"), FPL("filename.xhtml"), "application/xhtml+xml" },
    { FPL("filename.txt"), FPL("filename.txt"), "text/plain" },
    { FPL("filename"), FPL("filename.txt"), "text/plain" },
    { FPL("filename.css"), FPL("filename.css"), "text/css" },
    { FPL("filename"), FPL("filename.css"), "text/css" },
    { FPL("filename.abc"), FPL("filename.abc"), "unknown/unknown" },
    { FPL("filename"), FPL("filename"), "unknown/unknown" },
  };
  for (uint32 i = 0; i < ARRAYSIZE_UNSAFE(kExtensionTests); ++i) {
    base::FilePath original = base::FilePath(kExtensionTests[i].page_title);
    base::FilePath expected = base::FilePath(kExtensionTests[i].expected_name);
    std::string mime_type(kExtensionTests[i].contents_mime_type);
    base::FilePath actual = EnsureMimeExtension(original, mime_type);
    EXPECT_EQ(expected.value(), actual.value()) << "Failed for page title: " <<
        kExtensionTests[i].page_title << " MIME:" << mime_type;
  }
}

// Test that the suggested names generated by SavePackage are reasonable:
// If the name is a URL, retrieve only the path component since the path name
// generation code will turn the entire URL into the file name leading to bad
// extension names. For example, a page with no title and a URL:
// http://www.foo.com/a/path/name.txt will turn into file:
// "http www.foo.com a path name.txt", when we want to save it as "name.txt".

static const struct SuggestedSaveNameTestCase {
  const char* page_url;
  const base::string16 page_title;
  const base::FilePath::CharType* expected_name;
  bool ensure_html_extension;
} kSuggestedSaveNames[] = {
  // Title overrides the URL.
  { "http://foo.com",
    base::ASCIIToUTF16("A page title"),
    FPL("A page title") FPL_HTML_EXTENSION,
    true
  },
  // Extension is preserved.
  { "http://foo.com",
    base::ASCIIToUTF16("A page title with.ext"),
    FPL("A page title with.ext"),
    false
  },
  // If the title matches the URL, use the last component of the URL.
  { "http://foo.com/bar",
    base::ASCIIToUTF16("foo.com/bar"),
    FPL("bar"),
    false
  },
  // If the title matches the URL, but there is no "filename" component,
  // use the domain.
  { "http://foo.com",
    base::ASCIIToUTF16("foo.com"),
    FPL("foo.com"),
    false
  },
  // Make sure fuzzy matching works.
  { "http://foo.com/bar",
    base::ASCIIToUTF16("foo.com/bar"),
    FPL("bar"),
    false
  },
  // A URL-like title that does not match the title is respected in full.
  { "http://foo.com",
    base::ASCIIToUTF16("http://www.foo.com/path/title.txt"),
    FPL("http   www.foo.com path title.txt"),
    false
  },
};

// Crashing on Windows, see http://crbug.com/79365
#if defined(OS_WIN)
#define MAYBE_TestSuggestedSaveNames DISABLED_TestSuggestedSaveNames
#else
#define MAYBE_TestSuggestedSaveNames TestSuggestedSaveNames
#endif
TEST_F(SavePackageTest, MAYBE_TestSuggestedSaveNames) {
  for (size_t i = 0; i < arraysize(kSuggestedSaveNames); ++i) {
    scoped_refptr<SavePackage> save_package(
        new SavePackage(contents(), base::FilePath(), base::FilePath()));
    save_package->page_url_ = GURL(kSuggestedSaveNames[i].page_url);
    save_package->title_ = kSuggestedSaveNames[i].page_title;

    base::FilePath save_name = save_package->GetSuggestedNameForSaveAs(
        kSuggestedSaveNames[i].ensure_html_extension,
        std::string(), std::string());
    EXPECT_EQ(kSuggestedSaveNames[i].expected_name, save_name.value()) <<
        "Test case " << i;
  }
}

static const base::FilePath::CharType* kTestDir =
    FILE_PATH_LITERAL("save_page");

// GetUrlToBeSaved method should return correct url to be saved.
TEST_F(SavePackageTest, TestGetUrlToBeSaved) {
  base::FilePath file_name(FILE_PATH_LITERAL("a.htm"));
  GURL url = URLRequestMockHTTPJob::GetMockUrl(
                 base::FilePath(kTestDir).Append(file_name));
  NavigateAndCommit(url);
  EXPECT_EQ(url, GetUrlToBeSaved());
}

// GetUrlToBeSaved method sould return actual url to be saved,
// instead of the displayed url used to view source of a page.
// Ex:GetUrlToBeSaved method should return http://www.google.com
// when user types view-source:http://www.google.com
TEST_F(SavePackageTest, TestGetUrlToBeSavedViewSource) {
  base::FilePath file_name(FILE_PATH_LITERAL("a.htm"));
  GURL view_source_url = URLRequestMockHTTPJob::GetMockViewSourceUrl(
                             base::FilePath(kTestDir).Append(file_name));
  GURL actual_url = URLRequestMockHTTPJob::GetMockUrl(
                        base::FilePath(kTestDir).Append(file_name));
  NavigateAndCommit(view_source_url);
  EXPECT_EQ(actual_url, GetUrlToBeSaved());
  EXPECT_EQ(view_source_url, contents()->GetLastCommittedURL());
}

}  // namespace content

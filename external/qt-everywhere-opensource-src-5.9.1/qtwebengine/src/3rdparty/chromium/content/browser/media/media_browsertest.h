// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_BROWSERTEST_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_BROWSERTEST_H_

#include <string>

#include "base/strings/string_split.h"
#include "content/public/test/content_browser_test.h"

namespace content {

class TitleWatcher;

// A base class for media related browser tests.
class MediaBrowserTest : public ContentBrowserTest {
 public:
  // Common test results.
  static const char kEnded[];
  static const char kError[];
  static const char kFailed[];

  // ContentBrowserTest implementation.
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // Runs a html page with a list of URL query parameters.
  // If http is true, the test starts a local http test server to load the test
  // page, otherwise a local file URL is loaded inside the content shell.
  // It uses RunTest() to check for expected test output.
  void RunMediaTestPage(const std::string& html_page,
                        const base::StringPairs& query_params,
                        const std::string& expected_title,
                        bool http);

  // Opens a URL and waits for the document title to match any of the waited for
  // titles. Returns the matching title.
  std::string RunTest(const GURL& gurl, const std::string& expected_title);

  // Adds titles that RunTest() should wait for.
  virtual void AddTitlesToAwait(content::TitleWatcher* title_watcher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_BROWSERTEST_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

// These tests simulate exploited renderer processes, which can fetch arbitrary
// resources from other websites, not constrained by the Same Origin Policy.  We
// are trying to verify that the renderer cannot fetch any cross-site document
// responses even when the Same Origin Policy is turned off inside the renderer.
class SiteIsolationStatsGathererBrowserTest : public ContentBrowserTest {
 public:
  SiteIsolationStatsGathererBrowserTest() {}
  ~SiteIsolationStatsGathererBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->Start());
    // Add a host resolver rule to map all outgoing requests to the test server.
    // This allows us to use "real" hostnames in URLs, which we can use to
    // create arbitrary SiteInstances.
    command_line->AppendSwitchASCII(
        switches::kHostResolverRules,
        "MAP * " + embedded_test_server()->host_port_pair().ToString() +
            ",EXCLUDE localhost");

    // Since we assume exploited renderer process, it can bypass the same origin
    // policy at will. Simulate that by passing the disable-web-security flag.
    command_line->AppendSwitch(switches::kDisableWebSecurity);
  }

  void InspectHistograms(const base::HistogramTester& histograms,
                         bool should_be_blocked,
                         const std::string& resource_name) {
    std::string bucket;
    int mime_type = 0;  // Hardcoded because histogram enums mustn't change.
    if (base::MatchPattern(resource_name, "*.html")) {
      bucket = "HTML";
      mime_type = 0;
    } else if (base::MatchPattern(resource_name, "*.xml")) {
      bucket = "XML";
      mime_type = 1;
    } else if (base::MatchPattern(resource_name, "*.json")) {
      bucket = "JSON";
      mime_type = 2;
    } else if (base::MatchPattern(resource_name, "*.txt")) {
      bucket = "Plain";
      mime_type = 3;
      if (base::MatchPattern(resource_name, "json*")) {
        bucket += ".JSON";
      } else if (base::MatchPattern(resource_name, "html*")) {
        bucket += ".HTML";
      } else if (base::MatchPattern(resource_name, "xml*")) {
        bucket += ".XML";
      }
    } else {
      FAIL();
    }
    FetchHistogramsFromChildProcesses();

    // A few histograms are incremented unconditionally.
    histograms.ExpectUniqueSample("SiteIsolation.AllResponses", 1, 1);
    base::HistogramTester::CountsMap expected_metrics;
    expected_metrics["SiteIsolation.XSD.DataLength"] = 1;
    expected_metrics["SiteIsolation.XSD.MimeType"] = 1;

    // Determine the appropriate conditionally-incremented histograms.
    std::string base = "SiteIsolation.XSD." + bucket;
    if (should_be_blocked) {
      expected_metrics[base + ".Blocked"] = 1;
      expected_metrics[base + ".Blocked.RenderableStatusCode2"] = 1;
    } else {
      expected_metrics[base + ".NotBlocked"] = 1;
      if (base::MatchPattern(resource_name, "*js.*")) {
        expected_metrics[base + ".NotBlocked.MaybeJS"] = 1;
      }
    }

    // Make sure that the expected metrics, and only those metrics, were
    // incremented.
    EXPECT_THAT(histograms.GetTotalCountsForPrefix("SiteIsolation.XSD."),
                testing::ContainerEq(expected_metrics))
        << "For resource_name=" << resource_name
        << ", should_be_blocked=" << should_be_blocked;

    EXPECT_THAT(histograms.GetAllSamples("SiteIsolation.XSD.MimeType"),
                testing::ElementsAre(base::Bucket(mime_type, 1)))
        << "The wrong mime type bucket was incremented.";
    if (should_be_blocked) {
      static_assert(13 == RESOURCE_TYPE_XHR, "Histogram enums mustn't change.");
      EXPECT_THAT(
          histograms.GetAllSamples(base + ".Blocked.RenderableStatusCode2"),
          testing::ElementsAre(base::Bucket(RESOURCE_TYPE_XHR, 1)))
          << "The wrong RenderableStatusCode2 bucket was incremented.";
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SiteIsolationStatsGathererBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SiteIsolationStatsGathererBrowserTest,
                       CrossSiteDocumentBlockingForMimeType) {
  // Load a page that issues illegal cross-site document requests to bar.com.
  // The page uses XHR to request HTML/XML/JSON documents from bar.com, and
  // inspects if any of them were successfully received. Currently, on illegal
  // access, the XHR requests should succeed, but the UMA histograms should
  // record that they would have been blocked. This test is only possible since
  // we run the browser without the same origin policy.
  GURL foo("http://foo.com/cross_site_document_request.html");

  NavigateToURL(shell(), foo);

  // Flush out existing histogram activity.
  FetchHistogramsFromChildProcesses();

  // The following are files under content/test/data/site_isolation. All
  // should be disallowed for cross site XHR under the document blocking policy.
  const char* blocked_resources[] = {
      "comment_valid.html",
      "html.txt",
      "html4_dtd.html",
      "html4_dtd.txt",
      "html5_dtd.html",
      "html5_dtd.txt",
      "json.txt",
      "valid.html",
      "valid.json",
      "valid.xml",
      "xml.txt",
  };

  for (const char* resource : blocked_resources) {
    SCOPED_TRACE(base::StringPrintf("... while testing page: %s", resource));
    base::HistogramTester histograms;

    bool was_blocked;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(), base::StringPrintf("sendRequest(\"%s\");", resource),
        &was_blocked));
    ASSERT_FALSE(was_blocked);

    InspectHistograms(histograms, true, resource);
  }

  // These files should be allowed for XHR under the document blocking policy.
  const char* allowed_resources[] = {"js.html",
                                     "comment_js.html",
                                     "js.xml",
                                     "js.json",
                                     "js.txt",
                                     "img.html",
                                     "img.xml",
                                     "img.json",
                                     "img.txt",
                                     "comment_js.html"};
  for (const char* resource : allowed_resources) {
    SCOPED_TRACE(base::StringPrintf("... while testing page: %s", resource));
    base::HistogramTester histograms;

    bool was_blocked;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(), base::StringPrintf("sendRequest(\"%s\");", resource),
        &was_blocked));
    ASSERT_FALSE(was_blocked);

    InspectHistograms(histograms, false, resource);
  }
}

IN_PROC_BROWSER_TEST_F(SiteIsolationStatsGathererBrowserTest,
                       CrossSiteDocumentBlockingForDifferentTargets) {
  // This webpage loads a cross-site HTML page in different targets such as
  // <img>,<link>,<embed>, etc. Since the requested document is blocked, and one
  // character string (' ') is returned instead, this tests that the renderer
  // does not crash even when it receives a response body which is " ", whose
  // length is different from what's described in "content-length" for such
  // different targets.

  // TODO(nick): Split up these cases, and add positive assertions here about
  // what actually happens in these various resource-block cases.
  GURL foo("http://foo.com/cross_site_document_request_target.html");
  NavigateToURL(shell(), foo);
}

}  // namespace content

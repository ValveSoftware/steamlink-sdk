// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/string_tokenizer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"

namespace content {

// TODO(xianglu): Enable other platforms with support. https://crbug.com/646083
#if defined(OS_ANDROID)
#define MAYBE_ShapeDetectionBrowserTest ShapeDetectionBrowserTest
#else
#define MAYBE_ShapeDetectionBrowserTest DISABLED_ShapeDetectionBrowserTest
#endif

namespace {

const char kFaceDetectionTestHtml[] = "/media/face_detection_test.html";

}  // namespace

// This class contains content_browsertests for Shape Detection API, which
// allows for generating bounding boxes for faces on still images..
class MAYBE_ShapeDetectionBrowserTest : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Specific flag to enable ShapeDetection and DOMRect API.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "ShapeDetection, GeometryInterfaces");
  }

 protected:
  void RunDetectFacesOnImageUrl(
      const std::string& image_path,
      const std::vector<std::vector<float>>& expected_results) {
    ASSERT_TRUE(embedded_test_server()->Start());

    const GURL html_url(embedded_test_server()->GetURL(kFaceDetectionTestHtml));
    const GURL image_url(embedded_test_server()->GetURL(image_path));
    NavigateToURL(shell(), html_url);
    const std::string js_command =
        "detectFacesOnImageUrl('" + image_url.spec() + "')";
    std::string response_string;
    ASSERT_TRUE(
        ExecuteScriptAndExtractString(shell(), js_command, &response_string));

    base::StringTokenizer outer_tokenizer(response_string, "#");
    std::vector<std::vector<float>> results;
    while (outer_tokenizer.GetNext()) {
      std::string s = outer_tokenizer.token().c_str();
      std::vector<float> res;
      base::StringTokenizer inner_tokenizer(s, ",");
      while (inner_tokenizer.GetNext())
        res.push_back(atof(inner_tokenizer.token().c_str()));
      results.push_back(res);
    }

    ASSERT_EQ(expected_results.size(), results.size()) << "Number of faces";
    for (size_t face_id = 0; face_id < results.size(); ++face_id) {
      const std::vector<float> expected_result = expected_results[face_id];
      const std::vector<float> result = results[face_id];
      for (size_t i = 0; i < 4; ++i)
        EXPECT_NEAR(expected_result[i], result[i], 0.1) << "At index " << i;
    }
  }
};

IN_PROC_BROWSER_TEST_F(MAYBE_ShapeDetectionBrowserTest,
                       DetectFacesOnImageWithNoFaces) {
  const std::string image_path = "/blank.jpg";
  const std::vector<std::vector<float>> expected_empty_results;
  RunDetectFacesOnImageUrl(image_path, expected_empty_results);
}

IN_PROC_BROWSER_TEST_F(MAYBE_ShapeDetectionBrowserTest,
                       DetectFacesOnImageWithOneFace) {
  const std::string image_path = "/single_face.jpg";
  const std::vector<float> expected_result = {68.640625, 102.96875, 171.5625,
                                              171.5625};
  const std::vector<std::vector<float>> expected_results = {expected_result};
  RunDetectFacesOnImageUrl(image_path, expected_results);
}

}  // namespace content

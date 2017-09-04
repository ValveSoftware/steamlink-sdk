// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/clear_site_data_throttle.h"

#include <memory>

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ClearSiteDataThrottleTest : public testing::Test {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    throttle_ = ClearSiteDataThrottle::CreateThrottleForNavigation(nullptr);
  }

  ClearSiteDataThrottle* GetThrottle() {
    return static_cast<ClearSiteDataThrottle*>(throttle_.get());
  }

 private:
  std::unique_ptr<NavigationThrottle> throttle_;
};

TEST_F(ClearSiteDataThrottleTest, ParseHeader) {
  struct TestCase {
    const char* header;
    bool cookies;
    bool storage;
    bool cache;
  } test_cases[] = {
      // One data type.
      {"{ \"types\": [\"cookies\"] }", true, false, false},
      {"{ \"types\": [\"storage\"] }", false, true, false},
      {"{ \"types\": [\"cache\"] }", false, false, true},

      // Two data types.
      {"{ \"types\": [\"cookies\", \"storage\"] }", true, true, false},
      {"{ \"types\": [\"cookies\", \"cache\"] }", true, false, true},
      {"{ \"types\": [\"storage\", \"cache\"] }", false, true, true},

      // Three data types.
      {"{ \"types\": [\"storage\", \"cache\", \"cookies\"] }", true, true,
       true},
      {"{ \"types\": [\"cache\", \"cookies\", \"storage\"] }", true, true,
       true},
      {"{ \"types\": [\"cookies\", \"storage\", \"cache\"] }", true, true,
       true},

      // Different formatting.
      {"  { \"types\":   [\"cookies\" ]}", true, false, false},

      // Duplicates.
      {"{ \"types\": [\"cookies\", \"cookies\"] }", true, false, false},

      // Other entries in the dictionary.
      {"{ \"types\": [\"storage\"], \"other_params\": {} }", false, true,
       false},

      // Unknown types are ignored, but we still proceed with the deletion for
      // those that we recognize.
      {"{ \"types\": [\"cache\", \"foo\"] }", false, false, true},
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.header);

    bool actual_cookies;
    bool actual_storage;
    bool actual_cache;

    std::vector<ClearSiteDataThrottle::ConsoleMessage> messages;

    EXPECT_TRUE(GetThrottle()->ParseHeader(test_case.header, &actual_cookies,
                                           &actual_storage, &actual_cache,
                                           &messages));

    EXPECT_EQ(test_case.cookies, actual_cookies);
    EXPECT_EQ(test_case.storage, actual_storage);
    EXPECT_EQ(test_case.cache, actual_cache);
  }
}

TEST_F(ClearSiteDataThrottleTest, InvalidHeader) {
  struct TestCase {
    const char* header;
    const char* console_message;
  } test_cases[] = {
      {"", "Not a valid JSON.\n"},
      {"\"unclosed quote", "Not a valid JSON.\n"},
      {"\"some text\"", "Expecting a JSON dictionary with a 'types' field.\n"},
      {"{ \"field\" : {} }",
       "Expecting a JSON dictionary with a 'types' field.\n"},
      {"{ \"types\" : [ \"passwords\" ] }",
       "Invalid type: \"passwords\".\n"
       "No valid types specified in the 'types' field.\n"},
      {"{ \"types\" : [ [ \"list in a list\" ] ] }",
       "Invalid type: [\"list in a list\"].\n"
       "No valid types specified in the 'types' field.\n"},
      {"{ \"types\" : [ \"кукис\", \"сторидж\", \"кэш\" ]",
       "Must only contain ASCII characters.\n"}};

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.header);

    bool actual_cookies;
    bool actual_storage;
    bool actual_cache;

    std::vector<ClearSiteDataThrottle::ConsoleMessage> messages;

    EXPECT_FALSE(GetThrottle()->ParseHeader(test_case.header, &actual_cookies,
                                            &actual_storage, &actual_cache,
                                            &messages));

    std::string multiline_message;
    for (const auto& message : messages) {
      EXPECT_EQ(CONSOLE_MESSAGE_LEVEL_ERROR, message.level);
      multiline_message += message.text + "\n";
    }

    EXPECT_EQ(test_case.console_message, multiline_message);
  }
}

}  // namespace content

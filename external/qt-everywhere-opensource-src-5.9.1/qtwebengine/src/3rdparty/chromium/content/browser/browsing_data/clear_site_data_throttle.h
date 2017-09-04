// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_DATA_CLEAR_SITE_DATA_THROTTLE_H_
#define CONTENT_BROWSER_BROWSING_DATA_CLEAR_SITE_DATA_THROTTLE_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/console_message_level.h"
#include "url/gurl.h"

namespace content {

class NavigationHandle;

// This throttle parses the Clear-Site-Data header and executes the clearing
// of browsing data. The navigation is delayed until the header is parsed and,
// if valid, until the browsing data are deleted. See the W3C working draft at
// https://www.w3.org/TR/clear-site-data/.
class CONTENT_EXPORT ClearSiteDataThrottle : public NavigationThrottle {
 public:
  struct ConsoleMessage {
    GURL url;
    std::string text;
    ConsoleMessageLevel level;
  };

  static std::unique_ptr<NavigationThrottle> CreateThrottleForNavigation(
      NavigationHandle* handle);

  ~ClearSiteDataThrottle() override;

  // NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;

 private:
  friend class ClearSiteDataFuzzerTest;
  friend class ClearSiteDataThrottleTest;
  FRIEND_TEST_ALL_PREFIXES(ClearSiteDataThrottleTest, ParseHeader);
  FRIEND_TEST_ALL_PREFIXES(ClearSiteDataThrottleTest, InvalidHeader);

  explicit ClearSiteDataThrottle(NavigationHandle* navigation_handle);

  // Scans for the first occurrence of the 'Clear-Site-Data' header, calls
  // ParseHeader() to parse it, and requests the actual data clearing. This is
  // the common logic of WillRedirectRequest() and WillProcessResponse().
  void HandleHeader();

  // Parses the value of the 'Clear-Site-Data' header and outputs whether
  // the header requests to |clear_cookies|, |clear_storage|, and |clear_cache|.
  // The |messages| vector will be filled with messages to be output in the
  // console. Returns true if parsing was successful.
  bool ParseHeader(const std::string& header,
                   bool* clear_cookies,
                   bool* clear_storage,
                   bool* clear_cache,
                   std::vector<ConsoleMessage>* messages);

  // Signals that a parsing and deletion task was finished.
  void TaskFinished();

  // Cached console messages to be output when the RenderFrameHost is ready.
  std::vector<ConsoleMessage> messages_;
  GURL current_url_;

  // Whether we are currently waiting for a callback that data clearing has
  // been completed;
  bool clearing_in_progress_;

  // The time when the last clearing operation started. Used when clearing
  // finishes to compute the duration.
  base::TimeTicks clearing_started_;

  // Needed for asynchronous parsing and deletion tasks.
  base::WeakPtrFactory<ClearSiteDataThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClearSiteDataThrottle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_DATA_CLEAR_SITE_DATA_THROTTLE_H_

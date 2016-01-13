// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SAVABLE_RESOURCES_H_
#define CONTENT_RENDERER_SAVABLE_RESOURCES_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "url/gurl.h"

namespace blink {
class WebElement;
class WebString;
class WebView;
}

// A collection of operations that access the underlying WebKit DOM directly.
namespace content {

// Structure for storage the result of getting all savable resource links
// for current page. The consumer of the SavableResourcesResult is responsible
// for keeping these pointers valid for the lifetime of the
// SavableResourcesResult instance.
struct SavableResourcesResult {
  // vector which contains all savable links of sub resource.
  std::vector<GURL>* resources_list;
  // vector which contains corresponding all referral links of sub resource,
  // it matched with links one by one.
  std::vector<GURL>* referrer_urls_list;
  // and the corresponding referrer policies.
  std::vector<blink::WebReferrerPolicy>* referrer_policies_list;
  // vector which contains all savable links of main frame and sub frames.
  std::vector<GURL>* frames_list;

  // Constructor.
  SavableResourcesResult(
      std::vector<GURL>* resources_list,
      std::vector<GURL>* referrer_urls_list,
      std::vector<blink::WebReferrerPolicy>* referrer_policies_list,
      std::vector<GURL>* frames_list)
      : resources_list(resources_list),
        referrer_urls_list(referrer_urls_list),
        referrer_policies_list(referrer_policies_list),
        frames_list(frames_list) { }

 private:
  DISALLOW_COPY_AND_ASSIGN(SavableResourcesResult);
};

// Get all savable resource links from current webview, include main frame
// and sub-frame. After collecting all savable resource links, this function
// will send those links to embedder. Return value indicates whether we get
// all saved resource links successfully.
CONTENT_EXPORT bool GetAllSavableResourceLinksForCurrentPage(
    blink::WebView* view,
    const GURL& page_url,
    SavableResourcesResult* savable_resources_result,
    const char** savable_schemes);

// Returns the value in an elements resource url attribute. For IMG, SCRIPT or
// INPUT TYPE=image, returns the value in "src". For LINK TYPE=text/css, returns
// the value in "href". For BODY, TABLE, TR, TD, returns the value in
// "background". For BLOCKQUOTE, Q, DEL, INS, returns the value in "cite"
// attribute. Otherwise returns a null WebString.
CONTENT_EXPORT blink::WebString GetSubResourceLinkFromElement(
    const blink::WebElement& element);

}  // namespace content

#endif  // CONTENT_RENDERER_SAVABLE_RESOURCES_H_

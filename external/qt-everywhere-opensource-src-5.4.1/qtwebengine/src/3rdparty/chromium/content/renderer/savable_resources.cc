// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/savable_resources.h"

#include <set>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebElementCollection.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebNodeList.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebNodeList;
using blink::WebString;
using blink::WebVector;
using blink::WebView;

namespace content {
namespace {

// Structure for storage the unique set of all savable resource links for
// making sure that no duplicated resource link in final result. The consumer
// of the SavableResourcesUniqueCheck is responsible for keeping these pointers
// valid for the lifetime of the SavableResourcesUniqueCheck instance.
struct SavableResourcesUniqueCheck {
  // Unique set of all sub resource links.
  std::set<GURL>* resources_set;
  // Unique set of all frame links.
  std::set<GURL>* frames_set;
  // Collection of all frames we go through when getting all savable resource
  // links.
  std::vector<WebFrame*>* frames;

  SavableResourcesUniqueCheck()
      : resources_set(NULL),
        frames_set(NULL),
        frames(NULL) {}

  SavableResourcesUniqueCheck(std::set<GURL>* resources_set,
      std::set<GURL>* frames_set, std::vector<WebFrame*>* frames)
      : resources_set(resources_set),
        frames_set(frames_set),
        frames(frames) {}
};

// Get all savable resource links from current element. One element might
// have more than one resource link. It is possible to have some links
// in one CSS stylesheet.
void GetSavableResourceLinkForElement(
    const WebElement& element,
    const WebDocument& current_doc,
    SavableResourcesUniqueCheck* unique_check,
    SavableResourcesResult* result) {

  // Handle frame and iframe tag.
  if (element.hasTagName("iframe") ||
      element.hasTagName("frame")) {
    WebFrame* sub_frame = WebLocalFrame::fromFrameOwnerElement(element);
    if (sub_frame)
      unique_check->frames->push_back(sub_frame);
    return;
  }

  // Check whether the node has sub resource URL or not.
  WebString value = GetSubResourceLinkFromElement(element);
  if (value.isNull())
    return;
  // Get absolute URL.
  GURL u = current_doc.completeURL(value);
  // ignore invalid URL
  if (!u.is_valid())
    return;
  // Ignore those URLs which are not standard protocols. Because FTP
  // protocol does no have cache mechanism, we will skip all
  // sub-resources if they use FTP protocol.
  if (!u.SchemeIsHTTPOrHTTPS() && !u.SchemeIs("file"))
    return;
  // Ignore duplicated resource link.
  if (!unique_check->resources_set->insert(u).second)
    return;
  result->resources_list->push_back(u);
  // Insert referrer for above new resource link.
  result->referrer_urls_list->push_back(GURL());
  result->referrer_policies_list->push_back(blink::WebReferrerPolicyDefault);
}

// Get all savable resource links from current WebFrameImpl object pointer.
void GetAllSavableResourceLinksForFrame(WebFrame* current_frame,
    SavableResourcesUniqueCheck* unique_check,
    SavableResourcesResult* result,
    const char** savable_schemes) {
  // Get current frame's URL.
  GURL current_frame_url = current_frame->document().url();

  // If url of current frame is invalid, ignore it.
  if (!current_frame_url.is_valid())
    return;

  // If url of current frame is not a savable protocol, ignore it.
  bool is_valid_protocol = false;
  for (int i = 0; savable_schemes[i] != NULL; ++i) {
    if (current_frame_url.SchemeIs(savable_schemes[i])) {
      is_valid_protocol = true;
      break;
    }
  }
  if (!is_valid_protocol)
    return;

  // If find same frame we have recorded, ignore it.
  if (!unique_check->frames_set->insert(current_frame_url).second)
    return;

  // Get current using document.
  WebDocument current_doc = current_frame->document();
  // Go through all descent nodes.
  WebElementCollection all = current_doc.all();
  // Go through all elements in this frame.
  for (WebElement element = all.firstItem(); !element.isNull();
       element = all.nextItem()) {
    GetSavableResourceLinkForElement(element,
                                     current_doc,
                                     unique_check,
                                     result);
  }
}

}  // namespace

WebString GetSubResourceLinkFromElement(const WebElement& element) {
  const char* attribute_name = NULL;
  if (element.hasHTMLTagName("img") ||
      element.hasHTMLTagName("script")) {
    attribute_name = "src";
  } else if (element.hasHTMLTagName("input")) {
    const WebInputElement input = element.toConst<WebInputElement>();
    if (input.isImageButton()) {
      attribute_name = "src";
    }
  } else if (element.hasHTMLTagName("body") ||
             element.hasHTMLTagName("table") ||
             element.hasHTMLTagName("tr") ||
             element.hasHTMLTagName("td")) {
    attribute_name = "background";
  } else if (element.hasHTMLTagName("blockquote") ||
             element.hasHTMLTagName("q") ||
             element.hasHTMLTagName("del") ||
             element.hasHTMLTagName("ins")) {
    attribute_name = "cite";
  } else if (element.hasHTMLTagName("link")) {
    // If the link element is not linked to css, ignore it.
    if (LowerCaseEqualsASCII(element.getAttribute("type"), "text/css")) {
      // TODO(jnd): Add support for extracting links of sub-resources which
      // are inside style-sheet such as @import, url(), etc.
      // See bug: http://b/issue?id=1111667.
      attribute_name = "href";
    }
  }
  if (!attribute_name)
    return WebString();
  WebString value = element.getAttribute(WebString::fromUTF8(attribute_name));
  // If value has content and not start with "javascript:" then return it,
  // otherwise return NULL.
  if (!value.isNull() && !value.isEmpty() &&
      !StartsWithASCII(value.utf8(), "javascript:", false))
    return value;

  return WebString();
}

// Get all savable resource links from current webview, include main
// frame and sub-frame
bool GetAllSavableResourceLinksForCurrentPage(WebView* view,
    const GURL& page_url, SavableResourcesResult* result,
    const char** savable_schemes) {
  WebFrame* main_frame = view->mainFrame();
  if (!main_frame)
    return false;

  std::set<GURL> resources_set;
  std::set<GURL> frames_set;
  std::vector<WebFrame*> frames;
  SavableResourcesUniqueCheck unique_check(&resources_set,
                                           &frames_set,
                                           &frames);

  GURL main_page_gurl(main_frame->document().url());

  // Make sure we are saving same page between embedder and webkit.
  // If page has being navigated, embedder will get three empty vector,
  // which will make the saving page job ended.
  if (page_url != main_page_gurl)
    return true;

  // First, process main frame.
  frames.push_back(main_frame);

  // Check all resource in this page, include sub-frame.
  for (int i = 0; i < static_cast<int>(frames.size()); ++i) {
    // Get current frame's all savable resource links.
    GetAllSavableResourceLinksForFrame(frames[i], &unique_check, result,
                                       savable_schemes);
  }

  // Since frame's src can also point to sub-resources link, so it is possible
  // that some URLs in frames_list are also in resources_list. For those
  // URLs, we will remove it from frame_list, only keep them in resources_list.
  for (std::set<GURL>::iterator it = frames_set.begin();
       it != frames_set.end(); ++it) {
    // Append unique frame source to savable frame list.
    if (resources_set.find(*it) == resources_set.end())
      result->frames_list->push_back(*it);
  }

  return true;
}

}  // namespace content

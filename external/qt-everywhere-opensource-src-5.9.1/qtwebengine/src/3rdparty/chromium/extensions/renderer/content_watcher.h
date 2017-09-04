// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_CONTENT_WATCHER_H_
#define EXTENSIONS_RENDERER_CONTENT_WATCHER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "third_party/WebKit/public/platform/WebVector.h"

namespace blink {
class WebFrame;
class WebLocalFrame;
class WebString;
}

namespace extensions {
class Dispatcher;
class Extension;
class NativeHandler;

// Watches the content of WebFrames to notify extensions when they match various
// patterns.  This class tracks the set of relevant patterns (set by
// ExtensionMsg_WatchPages) and the set that match on each WebFrame, and sends a
// ExtensionHostMsg_OnWatchedPageChange whenever a RenderView's set changes.
//
// There's one ContentWatcher per Dispatcher rather than per RenderView because
// WebFrames can move between RenderViews through adoptNode.
// TODO(devlin): This class isn't OOPI-safe.
class ContentWatcher {
 public:
  ContentWatcher();
  ~ContentWatcher();

  // Handler for ExtensionMsg_WatchPages.
  void OnWatchPages(const std::vector<std::string>& css_selectors);

  // Uses WebDocument::watchCSSSelectors to watch the selectors in
  // css_selectors_ and get a callback into DidMatchCSS() whenever the set of
  // matching selectors in |frame| changes.
  void DidCreateDocumentElement(blink::WebLocalFrame* frame);

  // Records that |newly_matching_selectors| have started matching on |*frame|,
  // and |stopped_matching_selectors| have stopped matching.
  void DidMatchCSS(
      blink::WebLocalFrame* frame,
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors);

 private:
  // Given that we saw a change in the CSS selectors that |changed_frame|
  // matched, tell the browser about the new set of matching selectors in its
  // top-level page.  We filter this so that if an extension were to be granted
  // activeTab permission on that top-level page, we only send CSS selectors for
  // frames that it could run on.
  void NotifyBrowserOfChange(blink::WebLocalFrame* changed_frame) const;

  // If any of these selectors match on a page, we need to send an
  // ExtensionHostMsg_OnWatchedPageChange back to the browser.
  blink::WebVector<blink::WebString> css_selectors_;

  // Maps live WebFrames to the set of CSS selectors they match. Blink sends
  // back diffs, which we apply to these sets.
  std::map<blink::WebFrame*, std::set<std::string> > matching_selectors_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_CONTENT_WATCHER_H_

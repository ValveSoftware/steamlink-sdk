// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFrameContentDumper_h
#define WebFrameContentDumper_h

#include "public/platform/WebCommon.h"

namespace blink {

class WebLocalFrame;
class WebView;
class WebString;

// Functions in this class should only be used for
// testing purposes.
// The exceptions to this rule are tracked in http://crbug.com/585164.
class WebFrameContentDumper {
 public:
  // Control of layoutTreeAsText output
  enum LayoutAsTextControl {
    LayoutAsTextNormal = 0,
    LayoutAsTextDebug = 1 << 0,
    LayoutAsTextPrinting = 1 << 1,
    LayoutAsTextWithLineTrees = 1 << 2
  };
  typedef unsigned LayoutAsTextControls;

  // Returns the contents of this frame as a string.  If the text is
  // longer than maxChars, it will be clipped to that length.
  //
  // If there is room, subframe text will be recursively appended. Each
  // frame will be separated by an empty line.
  // TODO(dglazkov): WebFrameContentDumper should only be used for
  // testing purposes and this function is being deprecated.
  // Don't add new callsites, please.
  // See http://crbug.com/585164 for details.
  BLINK_EXPORT static WebString deprecatedDumpFrameTreeAsText(WebLocalFrame*,
                                                              size_t maxChars);

  // Dumps the contents of of a WebView as text, starting from the main
  // frame and recursively appending every subframe, separated by an
  // empty line.
  BLINK_EXPORT static WebString dumpWebViewAsText(WebView*, size_t maxChars);

  // Returns HTML text for the contents of this frame, generated
  // from the DOM.
  BLINK_EXPORT static WebString dumpAsMarkup(WebLocalFrame*);

  // Returns a text representation of the render tree.  This method is used
  // to support layout tests.
  BLINK_EXPORT static WebString dumpLayoutTreeAsText(
      WebLocalFrame*,
      LayoutAsTextControls toShow = LayoutAsTextNormal);
};

}  // namespace blink

#endif

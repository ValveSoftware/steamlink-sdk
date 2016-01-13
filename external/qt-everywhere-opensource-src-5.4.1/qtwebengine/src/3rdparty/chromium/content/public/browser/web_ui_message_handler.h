// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_MESSAGE_HANDLER_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_MESSAGE_HANDLER_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"

class GURL;
class WebUIBrowserTest;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace content {

class WebUI;
class WebUIImpl;

// Messages sent from the DOM are forwarded via the WebUI to handler
// classes. These objects are owned by WebUI and destroyed when the
// host is destroyed.
class CONTENT_EXPORT WebUIMessageHandler {
 public:
  WebUIMessageHandler() : web_ui_(NULL) {}
  virtual ~WebUIMessageHandler() {}

 protected:
  FRIEND_TEST_ALL_PREFIXES(WebUIMessageHandlerTest, ExtractIntegerValue);
  FRIEND_TEST_ALL_PREFIXES(WebUIMessageHandlerTest, ExtractDoubleValue);
  FRIEND_TEST_ALL_PREFIXES(WebUIMessageHandlerTest, ExtractStringValue);

  // Helper methods:

  // Extract an integer value from a list Value.
  static bool ExtractIntegerValue(const base::ListValue* value, int* out_int);

  // Extract a floating point (double) value from a list Value.
  static bool ExtractDoubleValue(const base::ListValue* value,
                                 double* out_value);

  // Extract a string value from a list Value.
  static base::string16 ExtractStringValue(const base::ListValue* value);

  // This is where subclasses specify which messages they'd like to handle and
  // perform any additional initialization.. At this point web_ui() will return
  // the associated WebUI object.
  virtual void RegisterMessages() = 0;

  // Returns the attached WebUI for this handler.
  WebUI* web_ui() const { return web_ui_; }

  // Sets the attached WebUI - exposed to subclasses for testing purposes.
  void set_web_ui(WebUI* web_ui) { web_ui_ = web_ui; }

 private:
  // Provide external classes access to web_ui() and set_web_ui().
  friend class WebUIImpl;
  friend class ::WebUIBrowserTest;

  WebUI* web_ui_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_MESSAGE_HANDLER_H_

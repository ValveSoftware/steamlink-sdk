// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBFILEUTILITIES_IMPL_H_
#define CONTENT_CHILD_WEBFILEUTILITIES_IMPL_H_

#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebFileInfo.h"
#include "third_party/WebKit/public/platform/WebFileUtilities.h"

namespace content {

class CONTENT_EXPORT WebFileUtilitiesImpl
    : NON_EXPORTED_BASE(public blink::WebFileUtilities) {
 public:
  WebFileUtilitiesImpl();
  virtual ~WebFileUtilitiesImpl();

  // WebFileUtilities methods:
  virtual bool getFileInfo(const blink::WebString& path,
                           blink::WebFileInfo& result);
  virtual blink::WebString directoryName(const blink::WebString& path);
  virtual blink::WebString baseName(const blink::WebString& path);
  virtual blink::WebURL filePathToURL(const blink::WebString& path);

  void set_sandbox_enabled(bool sandbox_enabled) {
    sandbox_enabled_ = sandbox_enabled;
  }

 protected:
  bool sandbox_enabled_;
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBFILEUTILITIES_IMPL_H_

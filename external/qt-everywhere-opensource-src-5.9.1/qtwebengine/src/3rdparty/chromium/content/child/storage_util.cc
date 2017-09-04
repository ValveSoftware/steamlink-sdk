// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/storage_util.h"

#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

GURL WebSecurityOriginToGURL(const blink::WebSecurityOrigin& security_origin) {
  // "file:///" URLs navigated to by the user may have "isLocal" set,
  // which stringify as "null" by default. Previous code that sent
  // origins from Blink to Chromium via DatabaseIdentifier would ignore
  // this, so we mimic that behavior here.
  // TODO(jsbell): Eliminate this. https://crbug.com/591482
  if (security_origin.protocol().utf8() == "file" &&
      security_origin.host().utf8() == "" && security_origin.port() == 0) {
    return GURL("file:///");
  }
  return url::Origin(security_origin).GetURL();
}

}  // namespace content

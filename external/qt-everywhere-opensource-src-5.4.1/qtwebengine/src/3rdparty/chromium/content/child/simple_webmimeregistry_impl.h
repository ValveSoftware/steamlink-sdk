// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SIMPLE_WEBMIMEREGISTRY_IMPL_H_
#define CONTENT_CHILD_SIMPLE_WEBMIMEREGISTRY_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebMimeRegistry.h"

namespace content {

class CONTENT_EXPORT SimpleWebMimeRegistryImpl :
    NON_EXPORTED_BASE(public blink::WebMimeRegistry) {
 public:
  SimpleWebMimeRegistryImpl() {}
  virtual ~SimpleWebMimeRegistryImpl() {}

  // Convert a WebString to ASCII, falling back on an empty string in the case
  // of a non-ASCII string.
  static std::string ToASCIIOrEmpty(const blink::WebString& string);

  // WebMimeRegistry methods:
  virtual blink::WebMimeRegistry::SupportsType supportsMIMEType(
      const blink::WebString&);
  virtual blink::WebMimeRegistry::SupportsType supportsImageMIMEType(
      const blink::WebString&);
  virtual blink::WebMimeRegistry::SupportsType supportsJavaScriptMIMEType(
      const blink::WebString&);
  virtual blink::WebMimeRegistry::SupportsType supportsMediaMIMEType(
      const blink::WebString&,
      const blink::WebString&,
      const blink::WebString&);
  virtual bool supportsMediaSourceMIMEType(const blink::WebString&,
                                           const blink::WebString&);
  virtual bool supportsEncryptedMediaMIMEType(const blink::WebString&,
                                              const blink::WebString&,
                                              const blink::WebString&);
  virtual blink::WebMimeRegistry::SupportsType supportsNonImageMIMEType(
      const blink::WebString&);
  virtual blink::WebString mimeTypeForExtension(const blink::WebString&);
  virtual blink::WebString wellKnownMimeTypeForExtension(
      const blink::WebString&);
  virtual blink::WebString mimeTypeFromFile(const blink::WebString&);
};

}  // namespace content

#endif  // CONTENT_CHILD_SIMPLE_WEBMIMEREGISTRY_IMPL_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_URL_UTILS_H_
#define CONTENT_PUBLIC_COMMON_URL_UTILS_H_

#include <stddef.h>         // For size_t

#include "build/build_config.h"
#include "content/common/content_export.h"

class GURL;

namespace content {

// Null terminated list of schemes that are savable. This function can be
// invoked on any thread.
CONTENT_EXPORT const char* const* GetSavableSchemes();

// Returns true if the url has a scheme for WebUI.  See also
// WebUIControllerFactory::UseWebUIForURL in the browser process.
CONTENT_EXPORT bool HasWebUIScheme(const GURL& url);

// Check whether we can do the saving page operation for the specified URL.
CONTENT_EXPORT bool IsSavableURL(const GURL& url);

#if defined(OS_ANDROID)
// Set a new max size for URL's that we are willing to accept in the browser
// process.
// Should not be used except by Android WebView for backwards compatibility.
// Should be called early in start up before forking child processes.
//
// This is for supporting legacy android apps, android webview needs to
// support loading long data urls. In chrome, the url length is limited to 2M to
// prevent renderer process DOS-ing the browser process. So only for android
// webview, increase the limit to 20M, which is a large enough value to satisfy
// legacy app compatibility requirements.
CONTENT_EXPORT void SetMaxURLChars(size_t max_chars);
#endif

// The maximum number of characters in the URL that we're willing to accept
// in the browser process. It is set low enough to avoid damage to the browser
// but high enough that a web site can abuse location.hash for a little storage.
// We have different values for "max accepted" and "max displayed" because
// a data: URI may be legitimately massive, but the full URI would kill all
// known operating systems if you dropped it into a UI control.
CONTENT_EXPORT size_t GetMaxURLChars();

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_URL_UTILS_H_

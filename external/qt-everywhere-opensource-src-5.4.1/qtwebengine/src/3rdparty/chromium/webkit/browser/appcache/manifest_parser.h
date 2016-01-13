// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a port of ManifestParser.h from WebKit/WebCore/loader/appcache.

/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WEBKIT_BROWSER_APPCACHE_MANIFEST_PARSER_H_
#define WEBKIT_BROWSER_APPCACHE_MANIFEST_PARSER_H_

#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/appcache/appcache_interfaces.h"

class GURL;

namespace appcache {

struct WEBKIT_STORAGE_BROWSER_EXPORT Manifest {
  Manifest();
  ~Manifest();

  base::hash_set<std::string> explicit_urls;
  NamespaceVector intercept_namespaces;
  NamespaceVector fallback_namespaces;
  NamespaceVector online_whitelist_namespaces;
  bool online_whitelist_all;
  bool did_ignore_intercept_namespaces;
};

enum ParseMode {
  PARSE_MANIFEST_PER_STANDARD,
  PARSE_MANIFEST_ALLOWING_INTERCEPTS
};

WEBKIT_STORAGE_BROWSER_EXPORT bool ParseManifest(
    const GURL& manifest_url,
    const char* data,
    int length,
    ParseMode parse_mode,
    Manifest& manifest);

}  // namespace appcache

#endif  // WEBKIT_BROWSER_APPCACHE_MANIFEST_PARSER_H_

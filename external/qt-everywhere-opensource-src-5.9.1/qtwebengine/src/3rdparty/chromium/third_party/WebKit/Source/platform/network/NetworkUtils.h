// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NetworkUtils_h
#define NetworkUtils_h

#include "platform/PlatformExport.h"
#include "wtf/Forward.h"

namespace blink {

class KURL;
class SharedBuffer;

namespace NetworkUtils {

enum PrivateRegistryFilter {
  IncludePrivateRegistries,
  ExcludePrivateRegistries,
};

PLATFORM_EXPORT bool isReservedIPAddress(const String& host);

PLATFORM_EXPORT bool isLocalHostname(const String& host, bool* isLocal6);

PLATFORM_EXPORT String getDomainAndRegistry(const String& host,
                                            PrivateRegistryFilter);

// Returns the decoded data url if url had a supported mimetype and parsing was
// successful.
PLATFORM_EXPORT PassRefPtr<SharedBuffer> parseDataURL(const KURL&,
                                                      AtomicString& mimetype,
                                                      AtomicString& charset);

PLATFORM_EXPORT bool isRedirectResponseCode(int);

}  // NetworkUtils

}  // namespace blink

#endif  // NetworkUtils_h

// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebAddressSpace_h
#define WebAddressSpace_h

namespace blink {

// The ordering is important, as it's used to determine whether preflights are required,
// as per https://mikewest.github.io/cors-rfc1918/#framework
enum WebAddressSpace {
    WebAddressSpaceLocal = 0, // loopback, link local
    WebAddressSpacePrivate, // Reserved by RFC1918
    WebAddressSpacePublic, // Everything else
    WebAddressSpaceLast = WebAddressSpacePublic
};

} // namespace blink

#endif // WebAddressSpace_h

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebClientRedirectPolicy_h
#define WebClientRedirectPolicy_h

namespace blink {

// WebClientRedirectPolicy will affect loading, e.g. ClientRedirect will
// update HTTP referrer header.
enum class WebClientRedirectPolicy {
    NotClientRedirect,
    ClientRedirect,
};

} // namespace blink

#endif // WebClientRedirectPolicy_h

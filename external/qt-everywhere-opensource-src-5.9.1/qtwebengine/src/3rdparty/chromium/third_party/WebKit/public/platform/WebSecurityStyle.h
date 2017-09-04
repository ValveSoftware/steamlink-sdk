// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSecurityStyle_h
#define WebSecurityStyle_h
namespace blink {
// This enum represents the security state of a resource.
enum WebSecurityStyle {
  WebSecurityStyleUnknown,
  WebSecurityStyleUnauthenticated,
  WebSecurityStyleAuthenticationBroken,
  WebSecurityStyleWarning,
  WebSecurityStyleAuthenticated,
  WebSecurityStyleLast = WebSecurityStyleAuthenticated
};
}  // namespace blink
#endif

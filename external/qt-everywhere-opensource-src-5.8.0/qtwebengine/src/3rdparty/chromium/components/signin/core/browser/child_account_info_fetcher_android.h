// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_CHILD_ACCOUNT_INFO_FETCHER_ANDROID_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_CHILD_ACCOUNT_INFO_FETCHER_ANDROID_H_

#include <jni.h>
#include <string>

#include "base/macros.h"

class AccountFetcherService;

class ChildAccountInfoFetcherAndroid {
 public:
  static void StartFetchingChildAccountInfo(AccountFetcherService* service,
                                            const std::string& account_id);
  // Register JNI methods.
  static bool Register(JNIEnv* env);

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChildAccountInfoFetcherAndroid);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_CHILD_ACCOUNT_INFO_FETCHER_ANDROID_H_

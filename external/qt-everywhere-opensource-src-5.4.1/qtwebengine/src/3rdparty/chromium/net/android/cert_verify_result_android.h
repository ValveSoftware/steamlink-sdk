// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_CERT_VERIFY_RESULT_ANDROID_H_
#define NET_ANDROID_CERT_VERIFY_RESULT_ANDROID_H_

#include <jni.h>

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace net {

namespace android {

enum CertVerifyStatusAndroid {
#define CERT_VERIFY_STATUS_ANDROID(label, value) VERIFY_ ## label = value,
#include "net/android/cert_verify_status_android_list.h"
#undef CERT_VERIFY_STATUS_ANDROID
};

// Extract parameters out of an AndroidCertVerifyResult object.
void ExtractCertVerifyResult(jobject result,
                             CertVerifyStatusAndroid* status,
                             bool* is_issued_by_known_root,
                             std::vector<std::string>* verified_chain);

// Register JNI methods.
bool RegisterCertVerifyResult(JNIEnv* env);

}  // namespace android

}  // namespace net

#endif  // NET_ANDROID_CERT_VERIFY_RESULT_ANDROID_H_

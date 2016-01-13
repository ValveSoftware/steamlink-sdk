// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verifier.h"

#include "net/cert/cert_verify_proc.h"
#include "net/cert/multi_threaded_cert_verifier.h"

namespace net {

CertVerifier* CertVerifier::CreateDefault() {
  return new MultiThreadedCertVerifier(CertVerifyProc::CreateDefault());
}

}  // namespace net

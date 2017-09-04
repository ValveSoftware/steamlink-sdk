// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_SEATBELT_H_
#define SANDBOX_MAC_SEATBELT_H_

#include <cstdint>

#include "base/macros.h"
#include "sandbox/mac/seatbelt_export.h"

namespace sandbox {

// This class exists because OS X deprecated the sandbox functions,
// and did not supply replacements that are suitable for Chrome.
// This class wraps the functions in deprecation warning supressions.
class SEATBELT_EXPORT Seatbelt {
 public:
  static int Init(const char* profile, uint64_t flags, char** errorbuf);

  static int InitWithParams(const char* profile,
                            uint64_t flags,
                            const char* const parameters[],
                            char** errorbuf);

  static void FreeError(char* errorbuf);

  static const char* kProfileNoInternet;

  static const char* kProfileNoNetwork;

  static const char* kProfileNoWrite;

  static const char* kProfileNoWriteExceptTemporary;

  static const char* kProfilePureComputation;

 private:
  Seatbelt();
  DISALLOW_COPY_AND_ASSIGN(Seatbelt);
};

}  // sandbox

#endif  // SANDBOX_MAC_SEATBELT_H_

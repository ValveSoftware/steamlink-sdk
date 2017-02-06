// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_APP_BLIMP_STARTUP_H_
#define BLIMP_CLIENT_APP_BLIMP_STARTUP_H_

namespace blimp {
namespace client {

void InitializeLogging();

bool InitializeMainMessageLoop();

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_BLIMP_STARTUP_H_

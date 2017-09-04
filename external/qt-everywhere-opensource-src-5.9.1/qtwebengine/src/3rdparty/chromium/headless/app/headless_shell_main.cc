// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/headless_shell.h"

int main(int argc, const char** argv) {
  return headless::HeadlessShellMain(argc, argv);
}

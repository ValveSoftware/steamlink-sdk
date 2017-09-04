// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/standalone/desktop/main_helper.h"

int main(int argc, char** argv) {
  return service_manager::StandaloneServiceManagerMain(argc, argv);
}

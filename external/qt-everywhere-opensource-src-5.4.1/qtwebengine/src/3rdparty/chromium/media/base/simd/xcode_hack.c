// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// XCode doesn't want to link a pure assembly target and will fail
// to link when it creates an empty file list.  So add a dummy file
// keep the linker happy.  See http://crbug.com/157073
int xcode_sucks() {
  return 0;
}

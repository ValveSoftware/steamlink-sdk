// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_TOP_CONTROLS_STATE_LIST_H_
#define CONTENT_PUBLIC_COMMON_TOP_CONTROLS_STATE_LIST_H_

#ifndef DEFINE_TOP_CONTROLS_STATE
#error "DEFINE_TOP_CONTROLS_STATE should be defined before including this file"
#endif

// These values are defined with macros so that a Java class can be generated
// for them.
DEFINE_TOP_CONTROLS_STATE(SHOWN, 1)
DEFINE_TOP_CONTROLS_STATE(HIDDEN, 2)
DEFINE_TOP_CONTROLS_STATE(BOTH, 3)

#endif  // CONTENT_PUBLIC_COMMON_TOP_CONTROLS_STATE_LIST_H_


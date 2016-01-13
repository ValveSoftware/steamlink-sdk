// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_STOP_FIND_ACTION_H_
#define CONTENT_PUBLIC_COMMON_STOP_FIND_ACTION_H_

namespace content {

// The user has completed a find-in-page; this type defines what actions the
// renderer should take next.
enum StopFindAction {
  STOP_FIND_ACTION_CLEAR_SELECTION,
  STOP_FIND_ACTION_KEEP_SELECTION,
  STOP_FIND_ACTION_ACTIVATE_SELECTION
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_STOP_FIND_ACTION_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_JAVASCRIPT_MESSAGE_TYPE_H_
#define CONTENT_PUBLIC_COMMON_JAVASCRIPT_MESSAGE_TYPE_H_

namespace content {

enum JavaScriptMessageType {
  JAVASCRIPT_MESSAGE_TYPE_ALERT,
  JAVASCRIPT_MESSAGE_TYPE_CONFIRM,
  JAVASCRIPT_MESSAGE_TYPE_PROMPT
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_JAVASCRIPT_MESSAGE_TYPE_H_

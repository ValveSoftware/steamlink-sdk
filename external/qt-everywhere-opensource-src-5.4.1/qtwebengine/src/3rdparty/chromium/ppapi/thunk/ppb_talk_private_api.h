// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_TALK_PRIVATE_API_H_
#define PPAPI_THUNK_PPB_TALK_PRIVATE_API_H_

#include "base/memory/ref_counted.h"
#include "ppapi/c/private/ppb_talk_private.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_Talk_Private_API {
 public:
  virtual ~PPB_Talk_Private_API() {}

  virtual int32_t RequestPermission(
      PP_TalkPermission permission,
      scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t StartRemoting(
      PP_TalkEventCallback event_callback,
      void* user_data,
      scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t StopRemoting(
      scoped_refptr<TrackedCallback> callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_TALK_PRIVATE_API_H_

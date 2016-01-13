// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_talk_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_talk_private_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateTalk(instance);
}

int32_t GetPermission(PP_Resource resource,
                      PP_CompletionCallback callback) {
  EnterResource<PPB_Talk_Private_API> enter(resource, callback, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.SetResult(enter.object()->RequestPermission(
      PP_TALKPERMISSION_SCREENCAST, enter.callback()));
}

int32_t RequestPermission(PP_Resource resource,
                          PP_TalkPermission permission,
                          PP_CompletionCallback callback) {
  EnterResource<PPB_Talk_Private_API> enter(resource, callback, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.SetResult(
      enter.object()->RequestPermission(permission, enter.callback()));
}

int32_t StartRemoting(PP_Resource resource,
                      PP_TalkEventCallback event_callback,
                      void* user_data,
                      PP_CompletionCallback callback) {
  EnterResource<PPB_Talk_Private_API> enter(resource, callback, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.SetResult(
      enter.object()->StartRemoting(event_callback, user_data,
                                    enter.callback()));
}

int32_t StopRemoting(PP_Resource resource,
                  PP_CompletionCallback callback) {
  EnterResource<PPB_Talk_Private_API> enter(resource, callback, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.SetResult(
      enter.object()->StopRemoting(enter.callback()));
}

const PPB_Talk_Private_1_0 g_ppb_talk_private_thunk_1_0 = {
  &Create,
  &GetPermission
};

const PPB_Talk_Private_2_0 g_ppb_talk_private_thunk_2_0 = {
  &Create,
  &RequestPermission,
  &StartRemoting,
  &StopRemoting
};

}  // namespace

const PPB_Talk_Private_1_0* GetPPB_Talk_Private_1_0_Thunk() {
  return &g_ppb_talk_private_thunk_1_0;
}

const PPB_Talk_Private_2_0* GetPPB_Talk_Private_2_0_Thunk() {
  return &g_ppb_talk_private_thunk_2_0;
}

}  // namespace thunk
}  // namespace ppapi

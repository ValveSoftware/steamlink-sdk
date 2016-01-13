// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_scrollbar_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

typedef EnterResource<PPB_Scrollbar_API> EnterScrollbar;

namespace {

PP_Resource Create(PP_Instance instance, PP_Bool vertical) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateScrollbar(instance, vertical);
}

PP_Bool IsScrollbar(PP_Resource resource) {
  EnterScrollbar enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

uint32_t GetThickness(PP_Resource scrollbar) {
  EnterScrollbar enter(scrollbar, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetThickness();
}

PP_Bool IsOverlay(PP_Resource scrollbar) {
  EnterScrollbar enter(scrollbar, true);
  if (enter.failed())
    return PP_FALSE;
  return PP_FromBool(enter.object()->IsOverlay());
}

uint32_t GetValue(PP_Resource scrollbar) {
  EnterScrollbar enter(scrollbar, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetValue();
}

void SetValue(PP_Resource scrollbar, uint32_t value) {
  EnterScrollbar enter(scrollbar, true);
  if (enter.succeeded())
    enter.object()->SetValue(value);
}

void SetDocumentSize(PP_Resource scrollbar, uint32_t size) {
  EnterScrollbar enter(scrollbar, true);
  if (enter.succeeded())
    enter.object()->SetDocumentSize(size);
}

void SetTickMarks(PP_Resource scrollbar,
                  const PP_Rect* tick_marks,
                  uint32_t count) {
  EnterScrollbar enter(scrollbar, true);
  if (enter.succeeded())
    enter.object()->SetTickMarks(tick_marks, count);
}

void ScrollBy(PP_Resource scrollbar, PP_ScrollBy_Dev unit, int32_t multiplier) {
  EnterScrollbar enter(scrollbar, true);
  if (enter.succeeded())
    enter.object()->ScrollBy(unit, multiplier);
}

const PPB_Scrollbar_Dev g_ppb_scrollbar_thunk = {
  &Create,
  &IsScrollbar,
  &GetThickness,
  &IsOverlay,
  &GetValue,
  &SetValue,
  &SetDocumentSize,
  &SetTickMarks,
  &ScrollBy
};

}  // namespace

const PPB_Scrollbar_Dev_0_5* GetPPB_Scrollbar_Dev_0_5_Thunk() {
  return &g_ppb_scrollbar_thunk;
}

}  // namespace thunk
}  // namespace ppapi

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center.h"

#include "base/observer_list.h"
#include "ui/message_center/message_center_impl.h"

namespace message_center {

//------------------------------------------------------------------------------

namespace {
static MessageCenter* g_message_center;
}

// static
void MessageCenter::Initialize() {
  DCHECK(g_message_center == NULL);
  g_message_center = new MessageCenterImpl();
}

// static
MessageCenter* MessageCenter::Get() {
  DCHECK(g_message_center);
  return g_message_center;
}

// static
void MessageCenter::Shutdown() {
  DCHECK(g_message_center);
  delete g_message_center;
  g_message_center = NULL;
}

MessageCenter::MessageCenter() {
}

MessageCenter::~MessageCenter() {
}

}  // namespace message_center

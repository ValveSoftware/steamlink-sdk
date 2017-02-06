// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/presentation_session_message.h"

namespace content {

PresentationSessionMessage::PresentationSessionMessage(
    PresentationMessageType type)
    : type(type) {}

PresentationSessionMessage::~PresentationSessionMessage() {}

bool PresentationSessionMessage::is_binary() const {
  return data != nullptr;
}

}  // namespace content

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/media/capabilities_message_filter.h"

#include "chromecast/common/media/cast_messages.h"
#include "chromecast/media/base/media_caps.h"

namespace chromecast {

CapabilitiesMessageFilter::CapabilitiesMessageFilter() {
}

CapabilitiesMessageFilter::~CapabilitiesMessageFilter() {
}

bool CapabilitiesMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CapabilitiesMessageFilter, message)
    IPC_MESSAGE_HANDLER(CmaMsg_UpdateSupportedHdmiSinkCodecs,
                        OnUpdateSupportedHdmiSinkCodecs)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CapabilitiesMessageFilter::OnUpdateSupportedHdmiSinkCodecs(int codecs) {
  ::media::SetHdmiSinkCodecs(codecs);
}

}  // namespace chromecast

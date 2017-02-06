// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEDIA_CAPABILITIES_MESSAGE_FILTER_H_
#define CHROMECAST_RENDERER_MEDIA_CAPABILITIES_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "ipc/message_filter.h"

namespace chromecast {

class CapabilitiesMessageFilter : public IPC::MessageFilter {
 public:
  CapabilitiesMessageFilter();

  // IPC::ChannelProxy::MessageFilter implementation:
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  ~CapabilitiesMessageFilter() override;

  void OnUpdateSupportedHdmiSinkCodecs(int codecs);

  DISALLOW_COPY_AND_ASSIGN(CapabilitiesMessageFilter);
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEDIA_CAPABILITIES_MESSAGE_FILTER_H_

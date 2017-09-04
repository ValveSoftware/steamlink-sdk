// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature_channel.h"

#include "components/version_info/version_info.h"

namespace {

const version_info::Channel kDefaultChannel = version_info::Channel::STABLE;
version_info::Channel g_current_channel = kDefaultChannel;

}  // namespace

namespace extensions {

version_info::Channel GetCurrentChannel() {
  return g_current_channel;
}

void SetCurrentChannel(version_info::Channel channel) {
  g_current_channel = channel;
}

version_info::Channel GetDefaultChannel() {
  return kDefaultChannel;
}

ScopedCurrentChannel::ScopedCurrentChannel(version_info::Channel channel)
    : original_channel_(version_info::Channel::UNKNOWN) {
  original_channel_ = GetCurrentChannel();
  SetCurrentChannel(channel);
}

ScopedCurrentChannel::~ScopedCurrentChannel() {
  SetCurrentChannel(original_channel_);
}

}  // namespace extensions

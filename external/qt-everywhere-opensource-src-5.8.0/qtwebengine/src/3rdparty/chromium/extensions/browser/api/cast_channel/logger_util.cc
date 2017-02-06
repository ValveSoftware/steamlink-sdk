// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/logger_util.h"
#include "extensions/common/api/cast_channel/logging.pb.h"
#include "net/base/net_errors.h"

namespace extensions {
namespace api {
namespace cast_channel {
LastErrors::LastErrors()
    : event_type(proto::EVENT_TYPE_UNKNOWN),
      challenge_reply_error_type(proto::CHALLENGE_REPLY_ERROR_NONE),
      net_return_value(net::OK) {}

LastErrors::~LastErrors() {
}

proto::ErrorState ErrorStateToProto(ChannelError state) {
  switch (state) {
    case CHANNEL_ERROR_NONE:
      return proto::CHANNEL_ERROR_NONE;
    case CHANNEL_ERROR_CHANNEL_NOT_OPEN:
      return proto::CHANNEL_ERROR_CHANNEL_NOT_OPEN;
    case CHANNEL_ERROR_AUTHENTICATION_ERROR:
      return proto::CHANNEL_ERROR_AUTHENTICATION_ERROR;
    case CHANNEL_ERROR_CONNECT_ERROR:
      return proto::CHANNEL_ERROR_CONNECT_ERROR;
    case CHANNEL_ERROR_SOCKET_ERROR:
      return proto::CHANNEL_ERROR_SOCKET_ERROR;
    case CHANNEL_ERROR_TRANSPORT_ERROR:
      return proto::CHANNEL_ERROR_TRANSPORT_ERROR;
    case CHANNEL_ERROR_INVALID_MESSAGE:
      return proto::CHANNEL_ERROR_INVALID_MESSAGE;
    case CHANNEL_ERROR_INVALID_CHANNEL_ID:
      return proto::CHANNEL_ERROR_INVALID_CHANNEL_ID;
    case CHANNEL_ERROR_CONNECT_TIMEOUT:
      return proto::CHANNEL_ERROR_CONNECT_TIMEOUT;
    case CHANNEL_ERROR_UNKNOWN:
      return proto::CHANNEL_ERROR_UNKNOWN;
    default:
      NOTREACHED();
      return proto::CHANNEL_ERROR_NONE;
  }
}

proto::ReadyState ReadyStateToProto(ReadyState state) {
  switch (state) {
    case READY_STATE_NONE:
      return proto::READY_STATE_NONE;
    case READY_STATE_CONNECTING:
      return proto::READY_STATE_CONNECTING;
    case READY_STATE_OPEN:
      return proto::READY_STATE_OPEN;
    case READY_STATE_CLOSING:
      return proto::READY_STATE_CLOSING;
    case READY_STATE_CLOSED:
      return proto::READY_STATE_CLOSED;
    default:
      NOTREACHED();
      return proto::READY_STATE_NONE;
  }
}
}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

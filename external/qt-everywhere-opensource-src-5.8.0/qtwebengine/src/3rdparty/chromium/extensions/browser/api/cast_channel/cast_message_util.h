// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_MESSAGE_UTIL_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_MESSAGE_UTIL_H_

#include <string>

namespace extensions {
namespace api {
namespace cast_channel {

class CastMessage;
class DeviceAuthMessage;
struct MessageInfo;

// Fills |message_proto| from |message| and returns true on success.
bool MessageInfoToCastMessage(const MessageInfo& message,
                              CastMessage* message_proto);

// Checks if the contents of |message_proto| are valid.
bool IsCastMessageValid(const CastMessage& message_proto);

// Fills |message| from |message_proto| and returns true on success.
bool CastMessageToMessageInfo(const CastMessage& message_proto,
                              MessageInfo* message);

// Returns a human readable string for |message_proto|.
std::string CastMessageToString(const CastMessage& message_proto);

// Returns a human readable string for |message|.
std::string AuthMessageToString(const DeviceAuthMessage& message);

// Fills |message_proto| appropriately for an auth challenge request message.
void CreateAuthChallengeMessage(CastMessage* message_proto);

// Returns whether the given message is an auth handshake message.
bool IsAuthMessage(const CastMessage& message);

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_MESSAGE_UTIL_H_

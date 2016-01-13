// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility methods for MCS interactions.

#ifndef GOOGLE_APIS_GCM_BASE_MCS_UTIL_H_
#define GOOGLE_APIS_GCM_BASE_MCS_UTIL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "google_apis/gcm/protocol/mcs.pb.h"

namespace base {
class Clock;
}

namespace net {
class StreamSocket;
}

namespace gcm {

// MCS Message tags.
// WARNING: the order of these tags must remain the same, as the tag values
// must be consistent with those used on the server.
enum MCSProtoTag {
  kHeartbeatPingTag = 0,
  kHeartbeatAckTag,
  kLoginRequestTag,
  kLoginResponseTag,
  kCloseTag,
  kMessageStanzaTag,
  kPresenceStanzaTag,
  kIqStanzaTag,
  kDataMessageStanzaTag,
  kBatchPresenceStanzaTag,
  kStreamErrorStanzaTag,
  kHttpRequestTag,
  kHttpResponseTag,
  kBindAccountRequestTag,
  kBindAccountResponseTag,
  kTalkMetadataTag,
  kNumProtoTypes,
};

enum MCSIqStanzaExtension {
  kSelectiveAck = 12,
  kStreamAck = 13,
};

// Builds a LoginRequest with the hardcoded local data.
GCM_EXPORT scoped_ptr<mcs_proto::LoginRequest> BuildLoginRequest(
    uint64 auth_id,
    uint64 auth_token,
    const std::string& version_string);

// Builds a StreamAck IqStanza message.
GCM_EXPORT scoped_ptr<mcs_proto::IqStanza> BuildStreamAck();
GCM_EXPORT scoped_ptr<mcs_proto::IqStanza> BuildSelectiveAck(
    const std::vector<std::string>& acked_ids);

// Utility methods for building and identifying MCS protobufs.
GCM_EXPORT scoped_ptr<google::protobuf::MessageLite>
    BuildProtobufFromTag(uint8 tag);
GCM_EXPORT int GetMCSProtoTag(const google::protobuf::MessageLite& message);

// RMQ utility methods for extracting/setting common data from/to protobufs.
GCM_EXPORT std::string GetPersistentId(
    const google::protobuf::MessageLite& message);
GCM_EXPORT void SetPersistentId(
    const std::string& persistent_id,
    google::protobuf::MessageLite* message);
GCM_EXPORT uint32 GetLastStreamIdReceived(
    const google::protobuf::MessageLite& protobuf);
GCM_EXPORT void SetLastStreamIdReceived(
    uint32 last_stream_id_received,
    google::protobuf::MessageLite* protobuf);

// Returns whether the TTL (time to live) for this message has expired, based
// on the |sent| timestamps and base::TimeTicks::Now(). If |protobuf| is not
// for a DataMessageStanza or the TTL is 0, will return false.
GCM_EXPORT bool HasTTLExpired(const google::protobuf::MessageLite& protobuf,
                              base::Clock* clock);
GCM_EXPORT int GetTTL(const google::protobuf::MessageLite& protobuf);

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_BASE_MCS_UTIL_H_

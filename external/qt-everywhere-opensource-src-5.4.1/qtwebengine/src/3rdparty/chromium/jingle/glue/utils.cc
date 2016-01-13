// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/utils.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "third_party/libjingle/source/talk/base/byteorder.h"
#include "third_party/libjingle/source/talk/base/socketaddress.h"
#include "third_party/libjingle/source/talk/p2p/base/candidate.h"

namespace jingle_glue {

bool IPEndPointToSocketAddress(const net::IPEndPoint& ip_endpoint,
                               talk_base::SocketAddress* address) {
  sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  return ip_endpoint.ToSockAddr(reinterpret_cast<sockaddr*>(&addr), &len) &&
      talk_base::SocketAddressFromSockAddrStorage(addr, address);
}

bool SocketAddressToIPEndPoint(const talk_base::SocketAddress& address,
                               net::IPEndPoint* ip_endpoint) {
  sockaddr_storage addr;
  int size = address.ToSockAddrStorage(&addr);
  return (size > 0) &&
      ip_endpoint->FromSockAddr(reinterpret_cast<sockaddr*>(&addr), size);
}

std::string SerializeP2PCandidate(const cricket::Candidate& candidate) {
  // TODO(sergeyu): Use SDP to format candidates?
  base::DictionaryValue value;
  value.SetString("ip", candidate.address().ipaddr().ToString());
  value.SetInteger("port", candidate.address().port());
  value.SetString("type", candidate.type());
  value.SetString("protocol", candidate.protocol());
  value.SetString("username", candidate.username());
  value.SetString("password", candidate.password());
  value.SetDouble("preference", candidate.preference());
  value.SetInteger("generation", candidate.generation());

  std::string result;
  base::JSONWriter::Write(&value, &result);
  return result;
}

bool DeserializeP2PCandidate(const std::string& candidate_str,
                             cricket::Candidate* candidate) {
  scoped_ptr<base::Value> value(
      base::JSONReader::Read(candidate_str, base::JSON_ALLOW_TRAILING_COMMAS));
  if (!value.get() || !value->IsType(base::Value::TYPE_DICTIONARY)) {
    return false;
  }

  base::DictionaryValue* dic_value =
      static_cast<base::DictionaryValue*>(value.get());

  std::string ip;
  int port;
  std::string type;
  std::string protocol;
  std::string username;
  std::string password;
  double preference;
  int generation;

  if (!dic_value->GetString("ip", &ip) ||
      !dic_value->GetInteger("port", &port) ||
      !dic_value->GetString("type", &type) ||
      !dic_value->GetString("protocol", &protocol) ||
      !dic_value->GetString("username", &username) ||
      !dic_value->GetString("password", &password) ||
      !dic_value->GetDouble("preference", &preference) ||
      !dic_value->GetInteger("generation", &generation)) {
    return false;
  }

  candidate->set_address(talk_base::SocketAddress(ip, port));
  candidate->set_type(type);
  candidate->set_protocol(protocol);
  candidate->set_username(username);
  candidate->set_password(password);
  candidate->set_preference(static_cast<float>(preference));
  candidate->set_generation(generation);

  return true;
}

}  // namespace jingle_glue

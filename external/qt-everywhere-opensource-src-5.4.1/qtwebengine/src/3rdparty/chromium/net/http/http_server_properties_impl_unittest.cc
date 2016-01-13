// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_impl.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class ListValue;
}

namespace net {

const int kMaxSupportsSpdyServerHosts = 500;

namespace {

class HttpServerPropertiesImplTest : public testing::Test {
 protected:
  HttpServerPropertiesImpl impl_;
};

typedef HttpServerPropertiesImplTest SpdyServerPropertiesTest;

TEST_F(SpdyServerPropertiesTest, Initialize) {
  HostPortPair spdy_server_google("www.google.com", 443);
  std::string spdy_server_g =
      HttpServerPropertiesImpl::GetFlattenedSpdyServer(spdy_server_google);

  HostPortPair spdy_server_docs("docs.google.com", 443);
  std::string spdy_server_d =
      HttpServerPropertiesImpl::GetFlattenedSpdyServer(spdy_server_docs);

  // Check by initializing NULL spdy servers.
  impl_.InitializeSpdyServers(NULL, true);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_google));

  // Check by initializing empty spdy servers.
  std::vector<std::string> spdy_servers;
  impl_.InitializeSpdyServers(&spdy_servers, true);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_google));

  // Check by initializing with www.google.com:443 spdy server.
  std::vector<std::string> spdy_servers1;
  spdy_servers1.push_back(spdy_server_g);
  impl_.InitializeSpdyServers(&spdy_servers1, true);
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_google));

  // Check by initializing with www.google.com:443 and docs.google.com:443 spdy
  // servers.
  std::vector<std::string> spdy_servers2;
  spdy_servers2.push_back(spdy_server_g);
  spdy_servers2.push_back(spdy_server_d);
  impl_.InitializeSpdyServers(&spdy_servers2, true);

  // Verify spdy_server_g and spdy_server_d are in the list in the same order.
  base::ListValue spdy_server_list;
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  EXPECT_EQ(2U, spdy_server_list.GetSize());
  std::string string_value_g;
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);
  std::string string_value_d;
  ASSERT_TRUE(spdy_server_list.GetString(1, &string_value_d));
  ASSERT_EQ(spdy_server_d, string_value_d);
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_google));
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_docs));
}

TEST_F(SpdyServerPropertiesTest, SupportsSpdyTest) {
  HostPortPair spdy_server_empty(std::string(), 443);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_empty));

  // Add www.google.com:443 as supporting SPDY.
  HostPortPair spdy_server_google("www.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_google, true);
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_google));

  // Add mail.google.com:443 as not supporting SPDY.
  HostPortPair spdy_server_mail("mail.google.com", 443);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_mail));

  // Add docs.google.com:443 as supporting SPDY.
  HostPortPair spdy_server_docs("docs.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_docs, true);
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_docs));

  // Verify all the entries are the same after additions.
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_google));
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_mail));
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_docs));
}

TEST_F(SpdyServerPropertiesTest, SetSupportsSpdy) {
  HostPortPair spdy_server_empty(std::string(), 443);
  impl_.SetSupportsSpdy(spdy_server_empty, true);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_empty));

  // Add www.google.com:443 as supporting SPDY.
  HostPortPair spdy_server_google("www.google.com", 443);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_google));
  impl_.SetSupportsSpdy(spdy_server_google, true);
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_google));

  // Make www.google.com:443 as not supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_google, false);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_google));

  // Add mail.google.com:443 as supporting SPDY.
  HostPortPair spdy_server_mail("mail.google.com", 443);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_mail));
  impl_.SetSupportsSpdy(spdy_server_mail, true);
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_mail));
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_google));
}

TEST_F(SpdyServerPropertiesTest, Clear) {
  // Add www.google.com:443 and mail.google.com:443 as supporting SPDY.
  HostPortPair spdy_server_google("www.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_google, true);
  HostPortPair spdy_server_mail("mail.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_mail, true);

  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_google));
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_mail));

  impl_.Clear();
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_google));
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_mail));
}

TEST_F(SpdyServerPropertiesTest, GetSpdyServerList) {
  base::ListValue spdy_server_list;

  // Check there are no spdy_servers.
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  EXPECT_EQ(0U, spdy_server_list.GetSize());

  // Check empty server is not added.
  HostPortPair spdy_server_empty(std::string(), 443);
  impl_.SetSupportsSpdy(spdy_server_empty, true);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  EXPECT_EQ(0U, spdy_server_list.GetSize());

  std::string string_value_g;
  std::string string_value_m;
  HostPortPair spdy_server_google("www.google.com", 443);
  std::string spdy_server_g =
      HttpServerPropertiesImpl::GetFlattenedSpdyServer(spdy_server_google);
  HostPortPair spdy_server_mail("mail.google.com", 443);
  std::string spdy_server_m =
      HttpServerPropertiesImpl::GetFlattenedSpdyServer(spdy_server_mail);

  // Add www.google.com:443 as not supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_google, false);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  EXPECT_EQ(0U, spdy_server_list.GetSize());

  // Add www.google.com:443 as supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_google, true);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(1U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);

  // Add mail.google.com:443 as not supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_mail, false);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(1U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);

  // Add mail.google.com:443 as supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_mail, true);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(2U, spdy_server_list.GetSize());

  // Verify www.google.com:443 and mail.google.com:443 are in the list.
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_m));
  ASSERT_EQ(spdy_server_m, string_value_m);
  ASSERT_TRUE(spdy_server_list.GetString(1, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);

  // Request for only one server and verify that we get only one server.
  impl_.GetSpdyServerList(&spdy_server_list, 1);
  ASSERT_EQ(1U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_m));
  ASSERT_EQ(spdy_server_m, string_value_m);
}

TEST_F(SpdyServerPropertiesTest, MRUOfGetSpdyServerList) {
  base::ListValue spdy_server_list;

  std::string string_value_g;
  std::string string_value_m;
  HostPortPair spdy_server_google("www.google.com", 443);
  std::string spdy_server_g =
      HttpServerPropertiesImpl::GetFlattenedSpdyServer(spdy_server_google);
  HostPortPair spdy_server_mail("mail.google.com", 443);
  std::string spdy_server_m =
      HttpServerPropertiesImpl::GetFlattenedSpdyServer(spdy_server_mail);

  // Add www.google.com:443 as supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_google, true);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(1U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);

  // Add mail.google.com:443 as supporting SPDY. Verify mail.google.com:443 and
  // www.google.com:443 are in the list.
  impl_.SetSupportsSpdy(spdy_server_mail, true);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(2U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_m));
  ASSERT_EQ(spdy_server_m, string_value_m);
  ASSERT_TRUE(spdy_server_list.GetString(1, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);

  // Get www.google.com:443 should reorder SpdyServerHostPortMap. Verify that it
  // is www.google.com:443 is the MRU server.
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_google));
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(2U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);
  ASSERT_TRUE(spdy_server_list.GetString(1, &string_value_m));
  ASSERT_EQ(spdy_server_m, string_value_m);
}

typedef HttpServerPropertiesImplTest AlternateProtocolServerPropertiesTest;

TEST_F(AlternateProtocolServerPropertiesTest, Basic) {
  HostPortPair test_host_port_pair("foo", 80);
  EXPECT_FALSE(impl_.HasAlternateProtocol(test_host_port_pair));
  impl_.SetAlternateProtocol(test_host_port_pair, 443, NPN_SPDY_3);
  ASSERT_TRUE(impl_.HasAlternateProtocol(test_host_port_pair));
  const PortAlternateProtocolPair alternate =
      impl_.GetAlternateProtocol(test_host_port_pair);
  EXPECT_EQ(443, alternate.port);
  EXPECT_EQ(NPN_SPDY_3, alternate.protocol);

  impl_.Clear();
  EXPECT_FALSE(impl_.HasAlternateProtocol(test_host_port_pair));
}

TEST_F(AlternateProtocolServerPropertiesTest, Initialize) {
  HostPortPair test_host_port_pair1("foo1", 80);
  impl_.SetBrokenAlternateProtocol(test_host_port_pair1);
  HostPortPair test_host_port_pair2("foo2", 80);
  impl_.SetAlternateProtocol(test_host_port_pair2, 443, NPN_SPDY_3);

  AlternateProtocolMap alternate_protocol_map(
      AlternateProtocolMap::NO_AUTO_EVICT);
  PortAlternateProtocolPair port_alternate_protocol_pair;
  port_alternate_protocol_pair.port = 123;
  port_alternate_protocol_pair.protocol = NPN_SPDY_3;
  alternate_protocol_map.Put(test_host_port_pair2,
                             port_alternate_protocol_pair);
  HostPortPair test_host_port_pair3("foo3", 80);
  port_alternate_protocol_pair.port = 1234;
  alternate_protocol_map.Put(test_host_port_pair3,
                             port_alternate_protocol_pair);
  impl_.InitializeAlternateProtocolServers(&alternate_protocol_map);

  // Verify test_host_port_pair3 is the MRU server.
  const net::AlternateProtocolMap& map = impl_.alternate_protocol_map();
  net::AlternateProtocolMap::const_iterator it = map.begin();
  it = map.begin();
  EXPECT_TRUE(it->first.Equals(test_host_port_pair3));
  EXPECT_EQ(1234, it->second.port);
  EXPECT_EQ(NPN_SPDY_3, it->second.protocol);

  ASSERT_TRUE(impl_.HasAlternateProtocol(test_host_port_pair1));
  ASSERT_TRUE(impl_.HasAlternateProtocol(test_host_port_pair2));
  port_alternate_protocol_pair =
      impl_.GetAlternateProtocol(test_host_port_pair1);
  EXPECT_EQ(ALTERNATE_PROTOCOL_BROKEN, port_alternate_protocol_pair.protocol);
  port_alternate_protocol_pair =
      impl_.GetAlternateProtocol(test_host_port_pair2);
  EXPECT_EQ(123, port_alternate_protocol_pair.port);
  EXPECT_EQ(NPN_SPDY_3, port_alternate_protocol_pair.protocol);
}

TEST_F(AlternateProtocolServerPropertiesTest, MRUOfHasAlternateProtocol) {
  HostPortPair test_host_port_pair1("foo1", 80);
  impl_.SetAlternateProtocol(test_host_port_pair1, 443, NPN_SPDY_3);
  HostPortPair test_host_port_pair2("foo2", 80);
  impl_.SetAlternateProtocol(test_host_port_pair2, 1234, NPN_SPDY_3);

  const net::AlternateProtocolMap& map = impl_.alternate_protocol_map();
  net::AlternateProtocolMap::const_iterator it = map.begin();
  EXPECT_TRUE(it->first.Equals(test_host_port_pair2));
  EXPECT_EQ(1234, it->second.port);
  EXPECT_EQ(NPN_SPDY_3, it->second.protocol);

  // HasAlternateProtocol should reoder the AlternateProtocol map.
  ASSERT_TRUE(impl_.HasAlternateProtocol(test_host_port_pair1));
  it = map.begin();
  EXPECT_TRUE(it->first.Equals(test_host_port_pair1));
  EXPECT_EQ(443, it->second.port);
  EXPECT_EQ(NPN_SPDY_3, it->second.protocol);
}

TEST_F(AlternateProtocolServerPropertiesTest, MRUOfGetAlternateProtocol) {
  HostPortPair test_host_port_pair1("foo1", 80);
  impl_.SetAlternateProtocol(test_host_port_pair1, 443, NPN_SPDY_3);
  HostPortPair test_host_port_pair2("foo2", 80);
  impl_.SetAlternateProtocol(test_host_port_pair2, 1234, NPN_SPDY_3);

  const net::AlternateProtocolMap& map = impl_.alternate_protocol_map();
  net::AlternateProtocolMap::const_iterator it = map.begin();
  EXPECT_TRUE(it->first.Equals(test_host_port_pair2));
  EXPECT_EQ(1234, it->second.port);
  EXPECT_EQ(NPN_SPDY_3, it->second.protocol);

  // GetAlternateProtocol should reoder the AlternateProtocol map.
  PortAlternateProtocolPair alternate =
      impl_.GetAlternateProtocol(test_host_port_pair1);
  EXPECT_EQ(443, alternate.port);
  EXPECT_EQ(NPN_SPDY_3, alternate.protocol);
  it = map.begin();
  EXPECT_TRUE(it->first.Equals(test_host_port_pair1));
  EXPECT_EQ(443, it->second.port);
  EXPECT_EQ(NPN_SPDY_3, it->second.protocol);
}

TEST_F(AlternateProtocolServerPropertiesTest, SetBroken) {
  HostPortPair test_host_port_pair("foo", 80);
  impl_.SetBrokenAlternateProtocol(test_host_port_pair);
  ASSERT_TRUE(impl_.HasAlternateProtocol(test_host_port_pair));
  PortAlternateProtocolPair alternate =
      impl_.GetAlternateProtocol(test_host_port_pair);
  EXPECT_EQ(ALTERNATE_PROTOCOL_BROKEN, alternate.protocol);

  impl_.SetAlternateProtocol(
      test_host_port_pair,
      1234,
      NPN_SPDY_3);
  alternate = impl_.GetAlternateProtocol(test_host_port_pair);
  EXPECT_EQ(ALTERNATE_PROTOCOL_BROKEN, alternate.protocol)
      << "Second attempt should be ignored.";
}

TEST_F(AlternateProtocolServerPropertiesTest, ClearBroken) {
  HostPortPair test_host_port_pair("foo", 80);
  impl_.SetBrokenAlternateProtocol(test_host_port_pair);
  ASSERT_TRUE(impl_.HasAlternateProtocol(test_host_port_pair));
  PortAlternateProtocolPair alternate =
      impl_.GetAlternateProtocol(test_host_port_pair);
  EXPECT_EQ(ALTERNATE_PROTOCOL_BROKEN, alternate.protocol);
  impl_.ClearAlternateProtocol(test_host_port_pair);
  EXPECT_FALSE(impl_.HasAlternateProtocol(test_host_port_pair));
}

TEST_F(AlternateProtocolServerPropertiesTest, Forced) {
  // Test forced alternate protocols.

  PortAlternateProtocolPair default_protocol;
  default_protocol.port = 1234;
  default_protocol.protocol = NPN_SPDY_3;
  HttpServerPropertiesImpl::ForceAlternateProtocol(default_protocol);

  // Verify the forced protocol.
  HostPortPair test_host_port_pair("foo", 80);
  EXPECT_TRUE(impl_.HasAlternateProtocol(test_host_port_pair));
  PortAlternateProtocolPair alternate =
      impl_.GetAlternateProtocol(test_host_port_pair);
  EXPECT_EQ(default_protocol.port, alternate.port);
  EXPECT_EQ(default_protocol.protocol, alternate.protocol);

  // Verify the real protocol overrides the forced protocol.
  impl_.SetAlternateProtocol(test_host_port_pair, 443, NPN_SPDY_3);
  ASSERT_TRUE(impl_.HasAlternateProtocol(test_host_port_pair));
  alternate = impl_.GetAlternateProtocol(test_host_port_pair);
  EXPECT_EQ(443, alternate.port);
  EXPECT_EQ(NPN_SPDY_3, alternate.protocol);

  // Turn off the static, forced alternate protocol so that tests don't
  // have this state.
  HttpServerPropertiesImpl::DisableForcedAlternateProtocol();

  // Verify the forced protocol is off.
  HostPortPair test_host_port_pair2("bar", 80);
  EXPECT_FALSE(impl_.HasAlternateProtocol(test_host_port_pair2));
}

TEST_F(AlternateProtocolServerPropertiesTest, Canonical) {
  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  EXPECT_FALSE(impl_.HasAlternateProtocol(test_host_port_pair));

  HostPortPair canonical_port_pair("bar.c.youtube.com", 80);
  EXPECT_FALSE(impl_.HasAlternateProtocol(canonical_port_pair));

  PortAlternateProtocolPair canonical_protocol;
  canonical_protocol.port = 1234;
  canonical_protocol.protocol = QUIC;

  impl_.SetAlternateProtocol(canonical_port_pair,
                             canonical_protocol.port,
                             canonical_protocol.protocol);
  // Verify the forced protocol.
  ASSERT_TRUE(impl_.HasAlternateProtocol(test_host_port_pair));
  PortAlternateProtocolPair alternate =
      impl_.GetAlternateProtocol(test_host_port_pair);
  EXPECT_EQ(canonical_protocol.port, alternate.port);
  EXPECT_EQ(canonical_protocol.protocol, alternate.protocol);

  // Verify the canonical suffix.
  EXPECT_EQ(".c.youtube.com", impl_.GetCanonicalSuffix(test_host_port_pair));
  EXPECT_EQ(".c.youtube.com", impl_.GetCanonicalSuffix(canonical_port_pair));
}

typedef HttpServerPropertiesImplTest SpdySettingsServerPropertiesTest;

TEST_F(SpdySettingsServerPropertiesTest, Initialize) {
  HostPortPair spdy_server_google("www.google.com", 443);

  // Check by initializing empty spdy settings.
  SpdySettingsMap spdy_settings_map(SpdySettingsMap::NO_AUTO_EVICT);
  impl_.InitializeSpdySettingsServers(&spdy_settings_map);
  EXPECT_TRUE(impl_.GetSpdySettings(spdy_server_google).empty());

  // Check by initializing with www.google.com:443 spdy server settings.
  SettingsMap settings_map;
  const SpdySettingsIds id = SETTINGS_UPLOAD_BANDWIDTH;
  const SpdySettingsFlags flags = SETTINGS_FLAG_PERSISTED;
  const uint32 value = 31337;
  SettingsFlagsAndValue flags_and_value(flags, value);
  settings_map[id] = flags_and_value;
  spdy_settings_map.Put(spdy_server_google, settings_map);
  impl_.InitializeSpdySettingsServers(&spdy_settings_map);

  const SettingsMap& settings_map2 = impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(1U, settings_map2.size());
  SettingsMap::const_iterator it = settings_map2.find(id);
  EXPECT_TRUE(it != settings_map2.end());
  SettingsFlagsAndValue flags_and_value2 = it->second;
  EXPECT_EQ(flags, flags_and_value2.first);
  EXPECT_EQ(value, flags_and_value2.second);
}

TEST_F(SpdySettingsServerPropertiesTest, SetSpdySetting) {
  HostPortPair spdy_server_empty(std::string(), 443);
  const SettingsMap& settings_map0 = impl_.GetSpdySettings(spdy_server_empty);
  EXPECT_EQ(0U, settings_map0.size());  // Returns kEmptySettingsMap.

  // Add www.google.com:443 as persisting.
  HostPortPair spdy_server_google("www.google.com", 443);
  const SpdySettingsIds id1 = SETTINGS_UPLOAD_BANDWIDTH;
  const SpdySettingsFlags flags1 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value1 = 31337;
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_google, id1, flags1, value1));
  // Check the values.
  const SettingsMap& settings_map1_ret =
      impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(1U, settings_map1_ret.size());
  SettingsMap::const_iterator it1_ret = settings_map1_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_ret.end());
  SettingsFlagsAndValue flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  // Add mail.google.com:443 as not persisting.
  HostPortPair spdy_server_mail("mail.google.com", 443);
  const SpdySettingsIds id2 = SETTINGS_DOWNLOAD_BANDWIDTH;
  const SpdySettingsFlags flags2 = SETTINGS_FLAG_NONE;
  const uint32 value2 = 62667;
  EXPECT_FALSE(impl_.SetSpdySetting(spdy_server_mail, id2, flags2, value2));
  const SettingsMap& settings_map2_ret =
      impl_.GetSpdySettings(spdy_server_mail);
  EXPECT_EQ(0U, settings_map2_ret.size());  // Returns kEmptySettingsMap.

  // Add docs.google.com:443 as persisting
  HostPortPair spdy_server_docs("docs.google.com", 443);
  const SpdySettingsIds id3 = SETTINGS_ROUND_TRIP_TIME;
  const SpdySettingsFlags flags3 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value3 = 93997;
  SettingsFlagsAndValue flags_and_value3(flags3, value3);
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_docs, id3, flags3, value3));
  // Check the values.
  const SettingsMap& settings_map3_ret =
      impl_.GetSpdySettings(spdy_server_docs);
  ASSERT_EQ(1U, settings_map3_ret.size());
  SettingsMap::const_iterator it3_ret = settings_map3_ret.find(id3);
  EXPECT_TRUE(it3_ret != settings_map3_ret.end());
  SettingsFlagsAndValue flags_and_value3_ret = it3_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value3_ret.first);
  EXPECT_EQ(value3, flags_and_value3_ret.second);

  // Check data for www.google.com:443 (id1).
  const SettingsMap& settings_map4_ret =
      impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(1U, settings_map4_ret.size());
  SettingsMap::const_iterator it4_ret = settings_map4_ret.find(id1);
  EXPECT_TRUE(it4_ret != settings_map4_ret.end());
  SettingsFlagsAndValue flags_and_value4_ret = it4_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value4_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  // Clear www.google.com:443 as persisting.
  impl_.ClearSpdySettings(spdy_server_google);
  // Check the values.
  const SettingsMap& settings_map5_ret =
      impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(0U, settings_map5_ret.size());

  // Clear all settings.
  ASSERT_GT(impl_.spdy_settings_map().size(), 0U);
  impl_.ClearAllSpdySettings();
  ASSERT_EQ(0U, impl_.spdy_settings_map().size());
}

TEST_F(SpdySettingsServerPropertiesTest, Clear) {
  // Add www.google.com:443 as persisting.
  HostPortPair spdy_server_google("www.google.com", 443);
  const SpdySettingsIds id1 = SETTINGS_UPLOAD_BANDWIDTH;
  const SpdySettingsFlags flags1 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value1 = 31337;
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_google, id1, flags1, value1));
  // Check the values.
  const SettingsMap& settings_map1_ret =
      impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(1U, settings_map1_ret.size());
  SettingsMap::const_iterator it1_ret = settings_map1_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_ret.end());
  SettingsFlagsAndValue flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  // Add docs.google.com:443 as persisting
  HostPortPair spdy_server_docs("docs.google.com", 443);
  const SpdySettingsIds id3 = SETTINGS_ROUND_TRIP_TIME;
  const SpdySettingsFlags flags3 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value3 = 93997;
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_docs, id3, flags3, value3));
  // Check the values.
  const SettingsMap& settings_map3_ret =
      impl_.GetSpdySettings(spdy_server_docs);
  ASSERT_EQ(1U, settings_map3_ret.size());
  SettingsMap::const_iterator it3_ret = settings_map3_ret.find(id3);
  EXPECT_TRUE(it3_ret != settings_map3_ret.end());
  SettingsFlagsAndValue flags_and_value3_ret = it3_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value3_ret.first);
  EXPECT_EQ(value3, flags_and_value3_ret.second);

  impl_.Clear();
  EXPECT_EQ(0U, impl_.GetSpdySettings(spdy_server_google).size());
  EXPECT_EQ(0U, impl_.GetSpdySettings(spdy_server_docs).size());
}

TEST_F(SpdySettingsServerPropertiesTest, MRUOfGetSpdySettings) {
  // Add www.google.com:443 as persisting.
  HostPortPair spdy_server_google("www.google.com", 443);
  const SpdySettingsIds id1 = SETTINGS_UPLOAD_BANDWIDTH;
  const SpdySettingsFlags flags1 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value1 = 31337;
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_google, id1, flags1, value1));

  // Add docs.google.com:443 as persisting
  HostPortPair spdy_server_docs("docs.google.com", 443);
  const SpdySettingsIds id2 = SETTINGS_ROUND_TRIP_TIME;
  const SpdySettingsFlags flags2 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value2 = 93997;
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_docs, id2, flags2, value2));

  // Verify the first element is docs.google.com:443.
  const net::SpdySettingsMap& map = impl_.spdy_settings_map();
  net::SpdySettingsMap::const_iterator it = map.begin();
  EXPECT_TRUE(it->first.Equals(spdy_server_docs));
  const SettingsMap& settings_map2_ret = it->second;
  ASSERT_EQ(1U, settings_map2_ret.size());
  SettingsMap::const_iterator it2_ret = settings_map2_ret.find(id2);
  EXPECT_TRUE(it2_ret != settings_map2_ret.end());
  SettingsFlagsAndValue flags_and_value2_ret = it2_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value2_ret.first);
  EXPECT_EQ(value2, flags_and_value2_ret.second);

  // GetSpdySettings should reoder the SpdySettingsMap.
  const SettingsMap& settings_map1_ret =
      impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(1U, settings_map1_ret.size());
  SettingsMap::const_iterator it1_ret = settings_map1_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_ret.end());
  SettingsFlagsAndValue flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  // Check the first entry is spdy_server_google by accessing it via iterator.
  it = map.begin();
  EXPECT_TRUE(it->first.Equals(spdy_server_google));
  const SettingsMap& settings_map1_it_ret = it->second;
  ASSERT_EQ(1U, settings_map1_it_ret.size());
  it1_ret = settings_map1_it_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_it_ret.end());
  flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);
}

}  // namespace

}  // namespace net

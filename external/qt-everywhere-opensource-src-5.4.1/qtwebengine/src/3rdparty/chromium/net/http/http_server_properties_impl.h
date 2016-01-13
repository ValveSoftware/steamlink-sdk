// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_
#define NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/threading/non_thread_safe.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/http/http_server_properties.h"

namespace base {
class ListValue;
}

namespace net {

// The implementation for setting/retrieving the HTTP server properties.
class NET_EXPORT HttpServerPropertiesImpl
    : public HttpServerProperties,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  HttpServerPropertiesImpl();
  virtual ~HttpServerPropertiesImpl();

  // Initializes |spdy_servers_map_| with the servers (host/port) from
  // |spdy_servers| that either support SPDY or not.
  void InitializeSpdyServers(std::vector<std::string>* spdy_servers,
                             bool support_spdy);

  void InitializeAlternateProtocolServers(
      AlternateProtocolMap* alternate_protocol_servers);

  void InitializeSpdySettingsServers(SpdySettingsMap* spdy_settings_map);

  // Get the list of servers (host/port) that support SPDY. The max_size is the
  // number of MRU servers that support SPDY that are to be returned.
  void GetSpdyServerList(base::ListValue* spdy_server_list,
                         size_t max_size) const;

  // Returns flattened string representation of the |host_port_pair|. Used by
  // unittests.
  static std::string GetFlattenedSpdyServer(
      const net::HostPortPair& host_port_pair);

  // Debugging to simulate presence of an AlternateProtocol.
  // If we don't have an alternate protocol in the map for any given host/port
  // pair, force this ProtocolPortPair.
  static void ForceAlternateProtocol(const PortAlternateProtocolPair& pair);
  static void DisableForcedAlternateProtocol();

  // Returns the canonical host suffix for |server|, or std::string() if none
  // exists.
  std::string GetCanonicalSuffix(const net::HostPortPair& server);

  // -----------------------------
  // HttpServerProperties methods:
  // -----------------------------

  // Gets a weak pointer for this object.
  virtual base::WeakPtr<HttpServerProperties> GetWeakPtr() OVERRIDE;

  // Deletes all data.
  virtual void Clear() OVERRIDE;

  // Returns true if |server| supports SPDY.
  virtual bool SupportsSpdy(const HostPortPair& server) OVERRIDE;

  // Add |server| into the persistent store.
  virtual void SetSupportsSpdy(const HostPortPair& server,
                               bool support_spdy) OVERRIDE;

  // Returns true if |server| has an Alternate-Protocol header.
  virtual bool HasAlternateProtocol(const HostPortPair& server) OVERRIDE;

  // Returns the Alternate-Protocol and port for |server|.
  // HasAlternateProtocol(server) must be true.
  virtual PortAlternateProtocolPair GetAlternateProtocol(
      const HostPortPair& server) OVERRIDE;

  // Sets the Alternate-Protocol for |server|.
  virtual void SetAlternateProtocol(
      const HostPortPair& server,
      uint16 alternate_port,
      AlternateProtocol alternate_protocol) OVERRIDE;

  // Sets the Alternate-Protocol for |server| to be BROKEN.
  virtual void SetBrokenAlternateProtocol(const HostPortPair& server) OVERRIDE;

  // Returns true if Alternate-Protocol for |server| was recently BROKEN.
  virtual bool WasAlternateProtocolRecentlyBroken(
      const HostPortPair& server) OVERRIDE;

  // Confirms that Alternate-Protocol for |server| is working.
  virtual void ConfirmAlternateProtocol(const HostPortPair& server) OVERRIDE;

  // Clears the Alternate-Protocol for |server|.
  virtual void ClearAlternateProtocol(const HostPortPair& server) OVERRIDE;

  // Returns all Alternate-Protocol mappings.
  virtual const AlternateProtocolMap& alternate_protocol_map() const OVERRIDE;

  virtual void SetAlternateProtocolExperiment(
      AlternateProtocolExperiment experiment) OVERRIDE;

  virtual AlternateProtocolExperiment GetAlternateProtocolExperiment()
      const OVERRIDE;

  // Gets a reference to the SettingsMap stored for a host.
  // If no settings are stored, returns an empty SettingsMap.
  virtual const SettingsMap& GetSpdySettings(
      const HostPortPair& host_port_pair) OVERRIDE;

  // Saves an individual SPDY setting for a host. Returns true if SPDY setting
  // is to be persisted.
  virtual bool SetSpdySetting(const HostPortPair& host_port_pair,
                              SpdySettingsIds id,
                              SpdySettingsFlags flags,
                              uint32 value) OVERRIDE;

  // Clears all entries in |spdy_settings_map_| for a host.
  virtual void ClearSpdySettings(const HostPortPair& host_port_pair) OVERRIDE;

  // Clears all entries in |spdy_settings_map_|.
  virtual void ClearAllSpdySettings() OVERRIDE;

  // Returns all persistent SPDY settings.
  virtual const SpdySettingsMap& spdy_settings_map() const OVERRIDE;

  virtual void SetServerNetworkStats(const HostPortPair& host_port_pair,
                                     NetworkStats stats) OVERRIDE;

  virtual const NetworkStats* GetServerNetworkStats(
      const HostPortPair& host_port_pair) const OVERRIDE;

 private:
  // |spdy_servers_map_| has flattened representation of servers (host, port)
  // that either support or not support SPDY protocol.
  typedef base::MRUCache<std::string, bool> SpdyServerHostPortMap;
  typedef std::map<HostPortPair, NetworkStats> ServerNetworkStatsMap;
  typedef std::map<HostPortPair, HostPortPair> CanonicalHostMap;
  typedef std::vector<std::string> CanonicalSufficList;
  // List of broken host:ports and the times when they can be expired.
  struct BrokenAlternateProtocolEntry {
    HostPortPair server;
    base::TimeTicks when;
  };
  typedef std::list<BrokenAlternateProtocolEntry>
      BrokenAlternateProtocolList;
  // Map from host:port to the number of times alternate protocol has
  // been marked broken.
  typedef std::map<HostPortPair, int> BrokenAlternateProtocolMap;

  // Return the canonical host for |server|, or end if none exists.
  CanonicalHostMap::const_iterator GetCanonicalHost(HostPortPair server) const;

  void ExpireBrokenAlternateProtocolMappings();
  void ScheduleBrokenAlternateProtocolMappingsExpiration();

  SpdyServerHostPortMap spdy_servers_map_;

  AlternateProtocolMap alternate_protocol_map_;
  BrokenAlternateProtocolList broken_alternate_protocol_list_;
  BrokenAlternateProtocolMap broken_alternate_protocol_map_;
  AlternateProtocolExperiment alternate_protocol_experiment_;

  SpdySettingsMap spdy_settings_map_;
  ServerNetworkStatsMap server_network_stats_map_;
  // Contains a map of servers which could share the same alternate protocol.
  // Map from a Canonical host/port (host is some postfix of host names) to an
  // actual origin, which has a plausible alternate protocol mapping.
  CanonicalHostMap canonical_host_to_origin_map_;
  // Contains list of suffixes (for exmaple ".c.youtube.com",
  // ".googlevideo.com", ".googleusercontent.com") of canoncial hostnames.
  CanonicalSufficList canoncial_suffixes_;

  base::WeakPtrFactory<HttpServerPropertiesImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesImpl);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_

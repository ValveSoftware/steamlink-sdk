// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SERVER_PROPERTIES_H_
#define NET_HTTP_HTTP_SERVER_PROPERTIES_H_

#include <map>
#include <string>
#include "base/basictypes.h"
#include "base/containers/mru_cache.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/socket/next_proto.h"
#include "net/spdy/spdy_framer.h"  // TODO(willchan): Reconsider this.

namespace net {

enum AlternateProtocolExperiment {
  // 200 alternate_protocol servers are loaded (persisted 200 MRU servers).
  ALTERNATE_PROTOCOL_NOT_PART_OF_EXPERIMENT = 0,
  // 200 alternate_protocol servers are loaded (persisted 1000 MRU servers).
  ALTERNATE_PROTOCOL_TRUNCATED_200_SERVERS,
  // 1000 alternate_protocol servers are loaded (persisted 1000 MRU servers).
  ALTERNATE_PROTOCOL_TRUNCATED_1000_SERVERS,
};

enum AlternateProtocolUsage {
  // Alternate Protocol was used without racing a normal connection.
  ALTERNATE_PROTOCOL_USAGE_NO_RACE = 0,
  // Alternate Protocol was used by winning a race with a normal connection.
  ALTERNATE_PROTOCOL_USAGE_WON_RACE = 1,
  // Alternate Protocol was not used by losing a race with a normal connection.
  ALTERNATE_PROTOCOL_USAGE_LOST_RACE = 2,
  // Alternate Protocol was not used because no Alternate-Protocol information
  // was available when the request was issued, but an Alternate-Protocol header
  // was present in the response.
  ALTERNATE_PROTOCOL_USAGE_MAPPING_MISSING = 3,
  // Alternate Protocol was not used because it was marked broken.
  ALTERNATE_PROTOCOL_USAGE_BROKEN = 4,
  // Maximum value for the enum.
  ALTERNATE_PROTOCOL_USAGE_MAX,
};

// Log a histogram to reflect |usage| and |alternate_protocol_experiment|.
NET_EXPORT void HistogramAlternateProtocolUsage(
    AlternateProtocolUsage usage,
    AlternateProtocolExperiment alternate_protocol_experiment);

enum BrokenAlternateProtocolLocation {
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_HTTP_STREAM_FACTORY_IMPL_JOB = 0,
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_QUIC_STREAM_FACTORY = 1,
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_HTTP_STREAM_FACTORY_IMPL_JOB_ALT = 2,
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_HTTP_STREAM_FACTORY_IMPL_JOB_MAIN = 3,
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_MAX,
};

// Log a histogram to reflect |location|.
NET_EXPORT void HistogramBrokenAlternateProtocolLocation(
    BrokenAlternateProtocolLocation location);

enum AlternateProtocol {
  DEPRECATED_NPN_SPDY_2 = 0,
  ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION = DEPRECATED_NPN_SPDY_2,
  NPN_SPDY_MINIMUM_VERSION = DEPRECATED_NPN_SPDY_2,
  NPN_SPDY_3,
  NPN_SPDY_3_1,
  NPN_SPDY_4,  // SPDY4 is HTTP/2.
  NPN_SPDY_MAXIMUM_VERSION = NPN_SPDY_4,
  QUIC,
  ALTERNATE_PROTOCOL_MAXIMUM_VALID_VERSION = QUIC,
  ALTERNATE_PROTOCOL_BROKEN,  // The alternate protocol is known to be broken.
  UNINITIALIZED_ALTERNATE_PROTOCOL,
};

// Simply returns whether |protocol| is between
// ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION and
// ALTERNATE_PROTOCOL_MAXIMUM_VALID_VERSION (inclusive).
NET_EXPORT bool IsAlternateProtocolValid(AlternateProtocol protocol);

enum AlternateProtocolSize {
  NUM_VALID_ALTERNATE_PROTOCOLS =
    ALTERNATE_PROTOCOL_MAXIMUM_VALID_VERSION -
    ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION + 1,
};

NET_EXPORT const char* AlternateProtocolToString(AlternateProtocol protocol);
NET_EXPORT AlternateProtocol AlternateProtocolFromString(
    const std::string& str);
NET_EXPORT_PRIVATE AlternateProtocol AlternateProtocolFromNextProto(
    NextProto next_proto);

struct NET_EXPORT PortAlternateProtocolPair {
  bool Equals(const PortAlternateProtocolPair& other) const {
    return port == other.port && protocol == other.protocol;
  }

  std::string ToString() const;

  uint16 port;
  AlternateProtocol protocol;
};

typedef base::MRUCache<
    HostPortPair, PortAlternateProtocolPair> AlternateProtocolMap;
typedef base::MRUCache<HostPortPair, SettingsMap> SpdySettingsMap;

extern const char kAlternateProtocolHeader[];

// The interface for setting/retrieving the HTTP server properties.
// Currently, this class manages servers':
// * SPDY support (based on NPN results)
// * Alternate-Protocol support
// * Spdy Settings (like CWND ID field)
class NET_EXPORT HttpServerProperties {
 public:
  struct NetworkStats {
    base::TimeDelta srtt;
    uint64 bandwidth_estimate;
  };

  HttpServerProperties() {}
  virtual ~HttpServerProperties() {}

  // Gets a weak pointer for this object.
  virtual base::WeakPtr<HttpServerProperties> GetWeakPtr() = 0;

  // Deletes all data.
  virtual void Clear() = 0;

  // Returns true if |server| supports SPDY.
  virtual bool SupportsSpdy(const HostPortPair& server) = 0;

  // Add |server| into the persistent store. Should only be called from IO
  // thread.
  virtual void SetSupportsSpdy(const HostPortPair& server,
                               bool support_spdy) = 0;

  // Returns true if |server| has an Alternate-Protocol header.
  virtual bool HasAlternateProtocol(const HostPortPair& server) = 0;

  // Returns the Alternate-Protocol and port for |server|.
  // HasAlternateProtocol(server) must be true.
  virtual PortAlternateProtocolPair GetAlternateProtocol(
      const HostPortPair& server) = 0;

  // Sets the Alternate-Protocol for |server|.
  virtual void SetAlternateProtocol(const HostPortPair& server,
                                    uint16 alternate_port,
                                    AlternateProtocol alternate_protocol) = 0;

  // Sets the Alternate-Protocol for |server| to be BROKEN.
  virtual void SetBrokenAlternateProtocol(const HostPortPair& server) = 0;

  // Returns true if Alternate-Protocol for |server| was recently BROKEN.
  virtual bool WasAlternateProtocolRecentlyBroken(
      const HostPortPair& server) = 0;

  // Confirms that Alternate-Protocol for |server| is working.
  virtual void ConfirmAlternateProtocol(const HostPortPair& server) = 0;

  // Clears the Alternate-Protocol for |server|.
  virtual void ClearAlternateProtocol(const HostPortPair& server) = 0;

  // Returns all Alternate-Protocol mappings.
  virtual const AlternateProtocolMap& alternate_protocol_map() const = 0;

  virtual void SetAlternateProtocolExperiment(
      AlternateProtocolExperiment experiment) = 0;

  virtual AlternateProtocolExperiment GetAlternateProtocolExperiment()
      const = 0;

  // Gets a reference to the SettingsMap stored for a host.
  // If no settings are stored, returns an empty SettingsMap.
  virtual const SettingsMap& GetSpdySettings(
      const HostPortPair& host_port_pair) = 0;

  // Saves an individual SPDY setting for a host. Returns true if SPDY setting
  // is to be persisted.
  virtual bool SetSpdySetting(const HostPortPair& host_port_pair,
                              SpdySettingsIds id,
                              SpdySettingsFlags flags,
                              uint32 value) = 0;

  // Clears all SPDY settings for a host.
  virtual void ClearSpdySettings(const HostPortPair& host_port_pair) = 0;

  // Clears all SPDY settings for all hosts.
  virtual void ClearAllSpdySettings() = 0;

  // Returns all persistent SPDY settings.
  virtual const SpdySettingsMap& spdy_settings_map() const = 0;

  virtual void SetServerNetworkStats(const HostPortPair& host_port_pair,
                                     NetworkStats stats) = 0;

  virtual const NetworkStats* GetServerNetworkStats(
      const HostPortPair& host_port_pair) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpServerProperties);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_SERVER_PROPERTIES_H_

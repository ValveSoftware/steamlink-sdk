// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include <set>
#include <sys/types.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/escape.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID) && !defined(OS_NACL)
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include <net/if_media.h>
#include <netinet/in_var.h>
#include <sys/ioctl.h>
#endif

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif

namespace net {

namespace {

#if !defined(OS_ANDROID)

struct NetworkInterfaceInfo {
  NetworkInterfaceInfo() : permanent(true) { }

  bool permanent;  // IPv6 has notion of temporary address. If the address is
                   // IPv6 and it's temporary this field will be false.
  NetworkInterface interface;
};

// This method will remove permanent IPv6 addresses if a temporary address
// is available for same network interface.
void RemovePermanentIPv6AddressesWhereTemporaryExists(
    std::vector<NetworkInterfaceInfo>* infos) {
  if (!infos || infos->empty())
    return;

  // Build a set containing the names of interfaces with a temp IPv6 address
  std::set<std::string> ifaces_with_temp_addrs;
  std::vector<NetworkInterfaceInfo>::iterator i;
  for (i = infos->begin(); i != infos->end(); ++i) {
    if (!i->permanent && i->interface.address.size() == kIPv6AddressSize) {
      ifaces_with_temp_addrs.insert(i->interface.name);
    }
  }

  // If there are no such interfaces then there's no further work.
  if (ifaces_with_temp_addrs.empty())
    return;

  // Search for permenent addresses belonging to same network interface.
  for (i = infos->begin(); i != infos->end(); ) {
    // If the address is IPv6 and it's permanent and there is temporary
    // address for it, then we can remove this address.
    if ((i->interface.address.size() == kIPv6AddressSize) && i->permanent &&
        (ifaces_with_temp_addrs.find(i->interface.name) !=
            ifaces_with_temp_addrs.end())) {
      i = infos->erase(i);
    } else {
      ++i;
    }
  }
}

#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)

NetworkChangeNotifier::ConnectionType GetNetworkInterfaceType(
    int addr_family, const std::string& interface_name) {
  NetworkChangeNotifier::ConnectionType type =
      NetworkChangeNotifier::CONNECTION_UNKNOWN;

  struct ifmediareq ifmr = {};
  strncpy(ifmr.ifm_name, interface_name.c_str(), sizeof(ifmr.ifm_name) - 1);

  int s = socket(addr_family, SOCK_DGRAM, 0);
  if (s == -1) {
    return type;
  }

  if (ioctl(s, SIOCGIFMEDIA, &ifmr) != -1) {
    if (ifmr.ifm_current & IFM_IEEE80211) {
      type = NetworkChangeNotifier::CONNECTION_WIFI;
    } else if (ifmr.ifm_current & IFM_ETHER) {
      type = NetworkChangeNotifier::CONNECTION_ETHERNET;
    }
  }
  close(s);
  return type;
}

#endif

}  // namespace

bool GetNetworkList(NetworkInterfaceList* networks, int policy) {
#if defined(OS_NACL)
  NOTIMPLEMENTED();
  return false;
#elif defined(OS_ANDROID)
  std::string network_list = android::GetNetworkList();
  base::StringTokenizer network_interfaces(network_list, "\n");
  while (network_interfaces.GetNext()) {
    std::string network_item = network_interfaces.token();
    base::StringTokenizer network_tokenizer(network_item, "\t");
    CHECK(network_tokenizer.GetNext());
    std::string name = network_tokenizer.token();

    CHECK(network_tokenizer.GetNext());
    std::string interface_address = network_tokenizer.token();
    IPAddressNumber address;
    size_t network_prefix = 0;
    CHECK(ParseCIDRBlock(network_tokenizer.token(),
                         &address,
                         &network_prefix));

    CHECK(network_tokenizer.GetNext());
    uint32 index = 0;
    CHECK(base::StringToUint(network_tokenizer.token(), &index));

    networks->push_back(
        NetworkInterface(name, name, index,
                         NetworkChangeNotifier::CONNECTION_UNKNOWN,
                         address, network_prefix));
  }
  return true;
#else
  // getifaddrs() may require IO operations.
  base::ThreadRestrictions::AssertIOAllowed();

  int ioctl_socket = -1;
  if (policy & INCLUDE_ONLY_TEMP_IPV6_ADDRESS_IF_POSSIBLE) {
    // we need a socket to query information about temporary address.
    ioctl_socket = socket(AF_INET6, SOCK_DGRAM, 0);
    DCHECK_GT(ioctl_socket, 0);
  }

  ifaddrs *interfaces;
  if (getifaddrs(&interfaces) < 0) {
    PLOG(ERROR) << "getifaddrs";
    return false;
  }

  std::vector<NetworkInterfaceInfo> network_infos;

  // Enumerate the addresses assigned to network interfaces which are up.
  for (ifaddrs *interface = interfaces;
       interface != NULL;
       interface = interface->ifa_next) {
    // Skip loopback interfaces, and ones which are down.
    if (!(IFF_UP & interface->ifa_flags))
      continue;
    if (IFF_LOOPBACK & interface->ifa_flags)
      continue;
    // Skip interfaces with no address configured.
    struct sockaddr* addr = interface->ifa_addr;
    if (!addr)
      continue;

    // Skip unspecified addresses (i.e. made of zeroes) and loopback addresses
    // configured on non-loopback interfaces.
    int addr_size = 0;
    if (addr->sa_family == AF_INET6) {
      struct sockaddr_in6* addr_in6 =
          reinterpret_cast<struct sockaddr_in6*>(addr);
      struct in6_addr* sin6_addr = &addr_in6->sin6_addr;
      addr_size = sizeof(*addr_in6);
      if (IN6_IS_ADDR_LOOPBACK(sin6_addr) ||
          IN6_IS_ADDR_UNSPECIFIED(sin6_addr)) {
        continue;
      }
    } else if (addr->sa_family == AF_INET) {
      struct sockaddr_in* addr_in =
          reinterpret_cast<struct sockaddr_in*>(addr);
      addr_size = sizeof(*addr_in);
      if (addr_in->sin_addr.s_addr == INADDR_LOOPBACK ||
          addr_in->sin_addr.s_addr == 0) {
        continue;
      }
    } else {
      // Skip non-IP addresses.
      continue;
    }

    const std::string& name = interface->ifa_name;
    // Filter out VMware interfaces, typically named vmnet1 and vmnet8.
    if ((policy & EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES) &&
        ((name.find("vmnet") != std::string::npos) ||
         (name.find("vnic") != std::string::npos))) {
      continue;
    }

    NetworkInterfaceInfo network_info;
    NetworkChangeNotifier::ConnectionType connection_type =
        NetworkChangeNotifier::CONNECTION_UNKNOWN;
#if defined(OS_MACOSX) && !defined(OS_IOS)
    // Check if this is a temporary address. Currently this is only supported
    // on Mac.
    if ((policy & INCLUDE_ONLY_TEMP_IPV6_ADDRESS_IF_POSSIBLE) &&
        ioctl_socket >= 0 && addr->sa_family == AF_INET6) {
      struct in6_ifreq ifr = {};
      strncpy(ifr.ifr_name, interface->ifa_name, sizeof(ifr.ifr_name) - 1);
      memcpy(&ifr.ifr_ifru.ifru_addr, interface->ifa_addr,
             interface->ifa_addr->sa_len);
      int rv = ioctl(ioctl_socket, SIOCGIFAFLAG_IN6, &ifr);
      if (rv >= 0) {
        network_info.permanent = !(ifr.ifr_ifru.ifru_flags & IN6_IFF_TEMPORARY);
      }
    }

    connection_type = GetNetworkInterfaceType(addr->sa_family, name);
#endif

    IPEndPoint address;
    if (address.FromSockAddr(addr, addr_size)) {
      uint8 net_mask = 0;
      if (interface->ifa_netmask) {
        // If not otherwise set, assume the same sa_family as ifa_addr.
        if (interface->ifa_netmask->sa_family == 0) {
          interface->ifa_netmask->sa_family = addr->sa_family;
        }
        IPEndPoint netmask;
        if (netmask.FromSockAddr(interface->ifa_netmask, addr_size)) {
          net_mask = MaskPrefixLength(netmask.address());
        }
      }
      network_info.interface = NetworkInterface(
          name, name, if_nametoindex(name.c_str()),
          connection_type, address.address(), net_mask);

      network_infos.push_back(NetworkInterfaceInfo(network_info));
    }
  }
  freeifaddrs(interfaces);
  if (ioctl_socket >= 0) {
    close(ioctl_socket);
  }

  if (policy & INCLUDE_ONLY_TEMP_IPV6_ADDRESS_IF_POSSIBLE) {
    RemovePermanentIPv6AddressesWhereTemporaryExists(&network_infos);
  }

  for (size_t i = 0; i < network_infos.size(); ++i) {
    networks->push_back(network_infos[i].interface);
  }
  return true;
#endif
}

WifiPHYLayerProtocol GetWifiPHYLayerProtocol() {
  return WIFI_PHY_LAYER_PROTOCOL_UNKNOWN;
}

}  // namespace net

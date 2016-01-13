// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier.h"

#include "base/metrics/histogram.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier_factory.h"
#include "net/dns/dns_config_service.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "net/android/network_library.h"
#endif

#if defined(OS_WIN)
#include "net/base/network_change_notifier_win.h"
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "net/base/network_change_notifier_linux.h"
#elif defined(OS_MACOSX)
#include "net/base/network_change_notifier_mac.h"
#endif

namespace net {

namespace {

// The actual singleton notifier.  The class contract forbids usage of the API
// in ways that would require us to place locks around access to this object.
// (The prohibition on global non-POD objects makes it tricky to do such a thing
// anyway.)
NetworkChangeNotifier* g_network_change_notifier = NULL;

// Class factory singleton.
NetworkChangeNotifierFactory* g_network_change_notifier_factory = NULL;

class MockNetworkChangeNotifier : public NetworkChangeNotifier {
 public:
  virtual ConnectionType GetCurrentConnectionType() const OVERRIDE {
    return CONNECTION_UNKNOWN;
  }
};

}  // namespace

// The main observer class that records UMAs for network events.
class HistogramWatcher
    : public NetworkChangeNotifier::ConnectionTypeObserver,
      public NetworkChangeNotifier::IPAddressObserver,
      public NetworkChangeNotifier::DNSObserver,
      public NetworkChangeNotifier::NetworkChangeObserver {
 public:
  HistogramWatcher()
      : last_ip_address_change_(base::TimeTicks::Now()),
        last_connection_change_(base::TimeTicks::Now()),
        last_dns_change_(base::TimeTicks::Now()),
        last_network_change_(base::TimeTicks::Now()),
        last_connection_type_(NetworkChangeNotifier::CONNECTION_UNKNOWN),
        offline_packets_received_(0),
        bytes_read_since_last_connection_change_(0),
        peak_kbps_since_last_connection_change_(0) {}

  // Registers our three Observer implementations.  This is called from the
  // network thread so that our Observer implementations are also called
  // from the network thread.  This avoids multi-threaded race conditions
  // because the only other interface, |NotifyDataReceived| is also
  // only called from the network thread.
  void Init() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(g_network_change_notifier);
    NetworkChangeNotifier::AddConnectionTypeObserver(this);
    NetworkChangeNotifier::AddIPAddressObserver(this);
    NetworkChangeNotifier::AddDNSObserver(this);
    NetworkChangeNotifier::AddNetworkChangeObserver(this);
  }

  virtual ~HistogramWatcher() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(g_network_change_notifier);
    NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
    NetworkChangeNotifier::RemoveIPAddressObserver(this);
    NetworkChangeNotifier::RemoveDNSObserver(this);
    NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }

  // NetworkChangeNotifier::IPAddressObserver implementation.
  virtual void OnIPAddressChanged() OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    UMA_HISTOGRAM_MEDIUM_TIMES("NCN.IPAddressChange",
                               SinceLast(&last_ip_address_change_));
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NCN.ConnectionTypeChangeToIPAddressChange",
        last_ip_address_change_ - last_connection_change_);
  }

  // NetworkChangeNotifier::ConnectionTypeObserver implementation.
  virtual void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType type) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    base::TimeTicks now = base::TimeTicks::Now();
    int32 kilobytes_read = bytes_read_since_last_connection_change_ / 1000;
    base::TimeDelta state_duration = SinceLast(&last_connection_change_);
    if (bytes_read_since_last_connection_change_) {
      switch (last_connection_type_) {
        case NetworkChangeNotifier::CONNECTION_UNKNOWN:
          UMA_HISTOGRAM_TIMES("NCN.CM.FirstReadOnUnknown",
                              first_byte_after_connection_change_);
          UMA_HISTOGRAM_TIMES("NCN.CM.FastestRTTOnUnknown",
                              fastest_RTT_since_last_connection_change_);
          break;
        case NetworkChangeNotifier::CONNECTION_ETHERNET:
          UMA_HISTOGRAM_TIMES("NCN.CM.FirstReadOnEthernet",
                              first_byte_after_connection_change_);
          UMA_HISTOGRAM_TIMES("NCN.CM.FastestRTTOnEthernet",
                              fastest_RTT_since_last_connection_change_);
          break;
        case NetworkChangeNotifier::CONNECTION_WIFI:
          UMA_HISTOGRAM_TIMES("NCN.CM.FirstReadOnWifi",
                              first_byte_after_connection_change_);
          UMA_HISTOGRAM_TIMES("NCN.CM.FastestRTTOnWifi",
                              fastest_RTT_since_last_connection_change_);
          break;
        case NetworkChangeNotifier::CONNECTION_2G:
          UMA_HISTOGRAM_TIMES("NCN.CM.FirstReadOn2G",
                              first_byte_after_connection_change_);
          UMA_HISTOGRAM_TIMES("NCN.CM.FastestRTTOn2G",
                              fastest_RTT_since_last_connection_change_);
          break;
        case NetworkChangeNotifier::CONNECTION_3G:
          UMA_HISTOGRAM_TIMES("NCN.CM.FirstReadOn3G",
                              first_byte_after_connection_change_);
          UMA_HISTOGRAM_TIMES("NCN.CM.FastestRTTOn3G",
                              fastest_RTT_since_last_connection_change_);
          break;
        case NetworkChangeNotifier::CONNECTION_4G:
          UMA_HISTOGRAM_TIMES("NCN.CM.FirstReadOn4G",
                              first_byte_after_connection_change_);
          UMA_HISTOGRAM_TIMES("NCN.CM.FastestRTTOn4G",
                              fastest_RTT_since_last_connection_change_);
          break;
        case NetworkChangeNotifier::CONNECTION_NONE:
          UMA_HISTOGRAM_TIMES("NCN.CM.FirstReadOnNone",
                              first_byte_after_connection_change_);
          UMA_HISTOGRAM_TIMES("NCN.CM.FastestRTTOnNone",
                              fastest_RTT_since_last_connection_change_);
          break;
        case NetworkChangeNotifier::CONNECTION_BLUETOOTH:
          UMA_HISTOGRAM_TIMES("NCN.CM.FirstReadOnBluetooth",
                              first_byte_after_connection_change_);
          UMA_HISTOGRAM_TIMES("NCN.CM.FastestRTTOnBluetooth",
                              fastest_RTT_since_last_connection_change_);
      }
    }
    if (peak_kbps_since_last_connection_change_) {
      switch (last_connection_type_) {
        case NetworkChangeNotifier::CONNECTION_UNKNOWN:
          UMA_HISTOGRAM_COUNTS("NCN.CM.PeakKbpsOnUnknown",
                               peak_kbps_since_last_connection_change_);
          break;
        case NetworkChangeNotifier::CONNECTION_ETHERNET:
          UMA_HISTOGRAM_COUNTS("NCN.CM.PeakKbpsOnEthernet",
                               peak_kbps_since_last_connection_change_);
          break;
        case NetworkChangeNotifier::CONNECTION_WIFI:
          UMA_HISTOGRAM_COUNTS("NCN.CM.PeakKbpsOnWifi",
                               peak_kbps_since_last_connection_change_);
          break;
        case NetworkChangeNotifier::CONNECTION_2G:
          UMA_HISTOGRAM_COUNTS("NCN.CM.PeakKbpsOn2G",
                               peak_kbps_since_last_connection_change_);
          break;
        case NetworkChangeNotifier::CONNECTION_3G:
          UMA_HISTOGRAM_COUNTS("NCN.CM.PeakKbpsOn3G",
                               peak_kbps_since_last_connection_change_);
          break;
        case NetworkChangeNotifier::CONNECTION_4G:
          UMA_HISTOGRAM_COUNTS("NCN.CM.PeakKbpsOn4G",
                               peak_kbps_since_last_connection_change_);
          break;
        case NetworkChangeNotifier::CONNECTION_NONE:
          UMA_HISTOGRAM_COUNTS("NCN.CM.PeakKbpsOnNone",
                               peak_kbps_since_last_connection_change_);
          break;
        case NetworkChangeNotifier::CONNECTION_BLUETOOTH:
          UMA_HISTOGRAM_COUNTS("NCN.CM.PeakKbpsOnBluetooth",
                               peak_kbps_since_last_connection_change_);
          break;
      }
    }
    switch (last_connection_type_) {
      case NetworkChangeNotifier::CONNECTION_UNKNOWN:
        UMA_HISTOGRAM_LONG_TIMES("NCN.CM.TimeOnUnknown", state_duration);
        UMA_HISTOGRAM_COUNTS("NCN.CM.KBTransferedOnUnknown", kilobytes_read);
        break;
      case NetworkChangeNotifier::CONNECTION_ETHERNET:
        UMA_HISTOGRAM_LONG_TIMES("NCN.CM.TimeOnEthernet", state_duration);
        UMA_HISTOGRAM_COUNTS("NCN.CM.KBTransferedOnEthernet", kilobytes_read);
        break;
      case NetworkChangeNotifier::CONNECTION_WIFI:
        UMA_HISTOGRAM_LONG_TIMES("NCN.CM.TimeOnWifi", state_duration);
        UMA_HISTOGRAM_COUNTS("NCN.CM.KBTransferedOnWifi", kilobytes_read);
        break;
      case NetworkChangeNotifier::CONNECTION_2G:
        UMA_HISTOGRAM_LONG_TIMES("NCN.CM.TimeOn2G", state_duration);
        UMA_HISTOGRAM_COUNTS("NCN.CM.KBTransferedOn2G", kilobytes_read);
        break;
      case NetworkChangeNotifier::CONNECTION_3G:
        UMA_HISTOGRAM_LONG_TIMES("NCN.CM.TimeOn3G", state_duration);
        UMA_HISTOGRAM_COUNTS("NCN.CM.KBTransferedOn3G", kilobytes_read);
        break;
      case NetworkChangeNotifier::CONNECTION_4G:
        UMA_HISTOGRAM_LONG_TIMES("NCN.CM.TimeOn4G", state_duration);
        UMA_HISTOGRAM_COUNTS("NCN.CM.KBTransferedOn4G", kilobytes_read);
        break;
      case NetworkChangeNotifier::CONNECTION_NONE:
        UMA_HISTOGRAM_LONG_TIMES("NCN.CM.TimeOnNone", state_duration);
        UMA_HISTOGRAM_COUNTS("NCN.CM.KBTransferedOnNone", kilobytes_read);
        break;
      case NetworkChangeNotifier::CONNECTION_BLUETOOTH:
        UMA_HISTOGRAM_LONG_TIMES("NCN.CM.TimeOnBluetooth", state_duration);
        UMA_HISTOGRAM_COUNTS("NCN.CM.KBTransferedOnBluetooth", kilobytes_read);
        break;
    }

    if (type != NetworkChangeNotifier::CONNECTION_NONE) {
      UMA_HISTOGRAM_MEDIUM_TIMES("NCN.OnlineChange", state_duration);

      if (offline_packets_received_) {
        if ((now - last_offline_packet_received_) <
            base::TimeDelta::FromSeconds(5)) {
          // We can compare this sum with the sum of NCN.OfflineDataRecv.
          UMA_HISTOGRAM_COUNTS_10000(
              "NCN.OfflineDataRecvAny5sBeforeOnline",
              offline_packets_received_);
        }

        UMA_HISTOGRAM_MEDIUM_TIMES("NCN.OfflineDataRecvUntilOnline",
                                   now - last_offline_packet_received_);
      }
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES("NCN.OfflineChange", state_duration);
    }

    NetworkChangeNotifier::LogOperatorCodeHistogram(type);

    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NCN.IPAddressChangeToConnectionTypeChange",
        now - last_ip_address_change_);

    offline_packets_received_ = 0;
    bytes_read_since_last_connection_change_ = 0;
    peak_kbps_since_last_connection_change_ = 0;
    last_connection_type_ = type;
    polling_interval_ = base::TimeDelta::FromSeconds(1);
  }

  // NetworkChangeNotifier::DNSObserver implementation.
  virtual void OnDNSChanged() OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    UMA_HISTOGRAM_MEDIUM_TIMES("NCN.DNSConfigChange",
                               SinceLast(&last_dns_change_));
  }

  // NetworkChangeNotifier::NetworkChangeObserver implementation.
  virtual void OnNetworkChanged(
      NetworkChangeNotifier::ConnectionType type) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (type != NetworkChangeNotifier::CONNECTION_NONE) {
      UMA_HISTOGRAM_MEDIUM_TIMES("NCN.NetworkOnlineChange",
                                 SinceLast(&last_network_change_));
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES("NCN.NetworkOfflineChange",
                                 SinceLast(&last_network_change_));
    }
  }

  // Record histogram data whenever we receive a packet. Should only be called
  // from the network thread.
  void NotifyDataReceived(const URLRequest& request, int bytes_read) {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (IsLocalhost(request.url().host()) ||
        !request.url().SchemeIsHTTPOrHTTPS()) {
      return;
    }

    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeDelta request_duration = now - request.creation_time();
    if (bytes_read_since_last_connection_change_ == 0) {
      first_byte_after_connection_change_ = now - last_connection_change_;
      fastest_RTT_since_last_connection_change_ = request_duration;
    }
    bytes_read_since_last_connection_change_ += bytes_read;
    if (request_duration < fastest_RTT_since_last_connection_change_)
      fastest_RTT_since_last_connection_change_ = request_duration;
    // Ignore tiny transfers which will not produce accurate rates.
    // Ignore zero duration transfers which might cause divide by zero.
    if (bytes_read > 10000 &&
        request_duration > base::TimeDelta::FromMilliseconds(1) &&
        request.creation_time() > last_connection_change_) {
      int32 kbps = bytes_read * 8 / request_duration.InMilliseconds();
      if (kbps > peak_kbps_since_last_connection_change_)
        peak_kbps_since_last_connection_change_ = kbps;
    }

    if (last_connection_type_ != NetworkChangeNotifier::CONNECTION_NONE)
      return;

    UMA_HISTOGRAM_MEDIUM_TIMES("NCN.OfflineDataRecv",
                               now - last_connection_change_);
    offline_packets_received_++;
    last_offline_packet_received_ = now;

    if ((now - last_polled_connection_) > polling_interval_) {
      polling_interval_ *= 2;
      last_polled_connection_ = now;
      last_polled_connection_type_ =
          NetworkChangeNotifier::GetConnectionType();
    }
    if (last_polled_connection_type_ ==
        NetworkChangeNotifier::CONNECTION_NONE) {
      UMA_HISTOGRAM_MEDIUM_TIMES("NCN.PollingOfflineDataRecv",
                                 now - last_connection_change_);
    }
  }

 private:
  static base::TimeDelta SinceLast(base::TimeTicks *last_time) {
    base::TimeTicks current_time = base::TimeTicks::Now();
    base::TimeDelta delta = current_time - *last_time;
    *last_time = current_time;
    return delta;
  }

  base::TimeTicks last_ip_address_change_;
  base::TimeTicks last_connection_change_;
  base::TimeTicks last_dns_change_;
  base::TimeTicks last_network_change_;
  base::TimeTicks last_offline_packet_received_;
  base::TimeTicks last_polled_connection_;
  // |polling_interval_| is initialized by |OnConnectionTypeChanged| on our
  // first transition to offline and on subsequent transitions.  Once offline,
  // |polling_interval_| doubles as offline data is received and we poll
  // with |NetworkChangeNotifier::GetConnectionType| to verify the connection
  // state.
  base::TimeDelta polling_interval_;
  // |last_connection_type_| is the last value passed to
  // |OnConnectionTypeChanged|.
  NetworkChangeNotifier::ConnectionType last_connection_type_;
  // |last_polled_connection_type_| is last result from calling
  // |NetworkChangeNotifier::GetConnectionType| in |NotifyDataReceived|.
  NetworkChangeNotifier::ConnectionType last_polled_connection_type_;
  // Count of how many times NotifyDataReceived() has been called while the
  // NetworkChangeNotifier thought network connection was offline.
  int32 offline_packets_received_;
  // Number of bytes of network data received since last connectivity change.
  int32 bytes_read_since_last_connection_change_;
  // Fastest round-trip-time (RTT) since last connectivity change. RTT measured
  // from URLRequest creation until first byte received.
  base::TimeDelta fastest_RTT_since_last_connection_change_;
  // Time between connectivity change and first network data byte received.
  base::TimeDelta first_byte_after_connection_change_;
  // Rough measurement of peak KB/s witnessed since last connectivity change.
  // The accuracy is decreased by ignoring these factors:
  // 1) Multiple URLRequests can occur concurrently.
  // 2) NotifyDataReceived() may be called repeatedly for one URLRequest.
  // 3) The transfer time includes at least one RTT while no bytes are read.
  // Erring on the conservative side is hopefully offset by taking the maximum.
  int32 peak_kbps_since_last_connection_change_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(HistogramWatcher);
};

// NetworkState is thread safe.
class NetworkChangeNotifier::NetworkState {
 public:
  NetworkState() {}
  ~NetworkState() {}

  void GetDnsConfig(DnsConfig* config) const {
    base::AutoLock lock(lock_);
    *config = dns_config_;
  }

  void SetDnsConfig(const DnsConfig& dns_config) {
    base::AutoLock lock(lock_);
    dns_config_ = dns_config;
  }

 private:
  mutable base::Lock lock_;
  DnsConfig dns_config_;
};

NetworkChangeNotifier::NetworkChangeCalculatorParams::
    NetworkChangeCalculatorParams() {
}

// Calculates NetworkChange signal from IPAddress and ConnectionType signals.
class NetworkChangeNotifier::NetworkChangeCalculator
    : public ConnectionTypeObserver,
      public IPAddressObserver {
 public:
  NetworkChangeCalculator(const NetworkChangeCalculatorParams& params)
      : params_(params),
        have_announced_(false),
        last_announced_connection_type_(CONNECTION_NONE),
        pending_connection_type_(CONNECTION_NONE) {}

  void Init() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(g_network_change_notifier);
    AddConnectionTypeObserver(this);
    AddIPAddressObserver(this);
  }

  virtual ~NetworkChangeCalculator() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(g_network_change_notifier);
    RemoveConnectionTypeObserver(this);
    RemoveIPAddressObserver(this);
  }

  // NetworkChangeNotifier::IPAddressObserver implementation.
  virtual void OnIPAddressChanged() OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    base::TimeDelta delay = last_announced_connection_type_ == CONNECTION_NONE
        ? params_.ip_address_offline_delay_ : params_.ip_address_online_delay_;
    // Cancels any previous timer.
    timer_.Start(FROM_HERE, delay, this, &NetworkChangeCalculator::Notify);
  }

  // NetworkChangeNotifier::ConnectionTypeObserver implementation.
  virtual void OnConnectionTypeChanged(ConnectionType type) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    pending_connection_type_ = type;
    base::TimeDelta delay = last_announced_connection_type_ == CONNECTION_NONE
        ? params_.connection_type_offline_delay_
        : params_.connection_type_online_delay_;
    // Cancels any previous timer.
    timer_.Start(FROM_HERE, delay, this, &NetworkChangeCalculator::Notify);
  }

 private:
  void Notify() {
    DCHECK(thread_checker_.CalledOnValidThread());
    // Don't bother signaling about dead connections.
    if (have_announced_ &&
        (last_announced_connection_type_ == CONNECTION_NONE) &&
        (pending_connection_type_ == CONNECTION_NONE)) {
      return;
    }
    have_announced_ = true;
    last_announced_connection_type_ = pending_connection_type_;
    // Immediately before sending out an online signal, send out an offline
    // signal to perform any destructive actions before constructive actions.
    if (pending_connection_type_ != CONNECTION_NONE)
      NetworkChangeNotifier::NotifyObserversOfNetworkChange(CONNECTION_NONE);
    NetworkChangeNotifier::NotifyObserversOfNetworkChange(
        pending_connection_type_);
  }

  const NetworkChangeCalculatorParams params_;

  // Indicates if NotifyObserversOfNetworkChange has been called yet.
  bool have_announced_;
  // Last value passed to NotifyObserversOfNetworkChange.
  ConnectionType last_announced_connection_type_;
  // Value to pass to NotifyObserversOfNetworkChange when Notify is called.
  ConnectionType pending_connection_type_;
  // Used to delay notifications so duplicates can be combined.
  base::OneShotTimer<NetworkChangeCalculator> timer_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeCalculator);
};

NetworkChangeNotifier::~NetworkChangeNotifier() {
  network_change_calculator_.reset();
  DCHECK_EQ(this, g_network_change_notifier);
  g_network_change_notifier = NULL;
}

// static
void NetworkChangeNotifier::SetFactory(
    NetworkChangeNotifierFactory* factory) {
  CHECK(!g_network_change_notifier_factory);
  g_network_change_notifier_factory = factory;
}

// static
NetworkChangeNotifier* NetworkChangeNotifier::Create() {
  if (g_network_change_notifier_factory)
    return g_network_change_notifier_factory->CreateInstance();

#if defined(OS_WIN)
  NetworkChangeNotifierWin* network_change_notifier =
      new NetworkChangeNotifierWin();
  network_change_notifier->WatchForAddressChange();
  return network_change_notifier;
#elif defined(OS_CHROMEOS) || defined(OS_ANDROID)
  // ChromeOS and Android builds MUST use their own class factory.
#if !defined(OS_CHROMEOS)
  // TODO(oshima): ash_shell do not have access to chromeos'es
  // notifier yet. Re-enable this when chromeos'es notifier moved to
  // chromeos root directory. crbug.com/119298.
  CHECK(false);
#endif
  return NULL;
#elif defined(OS_LINUX)
  return NetworkChangeNotifierLinux::Create();
#elif defined(OS_MACOSX)
  return new NetworkChangeNotifierMac();
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

// static
NetworkChangeNotifier::ConnectionType
NetworkChangeNotifier::GetConnectionType() {
  return g_network_change_notifier ?
      g_network_change_notifier->GetCurrentConnectionType() :
      CONNECTION_UNKNOWN;
}

// static
void NetworkChangeNotifier::GetDnsConfig(DnsConfig* config) {
  if (!g_network_change_notifier) {
    *config = DnsConfig();
  } else {
    g_network_change_notifier->network_state_->GetDnsConfig(config);
  }
}

// static
const char* NetworkChangeNotifier::ConnectionTypeToString(
    ConnectionType type) {
  static const char* kConnectionTypeNames[] = {
    "CONNECTION_UNKNOWN",
    "CONNECTION_ETHERNET",
    "CONNECTION_WIFI",
    "CONNECTION_2G",
    "CONNECTION_3G",
    "CONNECTION_4G",
    "CONNECTION_NONE",
    "CONNECTION_BLUETOOTH"
  };
  COMPILE_ASSERT(
      arraysize(kConnectionTypeNames) ==
          NetworkChangeNotifier::CONNECTION_LAST + 1,
      ConnectionType_name_count_mismatch);
  if (type < CONNECTION_UNKNOWN || type > CONNECTION_LAST) {
    NOTREACHED();
    return "CONNECTION_INVALID";
  }
  return kConnectionTypeNames[type];
}

// static
void NetworkChangeNotifier::NotifyDataReceived(const URLRequest& request,
                                               int bytes_read) {
  if (!g_network_change_notifier ||
      !g_network_change_notifier->histogram_watcher_) {
    return;
  }
  g_network_change_notifier->histogram_watcher_->NotifyDataReceived(request,
                                                                    bytes_read);
}

// static
void NetworkChangeNotifier::InitHistogramWatcher() {
  if (!g_network_change_notifier)
    return;
  g_network_change_notifier->histogram_watcher_.reset(new HistogramWatcher());
  g_network_change_notifier->histogram_watcher_->Init();
}

// static
void NetworkChangeNotifier::ShutdownHistogramWatcher() {
  if (!g_network_change_notifier)
    return;
  g_network_change_notifier->histogram_watcher_.reset();
}

// static
void NetworkChangeNotifier::LogOperatorCodeHistogram(ConnectionType type) {
#if defined(OS_ANDROID)
  // On a connection type change to 2/3/4G, log the network operator MCC/MNC.
  // Log zero in other cases.
  unsigned mcc_mnc = 0;
  if (type == NetworkChangeNotifier::CONNECTION_2G ||
      type == NetworkChangeNotifier::CONNECTION_3G ||
      type == NetworkChangeNotifier::CONNECTION_4G) {
    // Log zero if not perfectly converted.
    if (!base::StringToUint(
        net::android::GetTelephonyNetworkOperator(), &mcc_mnc)) {
      mcc_mnc = 0;
    }
  }
  UMA_HISTOGRAM_SPARSE_SLOWLY("NCN.NetworkOperatorMCCMNC", mcc_mnc);
#endif
}

#if defined(OS_LINUX)
// static
const internal::AddressTrackerLinux*
NetworkChangeNotifier::GetAddressTracker() {
  return g_network_change_notifier ?
        g_network_change_notifier->GetAddressTrackerInternal() : NULL;
}
#endif

// static
bool NetworkChangeNotifier::IsOffline() {
   return GetConnectionType() == CONNECTION_NONE;
}

// static
bool NetworkChangeNotifier::IsConnectionCellular(ConnectionType type) {
  bool is_cellular = false;
  switch (type) {
    case CONNECTION_2G:
    case CONNECTION_3G:
    case CONNECTION_4G:
      is_cellular =  true;
      break;
    case CONNECTION_UNKNOWN:
    case CONNECTION_ETHERNET:
    case CONNECTION_WIFI:
    case CONNECTION_NONE:
    case CONNECTION_BLUETOOTH:
      is_cellular = false;
      break;
  }
  return is_cellular;
}

// static
NetworkChangeNotifier* NetworkChangeNotifier::CreateMock() {
  return new MockNetworkChangeNotifier();
}

void NetworkChangeNotifier::AddIPAddressObserver(IPAddressObserver* observer) {
  if (g_network_change_notifier)
    g_network_change_notifier->ip_address_observer_list_->AddObserver(observer);
}

void NetworkChangeNotifier::AddConnectionTypeObserver(
    ConnectionTypeObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->connection_type_observer_list_->AddObserver(
        observer);
  }
}

void NetworkChangeNotifier::AddDNSObserver(DNSObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->resolver_state_observer_list_->AddObserver(
        observer);
  }
}

void NetworkChangeNotifier::AddNetworkChangeObserver(
    NetworkChangeObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->network_change_observer_list_->AddObserver(
        observer);
  }
}

void NetworkChangeNotifier::RemoveIPAddressObserver(
    IPAddressObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->ip_address_observer_list_->RemoveObserver(
        observer);
  }
}

void NetworkChangeNotifier::RemoveConnectionTypeObserver(
    ConnectionTypeObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->connection_type_observer_list_->RemoveObserver(
        observer);
  }
}

void NetworkChangeNotifier::RemoveDNSObserver(DNSObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->resolver_state_observer_list_->RemoveObserver(
        observer);
  }
}

void NetworkChangeNotifier::RemoveNetworkChangeObserver(
    NetworkChangeObserver* observer) {
  if (g_network_change_notifier) {
    g_network_change_notifier->network_change_observer_list_->RemoveObserver(
        observer);
  }
}

NetworkChangeNotifier::NetworkChangeNotifier(
    const NetworkChangeCalculatorParams& params
        /*= NetworkChangeCalculatorParams()*/)
    : ip_address_observer_list_(
        new ObserverListThreadSafe<IPAddressObserver>(
            ObserverListBase<IPAddressObserver>::NOTIFY_EXISTING_ONLY)),
      connection_type_observer_list_(
        new ObserverListThreadSafe<ConnectionTypeObserver>(
            ObserverListBase<ConnectionTypeObserver>::NOTIFY_EXISTING_ONLY)),
      resolver_state_observer_list_(
        new ObserverListThreadSafe<DNSObserver>(
            ObserverListBase<DNSObserver>::NOTIFY_EXISTING_ONLY)),
      network_change_observer_list_(
        new ObserverListThreadSafe<NetworkChangeObserver>(
            ObserverListBase<NetworkChangeObserver>::NOTIFY_EXISTING_ONLY)),
      network_state_(new NetworkState()),
      network_change_calculator_(new NetworkChangeCalculator(params)) {
  DCHECK(!g_network_change_notifier);
  g_network_change_notifier = this;
  network_change_calculator_->Init();
}

#if defined(OS_LINUX)
const internal::AddressTrackerLinux*
NetworkChangeNotifier::GetAddressTrackerInternal() const {
  return NULL;
}
#endif

// static
void NetworkChangeNotifier::NotifyObserversOfIPAddressChange() {
  if (g_network_change_notifier) {
    g_network_change_notifier->ip_address_observer_list_->Notify(
        &IPAddressObserver::OnIPAddressChanged);
  }
}

// static
void NetworkChangeNotifier::NotifyObserversOfDNSChange() {
  if (g_network_change_notifier) {
    g_network_change_notifier->resolver_state_observer_list_->Notify(
        &DNSObserver::OnDNSChanged);
  }
}

// static
void NetworkChangeNotifier::SetDnsConfig(const DnsConfig& config) {
  if (!g_network_change_notifier)
    return;
  g_network_change_notifier->network_state_->SetDnsConfig(config);
  NotifyObserversOfDNSChange();
}

void NetworkChangeNotifier::NotifyObserversOfConnectionTypeChange() {
  if (g_network_change_notifier) {
    g_network_change_notifier->connection_type_observer_list_->Notify(
        &ConnectionTypeObserver::OnConnectionTypeChanged,
        GetConnectionType());
  }
}

void NetworkChangeNotifier::NotifyObserversOfNetworkChange(
    ConnectionType type) {
  if (g_network_change_notifier) {
    g_network_change_notifier->network_change_observer_list_->Notify(
        &NetworkChangeObserver::OnNetworkChanged,
        type);
  }
}

NetworkChangeNotifier::DisableForTest::DisableForTest()
    : network_change_notifier_(g_network_change_notifier) {
  DCHECK(g_network_change_notifier);
  g_network_change_notifier = NULL;
}

NetworkChangeNotifier::DisableForTest::~DisableForTest() {
  DCHECK(!g_network_change_notifier);
  g_network_change_notifier = network_change_notifier_;
}

}  // namespace net

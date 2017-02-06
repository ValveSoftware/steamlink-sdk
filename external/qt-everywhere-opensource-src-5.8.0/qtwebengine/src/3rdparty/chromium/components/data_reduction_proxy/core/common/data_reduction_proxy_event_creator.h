// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_EVENT_CREATOR_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_EVENT_CREATOR_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_storage_delegate.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "net/log/net_log.h"

class GURL;

namespace base {
class TimeDelta;
class Value;
}

namespace net {
class BoundNetLog;
class ProxyServer;
}

namespace data_reduction_proxy {

// Central location for creating debug events for the Data Reduction Proxy.
// This object lives on the IO thread and all of its methods are expected to be
// called from there.
class DataReductionProxyEventCreator {
 public:
  // Constructs a DataReductionProxyEventCreator object. |storage_delegate| must
  // outlive |this| and can be used to store Data Reduction Proxy events for
  // debugging without requiring a net::NetLog.
  explicit DataReductionProxyEventCreator(
      DataReductionProxyEventStorageDelegate* storage_delegate);

  ~DataReductionProxyEventCreator();

  // Adds the DATA_REDUCTION_PROXY_ENABLED event (with enabled=true) to the
  // event store.
  void AddProxyEnabledEvent(
      net::NetLog* net_log,
      bool secure_transport_restricted,
      const std::vector<net::ProxyServer>& proxies_for_http);

  // Adds the DATA_REDUCTION_PROXY_ENABLED event (with enabled=false) to the
  // event store.
  void AddProxyDisabledEvent(net::NetLog* net_log);

  // Adds a DATA_REDUCTION_PROXY_BYPASS_REQUESTED event to the event store
  // when the bypass reason is initiated by the data reduction proxy.
  void AddBypassActionEvent(const net::BoundNetLog& net_log,
                            DataReductionProxyBypassAction bypass_action,
                            const std::string& request_method,
                            const GURL& gurl,
                            bool should_retry,
                            const base::TimeDelta& bypass_duration);

  // Adds a DATA_REDUCTION_PROXY_BYPASS_REQUESTED event to the event store
  // when the bypass reason is not initiated by the data reduction proxy, such
  // as HTTP errors.
  void AddBypassTypeEvent(const net::BoundNetLog& net_log,
                          DataReductionProxyBypassType bypass_type,
                          const std::string& request_method,
                          const GURL& gurl,
                          bool should_retry,
                          const base::TimeDelta& bypass_duration);

  // Adds a DATA_REDUCTION_PROXY_FALLBACK event to the event store when there
  // is a network error in attempting to use a Data Reduction Proxy server.
  void AddProxyFallbackEvent(net::NetLog* net_log,
                             const std::string& proxy_url,
                             int net_error);

  // Adds a DATA_REDUCTION_PROXY_CANARY_REQUEST event to the event store
  // when the secure proxy request has started.
  void BeginSecureProxyCheck(const net::BoundNetLog& net_log, const GURL& gurl);

  // Adds a DATA_REDUCTION_PROXY_CANARY_REQUEST event to the event store
  // when the secure proxy request has ended.
  void EndSecureProxyCheck(const net::BoundNetLog& net_log,
                           int net_error,
                           int http_response_code,
                           bool succeeded);

  // Adds a DATA_REDUCTION_PROXY_CONFIG_REQUEST event to the event store
  // when the config request has started.
  void BeginConfigRequest(const net::BoundNetLog& net_log, const GURL& url);

  // Adds a DATA_REDUCTION_PROXY_CONFIG_REQUEST event to the event store
  // when the config request has ended.
  void EndConfigRequest(const net::BoundNetLog& net_log,
                        int net_error,
                        int http_response_code,
                        int failure_count,
                        const std::vector<net::ProxyServer>& proxies_for_http,
                        const base::TimeDelta& refresh_duration,
                        const base::TimeDelta& retry_delay);

 private:
  // Prepare and post a generic Data Reduction Proxy event with no additional
  // parameters.
  void PostEvent(net::NetLog* net_log,
                 net::NetLog::EventType type,
                 const net::NetLog::ParametersCallback& callback);

  // Prepare and post enabling/disabling proxy events for the event store on the
  // a net::NetLog.
  void PostEnabledEvent(net::NetLog* net_log,
                        net::NetLog::EventType type,
                        bool enable,
                        const net::NetLog::ParametersCallback& callback);

  // Prepare and post a Data Reduction Proxy bypass event for the event store
  // on a BoundNetLog.
  void PostBoundNetLogBypassEvent(
      const net::BoundNetLog& net_log,
      net::NetLog::EventType type,
      net::NetLog::EventPhase phase,
      int64_t expiration_ticks,
      const net::NetLog::ParametersCallback& callback);

  // Prepare and post a secure proxy check event for the event store on a
  // BoundNetLog.
  void PostBoundNetLogSecureProxyCheckEvent(
      const net::BoundNetLog& net_log,
      net::NetLog::EventType type,
      net::NetLog::EventPhase phase,
      DataReductionProxyEventStorageDelegate::SecureProxyCheckState state,
      const net::NetLog::ParametersCallback& callback);

  // Prepare and post a config request event for the event store on a
  // BoundNetLog.
  void PostBoundNetLogConfigRequestEvent(
      const net::BoundNetLog& net_log,
      net::NetLog::EventType type,
      net::NetLog::EventPhase phase,
      const net::NetLog::ParametersCallback& callback);

  // Must outlive |this|. Used for posting calls to the UI thread.
  DataReductionProxyEventStorageDelegate* storage_delegate_;

  // Enforce usage on the IO thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyEventCreator);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_EVENT_CREATOR_H_

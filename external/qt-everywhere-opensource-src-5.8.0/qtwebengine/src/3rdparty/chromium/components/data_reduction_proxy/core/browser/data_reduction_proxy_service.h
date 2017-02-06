// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SERVICE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/non_thread_safe.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/db_data_owner.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_storage_delegate.h"

class GURL;
class PrefService;

namespace base {
class SequencedTaskRunner;
class TimeDelta;
class Value;
}

namespace net {
class URLRequestContextGetter;
}

namespace data_reduction_proxy {

class DataReductionProxyCompressionStats;
class DataReductionProxyEventStore;
class DataReductionProxyIOData;
class DataReductionProxyPingbackClient;
class DataReductionProxyServiceObserver;
class DataReductionProxySettings;
class DataUseGroup;

// Contains and initializes all Data Reduction Proxy objects that have a
// lifetime based on the UI thread.
class DataReductionProxyService
    : public base::NonThreadSafe,
      public DataReductionProxyEventStorageDelegate {
 public:
  // The caller must ensure that |settings|, |prefs|, |request_context|, and
  // |io_task_runner| remain alive for the lifetime of the
  // |DataReductionProxyService| instance. |prefs| may be null. This instance
  // will take ownership of |compression_stats|.
  // TODO(jeremyim): DataReductionProxyService should own
  // DataReductionProxySettings and not vice versa.
  DataReductionProxyService(
      DataReductionProxySettings* settings,
      PrefService* prefs,
      net::URLRequestContextGetter* request_context_getter,
      std::unique_ptr<DataStore> store,
      const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::TimeDelta& commit_delay);

  virtual ~DataReductionProxyService();

  // Sets the DataReductionProxyIOData weak pointer.
  void SetIOData(base::WeakPtr<DataReductionProxyIOData> io_data);

  void Shutdown();

  // Indicates whether |this| has been fully initialized. |SetIOData| is the
  // final step in initialization.
  bool Initialized() const;

  // Constructs compression stats with a noop |DataReductionProxyStore|; load
  // and store calls do nothing. This should not be called
  // if a valid compression stats is passed into the constructor.
  void EnableCompressionStatisticsLogging(
      PrefService* prefs,
      const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
      const base::TimeDelta& commit_delay);

  // Records daily data savings statistics in |compression_stats_|.
  void UpdateContentLengths(int64_t data_used,
                            int64_t original_size,
                            bool data_reduction_proxy_enabled,
                            DataReductionProxyRequestType request_type,
                            scoped_refptr<DataUseGroup> data_use_group,
                            const std::string& mime_type);

  // Overrides of DataReductionProxyEventStorageDelegate.
  void AddEvent(std::unique_ptr<base::Value> event) override;
  void AddEnabledEvent(std::unique_ptr<base::Value> event,
                       bool enabled) override;
  void AddEventAndSecureProxyCheckState(std::unique_ptr<base::Value> event,
                                        SecureProxyCheckState state) override;
  void AddAndSetLastBypassEvent(std::unique_ptr<base::Value> event,
                                int64_t expiration_ticks) override;

  // Records whether the Data Reduction Proxy is unreachable or not.
  void SetUnreachable(bool unreachable);

  // Sets if Lo-Fi was active on the last main frame load in
  // DataReductionProxySettings.
  void SetLoFiModeActiveOnMainFrame(bool lo_fi_mode_active);

  // Sets Lo-Fi mode off on the IO thread.
  void SetLoFiModeOff();

  // Initializes the Lo-Fi implicit opt out prefs.
  void InitializeLoFiPrefs();

  // Stores an int64_t value in |prefs_|.
  void SetInt64Pref(const std::string& pref_path, int64_t value);

  // Stores a string value in |prefs_|.
  void SetStringPref(const std::string& pref_path, const std::string& value);

  // Bridge methods to safely call to the UI thread objects.
  // Virtual for testing.
  virtual void SetProxyPrefs(bool enabled, bool at_startup);

  void LoadHistoricalDataUsage(
      const HistoricalDataUsageCallback& load_data_usage_callback);
  void LoadCurrentDataUsageBucket(
      const LoadCurrentDataUsageCallback& load_current_data_usage_callback);
  void StoreCurrentDataUsageBucket(std::unique_ptr<DataUsageBucket> current);
  void DeleteHistoricalDataUsage();
  void DeleteBrowsingHistory(const base::Time& start, const base::Time& end);

  // Methods for adding/removing observers on |this|.
  void AddObserver(DataReductionProxyServiceObserver* observer);
  void RemoveObserver(DataReductionProxyServiceObserver* observer);

  // Sets the reporting fraction in the pingback client.
  void SetPingbackReportingFraction(float pingback_reporting_fraction);

  // Accessor methods.
  DataReductionProxyCompressionStats* compression_stats() const {
    return compression_stats_.get();
  }

  DataReductionProxyEventStore* event_store() const {
    return event_store_.get();
  }

  net::URLRequestContextGetter* url_request_context_getter() const {
    return url_request_context_getter_;
  }

  DataReductionProxyPingbackClient* pingback_client() const {
    return pingback_client_.get();
  }

  base::WeakPtr<DataReductionProxyService> GetWeakPtr();

 private:
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestLoFiSessionStateHistograms);

  // Values of the UMA DataReductionProxy.LoFi.SessionState histogram.
  // This enum must remain synchronized with DataReductionProxyLoFiSessionState
  // in metrics/histograms/histograms.xml.
  enum LoFiSessionState {
    LO_FI_SESSION_STATE_USED = 0,
    LO_FI_SESSION_STATE_NOT_USED,
    LO_FI_SESSION_STATE_OPTED_OUT,  // Permanent opt out
    LO_FI_SESSION_STATE_TEMPORARILY_OPTED_OUT,
    LO_FI_SESSION_STATE_INDEX_BOUNDARY,
  };

  // Records UMA for Lo-Fi session state.
  void RecordLoFiSessionState(LoFiSessionState state);

  net::URLRequestContextGetter* url_request_context_getter_;

  // Tracks compression statistics to be displayed to the user.
  std::unique_ptr<DataReductionProxyCompressionStats> compression_stats_;

  std::unique_ptr<DataReductionProxyEventStore> event_store_;

  std::unique_ptr<DataReductionProxyPingbackClient> pingback_client_;

  DataReductionProxySettings* settings_;

  // A prefs service for storing data.
  PrefService* prefs_;

  std::unique_ptr<DBDataOwner> db_data_owner_;

  // Used to post tasks to |io_data_|.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Used to post tasks to |db_data_owner_|.
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  // A weak pointer to DataReductionProxyIOData so that UI based objects can
  // make calls to IO based objects.
  base::WeakPtr<DataReductionProxyIOData> io_data_;

  base::ObserverList<DataReductionProxyServiceObserver> observer_list_;

  bool initialized_;

  base::WeakPtrFactory<DataReductionProxyService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyService);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SERVICE_H_

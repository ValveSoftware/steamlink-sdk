// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service_observer.h"
#include "components/prefs/pref_member.h"
#include "url/gurl.h"

class PrefService;

namespace data_reduction_proxy {

class DataReductionProxyConfig;
class DataReductionProxyEventStore;
class DataReductionProxyIOData;
class DataReductionProxyService;
class DataReductionProxyCompressionStats;

// The header used to request a data reduction proxy pass through. When a
// request is sent to the data reduction proxy with this header, it will respond
// with the original uncompressed response.
extern const char kDataReductionPassThroughHeader[];

// Values of the UMA DataReductionProxy.StartupState histogram.
// This enum must remain synchronized with DataReductionProxyStartupState
// in metrics/histograms/histograms.xml.
enum ProxyStartupState {
  PROXY_NOT_AVAILABLE = 0,
  PROXY_DISABLED,
  PROXY_ENABLED,
  PROXY_STARTUP_STATE_COUNT,
};

// Values of the UMA DataReductionProxy.LoFi.ImplicitOptOutAction histogram.
// This enum must remain synchronized with
// DataReductionProxyLoFiImplicitOptOutAction in
// metrics/histograms/histograms.xml.
enum LoFiImplicitOptOutAction {
  LO_FI_OPT_OUT_ACTION_DISABLED_FOR_SESSION = 0,
  LO_FI_OPT_OUT_ACTION_DISABLED_UNTIL_NEXT_EPOCH,
  LO_FI_OPT_OUT_ACTION_NEXT_EPOCH,
  LO_FI_OPT_OUT_ACTION_INDEX_BOUNDARY,
};

// Values of the UMA DataReductionProxy.EnabledState histogram.
// This enum must remain synchronized with DataReductionProxyEnabledState
// in metrics/histograms/histograms.xml.
enum DataReductionSettingsEnabledAction {
  DATA_REDUCTION_SETTINGS_ACTION_OFF_TO_ON = 0,
  DATA_REDUCTION_SETTINGS_ACTION_ON_TO_OFF,
  DATA_REDUCTION_SETTINGS_ACTION_BOUNDARY,
};

// Central point for configuring the data reduction proxy.
// This object lives on the UI thread and all of its methods are expected to
// be called from there.
class DataReductionProxySettings : public DataReductionProxyServiceObserver {
 public:
  typedef base::Callback<bool(const std::string&, const std::string&)>
      SyntheticFieldTrialRegistrationCallback;

  DataReductionProxySettings();
  virtual ~DataReductionProxySettings();

  // Initializes the Data Reduction Proxy with the name of the preference that
  // controls enabling it, profile prefs and a |DataReductionProxyIOData|. The
  // caller must ensure that all parameters remain alive for the lifetime of
  // the |DataReductionProxySettings| instance.
  void InitDataReductionProxySettings(
      const std::string& data_reduction_proxy_enabled_pref_name,
      PrefService* prefs,
      DataReductionProxyIOData* io_data,
      std::unique_ptr<DataReductionProxyService> data_reduction_proxy_service);

  base::WeakPtr<DataReductionProxyCompressionStats> compression_stats();

  // Sets the |register_synthetic_field_trial_| callback and runs to register
  // the DataReductionProxyEnabled and the DataReductionProxyLoFiEnabled
  // synthetic field trial.
  void SetCallbackToRegisterSyntheticFieldTrial(
      const SyntheticFieldTrialRegistrationCallback&
          on_data_reduction_proxy_enabled);

  // Returns true if the proxy is enabled.
  bool IsDataReductionProxyEnabled() const;

  // Returns true if the proxy can be used for the given url. This method does
  // not take into account the proxy config or proxy retry list, so it can
  // return true even when the proxy will not be used. Specifically, if
  // another proxy configuration overrides use of data reduction proxy, or
  // if data reduction proxy is in proxy retry list, then data reduction proxy
  // will not be used, but this method will still return true. If this method
  // returns false, then we are guaranteed that data reduction proxy will not be
  // used.
  bool CanUseDataReductionProxy(const GURL& url) const;

  // Returns true if the proxy is managed by an adminstrator's policy.
  bool IsDataReductionProxyManaged();

  // Enables or disables the data reduction proxy.
  void SetDataReductionProxyEnabled(bool enabled);

  // Sets |lo_fi_mode_active_| to true if Lo-Fi is currently active, meaning
  // requests are being sent with "q=low" headers. Set from the IO thread only
  // on main frame requests.
  void SetLoFiModeActiveOnMainFrame(bool lo_fi_mode_active);

  // Returns true if Lo-Fi was active on the main frame request.
  bool WasLoFiModeActiveOnMainFrame() const;

  // Returns true if a "Load image" context menu request has not been made since
  // the last main frame request.
  bool WasLoFiLoadImageRequestedBefore();

  // Increments the number of times the Lo-Fi snackbar has been shown.
  void IncrementLoFiSnackbarShown();

  // Sets |lo_fi_load_image_requested_| to true, which means a "Load image"
  // context menu request has been made since the last main frame request.
  void SetLoFiLoadImageRequested();

  // Counts the number of requests to reload the page with images from the Lo-Fi
  // snackbar. If the user requests the page with images a certain number of
  // times, then Lo-Fi is disabled for the remainder of the session.
  void IncrementLoFiUserRequestsForImages();

  // Records UMA for Lo-Fi implicit opt out actions.
  void RecordLoFiImplicitOptOutAction(
      data_reduction_proxy::LoFiImplicitOptOutAction action) const;

  // Returns the time in microseconds that the last update was made to the
  // daily original and received content lengths.
  int64_t GetDataReductionLastUpdateTime();

  // Returns aggregate received and original content lengths over the specified
  // number of days, as well as the time these stats were last updated.
  void GetContentLengths(unsigned int days,
                         int64_t* original_content_length,
                         int64_t* received_content_length,
                         int64_t* last_update_time);

  // Records that the data reduction proxy is unreachable or not.
  void SetUnreachable(bool unreachable);

  // Returns whether the data reduction proxy is unreachable. Returns true
  // if no request has successfully completed through proxy, even though atleast
  // some of them should have.
  bool IsDataReductionProxyUnreachable();

  ContentLengthList GetDailyContentLengths(const char* pref_name);

  // Configures data reduction proxy. |at_startup| is true when this method is
  // called in response to creating or loading a new profile.
  void MaybeActivateDataReductionProxy(bool at_startup);

  // Returns the event store being used. May be null if
  // InitDataReductionProxySettings has not been called.
  DataReductionProxyEventStore* GetEventStore() const;

  // Returns true if the data reduction proxy configuration may be used.
  bool Allowed() const {
    return allowed_;
  }

  // Returns true if the data reduction proxy promo may be shown.
  // This is independent of whether the data reduction proxy is allowed.
  bool PromoAllowed() const {
    return promo_allowed_;
  }

  DataReductionProxyService* data_reduction_proxy_service() {
    return data_reduction_proxy_service_.get();
  }

  // Returns the |DataReductionProxyConfig| being used. May be null if
  // InitDataReductionProxySettings has not been called.
  DataReductionProxyConfig* Config() const {
    return config_;
  }

  // Permits changing the underlying |DataReductionProxyConfig| without running
  // the initialization loop.
  void ResetConfigForTest(DataReductionProxyConfig* config) {
    config_ = config;
  }

 protected:
  void InitPrefMembers();

  void UpdateConfigValues();

  // Virtualized for unit test support.
  virtual PrefService* GetOriginalProfilePrefs();

  // Metrics method. Subclasses should override if they wish to provide
  // alternatives.
  virtual void RecordDataReductionInit();

  // Virtualized for mocking. Records UMA specifying whether the proxy was
  // enabled or disabled at startup.
  virtual void RecordStartupState(
      data_reduction_proxy::ProxyStartupState state);

 private:
  friend class DataReductionProxySettingsTestBase;
  friend class DataReductionProxySettingsTest;
  friend class DataReductionProxyTestContext;
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestResetDataReductionStatistics);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestIsProxyEnabledOrManaged);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestCanUseDataReductionProxy);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest, TestContentLengths);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestGetDailyContentLengths);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestMaybeActivateDataReductionProxy);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestOnProxyEnabledPrefChange);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestInitDataReductionProxyOn);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestInitDataReductionProxyOff);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           CheckInitMetricsWhenNotAllowed);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestLoFiImplicitOptOutClicksPerSession);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestLoFiImplicitOptOutConsecutiveSessions);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestLoFiImplicitOptOutHistograms);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestLoFiSessionStateHistograms);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestSettingsEnabledStateHistograms);

  // Override of DataReductionProxyService::Observer.
  void OnServiceInitialized() override;

  // Registers the trial "SyntheticDataReductionProxySetting" with the group
  // "Enabled" or "Disabled". Indicates whether the proxy is turned on or not.
  void RegisterDataReductionProxyFieldTrial();

  void OnProxyEnabledPrefChange();

  void ResetDataReductionStatistics();

  // Update IO thread objects in response to UI thread changes.
  void UpdateIOData(bool at_startup);

  // For tests.
  void set_data_reduction_proxy_enabled_pref_name_for_test(
      const std::string& data_reduction_proxy_enabled_pref_name) {
    data_reduction_proxy_enabled_pref_name_ =
        data_reduction_proxy_enabled_pref_name;
  }

  bool unreachable_;

  // A call to MaybeActivateDataReductionProxy may take place before the
  // |data_reduction_proxy_service_| has received a DataReductionProxyIOData
  // pointer. In that case, the operation against the IO objects will not
  // succeed and |deferred_initialization_| will be set to true. When
  // OnServiceInitialized is called, if |deferred_initialization_| is true,
  // IO object calls will be performed at that time.
  bool deferred_initialization_;

  // The following values are cached in order to access the values on the
  // correct thread.
  bool allowed_;
  bool promo_allowed_;

  // True if Lo-Fi is active.
  bool lo_fi_mode_active_;

  // True if a "Load image" context menu request has not been made since the
  // last main frame request.
  bool lo_fi_load_image_requested_;

  // The number of requests to reload the page with images from the Lo-Fi
  // snackbar until Lo-Fi is disabled for the remainder of the
  // session.
  int lo_fi_user_requests_for_images_per_session_;

  // The number of consecutive sessions where Lo-Fi was disabled for
  // Lo-Fi to be disabled until the next implicit opt out epoch, which may be in
  // a later session, or never.
  int lo_fi_consecutive_session_disables_;

  BooleanPrefMember spdy_proxy_auth_enabled_;

  std::unique_ptr<DataReductionProxyService> data_reduction_proxy_service_;

  // The name of the preference that controls enabling and disabling the Data
  // Reduction Proxy.
  std::string data_reduction_proxy_enabled_pref_name_;

  PrefService* prefs_;

  // The caller must ensure that the |config_| outlives this instance.
  DataReductionProxyConfig* config_;

  SyntheticFieldTrialRegistrationCallback register_synthetic_field_trial_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxySettings);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_

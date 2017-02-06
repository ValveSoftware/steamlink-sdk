// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_METRICS_HELPER_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_METRICS_HELPER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/rappor/rappor_service.h"
#include "url/gurl.h"

namespace history {
class HistoryService;
}

namespace security_interstitials {

// MetricsHelper records user warning interactions in a common way via METRICS
// histograms and, optionally, RAPPOR metrics. The class will generate the
// following histograms:
//   METRICS: interstitial.<metric_prefix>.decision[.repeat_visit]
//   METRICS: interstitial.<metric_prefix>.interaction[.repeat_visi]
//   RAPPOR:  interstitial.<rappor_prefix> (SafeBrowsing parameters)
//   RAPPOR:  interstitial.<rappor_prefix>2 (Low frequency parameters)
// wherein |metric_prefix| and |rappor_prefix| are specified via ReportDetails.
// repeat_visit is also generated if the user has seen the page before.
//
// If |extra_suffix| is not empty, MetricsHelper will append ".<extra_suffix>"
// to generate an additional 2 or 4 more metrics.
class MetricsHelper {
 public:
  // These enums are used for histograms.  Don't reorder, delete, or insert
  // elements. New elements should be added at the end (right before the max).
  enum Decision {
    SHOW,
    PROCEED,
    DONT_PROCEED,
    PROCEEDING_DISABLED,
    MAX_DECISION
  };
  enum Interaction {
    TOTAL_VISITS,
    SHOW_ADVANCED,
    SHOW_PRIVACY_POLICY,
    SHOW_DIAGNOSTIC,
    SHOW_LEARN_MORE,
    RELOAD,
    OPEN_TIME_SETTINGS,
    SET_EXTENDED_REPORTING_ENABLED,
    SET_EXTENDED_REPORTING_DISABLED,
    EXTENDED_REPORTING_IS_ENABLED,
    REPORT_PHISHING_ERROR,
    MAX_INTERACTION
  };

  // metric_prefix: Histogram prefix for UMA.
  //                examples: "phishing", "ssl_overridable"
  // extra_suffix: If not-empty, will generate second set of metrics by
  //               placing at the end of the metric name.  Examples:
  //               "from_datasaver", "from_device"
  // rappor_prefix: Metric prefix for Rappor.
  //                examples: "phishing2", "ssl3"
  // rappor_report_type: Specifies the low-frequency RAPPOR configuration to use
  //                     (i.e. UMA or Safe Browsing).
  // deprecated_rappor_report_type: Specifies the deprecated RAPPOR
  //                                configuration to use for comparison with the
  //                                low-frequency metric.
  // The rappor preferences can be left blank if rappor_service is not set.
  // TODO(dominickn): remove deprecated_rappor_report_type once sufficient
  // comparison data has been collected and analysed - crbug.com/605836.
  struct ReportDetails {
    ReportDetails();
    ReportDetails(const ReportDetails& other);
    ~ReportDetails();
    std::string metric_prefix;
    std::string extra_suffix;
    std::string rappor_prefix;
    std::string deprecated_rappor_prefix;
    rappor::RapporType rappor_report_type;
    rappor::RapporType deprecated_rappor_report_type;
  };

  // Args:
  //   url: URL of page that triggered the interstitial. Only origin is used.
  //   history_service: Set this to record metrics based on whether the user
  //                    has visited this hostname before.
  //   rappor_service: If you want RAPPOR statistics, provide a service,
  //                   settings.rappor_prefix, and settings.rappor_report_type.
  //   settings: Specify reporting details (prefixes and report types).
  //   sampling_event_name: Event name for Experience Sampling.
  //                        e.g. "phishing_interstitial_"
  MetricsHelper(const GURL& url,
                const ReportDetails settings,
                history::HistoryService* history_service,
                const base::WeakPtr<rappor::RapporService>& rappor_service);
  virtual ~MetricsHelper();

  // Records a user decision or interaction to the appropriate UMA metrics
  // histogram and potentially in a RAPPOR metric.
  void RecordUserDecision(Decision decision);
  void RecordUserInteraction(Interaction interaction);
  void RecordShutdownMetrics();

  // Number of times user visited this origin before. -1 means not-yet-set.
  int NumVisits();

 protected:
  // Subclasses should implement any embedder-specific recording logic in these
  // methods. They'll be invoked from the matching Record methods.
  virtual void RecordExtraUserDecisionMetrics(Decision decision) = 0;
  virtual void RecordExtraUserInteractionMetrics(Interaction interaction) = 0;
  virtual void RecordExtraShutdownMetrics() = 0;

 private:
  // Used to query the HistoryService to see if the URL is in history.  It will
  // only be invoked if the constructor received |history_service|.
  void OnGotHistoryCount(bool success, int num_visits, base::Time first_visit);

  void RecordUserDecisionToMetrics(Decision decision,
                                   const std::string& histogram_name);
  void RecordUserDecisionToRappor(Decision decision,
                                  const rappor::RapporType rappor_report_type,
                                  const std::string& rappor_prefix);
  const GURL request_url_;
  const ReportDetails settings_;
  base::WeakPtr<rappor::RapporService> rappor_service_;
  int num_visits_;
  base::CancelableTaskTracker request_tracker_;

  DISALLOW_COPY_AND_ASSIGN(MetricsHelper);
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_METRICS_HELPER_H_

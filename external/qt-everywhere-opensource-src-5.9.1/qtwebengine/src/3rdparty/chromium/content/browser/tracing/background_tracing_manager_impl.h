// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_MANAGER_IMPL_H_
#define CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_MANAGER_IMPL_H_

#include <memory>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "content/browser/tracing/background_tracing_config_impl.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/background_tracing_manager.h"

namespace content {

class BackgroundTracingRule;
class TracingDelegate;

class BackgroundTracingManagerImpl : public BackgroundTracingManager {
 public:
  static CONTENT_EXPORT BackgroundTracingManagerImpl* GetInstance();

  bool SetActiveScenario(std::unique_ptr<BackgroundTracingConfig>,
                         const ReceiveCallback&,
                         DataFiltering data_filtering) override;
  void WhenIdle(IdleCallback idle_callback) override;

  void TriggerNamedEvent(TriggerHandle, StartedFinalizingCallback) override;
  TriggerHandle RegisterTriggerType(const char* trigger_name) override;

  void OnHistogramTrigger(const std::string& histogram_name);

  void OnRuleTriggered(const BackgroundTracingRule* triggered_rule,
                       StartedFinalizingCallback callback);
  void AbortScenario();
  bool HasActiveScenario() override;

  // For tests
  void InvalidateTriggerHandlesForTesting() override;
  void SetTracingEnabledCallbackForTesting(
      const base::Closure& callback) override;
  CONTENT_EXPORT void SetRuleTriggeredCallbackForTesting(
      const base::Closure& callback);
  void FireTimerForTesting() override;
  CONTENT_EXPORT bool IsTracingForTesting();

 private:
  BackgroundTracingManagerImpl();
  ~BackgroundTracingManagerImpl() override;

  void StartTracing(BackgroundTracingConfigImpl::CategoryPreset,
                    base::trace_event::TraceRecordMode);
  void StartTracingIfConfigNeedsIt();
  void OnFinalizeStarted(std::unique_ptr<const base::DictionaryValue> metadata,
                         base::RefCountedString*);
  void OnFinalizeComplete();
  void BeginFinalizing(StartedFinalizingCallback);
  void ValidateStartupScenario();

  void AddCustomMetadata();

  std::string GetTriggerNameFromHandle(TriggerHandle handle) const;
  bool IsTriggerHandleValid(TriggerHandle handle) const;

  BackgroundTracingRule* GetRuleAbleToTriggerTracing(
      TriggerHandle handle) const;
  bool IsSupportedConfig(BackgroundTracingConfigImpl* config);

  std::string GetCategoryFilterStringForCategoryPreset(
      BackgroundTracingConfigImpl::CategoryPreset) const;

  class TracingTimer {
   public:
    explicit TracingTimer(StartedFinalizingCallback);
    ~TracingTimer();

    void StartTimer(int seconds);
    void CancelTimer();
    void FireTimerForTesting();

   private:
    void TracingTimerFired();

    base::OneShotTimer tracing_timer_;
    StartedFinalizingCallback callback_;
  };

  std::unique_ptr<TracingDelegate> delegate_;
  std::unique_ptr<const content::BackgroundTracingConfigImpl> config_;
  std::map<TriggerHandle, std::string> trigger_handles_;
  std::unique_ptr<TracingTimer> tracing_timer_;
  ReceiveCallback receive_callback_;
  std::unique_ptr<base::DictionaryValue> last_triggered_rule_;
  bool is_gathering_;
  bool is_tracing_;
  bool requires_anonymized_data_;
  int trigger_handle_ids_;

  TriggerHandle triggered_named_event_handle_;

  IdleCallback idle_callback_;
  base::Closure tracing_enabled_callback_for_testing_;
  base::Closure rule_triggered_callback_for_testing_;

  friend struct base::DefaultLazyInstanceTraits<BackgroundTracingManagerImpl>;

  DISALLOW_COPY_AND_ASSIGN(BackgroundTracingManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_MANAGER_IMPL_H_

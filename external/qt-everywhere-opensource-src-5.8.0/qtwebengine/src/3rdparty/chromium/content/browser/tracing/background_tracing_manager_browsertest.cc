// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/pattern.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "third_party/zlib/zlib.h"

namespace content {

class BackgroundTracingManagerBrowserTest : public ContentBrowserTest {
 public:
  BackgroundTracingManagerBrowserTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundTracingManagerBrowserTest);
};

class BackgroundTracingManagerUploadConfigWrapper {
 public:
  BackgroundTracingManagerUploadConfigWrapper(const base::Closure& callback)
      : callback_(callback), receive_count_(0) {
    receive_callback_ =
        base::Bind(&BackgroundTracingManagerUploadConfigWrapper::Upload,
                   base::Unretained(this));
  }

  void Upload(const scoped_refptr<base::RefCountedString>& file_contents,
              std::unique_ptr<const base::DictionaryValue> metadata,
              base::Callback<void()> done_callback) {
    receive_count_ += 1;
    EXPECT_TRUE(file_contents);

    size_t compressed_length = file_contents->data().length();
    const size_t kOutputBufferLength = 10 * 1024 * 1024;
    std::vector<char> output_str(kOutputBufferLength);

    z_stream stream = {0};
    stream.avail_in = compressed_length;
    stream.avail_out = kOutputBufferLength;
    stream.next_in = (Bytef*)&file_contents->data()[0];
    stream.next_out = (Bytef*)output_str.data();

    // 16 + MAX_WBITS means only decoding gzip encoded streams, and using
    // the biggest window size, according to zlib.h
    int result = inflateInit2(&stream, 16 + MAX_WBITS);
    EXPECT_EQ(Z_OK, result);
    result = inflate(&stream, Z_FINISH);
    int bytes_written = kOutputBufferLength - stream.avail_out;

    inflateEnd(&stream);
    EXPECT_EQ(Z_STREAM_END, result);

    last_file_contents_.assign(output_str.data(), bytes_written);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(done_callback));
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(callback_));
  }

  bool TraceHasMatchingString(const char* str) {
    return last_file_contents_.find(str) != std::string::npos;
  }

  int get_receive_count() const { return receive_count_; }

  const BackgroundTracingManager::ReceiveCallback& get_receive_callback()
      const {
    return receive_callback_;
  }

 private:
  BackgroundTracingManager::ReceiveCallback receive_callback_;
  base::Closure callback_;
  int receive_count_;
  std::string last_file_contents_;
};

void StartedFinalizingCallback(base::Closure callback,
                               bool expected,
                               bool value) {
  EXPECT_EQ(expected, value);
  if (!callback.is_null())
    callback.Run();
}

std::unique_ptr<BackgroundTracingConfig> CreatePreemptiveConfig() {
  base::DictionaryValue dict;

  dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
  dict.SetString("category", "BENCHMARK");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict->SetString("trigger_name", "preemptive_test");
    rules_list->Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));

  EXPECT_TRUE(config);
  return config;
}

std::unique_ptr<BackgroundTracingConfig> CreateReactiveConfig() {
  base::DictionaryValue dict;

  dict.SetString("mode", "REACTIVE_TRACING_MODE");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
    rules_dict->SetString("trigger_name", "reactive_test");
    rules_dict->SetString("category", "BENCHMARK");
    rules_list->Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));

  EXPECT_TRUE(config);
  return config;
}

void SetupBackgroundTracingManager() {
  content::BackgroundTracingManager::GetInstance()
      ->InvalidateTriggerHandlesForTesting();
}

void DisableScenarioWhenIdle() {
  BackgroundTracingManager::GetInstance()->SetActiveScenario(
      NULL, BackgroundTracingManager::ReceiveCallback(),
      BackgroundTracingManager::NO_DATA_FILTERING);
}

// This tests that the endpoint receives the final trace data.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveTraceFinalContentsOnTrigger) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

    BackgroundTracingManager::TriggerHandle handle =
        BackgroundTracingManager::
            GetInstance()->RegisterTriggerType("preemptive_test");

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

// This tests triggering more than once still only gathers once.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       CallTriggersMoreThanOnceOnlyGatherOnce) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

    content::BackgroundTracingManager::TriggerHandle handle =
        content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "preemptive_test");

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

namespace {

bool IsTraceEventArgsWhitelisted(
    const char* category_group_name,
    const char* event_name,
    base::trace_event::ArgumentNameFilterPredicate* arg_filter) {
  if (base::MatchPattern(category_group_name, "benchmark") &&
      base::MatchPattern(event_name, "whitelisted")) {
    return true;
  }

  return false;
}

}  // namespace

// This tests that non-whitelisted args get stripped if required.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       NoWhitelistedArgsStripped) {
  SetupBackgroundTracingManager();

  base::trace_event::TraceLog::GetInstance()->SetArgumentFilterPredicate(
      base::Bind(&IsTraceEventArgsWhitelisted));

  base::RunLoop wait_for_upload;
  BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
      wait_for_upload.QuitClosure());

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  content::BackgroundTracingManager::TriggerHandle handle =
      content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  base::RunLoop wait_for_activated;
  BackgroundTracingManager::GetInstance()->SetTracingEnabledCallbackForTesting(
      wait_for_activated.QuitClosure());
  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), upload_config_wrapper.get_receive_callback(),
      BackgroundTracingManager::ANONYMIZE_DATA));

  wait_for_activated.Run();

  TRACE_EVENT1("benchmark", "whitelisted", "find_this", 1);
  TRACE_EVENT1("benchmark", "not_whitelisted", "this_not_found", 1);

  BackgroundTracingManager::GetInstance()->WhenIdle(
      base::Bind(&DisableScenarioWhenIdle));

  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

  wait_for_upload.Run();

  EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  EXPECT_TRUE(upload_config_wrapper.TraceHasMatchingString("{"));
  EXPECT_TRUE(upload_config_wrapper.TraceHasMatchingString("find_this"));
  EXPECT_FALSE(upload_config_wrapper.TraceHasMatchingString("this_not_found"));
}

// This tests that browser metadata gets included in the trace.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       TraceMetadataInTrace) {
  SetupBackgroundTracingManager();

  base::trace_event::TraceLog::GetInstance()->SetArgumentFilterPredicate(
      base::Bind(&IsTraceEventArgsWhitelisted));

  base::RunLoop wait_for_upload;
  BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
      wait_for_upload.QuitClosure());

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  content::BackgroundTracingManager::TriggerHandle handle =
      content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  base::RunLoop wait_for_activated;
  BackgroundTracingManager::GetInstance()->SetTracingEnabledCallbackForTesting(
      wait_for_activated.QuitClosure());
  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), upload_config_wrapper.get_receive_callback(),
      BackgroundTracingManager::ANONYMIZE_DATA));

  wait_for_activated.Run();

  BackgroundTracingManager::GetInstance()->WhenIdle(
      base::Bind(&DisableScenarioWhenIdle));

  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

  wait_for_upload.Run();

  EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  EXPECT_TRUE(upload_config_wrapper.TraceHasMatchingString("cpu-brand"));
  EXPECT_TRUE(upload_config_wrapper.TraceHasMatchingString("network-type"));
  EXPECT_TRUE(upload_config_wrapper.TraceHasMatchingString("user-agent"));
}

// This tests subprocesses (like a navigating renderer) which gets told to
// provide a argument-filtered trace and has no predicate in place to do the
// filtering (in this case, only the browser process gets it set), will crash
// rather than return potential PII.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       CrashWhenSubprocessWithoutArgumentFilter) {
  SetupBackgroundTracingManager();

  base::trace_event::TraceLog::GetInstance()->SetArgumentFilterPredicate(
      base::Bind(&IsTraceEventArgsWhitelisted));

  base::RunLoop wait_for_upload;
  BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
      wait_for_upload.QuitClosure());

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  content::BackgroundTracingManager::TriggerHandle handle =
      content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  base::RunLoop wait_for_activated;
  BackgroundTracingManager::GetInstance()->SetTracingEnabledCallbackForTesting(
      wait_for_activated.QuitClosure());
  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), upload_config_wrapper.get_receive_callback(),
      BackgroundTracingManager::ANONYMIZE_DATA));

  wait_for_activated.Run();

  NavigateToURL(shell(), GetTestUrl("", "about:blank"));

  BackgroundTracingManager::GetInstance()->WhenIdle(
      base::Bind(&DisableScenarioWhenIdle));

  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

  wait_for_upload.Run();

  EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  // We should *not* receive anything at all from the renderer,
  // the process should've crashed rather than letting that happen.
  EXPECT_TRUE(!upload_config_wrapper.TraceHasMatchingString("CrRendererMain"));
}

// This tests multiple triggers still only gathers once.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       CallMultipleTriggersOnlyGatherOnce) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    base::DictionaryValue dict;
    dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
    dict.SetString("category", "BENCHMARK");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
      rules_dict->SetString("trigger_name", "test1");
      rules_list->Append(std::move(rules_dict));
    }
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
      rules_dict->SetString("trigger_name", "test2");
      rules_list->Append(std::move(rules_dict));
    }

    dict.Set("configs", std::move(rules_list));

    std::unique_ptr<BackgroundTracingConfig> config(
        BackgroundTracingConfigImpl::FromDict(&dict));
    EXPECT_TRUE(config);

    BackgroundTracingManager::TriggerHandle handle1 =
        BackgroundTracingManager::GetInstance()->RegisterTriggerType("test1");
    BackgroundTracingManager::TriggerHandle handle2 =
        BackgroundTracingManager::GetInstance()->RegisterTriggerType("test2");

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle1, base::Bind(&StartedFinalizingCallback, base::Closure(), true));
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle2,
        base::Bind(&StartedFinalizingCallback, base::Closure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

// This tests that toggling Blink scenarios in the config alters the
// command-line.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ToggleBlinkScenarios) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    base::DictionaryValue dict;
    dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
    dict.SetString("category", "BENCHMARK");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
      rules_dict->SetString("trigger_name", "test2");
      rules_list->Append(std::move(rules_dict));
    }

    dict.Set("configs", std::move(rules_list));
    dict.SetString("enable_blink_features", "FasterWeb1,FasterWeb2");
    dict.SetString("disable_blink_features", "SlowerWeb1,SlowerWeb2");
    std::unique_ptr<BackgroundTracingConfig> config(
        BackgroundTracingConfigImpl::FromDict(&dict));
    EXPECT_TRUE(config);

    bool scenario_activated =
        BackgroundTracingManager::GetInstance()->SetActiveScenario(
            std::move(config), upload_config_wrapper.get_receive_callback(),
            BackgroundTracingManager::NO_DATA_FILTERING);

    EXPECT_TRUE(scenario_activated);

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    EXPECT_TRUE(command_line);

    EXPECT_EQ(command_line->GetSwitchValueASCII(switches::kEnableBlinkFeatures),
              "FasterWeb1,FasterWeb2");
    EXPECT_EQ(
        command_line->GetSwitchValueASCII(switches::kDisableBlinkFeatures),
        "SlowerWeb1,SlowerWeb2");
  }
}

// This tests that toggling Blink scenarios in a scenario won't activate
// if there's already Blink features toggled by something else (about://flags)
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ToggleBlinkScenariosNotOverridingSwitches) {
  SetupBackgroundTracingManager();

  base::RunLoop run_loop;
  BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
      run_loop.QuitClosure());

  base::DictionaryValue dict;
  dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
  dict.SetString("category", "BENCHMARK");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict->SetString("trigger_name", "test2");
    rules_list->Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));
  dict.SetString("enable_blink_features", "FasterWeb1,FasterWeb2");
  dict.SetString("disable_blink_features", "SlowerWeb1,SlowerWeb2");
  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));
  EXPECT_TRUE(config);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnableBlinkFeatures, "FooFeature");

  bool scenario_activated =
      BackgroundTracingManager::GetInstance()->SetActiveScenario(
          std::move(config), upload_config_wrapper.get_receive_callback(),
          BackgroundTracingManager::NO_DATA_FILTERING);

  EXPECT_FALSE(scenario_activated);
}

// This tests that delayed histogram triggers triggers work as expected
// with preemptive scenarios.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       CallPreemptiveTriggerWithDelay) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    base::DictionaryValue dict;
    dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
    dict.SetString("category", "BENCHMARK");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString(
          "rule", "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
      rules_dict->SetString("histogram_name", "fake");
      rules_dict->SetInteger("histogram_value", 1);
      rules_dict->SetInteger("trigger_delay", 10);
      rules_list->Append(std::move(rules_dict));
    }

    dict.Set("configs", std::move(rules_list));

    std::unique_ptr<BackgroundTracingConfig> config(
        BackgroundTracingConfigImpl::FromDict(&dict));
    EXPECT_TRUE(config);

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    base::RunLoop rule_triggered_runloop;
    BackgroundTracingManagerImpl::GetInstance()
        ->SetRuleTriggeredCallbackForTesting(
            rule_triggered_runloop.QuitClosure());

    // Our reference value is "1", so a value of "2" should trigger a trace.
    LOCAL_HISTOGRAM_COUNTS("fake", 2);

    rule_triggered_runloop.Run();

    // Since we specified a delay in the scenario, we should still be tracing
    // at this point.
    EXPECT_TRUE(
        BackgroundTracingManagerImpl::GetInstance()->IsTracingForTesting());

    // Fake the timer firing.
    BackgroundTracingManagerImpl::GetInstance()->FireTimerForTesting();
    EXPECT_FALSE(
        BackgroundTracingManagerImpl::GetInstance()->IsTracingForTesting());

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

// This tests that you can't trigger without a scenario set.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       CannotTriggerWithoutScenarioSet) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

    std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

    content::BackgroundTracingManager::TriggerHandle handle =
        content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "preemptive_test");

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle,
        base::Bind(&StartedFinalizingCallback, run_loop.QuitClosure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 0);
  }
}

// This tests that no trace is triggered with a handle that isn't specified
// in the config.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       DoesNotTriggerWithWrongHandle) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

    std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

    content::BackgroundTracingManager::TriggerHandle handle =
        content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "does_not_exist");

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle,
        base::Bind(&StartedFinalizingCallback, run_loop.QuitClosure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 0);
  }
}

// This tests that no trace is triggered with an invalid handle.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       DoesNotTriggerWithInvalidHandle) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

    std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

    content::BackgroundTracingManager::TriggerHandle handle =
        content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "preemptive_test");

    content::BackgroundTracingManager::GetInstance()
        ->InvalidateTriggerHandlesForTesting();

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle,
        base::Bind(&StartedFinalizingCallback, run_loop.QuitClosure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 0);
  }
}

// This tests that no preemptive trace is triggered with 0 chance set.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       PreemptiveNotTriggerWithZeroChance) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

    base::DictionaryValue dict;

    dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
    dict.SetString("category", "BENCHMARK");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
      rules_dict->SetString("trigger_name", "preemptive_test");
      rules_dict->SetDouble("trigger_chance", 0.0);
      rules_list->Append(std::move(rules_dict));
    }
    dict.Set("configs", std::move(rules_list));

    std::unique_ptr<BackgroundTracingConfig> config(
        BackgroundTracingConfigImpl::FromDict(&dict));

    EXPECT_TRUE(config);

    content::BackgroundTracingManager::TriggerHandle handle =
        content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "preemptive_test");

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle,
        base::Bind(&StartedFinalizingCallback, run_loop.QuitClosure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 0);
  }
}

// This tests that no reactive trace is triggered with 0 chance set.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveNotTriggerWithZeroChance) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

    base::DictionaryValue dict;

    dict.SetString("mode", "REACTIVE_TRACING_MODE");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString("rule",
                            "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
      rules_dict->SetString("trigger_name", "reactive_test1");
      rules_dict->SetString("category", "BENCHMARK");
      rules_dict->SetDouble("trigger_chance", 0.0);

      rules_list->Append(std::move(rules_dict));
    }
    dict.Set("configs", std::move(rules_list));

    std::unique_ptr<BackgroundTracingConfig> config(
        BackgroundTracingConfigImpl::FromDict(&dict));

    EXPECT_TRUE(config);

    content::BackgroundTracingManager::TriggerHandle handle =
        content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "preemptive_test");

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle,
        base::Bind(&StartedFinalizingCallback, run_loop.QuitClosure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 0);
  }
}

// This tests that histogram triggers for preemptive mode configs.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveTraceSucceedsOnHigherHistogramSample) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;

    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    base::DictionaryValue dict;
    dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
    dict.SetString("category", "BENCHMARK");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString(
          "rule", "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
      rules_dict->SetString("histogram_name", "fake");
      rules_dict->SetInteger("histogram_value", 1);
      rules_list->Append(std::move(rules_dict));
    }

    dict.Set("configs", std::move(rules_list));

    std::unique_ptr<BackgroundTracingConfig> config(
        BackgroundTracingConfigImpl::FromDict(&dict));
    EXPECT_TRUE(config);

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

    // Our reference value is "1", so a value of "2" should trigger a trace.
    LOCAL_HISTOGRAM_COUNTS("fake", 2);

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

// This tests that histogram triggers for reactive mode configs.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveReactiveTraceSucceedsOnHigherHistogramSample) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;

    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    base::DictionaryValue dict;
    dict.SetString("mode", "REACTIVE_TRACING_MODE");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString(
          "rule", "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
      rules_dict->SetString("histogram_name", "fake");
      rules_dict->SetInteger("histogram_value", 1);
      rules_dict->SetString("category", "BENCHMARK");
      rules_list->Append(std::move(rules_dict));
    }

    dict.Set("configs", std::move(rules_list));

    std::unique_ptr<BackgroundTracingConfig> config(
        BackgroundTracingConfigImpl::FromDict(&dict));
    EXPECT_TRUE(config);

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

    // Our reference value is "1", so a value of "2" should trigger a trace.
    LOCAL_HISTOGRAM_COUNTS("fake", 2);

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

// This tests that histogram values < reference value don't trigger.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveTraceFailsOnLowerHistogramSample) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;

    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    base::DictionaryValue dict;
    dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
    dict.SetString("category", "BENCHMARK");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString(
          "rule", "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
      rules_dict->SetString("histogram_name", "fake");
      rules_dict->SetInteger("histogram_value", 1);
      rules_list->Append(std::move(rules_dict));
    }

    dict.Set("configs", std::move(rules_list));

    std::unique_ptr<BackgroundTracingConfig> config(
        BackgroundTracingConfigImpl::FromDict(&dict));
    EXPECT_TRUE(config);

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

    // This should fail to trigger a trace since the sample value < the
    // the reference value above.
    LOCAL_HISTOGRAM_COUNTS("fake", 0);

    run_loop.RunUntilIdle();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 0);
  }
}

// This tests that histogram values > upper reference value don't trigger.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveTraceFailsOnHigherHistogramSample) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;

    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    base::DictionaryValue dict;
    dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
    dict.SetString("category", "BENCHMARK");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString(
          "rule", "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
      rules_dict->SetString("histogram_name", "fake");
      rules_dict->SetInteger("histogram_lower_value", 1);
      rules_dict->SetInteger("histogram_upper_value", 3);
      rules_list->Append(std::move(rules_dict));
    }

    dict.Set("configs", std::move(rules_list));

    std::unique_ptr<BackgroundTracingConfig> config(
        BackgroundTracingConfigImpl::FromDict(&dict));
    EXPECT_TRUE(config);

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

    // This should fail to trigger a trace since the sample value > the
    // the upper reference value above.
    LOCAL_HISTOGRAM_COUNTS("fake", 0);

    run_loop.RunUntilIdle();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 0);
  }
}

// This tests that invalid preemptive mode configs will fail.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       SetActiveScenarioFailsWithInvalidPreemptiveConfig) {
  {
    SetupBackgroundTracingManager();

    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

    base::DictionaryValue dict;
    dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
    dict.SetString("category", "BENCHMARK");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString("rule", "INVALID_RULE");
      rules_list->Append(std::move(rules_dict));
    }

    dict.Set("configs", std::move(rules_list));

    std::unique_ptr<BackgroundTracingConfig> config(
        BackgroundTracingConfigImpl::FromDict(&dict));
    // An invalid config should always return a nullptr here.
    EXPECT_FALSE(config);
  }
}

// This tests that reactive mode records and terminates with timeout.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveTimeoutTermination) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    std::unique_ptr<BackgroundTracingConfig> config = CreateReactiveConfig();

    BackgroundTracingManager::TriggerHandle handle =
        BackgroundTracingManager::
            GetInstance()->RegisterTriggerType("reactive_test");

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

    BackgroundTracingManager::GetInstance()->FireTimerForTesting();

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

// This tests that reactive mode records and terminates with a second trigger.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveSecondTriggerTermination) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    std::unique_ptr<BackgroundTracingConfig> config = CreateReactiveConfig();

    BackgroundTracingManager::TriggerHandle handle =
        BackgroundTracingManager::
            GetInstance()->RegisterTriggerType("reactive_test");

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));
    // second trigger to terminate.
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

// This tests that reactive mode only terminates with the same trigger.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveSecondTriggerMustMatchForTermination) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    base::DictionaryValue dict;
    dict.SetString("mode", "REACTIVE_TRACING_MODE");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString("rule",
                            "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
      rules_dict->SetString("trigger_name", "reactive_test1");
      rules_dict->SetString("category", "BENCHMARK");
      rules_list->Append(std::move(rules_dict));
    }
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString("rule",
                            "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
      rules_dict->SetString("trigger_name", "reactive_test2");
      rules_dict->SetString("category", "BENCHMARK");
      rules_list->Append(std::move(rules_dict));
    }
    dict.Set("configs", std::move(rules_list));

    std::unique_ptr<BackgroundTracingConfig> config(
        BackgroundTracingConfigImpl::FromDict(&dict));

    BackgroundTracingManager::TriggerHandle handle1 =
        BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "reactive_test1");
    BackgroundTracingManager::TriggerHandle handle2 =
        BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "reactive_test2");

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle1, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

    // This is expected to fail since we triggered with handle1.
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle2,
        base::Bind(&StartedFinalizingCallback, base::Closure(), false));

    // second trigger to terminate.
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle1, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

// This tests a third trigger in reactive more does not start another trace.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveThirdTriggerTimeout) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    std::unique_ptr<BackgroundTracingConfig> config = CreateReactiveConfig();

    BackgroundTracingManager::TriggerHandle handle =
        BackgroundTracingManager::
            GetInstance()->RegisterTriggerType("reactive_test");

    BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING);

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));
    // second trigger to terminate.
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));
    // third trigger to trigger again, fails as it is still gathering.
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

}  // namespace content

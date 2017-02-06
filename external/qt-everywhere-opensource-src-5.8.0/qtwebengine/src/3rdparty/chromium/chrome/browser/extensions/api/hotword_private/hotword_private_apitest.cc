// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/api/hotword_private/hotword_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/hotword_audio_history_handler.h"
#include "chrome/browser/search/hotword_client.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

const char kHotwordTestExtensionId[] = "cpfhkdbjfdgdebcjlifoldbijinjfifp";

// Mock the web history service so that we don't make actual requests over the
// network.
class MockWebHistoryService : public history::WebHistoryService {
 public:
  explicit MockWebHistoryService(Profile* profile)
      : WebHistoryService(
            ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
            SigninManagerFactory::GetForProfile(profile),
            profile->GetRequestContext()),
        expected_success_(true),
        expected_value_(false) {}
  ~MockWebHistoryService() override {}

  // For both of the following functions, just call the callback to simulate
  // a successful return from the url fetch.
  void GetAudioHistoryEnabled(
      const AudioWebHistoryCallback& callback) override {
    callback.Run(expected_success_, expected_value_ && expected_success_);
  }

  void SetAudioHistoryEnabled(
      bool new_enabled_value,
      const AudioWebHistoryCallback& callback) override {
    callback.Run(expected_success_, new_enabled_value && expected_success_);
  }

  void SetExpectedValue(bool expected_value) {
    expected_value_ = expected_value;
  }

  void SetFailureState() {
    expected_success_ = false;
  }

 private:
  bool expected_success_;
  bool expected_value_;
};

// Make a mock audio history handler so that the method for getting the web
// history can be overridden.
class MockAudioHistoryHandler : public HotwordAudioHistoryHandler {
 public:
  MockAudioHistoryHandler(content::BrowserContext* context,
                          history::WebHistoryService* web_history)
      : HotwordAudioHistoryHandler(context,
                                   base::ThreadTaskRunnerHandle::Get()),
        web_history_(web_history) {}
  ~MockAudioHistoryHandler() override {}

  history::WebHistoryService* GetWebHistory() override {
    return web_history_.get();
  }

 private:
  std::unique_ptr<history::WebHistoryService> web_history_;
};

class MockHotwordService : public HotwordService {
 public:
  explicit MockHotwordService(Profile* profile)
      : HotwordService(profile), service_available_(true) {}
  ~MockHotwordService() override {}

  bool IsServiceAvailable() override { return service_available_; }

  void setServiceAvailable(bool available) {
    service_available_ = available;
  }

  static std::unique_ptr<KeyedService> Build(content::BrowserContext* profile) {
    return base::WrapUnique(
        new MockHotwordService(static_cast<Profile*>(profile)));
  }

  LaunchMode GetHotwordAudioVerificationLaunchMode() override {
    return launch_mode_;
  }

  void SetHotwordAudioVerificationLaunchMode(const LaunchMode& launch_mode) {
    launch_mode_ = launch_mode;
  }

 private:
  bool service_available_;
  LaunchMode launch_mode_;

  DISALLOW_COPY_AND_ASSIGN(MockHotwordService);
};

class MockHotwordClient : public HotwordClient {
 public:
  MockHotwordClient()
      : last_enabled_(false),
        state_changed_count_(0),
        recognized_count_(0) {
  }

  ~MockHotwordClient() override {}

  void OnHotwordStateChanged(bool enabled) override {
    last_enabled_ = enabled;
    state_changed_count_++;
  }

  void OnHotwordRecognized(
      const scoped_refptr<content::SpeechRecognitionSessionPreamble>& preamble)
      override { recognized_count_++; }

  bool last_enabled() const { return last_enabled_; }
  int state_changed_count() const { return state_changed_count_; }
  int recognized_count() const { return recognized_count_; }

 private:
  bool last_enabled_;
  int state_changed_count_;
  int recognized_count_;

  DISALLOW_COPY_AND_ASSIGN(MockHotwordClient);
};

class HotwordPrivateApiTest : public ExtensionApiTest {
 public:
  HotwordPrivateApiTest() {}
  ~HotwordPrivateApiTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);

    // Whitelist the test extensions (which all share a common ID) to use
    // private APIs.
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID, kHotwordTestExtensionId);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    test_data_dir_ = test_data_dir_.AppendASCII("hotword_private");

    service_ = static_cast<MockHotwordService*>(
        HotwordServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), MockHotwordService::Build));
  }

  MockHotwordService* service() {
    return service_;
  }

 private:
  MockHotwordService* service_;

  DISALLOW_COPY_AND_ASSIGN(HotwordPrivateApiTest);
};

}  // anonymous namespace

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, SetEnabled) {
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kHotwordSearchEnabled));

  ExtensionTestMessageListener listenerTrue("ready", false);
  ASSERT_TRUE(RunComponentExtensionTest("setEnabledTrue")) << message_;
  EXPECT_TRUE(listenerTrue.WaitUntilSatisfied());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kHotwordSearchEnabled));

  ExtensionTestMessageListener listenerFalse("ready", false);
  ASSERT_TRUE(RunComponentExtensionTest("setEnabledFalse")) << message_;
  EXPECT_TRUE(listenerFalse.WaitUntilSatisfied());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kHotwordSearchEnabled));
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, SetAudioLoggingEnabled) {
  EXPECT_FALSE(service()->IsOptedIntoAudioLogging());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kHotwordAudioLoggingEnabled));

  ExtensionTestMessageListener listenerTrue("ready", false);
  ASSERT_TRUE(RunComponentExtensionTest("setAudioLoggingEnableTrue"))
      << message_;
  EXPECT_TRUE(listenerTrue.WaitUntilSatisfied());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kHotwordAudioLoggingEnabled));
  EXPECT_TRUE(service()->IsOptedIntoAudioLogging());

  ExtensionTestMessageListener listenerFalse("ready", false);
  ASSERT_TRUE(RunComponentExtensionTest("setAudioLoggingEnableFalse"))
      << message_;
  EXPECT_TRUE(listenerFalse.WaitUntilSatisfied());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kHotwordAudioLoggingEnabled));
  EXPECT_FALSE(service()->IsOptedIntoAudioLogging());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, SetHotwordAlwaysOnSearchEnabled) {
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kHotwordAlwaysOnSearchEnabled));

  ExtensionTestMessageListener listener("ready", false);
  ASSERT_TRUE(RunComponentExtensionTest("setHotwordAlwaysOnSearchEnableTrue"))
      << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kHotwordAlwaysOnSearchEnabled));

  listener.Reset();
  ASSERT_TRUE(RunComponentExtensionTest("setHotwordAlwaysOnSearchEnableFalse"))
      << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kHotwordAlwaysOnSearchEnabled));
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, GetStatus) {
  ASSERT_TRUE(RunComponentExtensionTest("getEnabled")) << message_;
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, IsAvailableTrue) {
  service()->setServiceAvailable(true);
  ExtensionTestMessageListener listener("available: true", false);
  ASSERT_TRUE(RunComponentExtensionTest("isAvailable")) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, IsAvailableTrue_NoGet) {
  service()->setServiceAvailable(true);
  ExtensionTestMessageListener listener("available: false", false);
  ASSERT_TRUE(RunComponentExtensionTest("isAvailableNoGet")) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, IsAvailableFalse) {
  service()->setServiceAvailable(false);
  ExtensionTestMessageListener listener("available: false", false);
  ASSERT_TRUE(RunComponentExtensionTest("isAvailable")) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, AlwaysOnEnabled) {
  // Bypass the hotword hardware check.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalHotwordHardware);

  {
    ExtensionTestMessageListener listener("alwaysOnEnabled: false",
                                          false);
    ASSERT_TRUE(RunComponentExtensionTest("alwaysOnEnabled"))
        << message_;
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  profile()->GetPrefs()->SetBoolean(prefs::kHotwordAlwaysOnSearchEnabled, true);
  {
    ExtensionTestMessageListener listener("alwaysOnEnabled: true",
                                          false);
    ASSERT_TRUE(RunComponentExtensionTest("alwaysOnEnabled"))
        << message_;
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, OnEnabledChanged) {
  // Trigger the pref registrar.
  extensions::HotwordPrivateEventService::GetFactoryInstance();
  ExtensionTestMessageListener listener("ready", false);
  ASSERT_TRUE(
      LoadExtensionAsComponent(test_data_dir_.AppendASCII("onEnabledChanged")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  ExtensionTestMessageListener listenerNotification("notification", false);
  profile()->GetPrefs()->SetBoolean(prefs::kHotwordSearchEnabled, true);
  EXPECT_TRUE(listenerNotification.WaitUntilSatisfied());

  listenerNotification.Reset();
  profile()->GetPrefs()->SetBoolean(prefs::kHotwordAlwaysOnSearchEnabled,
                                    true);
  EXPECT_TRUE(listenerNotification.WaitUntilSatisfied());

  listenerNotification.Reset();
  service()->StartTraining();
  EXPECT_TRUE(listenerNotification.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, HotwordSession) {
  extensions::HotwordPrivateEventService::GetFactoryInstance();
  ExtensionTestMessageListener listener("ready", false);
  LoadExtensionAsComponent(
      test_data_dir_.AppendASCII("hotwordSession"));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  ExtensionTestMessageListener listenerStopReady("stopReady", false);
  ExtensionTestMessageListener listenerStopped("stopped", false);
  MockHotwordClient client;
  service()->RequestHotwordSession(&client);
  EXPECT_TRUE(listenerStopReady.WaitUntilSatisfied());
  service()->StopHotwordSession(&client);
  EXPECT_TRUE(listenerStopped.WaitUntilSatisfied());

  EXPECT_TRUE(client.last_enabled());
  EXPECT_EQ(1, client.state_changed_count());
  EXPECT_EQ(1, client.recognized_count());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, GetLaunchStateHotwordOnly) {
  service()->SetHotwordAudioVerificationLaunchMode(
      HotwordService::HOTWORD_ONLY);
  ExtensionTestMessageListener listener("launchMode: 0", false);
  ASSERT_TRUE(RunComponentExtensionTest("getLaunchState")) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest,
    GetLaunchStateHotwordAudioHistory) {
  service()->SetHotwordAudioVerificationLaunchMode(
      HotwordService::HOTWORD_AND_AUDIO_HISTORY);
  ExtensionTestMessageListener listener("launchMode: 1", false);
  ASSERT_TRUE(RunComponentExtensionTest("getLaunchState")) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, OnFinalizeSpeakerModel) {
  // Trigger the pref registrar.
  extensions::HotwordPrivateEventService::GetFactoryInstance();
  ExtensionTestMessageListener listener("ready", false);
  ASSERT_TRUE(
      LoadExtensionAsComponent(test_data_dir_.AppendASCII(
          "onFinalizeSpeakerModel")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  ExtensionTestMessageListener listenerNotification("notification", false);
  service()->FinalizeSpeakerModel();
  EXPECT_TRUE(listenerNotification.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, OnHotwordTriggered) {
  // Trigger the pref registrar.
  extensions::HotwordPrivateEventService::GetFactoryInstance();
  ExtensionTestMessageListener listener("ready", false);
  ASSERT_TRUE(
      LoadExtensionAsComponent(test_data_dir_.AppendASCII(
          "onHotwordTriggered")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  ExtensionTestMessageListener listenerNotification("notification", false);
  service()->NotifyHotwordTriggered();
  EXPECT_TRUE(listenerNotification.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, OnDeleteSpeakerModel) {
  MockWebHistoryService* web_history = new MockWebHistoryService(profile());
  MockAudioHistoryHandler* handler =
      new MockAudioHistoryHandler(profile(), web_history);
  service()->SetAudioHistoryHandler(handler);
  web_history->SetExpectedValue(false);
  profile()->GetPrefs()->SetBoolean(prefs::kHotwordAlwaysOnSearchEnabled, true);

  // Trigger the pref registrar.
  extensions::HotwordPrivateEventService::GetFactoryInstance();
  ExtensionTestMessageListener listener("ready", false);
  ASSERT_TRUE(
      LoadExtensionAsComponent(test_data_dir_.AppendASCII(
          "onDeleteSpeakerModel")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  ExtensionTestMessageListener listenerNotification("notification", false);
  EXPECT_TRUE(listenerNotification.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, OnSpeakerModelExists) {
  extensions::HotwordPrivateEventService::GetFactoryInstance();
  ExtensionTestMessageListener listener("ready", false);
  ASSERT_TRUE(
      LoadExtensionAsComponent(test_data_dir_.AppendASCII(
          "onSpeakerModelExists")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  service()->OptIntoHotwording(HotwordService::HOTWORD_ONLY);

  ExtensionTestMessageListener listenerNotification("notification", false);
  EXPECT_TRUE(listenerNotification.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, SpeakerModelExistsResult) {
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kHotwordAlwaysOnSearchEnabled));

  ExtensionTestMessageListener listenerTrue("ready", false);
  ASSERT_TRUE(RunComponentExtensionTest(
      "speakerModelExistsResultTrue")) << message_;
  EXPECT_TRUE(listenerTrue.WaitUntilSatisfied());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kHotwordAlwaysOnSearchEnabled));

  PrefService* prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kHotwordAlwaysOnSearchEnabled, false);
  ExtensionTestMessageListener listenerFalse("ready", false);
  ASSERT_TRUE(RunComponentExtensionTest(
      "speakerModelExistsResultFalse")) << message_;
  EXPECT_TRUE(listenerFalse.WaitUntilSatisfied());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kHotwordAlwaysOnSearchEnabled));
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, Training) {
  EXPECT_FALSE(service()->IsTraining());

  ExtensionTestMessageListener listenerTrue("start training", false);
  ASSERT_TRUE(RunComponentExtensionTest("startTraining")) << message_;
  EXPECT_TRUE(listenerTrue.WaitUntilSatisfied());
  EXPECT_TRUE(service()->IsTraining());

  ExtensionTestMessageListener listenerFalse("stop training", false);
  ASSERT_TRUE(RunComponentExtensionTest("stopTraining")) << message_;
  EXPECT_TRUE(listenerFalse.WaitUntilSatisfied());
  EXPECT_FALSE(service()->IsTraining());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, OnSpeakerModelSaved) {
  extensions::HotwordPrivateEventService::GetFactoryInstance();
  ExtensionTestMessageListener listener("ready", false);
  ASSERT_TRUE(
      LoadExtensionAsComponent(test_data_dir_.AppendASCII(
          "onSpeakerModelSaved")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  ExtensionTestMessageListener listenerNotification("notification", false);
  EXPECT_TRUE(listenerNotification.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, NotifySpeakerModelSaved) {
  ExtensionTestMessageListener listener("speaker model saved", false);
  ASSERT_TRUE(
      RunComponentExtensionTest("notifySpeakerModelSaved")) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, AudioHistory) {
  MockWebHistoryService* web_history = new MockWebHistoryService(profile());
  MockAudioHistoryHandler* handler =
      new MockAudioHistoryHandler(profile(), web_history);
  service()->SetAudioHistoryHandler(handler);
  web_history->SetExpectedValue(true);

  ExtensionTestMessageListener setListenerT("set AH True: true success", false);
  ExtensionTestMessageListener setListenerF("set AH False: false success",
                                            false);
  ExtensionTestMessageListener getListener("get AH: true success", false);

  ASSERT_TRUE(RunComponentExtensionTest("audioHistory")) << message_;

  EXPECT_TRUE(setListenerT.WaitUntilSatisfied());
  EXPECT_TRUE(setListenerF.WaitUntilSatisfied());
  EXPECT_TRUE(getListener.WaitUntilSatisfied());

  web_history->SetExpectedValue(false);

  ExtensionTestMessageListener setListenerT2("set AH True: true success",
                                             false);
  ExtensionTestMessageListener setListenerF2("set AH False: false success",
                                             false);
  ExtensionTestMessageListener getListener2("get AH: false success", false);

  ASSERT_TRUE(RunComponentExtensionTest("audioHistory")) << message_;

  EXPECT_TRUE(setListenerT2.WaitUntilSatisfied());
  EXPECT_TRUE(setListenerF2.WaitUntilSatisfied());
  EXPECT_TRUE(getListener2.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, AudioHistoryNoWebHistory) {
  MockAudioHistoryHandler* handler =
      new MockAudioHistoryHandler(profile(), nullptr);
  service()->SetAudioHistoryHandler(handler);

  // Set an initial value for the audio logging pref.
  PrefService* prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kHotwordAudioLoggingEnabled, true);

  ExtensionTestMessageListener setListenerT("set AH True: true failure", false);
  ExtensionTestMessageListener setListenerF("set AH False: true failure",
                                            false);
  ExtensionTestMessageListener getListener("get AH: true failure", false);

  ASSERT_TRUE(RunComponentExtensionTest("audioHistory")) << message_;

  EXPECT_TRUE(setListenerT.WaitUntilSatisfied());
  EXPECT_TRUE(setListenerF.WaitUntilSatisfied());
  EXPECT_TRUE(getListener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, AudioHistoryWebHistoryFailure) {
  MockWebHistoryService* web_history = new MockWebHistoryService(profile());
  MockAudioHistoryHandler* handler =
      new MockAudioHistoryHandler(profile(), web_history);
  service()->SetAudioHistoryHandler(handler);
  web_history->SetFailureState();
  // It shouldn't matter if this is set to true. GetAudioHistoryEnabled should
  // still return false.
  web_history->SetExpectedValue(true);

  ExtensionTestMessageListener setListenerT("set AH True: true failure", false);
  ExtensionTestMessageListener setListenerF("set AH False: false failure",
                                            false);
  ExtensionTestMessageListener getListener("get AH: false failure", false);

  ASSERT_TRUE(RunComponentExtensionTest("audioHistory")) << message_;

  EXPECT_TRUE(setListenerT.WaitUntilSatisfied());
  EXPECT_TRUE(setListenerF.WaitUntilSatisfied());
  EXPECT_TRUE(getListener.WaitUntilSatisfied());
}

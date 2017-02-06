// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/webrtc_audio_private/webrtc_audio_private_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/media/webrtc_log_uploader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::JSONWriter;
using content::RenderProcessHost;
using content::WebContents;
using media::AudioDeviceNames;
using media::AudioManager;

namespace extensions {

using extension_function_test_utils::RunFunctionAndReturnError;
using extension_function_test_utils::RunFunctionAndReturnSingleResult;

class AudioWaitingExtensionTest : public ExtensionApiTest {
 protected:
  void WaitUntilAudioIsPlaying(WebContents* tab) {
    // Wait for audio to start playing. We gate this on there being one
    // or more AudioOutputController objects for our tab.
    bool audio_playing = false;
    for (size_t remaining_tries = 50; remaining_tries > 0; --remaining_tries) {
      tab->GetRenderProcessHost()->GetAudioOutputControllers(
          base::Bind(OnAudioControllers, &audio_playing));
      base::RunLoop().RunUntilIdle();
      if (audio_playing)
        break;

      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
    }

    if (!audio_playing)
      FAIL() << "Audio did not start playing within ~5 seconds.";
  }

  // Used by the test above to wait until audio is playing.
  static void OnAudioControllers(
      bool* audio_playing,
      const RenderProcessHost::AudioOutputControllerList& list) {
    if (!list.empty())
      *audio_playing = true;
  }
};

class WebrtcAudioPrivateTest : public AudioWaitingExtensionTest {
 public:
  WebrtcAudioPrivateTest()
      : enumeration_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void SetUpOnMainThread() override {
    AudioWaitingExtensionTest::SetUpOnMainThread();
    // Needs to happen after chrome's schemes are added.
    source_url_ = GURL("chrome-extension://fakeid012345678/fakepage.html");
  }

 protected:
  void AppendTabIdToRequestInfo(base::ListValue* params, int tab_id) {
    std::unique_ptr<base::DictionaryValue> request_info(
        new base::DictionaryValue());
    request_info->SetInteger("tabId", tab_id);
    params->Append(std::move(request_info));
  }

  std::string InvokeGetActiveSink(int tab_id) {
    base::ListValue parameters;
    AppendTabIdToRequestInfo(&parameters, tab_id);
    std::string parameter_string;
    JSONWriter::Write(parameters, &parameter_string);

    scoped_refptr<WebrtcAudioPrivateGetActiveSinkFunction> function =
        new WebrtcAudioPrivateGetActiveSinkFunction();
    function->set_source_url(source_url_);
    std::unique_ptr<base::Value> result(RunFunctionAndReturnSingleResult(
        function.get(), parameter_string, browser()));
    std::string device_id;
    result->GetAsString(&device_id);
    return device_id;
  }

  std::unique_ptr<base::Value> InvokeGetSinks(base::ListValue** sink_list) {
    scoped_refptr<WebrtcAudioPrivateGetSinksFunction> function =
        new WebrtcAudioPrivateGetSinksFunction();
    function->set_source_url(source_url_);

    std::unique_ptr<base::Value> result(
        RunFunctionAndReturnSingleResult(function.get(), "[]", browser()));
    result->GetAsList(sink_list);
    return result;
  }

  // Synchronously (from the calling thread's point of view) runs the
  // given enumeration function on the device thread. On return,
  // |device_names| has been filled with the device names resulting
  // from that call.
  void GetAudioDeviceNames(
      void (AudioManager::*EnumerationFunc)(AudioDeviceNames*),
      AudioDeviceNames* device_names) {
    AudioManager* audio_manager = AudioManager::Get();

    if (!audio_manager->GetTaskRunner()->BelongsToCurrentThread()) {
      audio_manager->GetTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(&WebrtcAudioPrivateTest::GetAudioDeviceNames, this,
                     EnumerationFunc, device_names));
      enumeration_event_.Wait();
    } else {
      (audio_manager->*EnumerationFunc)(device_names);
      enumeration_event_.Signal();
    }
  }

  // Synchronously (from the calling thread's point of view) retrieve the
  // device id in the |origin| on the IO thread. On return,
  // |id_in_origin| contains the id |raw_device_id| is known by in
  // the origin.
  void GetIDInOrigin(content::ResourceContext* resource_context,
                     GURL origin,
                     const std::string& raw_device_id,
                     std::string* id_in_origin) {
    if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::Bind(&WebrtcAudioPrivateTest::GetIDInOrigin,
                     this, resource_context, origin, raw_device_id,
                     id_in_origin));
      enumeration_event_.Wait();
    } else {
      *id_in_origin = content::GetHMACForMediaDeviceID(
          resource_context->GetMediaDeviceIDSalt(), url::Origin(origin),
          raw_device_id);
      enumeration_event_.Signal();
    }
  }

  // Event used to signal completion of enumeration.
  base::WaitableEvent enumeration_event_;

  GURL source_url_;
};

#if !defined(OS_MACOSX)
// http://crbug.com/334579
IN_PROC_BROWSER_TEST_F(WebrtcAudioPrivateTest, GetSinks) {
  AudioDeviceNames devices;
  GetAudioDeviceNames(&AudioManager::GetAudioOutputDeviceNames, &devices);

  base::ListValue* sink_list = NULL;
  std::unique_ptr<base::Value> result = InvokeGetSinks(&sink_list);

  std::string result_string;
  JSONWriter::Write(*result, &result_string);
  VLOG(2) << result_string;

  EXPECT_EQ(devices.size(), sink_list->GetSize());

  // Iterate through both lists in lockstep and compare. The order
  // should be identical.
  size_t ix = 0;
  AudioDeviceNames::const_iterator it = devices.begin();
  for (; ix < sink_list->GetSize() && it != devices.end();
       ++ix, ++it) {
    base::DictionaryValue* dict = NULL;
    sink_list->GetDictionary(ix, &dict);
    std::string sink_id;
    dict->GetString("sinkId", &sink_id);

    std::string expected_id;
    if (media::AudioDeviceDescription::IsDefaultDevice(it->unique_id)) {
      expected_id = media::AudioDeviceDescription::kDefaultDeviceId;
    } else {
      GetIDInOrigin(profile()->GetResourceContext(),
                    source_url_.GetOrigin(),
                    it->unique_id,
                    &expected_id);
    }

    EXPECT_EQ(expected_id, sink_id);
    std::string sink_label;
    dict->GetString("sinkLabel", &sink_label);
    EXPECT_EQ(it->device_name, sink_label);

    // TODO(joi): Verify the contents of these once we start actually
    // filling them in.
    EXPECT_TRUE(dict->HasKey("isDefault"));
    EXPECT_TRUE(dict->HasKey("isReady"));
    EXPECT_TRUE(dict->HasKey("sampleRate"));
  }
}
#endif  // OS_MACOSX

// This exercises the case where you have a tab with no active media
// stream and try to retrieve the currently active audio sink.
IN_PROC_BROWSER_TEST_F(WebrtcAudioPrivateTest, GetActiveSinkNoMediaStream) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = ExtensionTabUtil::GetTabId(tab);
  base::ListValue parameters;
  AppendTabIdToRequestInfo(&parameters, tab_id);
  std::string parameter_string;
  JSONWriter::Write(parameters, &parameter_string);

  scoped_refptr<WebrtcAudioPrivateGetActiveSinkFunction> function =
      new WebrtcAudioPrivateGetActiveSinkFunction();
  function->set_source_url(source_url_);
  std::unique_ptr<base::Value> result(RunFunctionAndReturnSingleResult(
      function.get(), parameter_string, browser()));

  std::string result_string;
  JSONWriter::Write(*result, &result_string);
  EXPECT_EQ("\"\"", result_string);
}

// This exercises the case where you have a tab with no active media
// stream and try to set the audio sink.
IN_PROC_BROWSER_TEST_F(WebrtcAudioPrivateTest, SetActiveSinkNoMediaStream) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = ExtensionTabUtil::GetTabId(tab);
  base::ListValue parameters;
  AppendTabIdToRequestInfo(&parameters, tab_id);
  parameters.AppendString("no such id");
  std::string parameter_string;
  JSONWriter::Write(parameters, &parameter_string);

  scoped_refptr<WebrtcAudioPrivateSetActiveSinkFunction> function =
      new WebrtcAudioPrivateSetActiveSinkFunction();
  function->set_source_url(source_url_);
  std::string error(RunFunctionAndReturnError(function.get(),
                                              parameter_string,
                                              browser()));
  EXPECT_EQ(base::StringPrintf("No active stream for tabId %d", tab_id),
            error);
}

IN_PROC_BROWSER_TEST_F(WebrtcAudioPrivateTest, GetAndSetWithMediaStream) {
  // Disabled on Win 7. https://crbug.com/500432.
#if defined(OS_WIN)
  if (base::win::GetVersion() == base::win::VERSION_WIN7)
    return;
#endif

  // First retrieve the list of all sinks, so that we can run a test
  // where we set the active sink to each of the different available
  // sinks in turn.
  base::ListValue* sink_list = NULL;
  std::unique_ptr<base::Value> result = InvokeGetSinks(&sink_list);

  ASSERT_TRUE(StartEmbeddedTestServer());

  // Open a normal page that uses an audio sink.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL(embedded_test_server()->GetURL("/extensions/loop_audio.html")));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = ExtensionTabUtil::GetTabId(tab);

  WaitUntilAudioIsPlaying(tab);

  std::string current_device = InvokeGetActiveSink(tab_id);
  VLOG(2) << "Before setting, current device: " << current_device;
  EXPECT_NE("", current_device);

  // Set to each of the other devices in turn.
  for (size_t ix = 0; ix < sink_list->GetSize(); ++ix) {
    base::DictionaryValue* dict = NULL;
    sink_list->GetDictionary(ix, &dict);
    std::string target_device;
    dict->GetString("sinkId", &target_device);

    base::ListValue parameters;
    AppendTabIdToRequestInfo(&parameters, tab_id);
    parameters.AppendString(target_device);
    std::string parameter_string;
    JSONWriter::Write(parameters, &parameter_string);

    scoped_refptr<WebrtcAudioPrivateSetActiveSinkFunction> function =
      new WebrtcAudioPrivateSetActiveSinkFunction();
    function->set_source_url(source_url_);
    std::unique_ptr<base::Value> result(RunFunctionAndReturnSingleResult(
        function.get(), parameter_string, browser()));
    // The function was successful if the above invocation doesn't
    // fail. Just for kicks, also check that it returns no result.
    EXPECT_EQ(NULL, result.get());

    current_device = InvokeGetActiveSink(tab_id);
    VLOG(2) << "After setting to " << target_device
            << ", current device is " << current_device;
    EXPECT_EQ(target_device, current_device);
  }
}

IN_PROC_BROWSER_TEST_F(WebrtcAudioPrivateTest, GetAssociatedSink) {
  // Get the list of input devices. We can cheat in the unit test and
  // run this on the main thread since nobody else will be running at
  // the same time.
  AudioDeviceNames devices;
  GetAudioDeviceNames(&AudioManager::GetAudioInputDeviceNames, &devices);

  // Try to get an associated sink for each source.
  for (AudioDeviceNames::const_iterator device = devices.begin();
       device != devices.end();
       ++device) {
    scoped_refptr<WebrtcAudioPrivateGetAssociatedSinkFunction> function =
        new WebrtcAudioPrivateGetAssociatedSinkFunction();
    function->set_source_url(source_url_);

    std::string raw_device_id = device->unique_id;
    VLOG(2) << "Trying to find associated sink for device " << raw_device_id;
    std::string source_id_in_origin;
    GURL origin(GURL("http://www.google.com/").GetOrigin());
    GetIDInOrigin(profile()->GetResourceContext(),
                  origin,
                  raw_device_id,
                  &source_id_in_origin);

    base::ListValue parameters;
    parameters.AppendString(origin.spec());
    parameters.AppendString(source_id_in_origin);
    std::string parameter_string;
    JSONWriter::Write(parameters, &parameter_string);

    std::unique_ptr<base::Value> result(RunFunctionAndReturnSingleResult(
        function.get(), parameter_string, browser()));
    std::string result_string;
    JSONWriter::Write(*result, &result_string);
    VLOG(2) << "Results: " << result_string;
  }
}

// Times out frequently on Windows, CrOS: http://crbug.com/517112
#if defined(OS_WIN) || defined(OS_CHROMEOS)
#define MAYBE_TriggerEvent DISABLED_TriggerEvent
#else
#define MAYBE_TriggerEvent TriggerEvent
#endif

IN_PROC_BROWSER_TEST_F(WebrtcAudioPrivateTest, MAYBE_TriggerEvent) {
  WebrtcAudioPrivateEventService* service =
      WebrtcAudioPrivateEventService::GetFactoryInstance()->Get(profile());

  // Just trigger, without any extension listening.
  service->OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO_CAPTURE);

  // Now load our test extension and do it again.
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("webrtc_audio_private_event_listener"));
  service->OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO_CAPTURE);

  // Check that the extension got the notification.
  std::string result = ExecuteScriptInBackgroundPage(extension->id(),
                                                     "reportIfGot()");
  EXPECT_EQ("true", result);
}

class HangoutServicesBrowserTest : public AudioWaitingExtensionTest {
 public:
  void SetUp() override {
    // Make sure the Hangout Services component extension gets loaded.
    ComponentLoader::EnableBackgroundExtensionsForTesting();
    AudioWaitingExtensionTest::SetUp();
  }
};

#if defined(GOOGLE_CHROME_BUILD) || defined(ENABLE_HANGOUT_SERVICES_EXTENSION)
IN_PROC_BROWSER_TEST_F(HangoutServicesBrowserTest,
                       RunComponentExtensionTest) {
  // This runs the end-to-end JavaScript test for the Hangout Services
  // component extension, which uses the webrtcAudioPrivate API among
  // others.
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL url(embedded_test_server()->GetURL(
               "/extensions/hangout_services_test.html"));
  // The "externally connectable" extension permission doesn't seem to
  // like when we use 127.0.0.1 as the host, but using localhost works.
  std::string url_spec = url.spec();
  base::ReplaceFirstSubstringAfterOffset(
      &url_spec, 0, "127.0.0.1", "localhost");
  GURL localhost_url(url_spec);
  ui_test_utils::NavigateToURL(browser(), localhost_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  WaitUntilAudioIsPlaying(tab);

  // Override, i.e. disable, uploading. We don't want to try sending data to
  // servers when running the test. We don't bother about the contents of the
  // buffer |dummy|, that's tested in other tests.
  std::string dummy;
  g_browser_process->webrtc_log_uploader()->
      OverrideUploadWithBufferForTesting(&dummy);

  ASSERT_TRUE(content::ExecuteScript(tab, "browsertestRunAllTests();"));

  content::TitleWatcher title_watcher(tab, base::ASCIIToUTF16("success"));
  title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("failure"));
  base::string16 result = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(base::ASCIIToUTF16("success"), result);

  g_browser_process->webrtc_log_uploader()->OverrideUploadWithBufferForTesting(
      NULL);
}
#endif  // defined(GOOGLE_CHROME_BUILD) || defined(ENABLE_HANGOUT_SERVICES_EXTENSION)

}  // namespace extensions

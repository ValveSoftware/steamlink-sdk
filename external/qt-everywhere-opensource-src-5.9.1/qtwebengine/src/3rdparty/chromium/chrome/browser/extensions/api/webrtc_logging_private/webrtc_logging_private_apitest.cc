// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/webrtc_logging_private/webrtc_logging_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/media/webrtc/webrtc_log_uploader.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/test_util.h"
#include "third_party/zlib/google/compression_utils.h"

using compression::GzipUncompress;
using extensions::Extension;
using extensions::WebrtcLoggingPrivateDiscardFunction;
using extensions::WebrtcLoggingPrivateSetMetaDataFunction;
using extensions::WebrtcLoggingPrivateStartFunction;
using extensions::WebrtcLoggingPrivateStartRtpDumpFunction;
using extensions::WebrtcLoggingPrivateStopFunction;
using extensions::WebrtcLoggingPrivateStopRtpDumpFunction;
using extensions::WebrtcLoggingPrivateStoreFunction;
using extensions::WebrtcLoggingPrivateUploadFunction;
using extensions::WebrtcLoggingPrivateUploadStoredFunction;
using extensions::WebrtcLoggingPrivateStartAudioDebugRecordingsFunction;
using extensions::WebrtcLoggingPrivateStopAudioDebugRecordingsFunction;

namespace utils = extension_function_test_utils;

namespace {

static const char kTestLoggingSessionIdKey[] = "app_session_id";
static const char kTestLoggingSessionIdValue[] = "0123456789abcdef";
static const char kTestLoggingUrl[] = "dummy url string";

std::string ParamsToString(const base::ListValue& parameters) {
  std::string parameter_string;
  EXPECT_TRUE(base::JSONWriter::Write(parameters, &parameter_string));
  return parameter_string;
}

void InitializeTestMetaData(base::ListValue* parameters) {
  std::unique_ptr<base::DictionaryValue> meta_data_entry(
      new base::DictionaryValue());
  meta_data_entry->SetString("key", kTestLoggingSessionIdKey);
  meta_data_entry->SetString("value", kTestLoggingSessionIdValue);
  std::unique_ptr<base::ListValue> meta_data(new base::ListValue());
  meta_data->Append(std::move(meta_data_entry));
  meta_data_entry.reset(new base::DictionaryValue());
  meta_data_entry->SetString("key", "url");
  meta_data_entry->SetString("value", kTestLoggingUrl);
  meta_data->Append(std::move(meta_data_entry));
  parameters->Append(std::move(meta_data));
}

class WebrtcLoggingPrivateApiTest : public ExtensionApiTest {
 protected:
  void SetUp() override {
    ExtensionApiTest::SetUp();
    extension_ = extensions::test_util::CreateEmptyExtension();
  }

  template<typename T>
  scoped_refptr<T> CreateFunction() {
    scoped_refptr<T> function(new T());
    function->set_extension(extension_.get());
    function->set_has_callback(true);
    return function;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void AppendTabIdAndUrl(base::ListValue* parameters) {
    std::unique_ptr<base::DictionaryValue> request_info(
        new base::DictionaryValue());
    request_info->SetInteger(
        "tabId", extensions::ExtensionTabUtil::GetTabId(web_contents()));
    parameters->Append(std::move(request_info));
    parameters->AppendString(web_contents()->GetURL().GetOrigin().spec());
  }

  bool RunFunction(UIThreadExtensionFunction* function,
                   const base::ListValue& parameters,
                   bool expect_results) {
    std::unique_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
        function, ParamsToString(parameters), browser()));
    if (expect_results) {
      EXPECT_TRUE(result.get());
      return result != nullptr;
    }

    EXPECT_FALSE(result.get());
    return result == nullptr;
  }

  template<typename Function>
  bool RunFunction(const base::ListValue& parameters, bool expect_results) {
    scoped_refptr<Function> function(CreateFunction<Function>());
    return RunFunction(function.get(), parameters, expect_results);
  }

  template<typename Function>
  bool RunNoArgsFunction(bool expect_results) {
    base::ListValue params;
    AppendTabIdAndUrl(&params);
    scoped_refptr<Function> function(CreateFunction<Function>());
    return RunFunction(function.get(), params, expect_results);
  }

  bool StartLogging() {
    return RunNoArgsFunction<WebrtcLoggingPrivateStartFunction>(false);
  }

  bool StopLogging() {
    return RunNoArgsFunction<WebrtcLoggingPrivateStopFunction>(false);
  }

  bool DiscardLog() {
    return RunNoArgsFunction<WebrtcLoggingPrivateDiscardFunction>(false);
  }

  bool UploadLog() {
    return RunNoArgsFunction<WebrtcLoggingPrivateUploadFunction>(true);
  }

  bool SetMetaData(const base::ListValue& data) {
    return RunFunction<WebrtcLoggingPrivateSetMetaDataFunction>(data, false);
  }

  bool StartRtpDump(bool incoming, bool outgoing) {
    base::ListValue params;
    AppendTabIdAndUrl(&params);
    params.AppendBoolean(incoming);
    params.AppendBoolean(outgoing);
    return RunFunction<WebrtcLoggingPrivateStartRtpDumpFunction>(params, false);
  }

  bool StopRtpDump(bool incoming, bool outgoing) {
    base::ListValue params;
    AppendTabIdAndUrl(&params);
    params.AppendBoolean(incoming);
    params.AppendBoolean(outgoing);
    return RunFunction<WebrtcLoggingPrivateStopRtpDumpFunction>(params, false);
  }

  bool StoreLog(const std::string& log_id) {
    base::ListValue params;
    AppendTabIdAndUrl(&params);
    params.AppendString(log_id);
    return RunFunction<WebrtcLoggingPrivateStoreFunction>(params, false);
  }

  bool UploadStoredLog(const std::string& log_id) {
    base::ListValue params;
    AppendTabIdAndUrl(&params);
    params.AppendString(log_id);
    return RunFunction<WebrtcLoggingPrivateUploadStoredFunction>(params, true);
  }

  bool StartAudioDebugRecordings(int seconds) {
    base::ListValue params;
    AppendTabIdAndUrl(&params);
    params.AppendInteger(seconds);
    return RunFunction<WebrtcLoggingPrivateStartAudioDebugRecordingsFunction>(
        params, true);
  }

  bool StopAudioDebugRecordings() {
    base::ListValue params;
    AppendTabIdAndUrl(&params);
    return RunFunction<WebrtcLoggingPrivateStopAudioDebugRecordingsFunction>(
        params, true);
  }

 private:
  scoped_refptr<Extension> extension_;
};

// Helper class to temporarily tell the uploader to save the multipart buffer to
// a test string instead of uploading.
class ScopedOverrideUploadBuffer {
 public:
  ScopedOverrideUploadBuffer() {
    g_browser_process->webrtc_log_uploader()->
        OverrideUploadWithBufferForTesting(&multipart_);
  }

  ~ScopedOverrideUploadBuffer() {
    g_browser_process->webrtc_log_uploader()->
        OverrideUploadWithBufferForTesting(nullptr);
  }

  const std::string& multipart() const { return multipart_; }

 private:
  std::string multipart_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest, TestStartStopDiscard) {
  ScopedOverrideUploadBuffer buffer_override;

  EXPECT_TRUE(StartLogging());
  EXPECT_TRUE(StopLogging());
  EXPECT_TRUE(DiscardLog());

  EXPECT_TRUE(buffer_override.multipart().empty());
}

// Tests WebRTC diagnostic logging. Sets up the browser to save the multipart
// contents to a buffer instead of uploading it, then verifies it after a calls.
// Example of multipart contents:
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="prod"
//
// Chrome_Linux
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="ver"
//
// 30.0.1554.0
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="guid"
//
// 0
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="type"
//
// webrtc_log
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="app_session_id"
//
// 0123456789abcdef
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="url"
//
// http://127.0.0.1:43213/webrtc/webrtc_jsep01_test.html
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="webrtc_log"; filename="webrtc_log.gz"
// Content-Type: application/gzip
//
// <compressed data (zip)>
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**------
//
IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest, TestStartStopUpload) {
  ScopedOverrideUploadBuffer buffer_override;

  base::ListValue parameters;
  AppendTabIdAndUrl(&parameters);
  InitializeTestMetaData(&parameters);

  SetMetaData(parameters);
  StartLogging();
  StopLogging();
  UploadLog();

  std::string multipart = buffer_override.multipart();
  ASSERT_FALSE(multipart.empty());

  // Check multipart data.

  const char boundary[] = "------**--yradnuoBgoLtrapitluMklaTelgooG--**----";

  // Move the compressed data to its own string, since it may contain "\r\n" and
  // it makes the tests below easier.
  const char zip_content_type[] = "Content-Type: application/gzip";
  size_t zip_pos = multipart.find(&zip_content_type[0]);
  ASSERT_NE(std::string::npos, zip_pos);
  // Move pos to where the zip begins. - 1 to remove '\0', + 4 for two "\r\n".
  zip_pos += sizeof(zip_content_type) + 3;
  size_t zip_length = multipart.find(boundary, zip_pos);
  ASSERT_NE(std::string::npos, zip_length);
  // Calculate length, adjust for a "\r\n".
  zip_length -= zip_pos + 2;
  ASSERT_GT(zip_length, 0u);
  std::string log_part = multipart.substr(zip_pos, zip_length);
  multipart.erase(zip_pos, zip_length);

  // Uncompress log and verify contents.
  EXPECT_TRUE(GzipUncompress(log_part, &log_part));
  EXPECT_GT(log_part.length(), 0u);
  // Verify that meta data exists.
  EXPECT_NE(std::string::npos, log_part.find(base::StringPrintf("%s: %s",
      kTestLoggingSessionIdKey, kTestLoggingSessionIdValue)));
  // Verify that the basic info generated at logging startup exists.
  EXPECT_NE(std::string::npos, log_part.find("Chrome version:"));
  EXPECT_NE(std::string::npos, log_part.find("Cpu brand:"));

  // Check the multipart contents.
  std::vector<std::string> multipart_lines = base::SplitStringUsingSubstr(
      multipart, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(31, static_cast<int>(multipart_lines.size()));

  EXPECT_STREQ(&boundary[0], multipart_lines[0].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"prod\"",
               multipart_lines[1].c_str());
  EXPECT_TRUE(multipart_lines[2].empty());
  EXPECT_NE(std::string::npos, multipart_lines[3].find("Chrome"));

  EXPECT_STREQ(&boundary[0], multipart_lines[4].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"ver\"",
               multipart_lines[5].c_str());
  EXPECT_TRUE(multipart_lines[6].empty());
  // Just check that the version contains a dot.
  EXPECT_NE(std::string::npos, multipart_lines[7].find('.'));

  EXPECT_STREQ(&boundary[0], multipart_lines[8].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"guid\"",
               multipart_lines[9].c_str());
  EXPECT_TRUE(multipart_lines[10].empty());
  EXPECT_STREQ("0", multipart_lines[11].c_str());

  EXPECT_STREQ(&boundary[0], multipart_lines[12].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"type\"",
               multipart_lines[13].c_str());
  EXPECT_TRUE(multipart_lines[14].empty());
  EXPECT_STREQ("webrtc_log", multipart_lines[15].c_str());

  EXPECT_STREQ(&boundary[0], multipart_lines[16].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"app_session_id\"",
               multipart_lines[17].c_str());
  EXPECT_TRUE(multipart_lines[18].empty());
  EXPECT_STREQ(kTestLoggingSessionIdValue, multipart_lines[19].c_str());

  EXPECT_STREQ(&boundary[0], multipart_lines[20].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"url\"",
               multipart_lines[21].c_str());
  EXPECT_TRUE(multipart_lines[22].empty());
  EXPECT_STREQ(kTestLoggingUrl, multipart_lines[23].c_str());

  EXPECT_STREQ(&boundary[0], multipart_lines[24].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"webrtc_log\";"
               " filename=\"webrtc_log.gz\"",
               multipart_lines[25].c_str());
  EXPECT_STREQ("Content-Type: application/gzip",
               multipart_lines[26].c_str());
  EXPECT_TRUE(multipart_lines[27].empty());
  EXPECT_TRUE(multipart_lines[28].empty());  // The removed zip part.
  std::string final_delimiter = boundary;
  final_delimiter += "--";
  EXPECT_STREQ(final_delimiter.c_str(), multipart_lines[29].c_str());
  EXPECT_TRUE(multipart_lines[30].empty());
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest, TestStartStopRtpDump) {
  // TODO(tommi): As is, these tests are missing verification of the actual
  // RTP dump data.  We should fix that, e.g. use SetDumpWriterForTesting.
  StartRtpDump(true, true);
  StopRtpDump(true, true);
}

// Tests trying to store a log when a log is not being captured.
// We should get a failure callback in this case.
IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest, TestStoreWithoutLog) {
  base::ListValue parameters;
  AppendTabIdAndUrl(&parameters);
  parameters.AppendString("MyLogId");
  scoped_refptr<WebrtcLoggingPrivateStoreFunction> store(
      CreateFunction<WebrtcLoggingPrivateStoreFunction>());
  const std::string error = utils::RunFunctionAndReturnError(
      store.get(), ParamsToString(parameters), browser());
  ASSERT_FALSE(error.empty());
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest, TestStartStopStore) {
  ASSERT_TRUE(StartLogging());
  ASSERT_TRUE(StopLogging());
  EXPECT_TRUE(StoreLog("MyLogID"));
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest,
                       TestStartStopStoreAndUpload) {
  static const char kLogId[] = "TestStartStopStoreAndUpload";
  ASSERT_TRUE(StartLogging());
  ASSERT_TRUE(StopLogging());
  ASSERT_TRUE(StoreLog(kLogId));

  ScopedOverrideUploadBuffer buffer_override;
  EXPECT_TRUE(UploadStoredLog(kLogId));
  EXPECT_NE(std::string::npos,
            buffer_override.multipart().find("filename=\"webrtc_log.gz\""));
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest,
                       TestStartStopStoreAndUploadWithRtp) {
  static const char kLogId[] = "TestStartStopStoreAndUploadWithRtp";
  ASSERT_TRUE(StartLogging());
  ASSERT_TRUE(StartRtpDump(true, true));
  ASSERT_TRUE(StopLogging());
  ASSERT_TRUE(StopRtpDump(true, true));
  ASSERT_TRUE(StoreLog(kLogId));

  ScopedOverrideUploadBuffer buffer_override;
  EXPECT_TRUE(UploadStoredLog(kLogId));
  EXPECT_NE(std::string::npos,
            buffer_override.multipart().find("filename=\"webrtc_log.gz\""));
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest,
                       TestStartStopStoreAndUploadWithMetaData) {
  static const char kLogId[] = "TestStartStopStoreAndUploadWithRtp";
  ASSERT_TRUE(StartLogging());

  base::ListValue parameters;
  AppendTabIdAndUrl(&parameters);
  InitializeTestMetaData(&parameters);
  SetMetaData(parameters);

  ASSERT_TRUE(StopLogging());
  ASSERT_TRUE(StoreLog(kLogId));

  ScopedOverrideUploadBuffer buffer_override;
  EXPECT_TRUE(UploadStoredLog(kLogId));
  EXPECT_NE(std::string::npos,
            buffer_override.multipart().find("filename=\"webrtc_log.gz\""));
  EXPECT_NE(std::string::npos,
            buffer_override.multipart().find(kTestLoggingUrl));
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest,
                       TestStartStopAudioDebugRecordings) {
  // TODO(guidou): These tests are missing verification of the actual AEC dump
  // data. This will be fixed with a separate browser test.
  // See crbug.com/569957.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAudioDebugRecordingsFromExtension);
  ASSERT_TRUE(StartAudioDebugRecordings(0));
  ASSERT_TRUE(StopAudioDebugRecordings());
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest,
                       TestStartTimedAudioDebugRecordings) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAudioDebugRecordingsFromExtension);
  ASSERT_TRUE(StartAudioDebugRecordings(1));
}

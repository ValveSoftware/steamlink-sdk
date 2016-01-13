// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/debug/trace_event_impl.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/test/trace_event_analyzer.h"
#include "base/values.h"
#include "content/browser/media/webrtc_internals.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/webrtc_content_browsertest_base.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/perf/perf_test.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using trace_analyzer::TraceAnalyzer;
using trace_analyzer::Query;
using trace_analyzer::TraceEventVector;

namespace {

static const char kGetUserMediaAndStop[] = "getUserMediaAndStop";
static const char kGetUserMediaAndGetStreamUp[] = "getUserMediaAndGetStreamUp";
static const char kGetUserMediaAndAnalyseAndStop[] =
    "getUserMediaAndAnalyseAndStop";
static const char kGetUserMediaAndExpectFailure[] =
    "getUserMediaAndExpectFailure";
static const char kRenderSameTrackMediastreamAndStop[] =
    "renderSameTrackMediastreamAndStop";
static const char kRenderClonedMediastreamAndStop[] =
    "renderClonedMediastreamAndStop";
static const char kRenderClonedTrackMediastreamAndStop[] =
    "renderClonedTrackMediastreamAndStop";
static const char kRenderDuplicatedMediastreamAndStop[] =
    "renderDuplicatedMediastreamAndStop";

// Results returned by JS.
static const char kOK[] = "OK";

std::string GenerateGetUserMediaWithMandatorySourceID(
    const std::string& function_name,
    const std::string& audio_source_id,
    const std::string& video_source_id) {
  const std::string audio_constraint =
      "audio: {mandatory: { sourceId:\"" + audio_source_id + "\"}}, ";

  const std::string video_constraint =
      "video: {mandatory: { sourceId:\"" + video_source_id + "\"}}";
  return function_name + "({" + audio_constraint + video_constraint + "});";
}

std::string GenerateGetUserMediaWithOptionalSourceID(
    const std::string& function_name,
    const std::string& audio_source_id,
    const std::string& video_source_id) {
  const std::string audio_constraint =
      "audio: {optional: [{sourceId:\"" + audio_source_id + "\"}]}, ";

  const std::string video_constraint =
      "video: {optional: [{ sourceId:\"" + video_source_id + "\"}]}";
  return function_name + "({" + audio_constraint + video_constraint + "});";
}

}  // namespace

namespace content {

class WebRtcGetUserMediaBrowserTest: public WebRtcContentBrowserTest,
                                     public testing::WithParamInterface<bool> {
 public:
  WebRtcGetUserMediaBrowserTest() : trace_log_(NULL) {}
  virtual ~WebRtcGetUserMediaBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    WebRtcContentBrowserTest::SetUpCommandLine(command_line);

    bool enable_audio_track_processing = GetParam();
    if (!enable_audio_track_processing)
      command_line->AppendSwitch(switches::kDisableAudioTrackProcessing);
  }

  void StartTracing() {
    CHECK(trace_log_ == NULL) << "Can only can start tracing once";
    trace_log_ = base::debug::TraceLog::GetInstance();
    trace_log_->SetEnabled(base::debug::CategoryFilter("video"),
                           base::debug::TraceLog::RECORDING_MODE,
                           base::debug::TraceLog::ENABLE_SAMPLING);
    // Check that we are indeed recording.
    EXPECT_EQ(trace_log_->GetNumTracesRecorded(), 1);
  }

  void StopTracing() {
    CHECK(message_loop_runner_ == NULL) << "Calling StopTracing more than once";
    trace_log_->SetDisabled();
    message_loop_runner_ = new MessageLoopRunner;
    trace_log_->Flush(base::Bind(
        &WebRtcGetUserMediaBrowserTest::OnTraceDataCollected,
        base::Unretained(this)));
    message_loop_runner_->Run();
  }

  void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr,
      bool has_more_events) {
    CHECK(!has_more_events);
    recorded_trace_data_ = events_str_ptr;
    message_loop_runner_->Quit();
  }

  TraceAnalyzer* CreateTraceAnalyzer() {
    return TraceAnalyzer::Create("[" + recorded_trace_data_->data() + "]");
  }

  void RunGetUserMediaAndCollectMeasures(const int time_to_sample_secs,
                                         const std::string& measure_filter,
                                         const std::string& graph_name) {
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

    GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
    NavigateToURL(shell(), url);

    // Put getUserMedia to work and let it run for a couple of seconds.
    DCHECK(time_to_sample_secs);
    ExecuteJavascriptAndWaitForOk(
        base::StringPrintf("%s({video: true});",
                           kGetUserMediaAndGetStreamUp));

    // Now the stream is up and running, start collecting traces.
    StartTracing();

    // Let the stream run for a while in javascript.
    ExecuteJavascriptAndWaitForOk(
        base::StringPrintf("waitAndStopVideoTrack(%d);", time_to_sample_secs));

    // Wait until the page title changes to "OK". Do not sleep() here since that
    // would stop both this code and the browser underneath.
    StopTracing();

    scoped_ptr<TraceAnalyzer> analyzer(CreateTraceAnalyzer());
    analyzer->AssociateBeginEndEvents();
    trace_analyzer::TraceEventVector events;
    DCHECK(measure_filter.size());
    analyzer->FindEvents(
        Query::EventNameIs(measure_filter),
        &events);
    ASSERT_GT(events.size(), 0u)
        << "Could not collect any samples during test, this is bad";

    std::string duration_us;
    std::string interarrival_us;
    for (size_t i = 0; i != events.size(); ++i) {
      duration_us.append(
          base::StringPrintf("%d,", static_cast<int>(events[i]->duration)));
    }

    for (size_t i = 1; i < events.size(); ++i) {
      // The event |timestamp| comes in ns, divide to get us like |duration|.
      interarrival_us.append(base::StringPrintf("%d,",
          static_cast<int>((events[i]->timestamp - events[i - 1]->timestamp) /
                           base::Time::kNanosecondsPerMicrosecond)));
    }

    perf_test::PrintResultList(
        graph_name, "", "sample_duration", duration_us, "us", true);

    perf_test::PrintResultList(
        graph_name, "", "interarrival_time", interarrival_us, "us", true);
  }

  void RunTwoGetTwoGetUserMediaWithDifferentContraints(
      const std::string& constraints1,
      const std::string& constraints2,
      const std::string& expected_result) {
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

    GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
    NavigateToURL(shell(), url);

    std::string command = "twoGetUserMedia(" + constraints1 + ',' +
        constraints2 + ')';

    EXPECT_EQ(expected_result, ExecuteJavascriptAndReturnResult(command));
  }

  void GetInputDevices(std::vector<std::string>* audio_ids,
                       std::vector<std::string>* video_ids) {
    GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
    NavigateToURL(shell(), url);

    std::string devices_as_json = ExecuteJavascriptAndReturnResult(
        "getSources()");
    EXPECT_FALSE(devices_as_json.empty());

    int error_code;
    std::string error_message;
    scoped_ptr<base::Value> value(
        base::JSONReader::ReadAndReturnError(devices_as_json,
                                             base::JSON_ALLOW_TRAILING_COMMAS,
                                             &error_code,
                                             &error_message));

    ASSERT_TRUE(value.get() != NULL) << error_message;
    EXPECT_EQ(value->GetType(), base::Value::TYPE_LIST);

    base::ListValue* values;
    ASSERT_TRUE(value->GetAsList(&values));

    for (base::ListValue::iterator it = values->begin();
         it != values->end(); ++it) {
      const base::DictionaryValue* dict;
      std::string kind;
      std::string device_id;
      ASSERT_TRUE((*it)->GetAsDictionary(&dict));
      ASSERT_TRUE(dict->GetString("kind", &kind));
      ASSERT_TRUE(dict->GetString("id", &device_id));
      ASSERT_FALSE(device_id.empty());
      EXPECT_TRUE(kind == "audio" || kind == "video");
      if (kind == "audio") {
        audio_ids->push_back(device_id);
      } else if (kind == "video") {
        video_ids->push_back(device_id);
      }
    }
    ASSERT_FALSE(audio_ids->empty());
    ASSERT_FALSE(video_ids->empty());
  }

 private:
  base::debug::TraceLog* trace_log_;
  scoped_refptr<base::RefCountedString> recorded_trace_data_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

static const bool kRunTestsWithFlag[] = { false, true };
INSTANTIATE_TEST_CASE_P(WebRtcGetUserMediaBrowserTests,
                        WebRtcGetUserMediaBrowserTest,
                        testing::ValuesIn(kRunTestsWithFlag));

// These tests will all make a getUserMedia call with different constraints and
// see that the success callback is called. If the error callback is called or
// none of the callbacks are called the tests will simply time out and fail.
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest, GetVideoStreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(
      base::StringPrintf("%s({video: true});", kGetUserMediaAndStop));
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       RenderSameTrackMediastreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(
      base::StringPrintf("%s({video: true});",
                         kRenderSameTrackMediastreamAndStop));
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       RenderClonedMediastreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);


  ExecuteJavascriptAndWaitForOk(
      base::StringPrintf("%s({video: true});",
                         kRenderClonedMediastreamAndStop));
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       kRenderClonedTrackMediastreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(
      base::StringPrintf("%s({video: true});",
                         kRenderClonedTrackMediastreamAndStop));
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       kRenderDuplicatedMediastreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(
      base::StringPrintf("%s({video: true});",
                          kRenderDuplicatedMediastreamAndStop));
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetAudioAndVideoStreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(base::StringPrintf(
      "%s({video: true, audio: true});", kGetUserMediaAndStop));
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetAudioAndVideoStreamAndClone) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk("getUserMediaAndClone();");
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       RenderVideoTrackInMultipleTagsAndPause) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk("getUserMediaAndRenderInSeveralVideoTags();");
}



IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetUserMediaWithMandatorySourceID) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetInputDevices(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  // Test all combinations of mandatory sourceID;
  for (std::vector<std::string>::const_iterator video_it = video_ids.begin();
       video_it != video_ids.end(); ++video_it) {
    for (std::vector<std::string>::const_iterator audio_it = audio_ids.begin();
         audio_it != audio_ids.end(); ++audio_it) {
      NavigateToURL(shell(), url);
      EXPECT_EQ(kOK, ExecuteJavascriptAndReturnResult(
          GenerateGetUserMediaWithMandatorySourceID(
              kGetUserMediaAndStop,
              *audio_it,
              *video_it)));
    }
  }
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetUserMediaWithInvalidMandatorySourceID) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetInputDevices(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  // Test with invalid mandatory audio sourceID.
  NavigateToURL(shell(), url);
  EXPECT_EQ("DevicesNotFoundError", ExecuteJavascriptAndReturnResult(
      GenerateGetUserMediaWithMandatorySourceID(
          kGetUserMediaAndExpectFailure,
          "something invalid",
          video_ids[0])));

  // Test with invalid mandatory video sourceID.
  EXPECT_EQ("DevicesNotFoundError", ExecuteJavascriptAndReturnResult(
      GenerateGetUserMediaWithMandatorySourceID(
          kGetUserMediaAndExpectFailure,
          audio_ids[0],
          "something invalid")));

  // Test with empty mandatory audio sourceID.
  EXPECT_EQ("DevicesNotFoundError", ExecuteJavascriptAndReturnResult(
      GenerateGetUserMediaWithMandatorySourceID(
          kGetUserMediaAndExpectFailure,
          "",
          video_ids[0])));
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetUserMediaWithInvalidOptionalSourceID) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetInputDevices(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  // Test with invalid optional audio sourceID.
  NavigateToURL(shell(), url);
  EXPECT_EQ(kOK, ExecuteJavascriptAndReturnResult(
      GenerateGetUserMediaWithOptionalSourceID(
          kGetUserMediaAndStop,
          "something invalid",
          video_ids[0])));

  // Test with invalid optional video sourceID.
  EXPECT_EQ(kOK, ExecuteJavascriptAndReturnResult(
      GenerateGetUserMediaWithOptionalSourceID(
          kGetUserMediaAndStop,
          audio_ids[0],
          "something invalid")));

  // Test with empty optional audio sourceID.
  EXPECT_EQ(kOK, ExecuteJavascriptAndReturnResult(
      GenerateGetUserMediaWithOptionalSourceID(
          kGetUserMediaAndStop,
          "",
          video_ids[0])));
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest, TwoGetUserMediaAndStop) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(
      "twoGetUserMediaAndStop({video: true, audio: true});");
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       TwoGetUserMediaWithEqualConstraints) {
  std::string constraints1 = "{video: true, audio: true}";
  const std::string& constraints2 = constraints1;
  std::string expected_result = "w=640:h=480-w=640:h=480";

  RunTwoGetTwoGetUserMediaWithDifferentContraints(constraints1, constraints2,
                                                  expected_result);
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       TwoGetUserMediaWithSecondVideoCropped) {
  std::string constraints1 = "{video: true}";
  std::string constraints2 = "{video: {mandatory: {maxHeight: 360}}}";
  std::string expected_result = "w=640:h=480-w=640:h=360";
  RunTwoGetTwoGetUserMediaWithDifferentContraints(constraints1, constraints2,
                                                  expected_result);
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       TwoGetUserMediaWithFirstHdSecondVga) {
  std::string constraints1 =
      "{video: {mandatory: {minWidth:1280 , minHeight: 720}}}";
  std::string constraints2 =
      "{video: {mandatory: {maxWidth:640 , maxHeight: 480}}}";
  std::string expected_result = "w=1280:h=720-w=640:h=480";
  RunTwoGetTwoGetUserMediaWithDifferentContraints(constraints1, constraints2,
                                                  expected_result);
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetUserMediaWithTooHighVideoConstraintsValues) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  int large_value = 99999;
  std::string call = GenerateGetUserMediaCall(kGetUserMediaAndExpectFailure,
                                              large_value,
                                              large_value,
                                              large_value,
                                              large_value,
                                              large_value,
                                              large_value);
  NavigateToURL(shell(), url);

  // TODO(perkj): A proper error code should be returned by gUM.
  EXPECT_EQ("TrackStartError", ExecuteJavascriptAndReturnResult(call));
}

// This test will make a simple getUserMedia page, verify that video is playing
// in a simple local <video>, and for a couple of seconds, collect some
// performance traces from VideoCaptureController colorspace conversion and
// potential resizing.
IN_PROC_BROWSER_TEST_P(
    WebRtcGetUserMediaBrowserTest,
    TraceVideoCaptureControllerPerformanceDuringGetUserMedia) {
  RunGetUserMediaAndCollectMeasures(
      10,
      "VideoCaptureController::OnIncomingCapturedData",
      "VideoCaptureController");
}

// This test calls getUserMedia and checks for aspect ratio behavior.
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       TestGetUserMediaAspectRatio4To3) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string constraints_4_3 = GenerateGetUserMediaCall(
      kGetUserMediaAndAnalyseAndStop, 640, 640, 480, 480, 10, 30);

  NavigateToURL(shell(), url);
  ASSERT_EQ("w=640:h=480",
            ExecuteJavascriptAndReturnResult(constraints_4_3));
}

// This test calls getUserMedia and checks for aspect ratio behavior.
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       TestGetUserMediaAspectRatio16To9) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string constraints_16_9 = GenerateGetUserMediaCall(
      kGetUserMediaAndAnalyseAndStop, 640, 640, 360, 360, 10, 30);

  NavigateToURL(shell(), url);
  ASSERT_EQ("w=640:h=360",
            ExecuteJavascriptAndReturnResult(constraints_16_9));
}

// This test calls getUserMedia and checks for aspect ratio behavior.
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       TestGetUserMediaAspectRatio1To1) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string constraints_1_1 = GenerateGetUserMediaCall(
      kGetUserMediaAndAnalyseAndStop, 320, 320, 320, 320, 10, 30);

  NavigateToURL(shell(), url);
  ASSERT_EQ("w=320:h=320",
            ExecuteJavascriptAndReturnResult(constraints_1_1));
}

namespace {

struct UserMediaSizes {
  int min_width;
  int max_width;
  int min_height;
  int max_height;
  int min_frame_rate;
  int max_frame_rate;
};

}  // namespace

class WebRtcConstraintsBrowserTest
    : public WebRtcContentBrowserTest,
      public testing::WithParamInterface<UserMediaSizes> {
 public:
  WebRtcConstraintsBrowserTest() : user_media_(GetParam()) {}
  const UserMediaSizes& user_media() const { return user_media_; }

 private:
  UserMediaSizes user_media_;
};

// This test calls getUserMedia in sequence with different constraints.
IN_PROC_BROWSER_TEST_P(WebRtcConstraintsBrowserTest, GetUserMediaConstraints) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string call = GenerateGetUserMediaCall(kGetUserMediaAndStop,
                                              user_media().min_width,
                                              user_media().max_width,
                                              user_media().min_height,
                                              user_media().max_height,
                                              user_media().min_frame_rate,
                                              user_media().max_frame_rate);
  DVLOG(1) << "Calling getUserMedia: " << call;
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk(call);
}

static const UserMediaSizes kAllUserMediaSizes[] = {
    {320, 320, 180, 180, 10, 30},
    {320, 320, 240, 240, 10, 30},
    {640, 640, 360, 360, 10, 30},
    {640, 640, 480, 480, 10, 30},
    {960, 960, 720, 720, 10, 30},
    {1280, 1280, 720, 720, 10, 30}};

INSTANTIATE_TEST_CASE_P(UserMedia,
                        WebRtcConstraintsBrowserTest,
                        testing::ValuesIn(kAllUserMediaSizes));

}  // namespace content

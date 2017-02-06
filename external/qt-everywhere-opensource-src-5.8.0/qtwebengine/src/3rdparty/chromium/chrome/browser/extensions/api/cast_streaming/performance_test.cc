// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/test/trace_event_analyzer.h"
#include "base/time/default_tick_clock.h"
#include "base/win/windows_version.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/tracing.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/features/base_feature_provider.h"
#include "extensions/common/features/complex_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "media/base/audio_bus.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/test/utility/audio_utility.h"
#include "media/cast/test/utility/barcode.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/in_process_receiver.h"
#include "media/cast/test/utility/standalone_cast_environment.h"
#include "media/cast/test/utility/udp_proxy.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"
#include "net/udp/udp_server_socket.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_switches.h"

namespace {

const char kExtensionId[] = "ddchlicdkolnonkihahngkmmmjnjlkkf";

// Skip a few events from the beginning.
static const size_t kSkipEvents = 3;

enum TestFlags {
  kUseGpu              = 1 << 0, // Only execute test if --enable-gpu was given
                                 // on the command line.  This is required for
                                 // tests that run on GPU.
  kDisableVsync        = 1 << 1, // Do not limit framerate to vertical refresh.
                                 // when on GPU, nor to 60hz when not on GPU.
  kSmallWindow         = 1 << 2, // 1 = 800x600, 0 = 2000x1000
  k24fps               = 1 << 3, // use 24 fps video
  k30fps               = 1 << 4, // use 30 fps video
  k60fps               = 1 << 5, // use 60 fps video
  kProxyWifi           = 1 << 6, // Run UDP through UDPProxy wifi profile
  kProxyBad            = 1 << 7, // Run UDP through UDPProxy bad profile
  kSlowClock           = 1 << 8, // Receiver clock is 10 seconds slow
  kFastClock           = 1 << 9, // Receiver clock is 10 seconds fast
};

class SkewedTickClock : public base::DefaultTickClock {
 public:
  explicit SkewedTickClock(const base::TimeDelta& delta) : delta_(delta) {
  }
  base::TimeTicks NowTicks() override {
    return DefaultTickClock::NowTicks() + delta_;
  }
 private:
  base::TimeDelta delta_;
};

class SkewedCastEnvironment : public media::cast::StandaloneCastEnvironment {
 public:
  explicit SkewedCastEnvironment(const base::TimeDelta& delta) :
      StandaloneCastEnvironment() {
    clock_.reset(new SkewedTickClock(delta));
  }

 protected:
  ~SkewedCastEnvironment() override {}
};

// We log one of these for each call to OnAudioFrame/OnVideoFrame.
struct TimeData {
  TimeData(uint16_t frame_no_, base::TimeTicks render_time_)
      : frame_no(frame_no_), render_time(render_time_) {}
  // The unit here is video frames, for audio data there can be duplicates.
  // This was decoded from the actual audio/video data.
  uint16_t frame_no;
  // This is when we should play this data, according to the sender.
  base::TimeTicks render_time;
};

// TODO(hubbe): Move to media/cast to use for offline log analysis.
class MeanAndError {
 public:
  MeanAndError() {}
  explicit MeanAndError(const std::vector<double>& values) {
    double sum = 0.0;
    double sqr_sum = 0.0;
    num_values = values.size();
    if (num_values) {
      for (size_t i = 0; i < num_values; i++) {
        sum += values[i];
        sqr_sum += values[i] * values[i];
      }
      mean = sum / num_values;
      std_dev = sqrt(std::max(0.0, num_values * sqr_sum - sum * sum)) /
          num_values;
    }
  }
  std::string AsString() const {
    return base::StringPrintf("%f,%f", mean, std_dev);
  }

  void Print(const std::string& measurement,
             const std::string& modifier,
             const std::string& trace,
             const std::string& unit) {
    if (num_values >= 20) {
      perf_test::PrintResultMeanAndError(measurement,
                                         modifier,
                                         trace,
                                         AsString(),
                                         unit,
                                         true);
    } else {
      LOG(ERROR) << "Not enough events for "
                 << measurement << modifier << " " << trace;
    }
  }

  size_t num_values;
  double mean;
  double std_dev;
};

// This function checks how smooth the data in |data| is.
// It computes the average error of deltas and the average delta.
// If data[x] == x * A + B, then this function returns zero.
// The unit is milliseconds.
static MeanAndError AnalyzeJitter(const std::vector<TimeData>& data) {
  CHECK_GT(data.size(), 1UL);
  VLOG(0) << "Jitter analyzis on " << data.size() << " values.";
  std::vector<double> deltas;
  double sum = 0.0;
  for (size_t i = 1; i < data.size(); i++) {
    double delta = (data[i].render_time -
                    data[i - 1].render_time).InMillisecondsF();
    deltas.push_back(delta);
    sum += delta;
  }
  double mean = sum / deltas.size();
  for (size_t i = 0; i < deltas.size(); i++) {
    deltas[i] = fabs(mean - deltas[i]);
  }

  return MeanAndError(deltas);
}

// An in-process Cast receiver that examines the audio/video frames being
// received and logs some data about each received audio/video frame.
class TestPatternReceiver : public media::cast::InProcessReceiver {
 public:
  explicit TestPatternReceiver(
      const scoped_refptr<media::cast::CastEnvironment>& cast_environment,
      const net::IPEndPoint& local_end_point)
      : InProcessReceiver(cast_environment,
                          local_end_point,
                          net::IPEndPoint(),
                          media::cast::GetDefaultAudioReceiverConfig(),
                          media::cast::GetDefaultVideoReceiverConfig()) {
  }

  typedef std::map<uint16_t, base::TimeTicks> TimeMap;

  // Build a map from frame ID (as encoded in the audio and video data)
  // to the rtp timestamp for that frame. Note that there will be multiple
  // audio frames which all have the same frame ID. When that happens we
  // want the minimum rtp timestamp, because that audio frame is supposed
  // to play at the same time that the corresponding image is presented.
  void MapFrameTimes(const std::vector<TimeData>& events, TimeMap* map) {
    for (size_t i = kSkipEvents; i < events.size(); i++) {
      base::TimeTicks& frame_tick = (*map)[events[i].frame_no];
      if (frame_tick.is_null()) {
        frame_tick = events[i].render_time;
      } else {
        frame_tick = std::min(events[i].render_time, frame_tick);
      }
    }
  }

  void Analyze(const std::string& name, const std::string& modifier) {
    // First, find the minimum rtp timestamp for each audio and video frame.
    // Note that the data encoded in the audio stream contains video frame
    // numbers. So in a 30-fps video stream, there will be 1/30s of "1", then
    // 1/30s of "2", etc.
    TimeMap audio_frame_times, video_frame_times;
    MapFrameTimes(audio_events_, &audio_frame_times);
    MapFrameTimes(video_events_, &video_frame_times);
    std::vector<double> deltas;
    for (TimeMap::const_iterator i = audio_frame_times.begin();
         i != audio_frame_times.end();
         ++i) {
      TimeMap::const_iterator j = video_frame_times.find(i->first);
      if (j != video_frame_times.end()) {
        deltas.push_back((i->second - j->second).InMillisecondsF());
      }
    }

    // Close to zero is better. (can be negative)
    MeanAndError(deltas).Print(name, modifier, "av_sync", "ms");
    // lower is better.
    AnalyzeJitter(audio_events_).Print(name, modifier, "audio_jitter", "ms");
    // lower is better.
    AnalyzeJitter(video_events_).Print(name, modifier, "video_jitter", "ms");
  }

 private:
  // Invoked by InProcessReceiver for each received audio frame.
  void OnAudioFrame(std::unique_ptr<media::AudioBus> audio_frame,
                    const base::TimeTicks& playout_time,
                    bool is_continuous) override {
    CHECK(cast_env()->CurrentlyOn(media::cast::CastEnvironment::MAIN));

    if (audio_frame->frames() <= 0) {
      NOTREACHED() << "OnAudioFrame called with no samples?!?";
      return;
    }

    // Note: This is the number of the video frame that this audio belongs to.
    uint16_t frame_no;
    if (media::cast::DecodeTimestamp(audio_frame->channel(0),
                                     audio_frame->frames(),
                                     &frame_no)) {
      audio_events_.push_back(TimeData(frame_no, playout_time));
    } else {
      VLOG(0) << "Failed to decode audio timestamp!";
    }
  }

  void OnVideoFrame(const scoped_refptr<media::VideoFrame>& video_frame,
                    const base::TimeTicks& render_time,
                    bool is_continuous) override {
    CHECK(cast_env()->CurrentlyOn(media::cast::CastEnvironment::MAIN));

    TRACE_EVENT_INSTANT1(
        "mirroring", "TestPatternReceiver::OnVideoFrame",
        TRACE_EVENT_SCOPE_THREAD,
        "render_time", render_time.ToInternalValue());

    uint16_t frame_no;
    if (media::cast::test::DecodeBarcode(video_frame, &frame_no)) {
      video_events_.push_back(TimeData(frame_no, render_time));
    } else {
      VLOG(0) << "Failed to decode barcode!";
    }
  }

  std::vector<TimeData> audio_events_;
  std::vector<TimeData> video_events_;

  DISALLOW_COPY_AND_ASSIGN(TestPatternReceiver);
};

class CastV2PerformanceTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<int> {
 public:
  CastV2PerformanceTest() {}

  bool HasFlag(TestFlags flag) const {
    return (GetParam() & flag) == flag;
  }

  bool IsGpuAvailable() const {
    return base::CommandLine::ForCurrentProcess()->HasSwitch("enable-gpu");
  }

  std::string GetSuffixForTestFlags() {
    std::string suffix;
    if (HasFlag(kUseGpu))
      suffix += "_gpu";
    if (HasFlag(kDisableVsync))
      suffix += "_novsync";
    if (HasFlag(kSmallWindow))
      suffix += "_small";
    if (HasFlag(k24fps))
      suffix += "_24fps";
    if (HasFlag(k30fps))
      suffix += "_30fps";
    if (HasFlag(k60fps))
      suffix += "_60fps";
    if (HasFlag(kProxyWifi))
      suffix += "_wifi";
    if (HasFlag(kProxyBad))
      suffix += "_bad";
    if (HasFlag(kSlowClock))
      suffix += "_slow";
    if (HasFlag(kFastClock))
      suffix += "_fast";
    return suffix;
  }

  int getfps() {
    if (HasFlag(k24fps))
      return 24;
    if (HasFlag(k30fps))
      return 30;
    if (HasFlag(k60fps))
      return 60;
    NOTREACHED();
    return 0;
  }

  net::IPEndPoint GetFreeLocalPort() {
    // Determine a unused UDP port for the in-process receiver to listen on.
    // Method: Bind a UDP socket on port 0, and then check which port the
    // operating system assigned to it.
    std::unique_ptr<net::UDPServerSocket> receive_socket(
        new net::UDPServerSocket(NULL, net::NetLog::Source()));
    receive_socket->AllowAddressReuse();
    CHECK_EQ(net::OK, receive_socket->Listen(
                          net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0)));
    net::IPEndPoint endpoint;
    CHECK_EQ(net::OK, receive_socket->GetLocalAddress(&endpoint));
    return endpoint;
  }

  void SetUp() override {
    EnablePixelOutput();
    ExtensionApiTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Some of the tests may launch http requests through JSON or AJAX
    // which causes a security error (cross domain request) when the page
    // is loaded from the local file system ( file:// ). The following switch
    // fixes that error.
    command_line->AppendSwitch(switches::kAllowFileAccessFromFiles);

    if (HasFlag(kSmallWindow)) {
      command_line->AppendSwitchASCII(switches::kWindowSize, "800,600");
    } else {
      command_line->AppendSwitchASCII(switches::kWindowSize, "2000,1500");
    }

    if (!HasFlag(kUseGpu))
      command_line->AppendSwitch(switches::kDisableGpu);

    if (HasFlag(kDisableVsync))
      command_line->AppendSwitch(switches::kDisableGpuVsync);

    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        kExtensionId);
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  void GetTraceEvents(trace_analyzer::TraceAnalyzer* analyzer,
                      const std::string& event_name,
                      trace_analyzer::TraceEventVector* events) {
    trace_analyzer::Query query =
        trace_analyzer::Query::EventNameIs(event_name) &&
        (trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_BEGIN) ||
         trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_ASYNC_BEGIN) ||
         trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_FLOW_BEGIN) ||
         trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_INSTANT) ||
         trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_COMPLETE));
    analyzer->FindEvents(query, events);
  }

  // The key contains the name of the argument and the argument.
  typedef std::pair<std::string, double> EventMapKey;
  typedef std::map<EventMapKey, const trace_analyzer::TraceEvent*> EventMap;

  // Make events findable by their arguments, for instance, if an
  // event has a "timestamp": 238724 argument, the map will contain
  // pair<"timestamp", 238724> -> &event.  All arguments are indexed.
  void IndexEvents(trace_analyzer::TraceAnalyzer* analyzer,
                   const std::string& event_name,
                   EventMap* event_map) {
    trace_analyzer::TraceEventVector events;
    GetTraceEvents(analyzer, event_name, &events);
    for (size_t i = 0; i < events.size(); i++) {
      std::map<std::string, double>::const_iterator j;
      for (j = events[i]->arg_numbers.begin();
           j != events[i]->arg_numbers.end();
           ++j) {
        (*event_map)[*j] = events[i];
      }
    }
  }

  // Look up an event in |event_map|. The return event will have the same
  // value for the argument |key_name| as |prev_event|. Note that if
  // the |key_name| is "time_delta", then we allow some fuzzy logic since
  // the time deltas are truncated to milliseconds in the code.
  const trace_analyzer::TraceEvent* FindNextEvent(
      const EventMap& event_map,
      std::vector<const trace_analyzer::TraceEvent*> prev_events,
      std::string key_name) {
    EventMapKey key;
    for (size_t i = prev_events.size(); i;) {
      --i;
      std::map<std::string, double>::const_iterator j =
          prev_events[i]->arg_numbers.find(key_name);
      if (j != prev_events[i]->arg_numbers.end()) {
        key = *j;
        break;
      }
    }
    EventMap::const_iterator i = event_map.lower_bound(key);
    if (i == event_map.end())
      return NULL;
    if (i->first.second == key.second)
      return i->second;
    if (key_name != "time_delta")
      return NULL;
    if (fabs(i->first.second - key.second) < 1000)
      return i->second;
    if (i == event_map.begin())
      return NULL;
    i--;
    if (fabs(i->first.second - key.second) < 1000)
      return i->second;
    return NULL;
  }

  // Given a vector of vector of data, extract the difference between
  // two columns (|col_a| and |col_b|) and output the result as a
  // performance metric.
  void OutputMeasurement(const std::string& test_name,
                         const std::vector<std::vector<double> > data,
                         const std::string& measurement_name,
                         int col_a,
                         int col_b) {
    std::vector<double> tmp;
    for (size_t i = 0; i < data.size(); i++) {
      tmp.push_back((data[i][col_b] - data[i][col_a]) / 1000.0);
    }
    return MeanAndError(tmp).Print(test_name,
                                   GetSuffixForTestFlags(),
                                   measurement_name,
                                   "ms");
  }

  // Analyzing latency is hard, because there is no unifying identifier for
  // frames throughout the code. At first, we have a capture timestamp, which
  // gets converted to a time delta, then back to a timestamp. Once it enters
  // the cast library it gets converted to an rtp_timestamp, and when it leaves
  // the cast library, all we have is the render_time.
  //
  // To be able to follow the frame throughout all this, we insert TRACE
  // calls that tracks each conversion as it happens. Then we extract all
  // these events and link them together.
  void AnalyzeLatency(const std::string& test_name,
                      trace_analyzer::TraceAnalyzer* analyzer) {
    EventMap onbuffer, sink, inserted, encoded, transmitted, decoded, done;
    IndexEvents(analyzer, "OnBufferReceived", &onbuffer);
    IndexEvents(analyzer, "MediaStreamVideoSink::OnVideoFrame", &sink);
    IndexEvents(analyzer, "InsertRawVideoFrame", &inserted);
    IndexEvents(analyzer, "VideoFrameEncoded", &encoded);
    IndexEvents(analyzer, "PullEncodedVideoFrame", &transmitted);
    IndexEvents(analyzer, "FrameDecoded", &decoded);
    IndexEvents(analyzer, "TestPatternReceiver::OnVideoFrame", &done);
    std::vector<std::pair<EventMap*, std::string> > event_maps;
    event_maps.push_back(std::make_pair(&onbuffer, "timestamp"));
    event_maps.push_back(std::make_pair(&sink, "time_delta"));
    event_maps.push_back(std::make_pair(&inserted, "timestamp"));
    event_maps.push_back(std::make_pair(&encoded, "rtp_timestamp"));
    event_maps.push_back(std::make_pair(&transmitted, "rtp_timestamp"));
    event_maps.push_back(std::make_pair(&decoded, "rtp_timestamp"));
    event_maps.push_back(std::make_pair(&done, "render_time"));

    trace_analyzer::TraceEventVector capture_events;
    GetTraceEvents(analyzer, "Capture" , &capture_events);
    EXPECT_GT(capture_events.size(), 0UL);
    std::vector<std::vector<double> > traced_frames;
    for (size_t i = kSkipEvents; i < capture_events.size(); i++) {
      std::vector<double> times;
      const trace_analyzer::TraceEvent *event = capture_events[i];
      times.push_back(event->timestamp);  // begin capture
      event = event->other_event;
      if (!event) {
        continue;
      }
      times.push_back(event->timestamp);  // end capture (with timestamp)
      std::vector<const trace_analyzer::TraceEvent*> prev_events;
      prev_events.push_back(event);
      for (size_t j = 0; j < event_maps.size(); j++) {
        event = FindNextEvent(*event_maps[j].first,
                              prev_events,
                              event_maps[j].second);
        if (!event) {
          break;
        }
        prev_events.push_back(event);
        times.push_back(event->timestamp);
      }
      if (event) {
        // Successfully traced frame from beginning to end
        traced_frames.push_back(times);
      }
    }

    // 0 = capture begin
    // 1 = capture end
    // 2 = onbuffer
    // 3 = sink
    // 4 = inserted
    // 5 = encoded
    // 6 = transmitted
    // 7 = decoded
    // 8 = done

    // Lower is better for all of these.
    OutputMeasurement(test_name, traced_frames, "total_latency", 0, 8);
    OutputMeasurement(test_name, traced_frames, "capture_duration", 0, 1);
    OutputMeasurement(test_name, traced_frames, "send_to_renderer", 1, 3);
    OutputMeasurement(test_name, traced_frames, "encode", 3, 5);
    OutputMeasurement(test_name, traced_frames, "transmit", 5, 6);
    OutputMeasurement(test_name, traced_frames, "decode", 6, 7);
    OutputMeasurement(test_name, traced_frames, "cast_latency", 3, 8);
  }

  MeanAndError AnalyzeTraceDistance(trace_analyzer::TraceAnalyzer* analyzer,
                                    const std::string& event_name) {
    trace_analyzer::TraceEventVector events;
    GetTraceEvents(analyzer, event_name, &events);

    std::vector<double> deltas;
    for (size_t i = kSkipEvents + 1; i < events.size(); ++i) {
      double delta_micros = events[i]->timestamp - events[i - 1]->timestamp;
      deltas.push_back(delta_micros / 1000.0);
    }
    return MeanAndError(deltas);
  }

  void RunTest(const std::string& test_name) {
    if (HasFlag(kUseGpu) && !IsGpuAvailable()) {
      LOG(WARNING) <<
          "Test skipped: requires gpu. Pass --enable-gpu on the command "
          "line if use of GPU is desired.";
      return;
    }

    ASSERT_EQ(1,
              (HasFlag(k24fps) ? 1 : 0) +
              (HasFlag(k30fps) ? 1 : 0) +
              (HasFlag(k60fps) ? 1 : 0));

    net::IPEndPoint receiver_end_point = GetFreeLocalPort();

    // Start the in-process receiver that examines audio/video for the expected
    // test patterns.
    base::TimeDelta delta = base::TimeDelta::FromSeconds(0);
    if (HasFlag(kFastClock)) {
      delta = base::TimeDelta::FromSeconds(10);
    }
    if (HasFlag(kSlowClock)) {
      delta = base::TimeDelta::FromSeconds(-10);
    }
    scoped_refptr<media::cast::StandaloneCastEnvironment> cast_environment(
        new SkewedCastEnvironment(delta));
    TestPatternReceiver* const receiver =
        new TestPatternReceiver(cast_environment, receiver_end_point);
    receiver->Start();

    std::unique_ptr<media::cast::test::UDPProxy> udp_proxy;
    if (HasFlag(kProxyWifi) || HasFlag(kProxyBad)) {
      net::IPEndPoint proxy_end_point = GetFreeLocalPort();
      if (HasFlag(kProxyWifi)) {
        udp_proxy = media::cast::test::UDPProxy::Create(
            proxy_end_point, receiver_end_point,
            media::cast::test::WifiNetwork(), media::cast::test::WifiNetwork(),
            NULL);
      } else if (HasFlag(kProxyBad)) {
        udp_proxy = media::cast::test::UDPProxy::Create(
            proxy_end_point, receiver_end_point,
            media::cast::test::BadNetwork(), media::cast::test::BadNetwork(),
            NULL);
      }
      receiver_end_point = proxy_end_point;
    }

    std::string json_events;
    ASSERT_TRUE(tracing::BeginTracing(
        "test_fps,mirroring,gpu.capture,cast_perf_test"));
    const std::string page_url = base::StringPrintf(
        "performance%d.html?port=%d",
        getfps(),
        receiver_end_point.port());
    ASSERT_TRUE(RunExtensionSubtest("cast_streaming", page_url)) << message_;
    ASSERT_TRUE(tracing::EndTracing(&json_events));
    receiver->Stop();

    // Stop all threads, removes the need for synchronization when analyzing
    // the data.
    cast_environment->Shutdown();
    std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer;
    analyzer.reset(trace_analyzer::TraceAnalyzer::Create(json_events));
    analyzer->AssociateAsyncBeginEndEvents();

    MeanAndError frame_data = AnalyzeTraceDistance(
        analyzer.get(),
        "OnSwapCompositorFrame");

    EXPECT_GT(frame_data.num_values, 0UL);
    // Lower is better.
    frame_data.Print(test_name,
                     GetSuffixForTestFlags(),
                     "time_between_frames",
                     "ms");

    // This prints out the average time between capture events.
    // As the capture frame rate is capped at 30fps, this score
    // cannot get any better than (lower) 33.33 ms.
    MeanAndError capture_data = AnalyzeTraceDistance(analyzer.get(), "Capture");
    // Lower is better.
    capture_data.Print(test_name,
                       GetSuffixForTestFlags(),
                       "time_between_captures",
                       "ms");

    receiver->Analyze(test_name, GetSuffixForTestFlags());

    AnalyzeLatency(test_name, analyzer.get());
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_P(CastV2PerformanceTest, Performance) {
  RunTest("CastV2Performance");
}

// Note: First argument is optional and intentionally left blank.
// (it's a prefix for the generated test cases)
INSTANTIATE_TEST_CASE_P(
    ,
    CastV2PerformanceTest,
    testing::Values(
        kUseGpu | k24fps,
        kUseGpu | k30fps,
        kUseGpu | k60fps,
        kUseGpu | k24fps | kDisableVsync,
        kUseGpu | k30fps | kProxyWifi,
        kUseGpu | k30fps | kProxyBad,
        kUseGpu | k30fps | kSlowClock,
        kUseGpu | k30fps | kFastClock));

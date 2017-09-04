// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_internals.h"

#include <memory>
#include <string>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "content/browser/webrtc/webrtc_internals_ui_observer.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

static const char kContraints[] = "c";
static const char kRtcConfiguration[] = "r";
static const char kUrl[] = "u";

class MockWebRtcInternalsProxy : public WebRTCInternalsUIObserver {
 public:
  MockWebRtcInternalsProxy() : loop_(nullptr) {}
  explicit MockWebRtcInternalsProxy(base::RunLoop* loop) : loop_(loop) {}

  const std::string& command() const {
    return command_;
  }

  base::Value* value() {
    return value_.get();
  }

 private:
  void OnUpdate(const char* command, const base::Value* value) override {
    command_ = command;
    value_.reset(value ? value->DeepCopy() : nullptr);
    if (loop_)
      loop_->Quit();
  }

  std::string command_;
  std::unique_ptr<base::Value> value_;
  base::RunLoop* loop_;
};

// Derived class for testing only.  Allows the tests to have their own instance
// for testing and control the period for which WebRTCInternals will bulk up
// updates (changes down from 500ms to 1ms).
class WebRTCInternalsForTest : public NON_EXPORTED_BASE(WebRTCInternals) {
 public:
  WebRTCInternalsForTest() : WebRTCInternals(1, true) {}
  ~WebRTCInternalsForTest() override {}
};

}  // namespace

class WebRtcInternalsTest : public testing::Test {
 public:
  WebRtcInternalsTest() : io_thread_(BrowserThread::UI, &io_loop_) {}

 protected:
  void VerifyString(const base::DictionaryValue* dict,
                    const std::string& key,
                    const std::string& expected) {
    std::string actual;
    EXPECT_TRUE(dict->GetString(key, &actual));
    EXPECT_EQ(expected, actual);
  }

  void VerifyInt(const base::DictionaryValue* dict,
                 const std::string& key,
                 int expected) {
    int actual;
    EXPECT_TRUE(dict->GetInteger(key, &actual));
    EXPECT_EQ(expected, actual);
  }

  void VerifyList(const base::DictionaryValue* dict,
                  const std::string& key,
                  const base::ListValue& expected) {
    const base::ListValue* actual = NULL;
    EXPECT_TRUE(dict->GetList(key, &actual));
    EXPECT_TRUE(expected.Equals(actual));
  }

  void VerifyGetUserMediaData(base::Value* actual_data,
                              int rid,
                              int pid,
                              const std::string& origin,
                              const std::string& audio,
                              const std::string& video) {
    base::DictionaryValue* dict = NULL;
    EXPECT_TRUE(actual_data->GetAsDictionary(&dict));

    VerifyInt(dict, "rid", rid);
    VerifyInt(dict, "pid", pid);
    VerifyString(dict, "origin", origin);
    VerifyString(dict, "audio", audio);
    VerifyString(dict, "video", video);
  }

  base::MessageLoop io_loop_;
  TestBrowserThread io_thread_;
};

TEST_F(WebRtcInternalsTest, AddRemoveObserver) {
  base::RunLoop loop;
  MockWebRtcInternalsProxy observer(&loop);
  WebRTCInternalsForTest webrtc_internals;
  webrtc_internals.AddObserver(&observer);

  webrtc_internals.RemoveObserver(&observer);
  // The observer should not get notified of this activity.
  webrtc_internals.OnAddPeerConnection(
      0, 3, 4, kUrl, kRtcConfiguration, kContraints);

  BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE,
      loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(5));
  loop.Run();

  EXPECT_EQ("", observer.command());

  webrtc_internals.OnRemovePeerConnection(3, 4);
}

TEST_F(WebRtcInternalsTest, SendAddPeerConnectionUpdate) {
  base::RunLoop loop;
  MockWebRtcInternalsProxy observer(&loop);
  WebRTCInternalsForTest webrtc_internals;
  webrtc_internals.AddObserver(&observer);
  webrtc_internals.OnAddPeerConnection(
      0, 1, 2, kUrl, kRtcConfiguration, kContraints);

  loop.Run();

  ASSERT_EQ("addPeerConnection", observer.command());

  base::DictionaryValue* dict = NULL;
  EXPECT_TRUE(observer.value()->GetAsDictionary(&dict));

  VerifyInt(dict, "pid", 1);
  VerifyInt(dict, "lid", 2);
  VerifyString(dict, "url", kUrl);
  VerifyString(dict, "rtcConfiguration", kRtcConfiguration);
  VerifyString(dict, "constraints", kContraints);

  webrtc_internals.RemoveObserver(&observer);
  webrtc_internals.OnRemovePeerConnection(1, 2);
}

TEST_F(WebRtcInternalsTest, SendRemovePeerConnectionUpdate) {
  base::RunLoop loop;
  MockWebRtcInternalsProxy observer(&loop);
  WebRTCInternalsForTest webrtc_internals;
  webrtc_internals.AddObserver(&observer);
  webrtc_internals.OnAddPeerConnection(
      0, 1, 2, kUrl, kRtcConfiguration, kContraints);
  webrtc_internals.OnRemovePeerConnection(1, 2);

  loop.Run();

  ASSERT_EQ("removePeerConnection", observer.command());

  base::DictionaryValue* dict = NULL;
  EXPECT_TRUE(observer.value()->GetAsDictionary(&dict));

  VerifyInt(dict, "pid", 1);
  VerifyInt(dict, "lid", 2);

  webrtc_internals.RemoveObserver(&observer);
}

TEST_F(WebRtcInternalsTest, SendUpdatePeerConnectionUpdate) {
  base::RunLoop loop;
  MockWebRtcInternalsProxy observer(&loop);
  WebRTCInternalsForTest webrtc_internals;
  webrtc_internals.AddObserver(&observer);
  webrtc_internals.OnAddPeerConnection(
      0, 1, 2, kUrl, kRtcConfiguration, kContraints);

  const std::string update_type = "fakeType";
  const std::string update_value = "fakeValue";
  webrtc_internals.OnUpdatePeerConnection(
      1, 2, update_type, update_value);

  loop.Run();

  ASSERT_EQ("updatePeerConnection", observer.command());

  base::DictionaryValue* dict = NULL;
  EXPECT_TRUE(observer.value()->GetAsDictionary(&dict));

  VerifyInt(dict, "pid", 1);
  VerifyInt(dict, "lid", 2);
  VerifyString(dict, "type", update_type);
  VerifyString(dict, "value", update_value);

  std::string time;
  EXPECT_TRUE(dict->GetString("time", &time));
  EXPECT_FALSE(time.empty());

  webrtc_internals.OnRemovePeerConnection(1, 2);
  webrtc_internals.RemoveObserver(&observer);
}

TEST_F(WebRtcInternalsTest, AddGetUserMedia) {
  base::RunLoop loop;
  MockWebRtcInternalsProxy observer(&loop);
  WebRTCInternalsForTest webrtc_internals;

  // Add one observer before "getUserMedia".
  webrtc_internals.AddObserver(&observer);

  const int rid = 1;
  const int pid = 2;
  const std::string audio_constraint = "aaa";
  const std::string video_constraint = "vvv";
  webrtc_internals.OnGetUserMedia(
      rid, pid, kUrl, true, true, audio_constraint, video_constraint);

  loop.Run();

  ASSERT_EQ("addGetUserMedia", observer.command());
  VerifyGetUserMediaData(
      observer.value(), rid, pid, kUrl, audio_constraint, video_constraint);

  webrtc_internals.RemoveObserver(&observer);
}

TEST_F(WebRtcInternalsTest, SendAllUpdateWithGetUserMedia) {
  const int rid = 1;
  const int pid = 2;
  const std::string audio_constraint = "aaa";
  const std::string video_constraint = "vvv";
  WebRTCInternalsForTest webrtc_internals;
  webrtc_internals.OnGetUserMedia(
      rid, pid, kUrl, true, true, audio_constraint, video_constraint);

  MockWebRtcInternalsProxy observer;
  // Add one observer after "getUserMedia".
  webrtc_internals.AddObserver(&observer);
  webrtc_internals.UpdateObserver(&observer);

  EXPECT_EQ("addGetUserMedia", observer.command());
  VerifyGetUserMediaData(
      observer.value(), rid, pid, kUrl, audio_constraint, video_constraint);

  webrtc_internals.RemoveObserver(&observer);
}

TEST_F(WebRtcInternalsTest, SendAllUpdatesWithPeerConnectionUpdate) {
  const int rid = 0, pid = 1, lid = 2;
  const std::string update_type = "fakeType";
  const std::string update_value = "fakeValue";

  WebRTCInternalsForTest webrtc_internals;

  webrtc_internals.OnAddPeerConnection(
      rid, pid, lid, kUrl, kRtcConfiguration, kContraints);
  webrtc_internals.OnUpdatePeerConnection(
      pid, lid, update_type, update_value);

  MockWebRtcInternalsProxy observer;
  webrtc_internals.AddObserver(&observer);

  webrtc_internals.UpdateObserver(&observer);

  EXPECT_EQ("updateAllPeerConnections", observer.command());
  ASSERT_TRUE(observer.value());

  base::ListValue* list = NULL;
  EXPECT_TRUE(observer.value()->GetAsList(&list));
  EXPECT_EQ(1U, list->GetSize());

  base::DictionaryValue* dict = NULL;
  EXPECT_TRUE((*list->begin())->GetAsDictionary(&dict));

  VerifyInt(dict, "rid", rid);
  VerifyInt(dict, "pid", pid);
  VerifyInt(dict, "lid", lid);
  VerifyString(dict, "url", kUrl);
  VerifyString(dict, "rtcConfiguration", kRtcConfiguration);
  VerifyString(dict, "constraints", kContraints);

  base::ListValue* log = NULL;
  EXPECT_TRUE(dict->GetList("log", &log));
  EXPECT_EQ(1U, log->GetSize());

  EXPECT_TRUE((*log->begin())->GetAsDictionary(&dict));
  VerifyString(dict, "type", update_type);
  VerifyString(dict, "value", update_value);
  std::string time;
  EXPECT_TRUE(dict->GetString("time", &time));
  EXPECT_FALSE(time.empty());
}

TEST_F(WebRtcInternalsTest, OnAddStats) {
  const int rid = 0, pid = 1, lid = 2;
  base::RunLoop loop;
  MockWebRtcInternalsProxy observer(&loop);
  WebRTCInternalsForTest webrtc_internals;
  webrtc_internals.AddObserver(&observer);
  webrtc_internals.OnAddPeerConnection(
      rid, pid, lid, kUrl, kRtcConfiguration, kContraints);

  base::ListValue list;
  list.AppendString("xxx");
  list.AppendString("yyy");
  webrtc_internals.OnAddStats(pid, lid, list);

  loop.Run();

  EXPECT_EQ("addStats", observer.command());
  ASSERT_TRUE(observer.value());

  base::DictionaryValue* dict = NULL;
  EXPECT_TRUE(observer.value()->GetAsDictionary(&dict));

  VerifyInt(dict, "pid", pid);
  VerifyInt(dict, "lid", lid);
  VerifyList(dict, "reports", list);
}

TEST_F(WebRtcInternalsTest, AudioDebugRecordingsFileSelectionCanceled) {
  base::RunLoop loop;

  MockWebRtcInternalsProxy observer(&loop);
  WebRTCInternalsForTest webrtc_internals;

  webrtc_internals.AddObserver(&observer);
  webrtc_internals.FileSelectionCanceled(nullptr);

  loop.Run();

  EXPECT_EQ("audioDebugRecordingsFileSelectionCancelled", observer.command());
  EXPECT_EQ(nullptr, observer.value());
}

TEST_F(WebRtcInternalsTest, PowerSaveBlock) {
  int kRenderProcessId = 1;
  int pid = 1;
  int lid[] = {1, 2, 3};

  WebRTCInternalsForTest webrtc_internals;

  // Add a few peer connections.
  EXPECT_EQ(0, webrtc_internals.num_open_connections());
  EXPECT_FALSE(webrtc_internals.IsPowerSavingBlocked());
  webrtc_internals.OnAddPeerConnection(kRenderProcessId, pid, lid[0], kUrl,
                                       kRtcConfiguration, kContraints);
  EXPECT_EQ(1, webrtc_internals.num_open_connections());
  EXPECT_TRUE(webrtc_internals.IsPowerSavingBlocked());

  webrtc_internals.OnAddPeerConnection(kRenderProcessId, pid, lid[1], kUrl,
                                       kRtcConfiguration, kContraints);
  EXPECT_EQ(2, webrtc_internals.num_open_connections());
  EXPECT_TRUE(webrtc_internals.IsPowerSavingBlocked());

  webrtc_internals.OnAddPeerConnection(kRenderProcessId, pid, lid[2], kUrl,
                                       kRtcConfiguration, kContraints);
  EXPECT_EQ(3, webrtc_internals.num_open_connections());
  EXPECT_TRUE(webrtc_internals.IsPowerSavingBlocked());

  // Remove a peer connection without closing it first.
  webrtc_internals.OnRemovePeerConnection(pid, lid[2]);
  EXPECT_EQ(2, webrtc_internals.num_open_connections());
  EXPECT_TRUE(webrtc_internals.IsPowerSavingBlocked());

  // Close the remaining peer connections.
  webrtc_internals.OnUpdatePeerConnection(pid, lid[1], "stop", std::string());
  EXPECT_EQ(1, webrtc_internals.num_open_connections());
  EXPECT_TRUE(webrtc_internals.IsPowerSavingBlocked());

  webrtc_internals.OnUpdatePeerConnection(pid, lid[0], "stop", std::string());
  EXPECT_EQ(0, webrtc_internals.num_open_connections());
  EXPECT_FALSE(webrtc_internals.IsPowerSavingBlocked());

  // Remove the remaining peer connections.
  webrtc_internals.OnRemovePeerConnection(pid, lid[1]);
  EXPECT_EQ(0, webrtc_internals.num_open_connections());
  EXPECT_FALSE(webrtc_internals.IsPowerSavingBlocked());

  webrtc_internals.OnRemovePeerConnection(pid, lid[0]);
  EXPECT_EQ(0, webrtc_internals.num_open_connections());
  EXPECT_FALSE(webrtc_internals.IsPowerSavingBlocked());
}

}  // namespace content

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "content/browser/media/webrtc_internals.h"
#include "content/browser/media/webrtc_internals_ui_observer.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

static const std::string kContraints = "c";
static const std::string kServers = "s";
static const std::string kUrl = "u";

class MockWebRTCInternalsProxy : public WebRTCInternalsUIObserver {
 public:
  virtual void OnUpdate(const std::string& command,
                        const base::Value* value) OVERRIDE {
    command_ = command;
    if (value)
      value_.reset(value->DeepCopy());
  }

  std::string command() {
    return command_;
  }

  base::Value* value() {
    return value_.get();
  }

 private:
   std::string command_;
   scoped_ptr<base::Value> value_;
};

class WebRTCInternalsTest : public testing::Test {
 public:
  WebRTCInternalsTest() : io_thread_(BrowserThread::UI, &io_loop_) {
    WebRTCInternals::GetInstance()->ResetForTesting();
  }

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

}  // namespace

TEST_F(WebRTCInternalsTest, AddRemoveObserver) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  WebRTCInternals::GetInstance()->AddObserver(observer.get());
  WebRTCInternals::GetInstance()->RemoveObserver(observer.get());
  WebRTCInternals::GetInstance()->OnAddPeerConnection(
      0, 3, 4, kUrl, kServers, kContraints);
  EXPECT_EQ("", observer->command());

  WebRTCInternals::GetInstance()->OnRemovePeerConnection(3, 4);
}

TEST_F(WebRTCInternalsTest, SendAddPeerConnectionUpdate) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  WebRTCInternals::GetInstance()->AddObserver(observer.get());
  WebRTCInternals::GetInstance()->OnAddPeerConnection(
      0, 1, 2, kUrl, kServers, kContraints);
  EXPECT_EQ("addPeerConnection", observer->command());

  base::DictionaryValue* dict = NULL;
  EXPECT_TRUE(observer->value()->GetAsDictionary(&dict));

  VerifyInt(dict, "pid", 1);
  VerifyInt(dict, "lid", 2);
  VerifyString(dict, "url", kUrl);
  VerifyString(dict, "servers", kServers);
  VerifyString(dict, "constraints", kContraints);

  WebRTCInternals::GetInstance()->RemoveObserver(observer.get());
  WebRTCInternals::GetInstance()->OnRemovePeerConnection(1, 2);
}

TEST_F(WebRTCInternalsTest, SendRemovePeerConnectionUpdate) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  WebRTCInternals::GetInstance()->AddObserver(observer.get());
  WebRTCInternals::GetInstance()->OnAddPeerConnection(
      0, 1, 2, kUrl, kServers, kContraints);
  WebRTCInternals::GetInstance()->OnRemovePeerConnection(1, 2);
  EXPECT_EQ("removePeerConnection", observer->command());

  base::DictionaryValue* dict = NULL;
  EXPECT_TRUE(observer->value()->GetAsDictionary(&dict));

  VerifyInt(dict, "pid", 1);
  VerifyInt(dict, "lid", 2);

  WebRTCInternals::GetInstance()->RemoveObserver(observer.get());
}

TEST_F(WebRTCInternalsTest, SendUpdatePeerConnectionUpdate) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(
      new MockWebRTCInternalsProxy());
  WebRTCInternals::GetInstance()->AddObserver(observer.get());
  WebRTCInternals::GetInstance()->OnAddPeerConnection(
      0, 1, 2, kUrl, kServers, kContraints);

  const std::string update_type = "fakeType";
  const std::string update_value = "fakeValue";
  WebRTCInternals::GetInstance()->OnUpdatePeerConnection(
      1, 2, update_type, update_value);

  EXPECT_EQ("updatePeerConnection", observer->command());

  base::DictionaryValue* dict = NULL;
  EXPECT_TRUE(observer->value()->GetAsDictionary(&dict));

  VerifyInt(dict, "pid", 1);
  VerifyInt(dict, "lid", 2);
  VerifyString(dict, "type", update_type);
  VerifyString(dict, "value", update_value);

  WebRTCInternals::GetInstance()->OnRemovePeerConnection(1, 2);
  WebRTCInternals::GetInstance()->RemoveObserver(observer.get());
}

TEST_F(WebRTCInternalsTest, AddGetUserMedia) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(new MockWebRTCInternalsProxy());

  // Add one observer before "getUserMedia".
  WebRTCInternals::GetInstance()->AddObserver(observer.get());

  const int rid = 1;
  const int pid = 2;
  const std::string audio_constraint = "aaa";
  const std::string video_constraint = "vvv";
  WebRTCInternals::GetInstance()->OnGetUserMedia(
      rid, pid, kUrl, true, true, audio_constraint, video_constraint);

  EXPECT_EQ("addGetUserMedia", observer->command());
  VerifyGetUserMediaData(
      observer->value(), rid, pid, kUrl, audio_constraint, video_constraint);

  WebRTCInternals::GetInstance()->RemoveObserver(observer.get());
}

TEST_F(WebRTCInternalsTest, SendAllUpdateWithGetUserMedia) {
  const int rid = 1;
  const int pid = 2;
  const std::string audio_constraint = "aaa";
  const std::string video_constraint = "vvv";
  WebRTCInternals::GetInstance()->OnGetUserMedia(
      rid, pid, kUrl, true, true, audio_constraint, video_constraint);

  scoped_ptr<MockWebRTCInternalsProxy> observer(new MockWebRTCInternalsProxy());
  // Add one observer after "getUserMedia".
  WebRTCInternals::GetInstance()->AddObserver(observer.get());
  WebRTCInternals::GetInstance()->UpdateObserver(observer.get());

  EXPECT_EQ("addGetUserMedia", observer->command());
  VerifyGetUserMediaData(
      observer->value(), rid, pid, kUrl, audio_constraint, video_constraint);

  WebRTCInternals::GetInstance()->RemoveObserver(observer.get());
}

TEST_F(WebRTCInternalsTest, SendAllUpdatesWithPeerConnectionUpdate) {
  const int rid = 0, pid = 1, lid = 2;
  const std::string update_type = "fakeType";
  const std::string update_value = "fakeValue";

  WebRTCInternals::GetInstance()->OnAddPeerConnection(
      rid, pid, lid, kUrl, kServers, kContraints);
  WebRTCInternals::GetInstance()->OnUpdatePeerConnection(
      pid, lid, update_type, update_value);

  scoped_ptr<MockWebRTCInternalsProxy> observer(new MockWebRTCInternalsProxy());
  WebRTCInternals::GetInstance()->AddObserver(observer.get());

  WebRTCInternals::GetInstance()->UpdateObserver(observer.get());

  EXPECT_EQ("updateAllPeerConnections", observer->command());

  base::ListValue* list = NULL;
  EXPECT_TRUE(observer->value()->GetAsList(&list));
  EXPECT_EQ(1U, list->GetSize());

  base::DictionaryValue* dict = NULL;
  EXPECT_TRUE((*list->begin())->GetAsDictionary(&dict));

  VerifyInt(dict, "rid", rid);
  VerifyInt(dict, "pid", pid);
  VerifyInt(dict, "lid", lid);
  VerifyString(dict, "url", kUrl);
  VerifyString(dict, "servers", kServers);
  VerifyString(dict, "constraints", kContraints);

  base::ListValue* log = NULL;
  EXPECT_TRUE(dict->GetList("log", &log));
  EXPECT_EQ(1U, log->GetSize());

  EXPECT_TRUE((*log->begin())->GetAsDictionary(&dict));
  VerifyString(dict, "type", update_type);
  VerifyString(dict, "value", update_value);
}

TEST_F(WebRTCInternalsTest, OnAddStats) {
  const int rid = 0, pid = 1, lid = 2;
  scoped_ptr<MockWebRTCInternalsProxy> observer(new MockWebRTCInternalsProxy());
  WebRTCInternals::GetInstance()->AddObserver(observer.get());
  WebRTCInternals::GetInstance()->OnAddPeerConnection(
      rid, pid, lid, kUrl, kServers, kContraints);

  base::ListValue list;
  list.AppendString("xxx");
  list.AppendString("yyy");
  WebRTCInternals::GetInstance()->OnAddStats(pid, lid, list);

  EXPECT_EQ("addStats", observer->command());
  base::DictionaryValue* dict = NULL;
  EXPECT_TRUE(observer->value()->GetAsDictionary(&dict));

  VerifyInt(dict, "pid", pid);
  VerifyInt(dict, "lid", lid);
  VerifyList(dict, "reports", list);
}

TEST_F(WebRTCInternalsTest, AecRecordingFileSelectionCanceled) {
  scoped_ptr<MockWebRTCInternalsProxy> observer(new MockWebRTCInternalsProxy());
  WebRTCInternals::GetInstance()->AddObserver(observer.get());
  WebRTCInternals::GetInstance()->FileSelectionCanceled(NULL);
  EXPECT_EQ("aecRecordingFileSelectionCancelled", observer->command());
  EXPECT_EQ(NULL, observer->value());
}

}  // namespace content

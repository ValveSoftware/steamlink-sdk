// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/buffered_spdy_framer.h"

#include "net/spdy/spdy_test_util_common.h"
#include "testing/platform_test.h"

namespace net {

namespace {

class TestBufferedSpdyVisitor : public BufferedSpdyFramerVisitorInterface {
 public:
  explicit TestBufferedSpdyVisitor(SpdyMajorVersion spdy_version)
      : buffered_spdy_framer_(spdy_version, true),
        error_count_(0),
        setting_count_(0),
        syn_frame_count_(0),
        syn_reply_frame_count_(0),
        headers_frame_count_(0),
        push_promise_frame_count_(0),
        header_stream_id_(-1),
        promised_stream_id_(-1) {
  }

  virtual void OnError(SpdyFramer::SpdyError error_code) OVERRIDE {
    LOG(INFO) << "SpdyFramer Error: " << error_code;
    error_count_++;
  }

  virtual void OnStreamError(
      SpdyStreamId stream_id,
      const std::string& description) OVERRIDE {
    LOG(INFO) << "SpdyFramer Error on stream: " << stream_id  << " "
              << description;
    error_count_++;
  }

  virtual void OnSynStream(SpdyStreamId stream_id,
                           SpdyStreamId associated_stream_id,
                           SpdyPriority priority,
                           bool fin,
                           bool unidirectional,
                           const SpdyHeaderBlock& headers) OVERRIDE {
    header_stream_id_ = stream_id;
    EXPECT_NE(header_stream_id_, SpdyFramer::kInvalidStream);
    syn_frame_count_++;
    headers_ = headers;
  }

  virtual void OnSynReply(SpdyStreamId stream_id,
                          bool fin,
                          const SpdyHeaderBlock& headers) OVERRIDE {
    header_stream_id_ = stream_id;
    EXPECT_NE(header_stream_id_, SpdyFramer::kInvalidStream);
    syn_reply_frame_count_++;
    headers_ = headers;
  }

  virtual void OnHeaders(SpdyStreamId stream_id,
                         bool fin,
                         const SpdyHeaderBlock& headers) OVERRIDE {
    header_stream_id_ = stream_id;
    EXPECT_NE(header_stream_id_, SpdyFramer::kInvalidStream);
    headers_frame_count_++;
    headers_ = headers;
  }

  virtual void OnDataFrameHeader(SpdyStreamId stream_id,
                                 size_t length,
                                 bool fin) OVERRIDE {
    ADD_FAILURE() << "Unexpected OnDataFrameHeader call.";
  }

  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len,
                                 bool fin) OVERRIDE {
    LOG(FATAL) << "Unexpected OnStreamFrameData call.";
  }

  virtual void OnSettings(bool clear_persisted) OVERRIDE {}

  virtual void OnSetting(SpdySettingsIds id,
                         uint8 flags,
                         uint32 value) OVERRIDE {
    setting_count_++;
  }

  virtual void OnPing(SpdyPingId unique_id, bool is_ack) OVERRIDE {}

  virtual void OnRstStream(SpdyStreamId stream_id,
                           SpdyRstStreamStatus status) OVERRIDE {
  }

  virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                        SpdyGoAwayStatus status) OVERRIDE {
  }

  bool OnCredentialFrameData(const char*, size_t) {
    LOG(FATAL) << "Unexpected OnCredentialFrameData call.";
    return false;
  }

  void OnDataFrameHeader(const SpdyFrame* frame) {
    LOG(FATAL) << "Unexpected OnDataFrameHeader call.";
  }

  void OnRstStream(const SpdyFrame& frame) {}
  void OnGoAway(const SpdyFrame& frame) {}
  void OnPing(const SpdyFrame& frame) {}
  virtual void OnWindowUpdate(SpdyStreamId stream_id,
                              uint32 delta_window_size) OVERRIDE {}

  virtual void OnPushPromise(SpdyStreamId stream_id,
                             SpdyStreamId promised_stream_id,
                             const SpdyHeaderBlock& headers) OVERRIDE {
    header_stream_id_ = stream_id;
    EXPECT_NE(header_stream_id_, SpdyFramer::kInvalidStream);
    push_promise_frame_count_++;
    promised_stream_id_ = promised_stream_id;
    EXPECT_NE(promised_stream_id_, SpdyFramer::kInvalidStream);
    headers_ = headers;
  }

  void OnCredential(const SpdyFrame& frame) {}

  // Convenience function which runs a framer simulation with particular input.
  void SimulateInFramer(const unsigned char* input, size_t size) {
    buffered_spdy_framer_.set_visitor(this);
    size_t input_remaining = size;
    const char* input_ptr = reinterpret_cast<const char*>(input);
    while (input_remaining > 0 &&
           buffered_spdy_framer_.error_code() == SpdyFramer::SPDY_NO_ERROR) {
      // To make the tests more interesting, we feed random (amd small) chunks
      // into the framer.  This simulates getting strange-sized reads from
      // the socket.
      const size_t kMaxReadSize = 32;
      size_t bytes_read =
          (rand() % std::min(input_remaining, kMaxReadSize)) + 1;
      size_t bytes_processed =
          buffered_spdy_framer_.ProcessInput(input_ptr, bytes_read);
      input_remaining -= bytes_processed;
      input_ptr += bytes_processed;
    }
  }

  BufferedSpdyFramer buffered_spdy_framer_;

  // Counters from the visitor callbacks.
  int error_count_;
  int setting_count_;
  int syn_frame_count_;
  int syn_reply_frame_count_;
  int headers_frame_count_;
  int push_promise_frame_count_;

  // Header block streaming state:
  SpdyStreamId header_stream_id_;
  SpdyStreamId promised_stream_id_;

  // Headers from OnSyn, OnSynReply, OnHeaders and OnPushPromise for
  // verification.
  SpdyHeaderBlock headers_;
};

}  // namespace

class BufferedSpdyFramerTest
    : public PlatformTest,
      public ::testing::WithParamInterface<NextProto> {
 protected:
  // Returns true if the two header blocks have equivalent content.
  bool CompareHeaderBlocks(const SpdyHeaderBlock* expected,
                           const SpdyHeaderBlock* actual) {
    if (expected->size() != actual->size()) {
      LOG(ERROR) << "Expected " << expected->size() << " headers; actually got "
                 << actual->size() << ".";
      return false;
    }
    for (SpdyHeaderBlock::const_iterator it = expected->begin();
         it != expected->end();
         ++it) {
      SpdyHeaderBlock::const_iterator it2 = actual->find(it->first);
      if (it2 == actual->end()) {
        LOG(ERROR) << "Expected header name '" << it->first << "'.";
        return false;
      }
      if (it->second.compare(it2->second) != 0) {
        LOG(ERROR) << "Expected header named '" << it->first
                   << "' to have a value of '" << it->second
                   << "'. The actual value received was '" << it2->second
                   << "'.";
        return false;
      }
    }
    return true;
  }

  SpdyMajorVersion spdy_version() {
    return NextProtoToSpdyMajorVersion(GetParam());
  }
};

INSTANTIATE_TEST_CASE_P(
    NextProto,
    BufferedSpdyFramerTest,
    testing::Values(kProtoDeprecatedSPDY2,
                    kProtoSPDY3, kProtoSPDY31, kProtoSPDY4));

TEST_P(BufferedSpdyFramerTest, OnSetting) {
  SpdyFramer framer(spdy_version());
  SpdySettingsIR settings_ir;
  settings_ir.AddSetting(SETTINGS_INITIAL_WINDOW_SIZE, false, false, 2);
  settings_ir.AddSetting(SETTINGS_MAX_CONCURRENT_STREAMS, false, false, 3);
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSettings(settings_ir));
  TestBufferedSpdyVisitor visitor(spdy_version());

  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(2, visitor.setting_count_);
}

TEST_P(BufferedSpdyFramerTest, ReadSynStreamHeaderBlock) {
  SpdyHeaderBlock headers;
  headers["aa"] = "vv";
  headers["bb"] = "ww";
  BufferedSpdyFramer framer(spdy_version(), true);
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreateSynStream(1,                        // stream_id
                             0,                        // associated_stream_id
                             1,                        // priority
                             CONTROL_FLAG_NONE,
                             &headers));
  EXPECT_TRUE(control_frame.get() != NULL);

  TestBufferedSpdyVisitor visitor(spdy_version());
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.get()->data()),
      control_frame.get()->size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.push_promise_frame_count_);
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
}

TEST_P(BufferedSpdyFramerTest, ReadSynReplyHeaderBlock) {
  SpdyHeaderBlock headers;
  headers["alpha"] = "beta";
  headers["gamma"] = "delta";
  BufferedSpdyFramer framer(spdy_version(), true);
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreateSynReply(1,                        // stream_id
                            CONTROL_FLAG_NONE,
                            &headers));
  EXPECT_TRUE(control_frame.get() != NULL);

  TestBufferedSpdyVisitor visitor(spdy_version());
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.get()->data()),
      control_frame.get()->size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(0, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.push_promise_frame_count_);
  if(spdy_version() < SPDY4) {
    EXPECT_EQ(1, visitor.syn_reply_frame_count_);
    EXPECT_EQ(0, visitor.headers_frame_count_);
  } else {
    EXPECT_EQ(0, visitor.syn_reply_frame_count_);
    EXPECT_EQ(1, visitor.headers_frame_count_);
  }
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
}

TEST_P(BufferedSpdyFramerTest, ReadHeadersHeaderBlock) {
  SpdyHeaderBlock headers;
  headers["alpha"] = "beta";
  headers["gamma"] = "delta";
  BufferedSpdyFramer framer(spdy_version(), true);
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreateHeaders(1,                        // stream_id
                           CONTROL_FLAG_NONE,
                           &headers));
  EXPECT_TRUE(control_frame.get() != NULL);

  TestBufferedSpdyVisitor visitor(spdy_version());
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.get()->data()),
      control_frame.get()->size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(0, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.push_promise_frame_count_);
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
}

TEST_P(BufferedSpdyFramerTest, ReadPushPromiseHeaderBlock) {
  if (spdy_version() < SPDY4)
    return;
  SpdyHeaderBlock headers;
  headers["alpha"] = "beta";
  headers["gamma"] = "delta";
  BufferedSpdyFramer framer(spdy_version(), true);
  scoped_ptr<SpdyFrame> control_frame(
      framer.CreatePushPromise(1, 2, &headers));
  EXPECT_TRUE(control_frame.get() != NULL);

  TestBufferedSpdyVisitor visitor(spdy_version());
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.get()->data()),
      control_frame.get()->size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(0, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);
  EXPECT_EQ(1, visitor.push_promise_frame_count_);
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
  EXPECT_EQ(1u, visitor.header_stream_id_);
  EXPECT_EQ(2u, visitor.promised_stream_id_);
}

}  // namespace net

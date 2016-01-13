// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iostream>
#include <limits>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/spdy/hpack_output_stream.h"
#include "net/spdy/mock_spdy_framer_visitor.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_frame_reader.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

using base::StringPiece;
using std::string;
using std::max;
using std::min;
using std::numeric_limits;
using testing::ElementsAre;
using testing::Pair;
using testing::_;

namespace net {

namespace test {

static const size_t kMaxDecompressedSize = 1024;

class MockDebugVisitor : public SpdyFramerDebugVisitorInterface {
 public:
  MOCK_METHOD4(OnSendCompressedFrame, void(SpdyStreamId stream_id,
                                           SpdyFrameType type,
                                           size_t payload_len,
                                           size_t frame_len));

  MOCK_METHOD3(OnReceiveCompressedFrame, void(SpdyStreamId stream_id,
                                              SpdyFrameType type,
                                              size_t frame_len));
};

class SpdyFramerTestUtil {
 public:
  // Decompress a single frame using the decompression context held by
  // the SpdyFramer.  The implemention is meant for use only in tests
  // and will CHECK fail if the input is anything other than a single,
  // well-formed compressed frame.
  //
  // Returns a new decompressed SpdyFrame.
  template<class SpdyFrameType> static SpdyFrame* DecompressFrame(
      SpdyFramer* framer, const SpdyFrameType& frame) {
    DecompressionVisitor visitor(framer->protocol_version());
    framer->set_visitor(&visitor);
    CHECK_EQ(frame.size(), framer->ProcessInput(frame.data(), frame.size()));
    CHECK_EQ(SpdyFramer::SPDY_RESET, framer->state());
    framer->set_visitor(NULL);

    char* buffer = visitor.ReleaseBuffer();
    CHECK(buffer != NULL);
    SpdyFrame* decompressed_frame = new SpdyFrame(buffer, visitor.size(), true);
    SetFrameLength(decompressed_frame,
                   visitor.size() - framer->GetControlFrameHeaderSize(),
                   framer->protocol_version());
    return decompressed_frame;
  }

  class DecompressionVisitor : public SpdyFramerVisitorInterface {
   public:
    explicit DecompressionVisitor(SpdyMajorVersion version)
        : version_(version), size_(0), finished_(false) {}

    void ResetBuffer() {
      CHECK(buffer_.get() == NULL);
      CHECK_EQ(0u, size_);
      CHECK(!finished_);
      buffer_.reset(new char[kMaxDecompressedSize]);
    }

    virtual void OnError(SpdyFramer* framer) OVERRIDE { LOG(FATAL); }
    virtual void OnDataFrameHeader(SpdyStreamId stream_id,
                                   size_t length,
                                   bool fin) OVERRIDE {
      LOG(FATAL) << "Unexpected data frame header";
    }
    virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                   const char* data,
                                   size_t len,
                                   bool fin) OVERRIDE {
      LOG(FATAL);
    }

    virtual bool OnControlFrameHeaderData(SpdyStreamId stream_id,
                                          const char* header_data,
                                          size_t len) OVERRIDE {
      CHECK(buffer_.get() != NULL);
      CHECK_GE(kMaxDecompressedSize, size_ + len);
      CHECK(!finished_);
      if (len != 0) {
        memcpy(buffer_.get() + size_, header_data, len);
        size_ += len;
      } else {
        // Done.
        finished_ = true;
      }
      return true;
    }

    virtual void OnSynStream(SpdyStreamId stream_id,
                             SpdyStreamId associated_stream_id,
                             SpdyPriority priority,
                             bool fin,
                             bool unidirectional) OVERRIDE {
      SpdyFramer framer(version_);
      framer.set_enable_compression(false);
      SpdySynStreamIR syn_stream(stream_id);
      syn_stream.set_associated_to_stream_id(associated_stream_id);
      syn_stream.set_priority(priority);
      syn_stream.set_fin(fin);
      syn_stream.set_unidirectional(unidirectional);
      scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
      ResetBuffer();
      memcpy(buffer_.get(), frame->data(), framer.GetSynStreamMinimumSize());
      size_ += framer.GetSynStreamMinimumSize();
    }

    virtual void OnSynReply(SpdyStreamId stream_id, bool fin) OVERRIDE {
      SpdyFramer framer(version_);
      framer.set_enable_compression(false);
      SpdyHeadersIR headers(stream_id);
      headers.set_fin(fin);
      scoped_ptr<SpdyFrame> frame(framer.SerializeHeaders(headers));
      ResetBuffer();
      memcpy(buffer_.get(), frame->data(), framer.GetHeadersMinimumSize());
      size_ += framer.GetSynStreamMinimumSize();
    }

    virtual void OnRstStream(SpdyStreamId stream_id,
                             SpdyRstStreamStatus status) OVERRIDE {
      LOG(FATAL);
    }
    virtual void OnSetting(SpdySettingsIds id,
                           uint8 flags,
                           uint32 value) OVERRIDE {
      LOG(FATAL);
    }
    virtual void OnPing(SpdyPingId unique_id, bool is_ack) OVERRIDE {
      LOG(FATAL);
    }
    virtual void OnSettingsEnd() OVERRIDE { LOG(FATAL); }
    virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                          SpdyGoAwayStatus status) OVERRIDE {
      LOG(FATAL);
    }

    virtual void OnHeaders(SpdyStreamId stream_id,
                           bool fin,
                           bool end) OVERRIDE {
      SpdyFramer framer(version_);
      framer.set_enable_compression(false);
      SpdyHeadersIR headers(stream_id);
      headers.set_fin(fin);
      scoped_ptr<SpdyFrame> frame(framer.SerializeHeaders(headers));
      ResetBuffer();
      memcpy(buffer_.get(), frame->data(), framer.GetHeadersMinimumSize());
      size_ += framer.GetHeadersMinimumSize();
    }

    virtual void OnWindowUpdate(SpdyStreamId stream_id, int delta_window_size) {
      LOG(FATAL);
    }

    virtual void OnPushPromise(SpdyStreamId stream_id,
                               SpdyStreamId promised_stream_id,
                               bool end) OVERRIDE {
      SpdyFramer framer(version_);
      framer.set_enable_compression(false);
      SpdyPushPromiseIR push_promise(stream_id, promised_stream_id);
      scoped_ptr<SpdyFrame> frame(framer.SerializePushPromise(push_promise));
      ResetBuffer();
      memcpy(buffer_.get(), frame->data(), framer.GetPushPromiseMinimumSize());
      size_ += framer.GetPushPromiseMinimumSize();
    }

    virtual void OnContinuation(SpdyStreamId stream_id, bool end) OVERRIDE {
      LOG(FATAL);
    }

    char* ReleaseBuffer() {
      CHECK(finished_);
      return buffer_.release();
    }

    virtual void OnWindowUpdate(SpdyStreamId stream_id,
                                uint32 delta_window_size) OVERRIDE {
      LOG(FATAL);
    }

    size_t size() const {
      CHECK(finished_);
      return size_;
    }

   private:
    SpdyMajorVersion version_;
    scoped_ptr<char[]> buffer_;
    size_t size_;
    bool finished_;

    DISALLOW_COPY_AND_ASSIGN(DecompressionVisitor);
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(SpdyFramerTestUtil);
};

class TestSpdyVisitor : public SpdyFramerVisitorInterface,
                        public SpdyFramerDebugVisitorInterface {
 public:
  // This is larger than our max frame size because header blocks that
  // are too long can spill over into CONTINUATION frames.
  static const size_t kDefaultHeaderBufferSize = 16 * 1024 * 1024;

  explicit TestSpdyVisitor(SpdyMajorVersion version)
    : framer_(version),
      use_compression_(false),
      error_count_(0),
      syn_frame_count_(0),
      syn_reply_frame_count_(0),
      headers_frame_count_(0),
      push_promise_frame_count_(0),
      goaway_count_(0),
      setting_count_(0),
      settings_ack_sent_(0),
      settings_ack_received_(0),
      continuation_count_(0),
      altsvc_count_(0),
      test_altsvc_ir_(0),
      last_window_update_stream_(0),
      last_window_update_delta_(0),
      last_push_promise_stream_(0),
      last_push_promise_promised_stream_(0),
      data_bytes_(0),
      fin_frame_count_(0),
      fin_opaque_data_(),
      fin_flag_count_(0),
      zero_length_data_frame_count_(0),
      control_frame_header_data_count_(0),
      zero_length_control_frame_header_data_count_(0),
      data_frame_count_(0),
      last_payload_len_(0),
      last_frame_len_(0),
      header_buffer_(new char[kDefaultHeaderBufferSize]),
      header_buffer_length_(0),
      header_buffer_size_(kDefaultHeaderBufferSize),
      header_stream_id_(-1),
      header_control_type_(DATA),
      header_buffer_valid_(false) {
  }

  virtual void OnError(SpdyFramer* f) OVERRIDE {
    LOG(INFO) << "SpdyFramer Error: "
              << SpdyFramer::ErrorCodeToString(f->error_code());
    ++error_count_;
  }

  virtual void OnDataFrameHeader(SpdyStreamId stream_id,
                                 size_t length,
                                 bool fin) OVERRIDE {
    ++data_frame_count_;
    header_stream_id_ = stream_id;
  }

  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len,
                                 bool fin) OVERRIDE {
    EXPECT_EQ(header_stream_id_, stream_id);
    if (len == 0)
      ++zero_length_data_frame_count_;

    data_bytes_ += len;
    std::cerr << "OnStreamFrameData(" << stream_id << ", \"";
    if (len > 0) {
      for (size_t i = 0 ; i < len; ++i) {
        std::cerr << std::hex << (0xFF & (unsigned int)data[i]) << std::dec;
      }
    }
    std::cerr << "\", " << len << ")\n";
  }

  virtual bool OnControlFrameHeaderData(SpdyStreamId stream_id,
                                        const char* header_data,
                                        size_t len) OVERRIDE {
    ++control_frame_header_data_count_;
    CHECK_EQ(header_stream_id_, stream_id);
    if (len == 0) {
      ++zero_length_control_frame_header_data_count_;
      // Indicates end-of-header-block.
      headers_.clear();
      CHECK(header_buffer_valid_);
      size_t parsed_length = framer_.ParseHeaderBlockInBuffer(
          header_buffer_.get(), header_buffer_length_, &headers_);
      LOG_IF(DFATAL, header_buffer_length_ != parsed_length)
          << "Check failed: header_buffer_length_ == parsed_length "
          << "(" << header_buffer_length_ << " vs. " << parsed_length << ")";
      return true;
    }
    const size_t available = header_buffer_size_ - header_buffer_length_;
    if (len > available) {
      header_buffer_valid_ = false;
      return false;
    }
    memcpy(header_buffer_.get() + header_buffer_length_, header_data, len);
    header_buffer_length_ += len;
    return true;
  }

  virtual void OnSynStream(SpdyStreamId stream_id,
                           SpdyStreamId associated_stream_id,
                           SpdyPriority priority,
                           bool fin,
                           bool unidirectional) OVERRIDE {
    ++syn_frame_count_;
    InitHeaderStreaming(SYN_STREAM, stream_id);
    if (fin) {
      ++fin_flag_count_;
    }
  }

  virtual void OnSynReply(SpdyStreamId stream_id, bool fin) OVERRIDE {
    ++syn_reply_frame_count_;
    InitHeaderStreaming(SYN_REPLY, stream_id);
    if (fin) {
      ++fin_flag_count_;
    }
  }

  virtual void OnRstStream(SpdyStreamId stream_id,
                           SpdyRstStreamStatus status) OVERRIDE {
    ++fin_frame_count_;
  }

  virtual bool OnRstStreamFrameData(const char* rst_stream_data,
                                    size_t len) OVERRIDE {
    if ((rst_stream_data != NULL) && (len > 0)) {
      fin_opaque_data_ += std::string(rst_stream_data, len);
    }
    return true;
  }

  virtual void OnSetting(SpdySettingsIds id,
                         uint8 flags,
                         uint32 value) OVERRIDE {
    ++setting_count_;
  }

  virtual void OnSettingsAck() OVERRIDE {
    DCHECK_LT(SPDY3, framer_.protocol_version());
    ++settings_ack_received_;
  }

  virtual void OnSettingsEnd() OVERRIDE {
    if (framer_.protocol_version() <= SPDY3) { return; }
    ++settings_ack_sent_;
  }

  virtual void OnPing(SpdyPingId unique_id, bool is_ack) OVERRIDE {
    DLOG(FATAL);
  }

  virtual void OnGoAway(SpdyStreamId last_accepted_stream_id,
                        SpdyGoAwayStatus status) OVERRIDE {
    ++goaway_count_;
  }

  virtual void OnHeaders(SpdyStreamId stream_id, bool fin, bool end) OVERRIDE {
    ++headers_frame_count_;
    InitHeaderStreaming(HEADERS, stream_id);
    if (fin) {
      ++fin_flag_count_;
    }
  }

  virtual void OnWindowUpdate(SpdyStreamId stream_id,
                              uint32 delta_window_size) OVERRIDE {
    last_window_update_stream_ = stream_id;
    last_window_update_delta_ = delta_window_size;
  }

  virtual void OnPushPromise(SpdyStreamId stream_id,
                             SpdyStreamId promised_stream_id,
                             bool end) OVERRIDE {
    ++push_promise_frame_count_;
    InitHeaderStreaming(PUSH_PROMISE, stream_id);
    last_push_promise_stream_ = stream_id;
    last_push_promise_promised_stream_ = promised_stream_id;
  }

  virtual void OnContinuation(SpdyStreamId stream_id, bool end) OVERRIDE {
    ++continuation_count_;
  }

  virtual void OnAltSvc(SpdyStreamId stream_id,
                        uint32 max_age,
                        uint16 port,
                        StringPiece protocol_id,
                        StringPiece host,
                        StringPiece origin) OVERRIDE {
    test_altsvc_ir_.set_stream_id(stream_id);
    test_altsvc_ir_.set_max_age(max_age);
    test_altsvc_ir_.set_port(port);
    test_altsvc_ir_.set_protocol_id(protocol_id.as_string());
    test_altsvc_ir_.set_host(host.as_string());
    if (origin.length() > 0) {
      test_altsvc_ir_.set_origin(origin.as_string());
    }
    ++altsvc_count_;
  }

  virtual void OnSendCompressedFrame(SpdyStreamId stream_id,
                                     SpdyFrameType type,
                                     size_t payload_len,
                                     size_t frame_len) OVERRIDE {
    last_payload_len_ = payload_len;
    last_frame_len_ = frame_len;
  }

  virtual void OnReceiveCompressedFrame(SpdyStreamId stream_id,
                                        SpdyFrameType type,
                                        size_t frame_len) OVERRIDE {
    last_frame_len_ = frame_len;
  }

  // Convenience function which runs a framer simulation with particular input.
  void SimulateInFramer(const unsigned char* input, size_t size) {
    framer_.set_enable_compression(use_compression_);
    framer_.set_visitor(this);
    size_t input_remaining = size;
    const char* input_ptr = reinterpret_cast<const char*>(input);
    while (input_remaining > 0 &&
           framer_.error_code() == SpdyFramer::SPDY_NO_ERROR) {
      // To make the tests more interesting, we feed random (amd small) chunks
      // into the framer.  This simulates getting strange-sized reads from
      // the socket.
      const size_t kMaxReadSize = 32;
      size_t bytes_read =
          (rand() % min(input_remaining, kMaxReadSize)) + 1;
      size_t bytes_processed = framer_.ProcessInput(input_ptr, bytes_read);
      input_remaining -= bytes_processed;
      input_ptr += bytes_processed;
    }
  }

  void InitHeaderStreaming(SpdyFrameType header_control_type,
                           SpdyStreamId stream_id) {
    DCHECK_GE(header_control_type, FIRST_CONTROL_TYPE);
    DCHECK_LE(header_control_type, LAST_CONTROL_TYPE);
    memset(header_buffer_.get(), 0, header_buffer_size_);
    header_buffer_length_ = 0;
    header_stream_id_ = stream_id;
    header_control_type_ = header_control_type;
    header_buffer_valid_ = true;
    DCHECK_NE(header_stream_id_, SpdyFramer::kInvalidStream);
  }

  // Override the default buffer size (16K). Call before using the framer!
  void set_header_buffer_size(size_t header_buffer_size) {
    header_buffer_size_ = header_buffer_size;
    header_buffer_.reset(new char[header_buffer_size]);
  }

  static size_t header_data_chunk_max_size() {
    return SpdyFramer::kHeaderDataChunkMaxSize;
  }

  SpdyFramer framer_;
  bool use_compression_;

  // Counters from the visitor callbacks.
  int error_count_;
  int syn_frame_count_;
  int syn_reply_frame_count_;
  int headers_frame_count_;
  int push_promise_frame_count_;
  int goaway_count_;
  int setting_count_;
  int settings_ack_sent_;
  int settings_ack_received_;
  int continuation_count_;
  int altsvc_count_;
  SpdyAltSvcIR test_altsvc_ir_;
  SpdyStreamId last_window_update_stream_;
  uint32 last_window_update_delta_;
  SpdyStreamId last_push_promise_stream_;
  SpdyStreamId last_push_promise_promised_stream_;
  int data_bytes_;
  int fin_frame_count_;  // The count of RST_STREAM type frames received.
  std::string fin_opaque_data_;
  int fin_flag_count_;  // The count of frames with the FIN flag set.
  int zero_length_data_frame_count_;  // The count of zero-length data frames.
  int control_frame_header_data_count_;  // The count of chunks received.
  // The count of zero-length control frame header data chunks received.
  int zero_length_control_frame_header_data_count_;
  int data_frame_count_;
  size_t last_payload_len_;
  size_t last_frame_len_;

  // Header block streaming state:
  scoped_ptr<char[]> header_buffer_;
  size_t header_buffer_length_;
  size_t header_buffer_size_;
  SpdyStreamId header_stream_id_;
  SpdyFrameType header_control_type_;
  bool header_buffer_valid_;
  SpdyHeaderBlock headers_;
};

// Retrieves serialized headers from a HEADERS or SYN_STREAM frame.
base::StringPiece GetSerializedHeaders(const SpdyFrame* frame,
                                       const SpdyFramer& framer) {
  SpdyFrameReader reader(frame->data(), frame->size());
  reader.Seek(2);  // Seek past the frame length.
  SpdyFrameType frame_type;
  if (framer.protocol_version() > SPDY3) {
    uint8 serialized_type;
    reader.ReadUInt8(&serialized_type);
    frame_type = SpdyConstants::ParseFrameType(framer.protocol_version(),
                                               serialized_type);
    DCHECK_EQ(HEADERS, frame_type);
    uint8 flags;
    reader.ReadUInt8(&flags);
    if (flags & HEADERS_FLAG_PRIORITY) {
      frame_type = SYN_STREAM;
    }
  } else {
    uint16 serialized_type;
    reader.ReadUInt16(&serialized_type);
    frame_type = SpdyConstants::ParseFrameType(framer.protocol_version(),
                                               serialized_type);
    DCHECK(frame_type == HEADERS ||
           frame_type == SYN_STREAM) << frame_type;
  }

  if (frame_type == SYN_STREAM) {
    return StringPiece(frame->data() + framer.GetSynStreamMinimumSize(),
                       frame->size() - framer.GetSynStreamMinimumSize());
  } else {
    return StringPiece(frame->data() + framer.GetHeadersMinimumSize(),
                       frame->size() - framer.GetHeadersMinimumSize());
  }
}

}  // namespace test

}  // namespace net

using net::test::SetFrameLength;
using net::test::SetFrameFlags;
using net::test::CompareCharArraysWithHexError;
using net::test::SpdyFramerTestUtil;
using net::test::TestSpdyVisitor;
using net::test::GetSerializedHeaders;

namespace net {

class SpdyFramerTest : public ::testing::TestWithParam<SpdyMajorVersion> {
 protected:
  virtual void SetUp() {
    spdy_version_ = GetParam();
    spdy_version_ch_ = static_cast<unsigned char>(
        SpdyConstants::SerializeMajorVersion(spdy_version_));
  }

  void CompareFrame(const string& description,
                    const SpdyFrame& actual_frame,
                    const unsigned char* expected,
                    const int expected_len) {
    const unsigned char* actual =
        reinterpret_cast<const unsigned char*>(actual_frame.data());
    CompareCharArraysWithHexError(
        description, actual, actual_frame.size(), expected, expected_len);
  }

  void CompareFrames(const string& description,
                     const SpdyFrame& expected_frame,
                     const SpdyFrame& actual_frame) {
    CompareCharArraysWithHexError(
        description,
        reinterpret_cast<const unsigned char*>(expected_frame.data()),
        expected_frame.size(),
        reinterpret_cast<const unsigned char*>(actual_frame.data()),
        actual_frame.size());
  }

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

  bool IsSpdy2() { return spdy_version_ == SPDY2; }
  bool IsSpdy3() { return spdy_version_ == SPDY3; }
  bool IsSpdy4() { return spdy_version_ == SPDY4; }
  bool IsSpdy5() { return spdy_version_ == SPDY5; }

  // Version of SPDY protocol to be used.
  SpdyMajorVersion spdy_version_;
  unsigned char spdy_version_ch_;
};

// All tests are run with 3 different SPDY versions: SPDY/2, SPDY/3, SPDY/4.
INSTANTIATE_TEST_CASE_P(SpdyFramerTests,
                        SpdyFramerTest,
                        ::testing::Values(SPDY2, SPDY3, SPDY4));

// Test that we can encode and decode a SpdyHeaderBlock in serialized form.
TEST_P(SpdyFramerTest, HeaderBlockInBuffer) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);

  // Encode the header block into a SynStream frame.
  SpdySynStreamIR syn_stream(1);
  syn_stream.set_priority(1);
  syn_stream.SetHeader("alpha", "beta");
  syn_stream.SetHeader("gamma", "charlie");
  syn_stream.SetHeader("cookie", "key1=value1; key2=value2");
  scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
  EXPECT_TRUE(frame.get() != NULL);

  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(frame->data()),
      frame->size());

  EXPECT_EQ(1, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_TRUE(CompareHeaderBlocks(&syn_stream.name_value_block(),
                                  &visitor.headers_));
}

// Test that if there's not a full frame, we fail to parse it.
TEST_P(SpdyFramerTest, UndersizedHeaderBlockInBuffer) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);

  // Encode the header block into a SynStream frame.
  SpdySynStreamIR syn_stream(1);
  syn_stream.set_priority(1);
  syn_stream.SetHeader("alpha", "beta");
  syn_stream.SetHeader("gamma", "charlie");
  scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
  EXPECT_TRUE(frame.get() != NULL);

  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(frame->data()),
      frame->size() - 2);

  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(0u, visitor.headers_.size());
}

// Test that if we receive a SYN_REPLY with stream ID zero, we signal an error
// (but don't crash).
TEST_P(SpdyFramerTest, SynReplyWithStreamIdZero) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  SpdySynReplyIR syn_reply(0);
  syn_reply.SetHeader("alpha", "beta");
  scoped_ptr<SpdySerializedFrame> frame(framer.SerializeSynReply(syn_reply));
  ASSERT_TRUE(frame.get() != NULL);

  // We shouldn't have to read the whole frame before we signal an error.
  EXPECT_CALL(visitor, OnError(testing::Eq(&framer)));
  EXPECT_GT(frame->size(), framer.ProcessInput(frame->data(), frame->size()));
  EXPECT_TRUE(framer.HasError());
  EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
}

// Test that if we receive a HEADERS with stream ID zero, we signal an error
// (but don't crash).
TEST_P(SpdyFramerTest, HeadersWithStreamIdZero) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  SpdyHeadersIR headers_ir(0);
  headers_ir.SetHeader("alpha", "beta");
  scoped_ptr<SpdySerializedFrame> frame(framer.SerializeHeaders(headers_ir));
  ASSERT_TRUE(frame.get() != NULL);

  // We shouldn't have to read the whole frame before we signal an error.
  EXPECT_CALL(visitor, OnError(testing::Eq(&framer)));
  EXPECT_GT(frame->size(), framer.ProcessInput(frame->data(), frame->size()));
  EXPECT_TRUE(framer.HasError());
  EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
}

// Test that if we receive a PUSH_PROMISE with stream ID zero, we signal an
// error (but don't crash).
TEST_P(SpdyFramerTest, PushPromiseWithStreamIdZero) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  SpdyPushPromiseIR push_promise(0, 4);
  push_promise.SetHeader("alpha", "beta");
  scoped_ptr<SpdySerializedFrame> frame(
      framer.SerializePushPromise(push_promise));
  ASSERT_TRUE(frame.get() != NULL);

  // We shouldn't have to read the whole frame before we signal an error.
  EXPECT_CALL(visitor, OnError(testing::Eq(&framer)));
  EXPECT_GT(frame->size(), framer.ProcessInput(frame->data(), frame->size()));
  EXPECT_TRUE(framer.HasError());
  EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
}

// Test that if we receive a PUSH_PROMISE with promised stream ID zero, we
// signal an error (but don't crash).
TEST_P(SpdyFramerTest, PushPromiseWithPromisedStreamIdZero) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  SpdyPushPromiseIR push_promise(3, 0);
  push_promise.SetHeader("alpha", "beta");
  scoped_ptr<SpdySerializedFrame> frame(
    framer.SerializePushPromise(push_promise));
  ASSERT_TRUE(frame.get() != NULL);

  // We shouldn't have to read the whole frame before we signal an error.
  EXPECT_CALL(visitor, OnError(testing::Eq(&framer)));
  EXPECT_GT(frame->size(), framer.ProcessInput(frame->data(), frame->size()));
  EXPECT_TRUE(framer.HasError());
  EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
}

TEST_P(SpdyFramerTest, DuplicateHeader) {
  if (spdy_version_ > SPDY3) {
    // TODO(jgraettinger): Punting on this because we haven't determined
    // whether duplicate HPACK headers other than Cookie are an error.
    // If they are, this will need to be updated to use HpackOutputStream.
    return;
  }
  SpdyFramer framer(spdy_version_);
  // Frame builder with plentiful buffer size.
  SpdyFrameBuilder frame(1024, spdy_version_);
  if (spdy_version_ <= SPDY3) {
    frame.WriteControlFrameHeader(framer, SYN_STREAM, CONTROL_FLAG_NONE);
    frame.WriteUInt32(3);  // stream_id
    frame.WriteUInt32(0);  // associated stream id
    frame.WriteUInt16(0);  // Priority.
  } else {
    frame.BeginNewFrame(framer, HEADERS, HEADERS_FLAG_PRIORITY, 3);
    frame.WriteUInt32(framer.GetHighestPriority());
  }

  if (IsSpdy2()) {
    frame.WriteUInt16(2);  // Number of headers.
    frame.WriteString("name");
    frame.WriteString("value1");
    frame.WriteString("name");
    frame.WriteString("value2");
  } else {
    frame.WriteUInt32(2);  // Number of headers.
    frame.WriteStringPiece32("name");
    frame.WriteStringPiece32("value1");
    frame.WriteStringPiece32("name");
    frame.WriteStringPiece32("value2");
  }
  // write the length
  frame.RewriteLength(framer);

  SpdyHeaderBlock new_headers;
  framer.set_enable_compression(false);
  scoped_ptr<SpdyFrame> control_frame(frame.take());
  base::StringPiece serialized_headers =
      GetSerializedHeaders(control_frame.get(), framer);
  // This should fail because duplicate headers are verboten by the spec.
  EXPECT_FALSE(framer.ParseHeaderBlockInBuffer(serialized_headers.data(),
                                               serialized_headers.size(),
                                               &new_headers));
}

TEST_P(SpdyFramerTest, MultiValueHeader) {
  SpdyFramer framer(spdy_version_);
  // Frame builder with plentiful buffer size.
  SpdyFrameBuilder frame(1024, spdy_version_);
  if (spdy_version_ <= SPDY3) {
    frame.WriteControlFrameHeader(framer, SYN_STREAM, CONTROL_FLAG_NONE);
    frame.WriteUInt32(3);  // stream_id
    frame.WriteUInt32(0);  // associated stream id
    frame.WriteUInt16(0);  // Priority.
  } else {
    frame.BeginNewFrame(framer,
                        HEADERS,
                        HEADERS_FLAG_PRIORITY | HEADERS_FLAG_END_HEADERS,
                        3);
    frame.WriteUInt32(0);  // Priority exclusivity and dependent stream.
    frame.WriteUInt8(255);  // Priority weight.
  }

  string value("value1\0value2", 13);
  if (IsSpdy2()) {
    frame.WriteUInt16(1);  // Number of headers.
    frame.WriteString("name");
    frame.WriteString(value);
  } else if (spdy_version_ > SPDY3) {
    // TODO(jgraettinger): If this pattern appears again, move to test class.
    std::map<string, string> header_set;
    header_set["name"] = value;
    string buffer;
    HpackEncoder encoder(ObtainHpackHuffmanTable());
    encoder.EncodeHeaderSetWithoutCompression(header_set, &buffer);
    frame.WriteBytes(&buffer[0], buffer.size());
  } else {
    frame.WriteUInt32(1);  // Number of headers.
    frame.WriteStringPiece32("name");
    frame.WriteStringPiece32(value);
  }
  // write the length
  frame.RewriteLength(framer);

  framer.set_enable_compression(false);
  scoped_ptr<SpdyFrame> control_frame(frame.take());

  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());

  EXPECT_THAT(visitor.headers_, ElementsAre(
      Pair("name", value)));
}

TEST_P(SpdyFramerTest, BasicCompression) {
  if (spdy_version_ > SPDY3) {
    // Deflate compression doesn't apply to HPACK.
    return;
  }
  scoped_ptr<TestSpdyVisitor> visitor(new TestSpdyVisitor(spdy_version_));
  SpdyFramer framer(spdy_version_);
  framer.set_debug_visitor(visitor.get());
  SpdySynStreamIR syn_stream(1);
  syn_stream.set_priority(1);
  syn_stream.SetHeader("server", "SpdyServer 1.0");
  syn_stream.SetHeader("date", "Mon 12 Jan 2009 12:12:12 PST");
  syn_stream.SetHeader("status", "200");
  syn_stream.SetHeader("version", "HTTP/1.1");
  syn_stream.SetHeader("content-type", "text/html");
  syn_stream.SetHeader("content-length", "12");
  scoped_ptr<SpdyFrame> frame1(framer.SerializeSynStream(syn_stream));
  size_t uncompressed_size1 = visitor->last_payload_len_;
  size_t compressed_size1 =
      visitor->last_frame_len_ - framer.GetSynStreamMinimumSize();
  if (IsSpdy2()) {
    EXPECT_EQ(139u, uncompressed_size1);
#if defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(155u, compressed_size1);
#else  // !defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(135u, compressed_size1);
#endif  // !defined(USE_SYSTEM_ZLIB)
  } else {
    EXPECT_EQ(165u, uncompressed_size1);
#if defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(181u, compressed_size1);
#else  // !defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(117u, compressed_size1);
#endif  // !defined(USE_SYSTEM_ZLIB)
  }
  scoped_ptr<SpdyFrame> frame2(framer.SerializeSynStream(syn_stream));
  size_t uncompressed_size2 = visitor->last_payload_len_;
  size_t compressed_size2 =
      visitor->last_frame_len_ - framer.GetSynStreamMinimumSize();

  // Expect the second frame to be more compact than the first.
  EXPECT_LE(frame2->size(), frame1->size());

  // Decompress the first frame
  scoped_ptr<SpdyFrame> frame3(SpdyFramerTestUtil::DecompressFrame(
      &framer, *frame1.get()));

  // Decompress the second frame
  visitor.reset(new TestSpdyVisitor(spdy_version_));
  framer.set_debug_visitor(visitor.get());
  scoped_ptr<SpdyFrame> frame4(SpdyFramerTestUtil::DecompressFrame(
      &framer, *frame2.get()));
  size_t uncompressed_size4 =
      frame4->size() - framer.GetSynStreamMinimumSize();
  size_t compressed_size4 =
      visitor->last_frame_len_ - framer.GetSynStreamMinimumSize();
  if (IsSpdy2()) {
    EXPECT_EQ(139u, uncompressed_size4);
#if defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(149u, compressed_size4);
#else  // !defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(101u, compressed_size4);
#endif  // !defined(USE_SYSTEM_ZLIB)
  } else {
    EXPECT_EQ(165u, uncompressed_size4);
#if defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(175u, compressed_size4);
#else  // !defined(USE_SYSTEM_ZLIB)
    EXPECT_EQ(102u, compressed_size4);
#endif  // !defined(USE_SYSTEM_ZLIB)
  }

  EXPECT_EQ(uncompressed_size1, uncompressed_size2);
  EXPECT_EQ(uncompressed_size1, uncompressed_size4);
  EXPECT_EQ(compressed_size2, compressed_size4);

  // Expect frames 3 & 4 to be the same.
  CompareFrames("Uncompressed SYN_STREAM", *frame3, *frame4);

  // Expect frames 3 to be the same as a uncompressed frame created
  // from scratch.
  framer.set_enable_compression(false);
  scoped_ptr<SpdyFrame> uncompressed_frame(
      framer.SerializeSynStream(syn_stream));
  CompareFrames("Uncompressed SYN_STREAM", *frame3, *uncompressed_frame);
}

TEST_P(SpdyFramerTest, CompressEmptyHeaders) {
  // See crbug.com/172383
  SpdySynStreamIR syn_stream(1);
  syn_stream.SetHeader("server", "SpdyServer 1.0");
  syn_stream.SetHeader("date", "Mon 12 Jan 2009 12:12:12 PST");
  syn_stream.SetHeader("status", "200");
  syn_stream.SetHeader("version", "HTTP/1.1");
  syn_stream.SetHeader("content-type", "text/html");
  syn_stream.SetHeader("content-length", "12");
  syn_stream.SetHeader("x-empty-header", "");

  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(true);
  scoped_ptr<SpdyFrame> frame1(framer.SerializeSynStream(syn_stream));
}

TEST_P(SpdyFramerTest, Basic) {
  const unsigned char kV2Input[] = {
    0x80, spdy_version_ch_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x80, spdy_version_ch_, 0x00, 0x08,  // HEADERS on Stream #1
    0x00, 0x00, 0x00, 0x18,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x02, 'h', '2',
    0x00, 0x02, 'v', '2',
    0x00, 0x02, 'h', '3',
    0x00, 0x02, 'v', '3',

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x0c,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x80, spdy_version_ch_, 0x00, 0x01,  // SYN Stream #3
    0x00, 0x00, 0x00, 0x0c,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x03,           // DATA on Stream #3
    0x00, 0x00, 0x00, 0x08,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x04,
    0xde, 0xad, 0xbe, 0xef,

    0x80, spdy_version_ch_, 0x00, 0x03,  // RST_STREAM on Stream #1
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x05,           // RST_STREAM_CANCEL

    0x00, 0x00, 0x00, 0x03,           // DATA on Stream #3
    0x00, 0x00, 0x00, 0x00,

    0x80, spdy_version_ch_, 0x00, 0x03,  // RST_STREAM on Stream #3
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x05,           // RST_STREAM_CANCEL
  };

  const unsigned char kV3Input[] = {
    0x80, spdy_version_ch_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x1a,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00,
    0x00, 0x02, 'h', 'h',
    0x00, 0x00, 0x00, 0x02,
    'v', 'v',

    0x80, spdy_version_ch_, 0x00, 0x08,  // HEADERS on Stream #1
    0x00, 0x00, 0x00, 0x20,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x02,
    'h', '2',
    0x00, 0x00, 0x00, 0x02,
    'v', '2', 0x00, 0x00,
    0x00, 0x02, 'h', '3',
    0x00, 0x00, 0x00, 0x02,
    'v', '3',

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x0c,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x80, spdy_version_ch_, 0x00, 0x01,  // SYN Stream #3
    0x00, 0x00, 0x00, 0x0e,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,

    0x00, 0x00, 0x00, 0x03,           // DATA on Stream #3
    0x00, 0x00, 0x00, 0x08,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x04,
    0xde, 0xad, 0xbe, 0xef,

    0x80, spdy_version_ch_, 0x00, 0x03,  // RST_STREAM on Stream #1
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x05,           // RST_STREAM_CANCEL

    0x00, 0x00, 0x00, 0x03,           // DATA on Stream #3
    0x00, 0x00, 0x00, 0x00,

    0x80, spdy_version_ch_, 0x00, 0x03,  // RST_STREAM on Stream #3
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x05,           // RST_STREAM_CANCEL
  };

  // SYN_STREAM doesn't exist in SPDY4, so instead we send
  // HEADERS frames with PRIORITY and END_HEADERS set.
  const unsigned char kV4Input[] = {
    0x00, 0x05, 0x01, 0x24,           // HEADERS: PRIORITY | END_HEADERS
    0x00, 0x00, 0x00, 0x01,           // Stream 1
    0x00, 0x00, 0x00, 0x00,           // Priority 0
    0x82,                             // :method: GET

    0x00, 0x01, 0x01, 0x04,           // HEADERS: END_HEADERS
    0x00, 0x00, 0x00, 0x01,           // Stream 1
    0x8c,                             // :status: 200

    0x00, 0x0c, 0x00, 0x00,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x01,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x05, 0x01, 0x24,           // HEADERS: PRIORITY | END_HEADERS
    0x00, 0x00, 0x00, 0x03,           // Stream 3
    0x00, 0x00, 0x00, 0x00,           // Priority 0
    0x82,                             // :method: GET

    0x00, 0x08, 0x00, 0x00,           // DATA on Stream #3
    0x00, 0x00, 0x00, 0x03,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x04, 0x00, 0x00,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x01,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x04, 0x03, 0x00,           // RST_STREAM on Stream #1
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x08,           // RST_STREAM_CANCEL

    0x00, 0x00, 0x00, 0x00,           // DATA on Stream #3
    0x00, 0x00, 0x00, 0x03,

    0x00, 0x0f, 0x03, 0x00,           // RST_STREAM on Stream #3
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x08,           // RST_STREAM_CANCEL
    0x52, 0x45, 0x53, 0x45,           // opaque data
    0x54, 0x53, 0x54, 0x52,
    0x45, 0x41, 0x4d,
  };

  TestSpdyVisitor visitor(spdy_version_);
  if (IsSpdy2()) {
    visitor.SimulateInFramer(kV2Input, sizeof(kV2Input));
  } else if (IsSpdy3()) {
    visitor.SimulateInFramer(kV3Input, sizeof(kV3Input));
  } else {
    visitor.SimulateInFramer(kV4Input, sizeof(kV4Input));
  }

  EXPECT_EQ(2, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(24, visitor.data_bytes_);

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(2, visitor.fin_frame_count_);

  if (IsSpdy4()) {
    base::StringPiece reset_stream = "RESETSTREAM";
    EXPECT_EQ(reset_stream, visitor.fin_opaque_data_);
  } else {
    EXPECT_TRUE(visitor.fin_opaque_data_.empty());
  }

  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(0, visitor.zero_length_data_frame_count_);
  EXPECT_EQ(4, visitor.data_frame_count_);
  visitor.fin_opaque_data_.clear();
}

// Test that the FIN flag on a data frame signifies EOF.
TEST_P(SpdyFramerTest, FinOnDataFrame) {
  const unsigned char kV2Input[] = {
    0x80, spdy_version_ch_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x80, spdy_version_ch_, 0x00, 0x02,  // SYN REPLY Stream #1
    0x00, 0x00, 0x00, 0x10,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'a', 'a',
    0x00, 0x02, 'b', 'b',

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x0c,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1, with EOF
    0x01, 0x00, 0x00, 0x04,
    0xde, 0xad, 0xbe, 0xef,
  };
  const unsigned char kV3Input[] = {
    0x80, spdy_version_ch_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x1a,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00,
    0x00, 0x02, 'h', 'h',
    0x00, 0x00, 0x00, 0x02,
    'v',  'v',

    0x80, spdy_version_ch_, 0x00, 0x02,  // SYN REPLY Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    'a',  'a',  0x00, 0x00,
    0x00, 0x02, 'b', 'b',

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x0c,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x00, 0x00, 0x01,           // DATA on Stream #1, with EOF
    0x01, 0x00, 0x00, 0x04,
    0xde, 0xad, 0xbe, 0xef,
  };

  // SYN_STREAM and SYN_REPLY don't exist in SPDY4, so instead we send
  // HEADERS frames with PRIORITY(SYN_STREAM only) and END_HEADERS set.
  const unsigned char kV4Input[] = {
    0x00, 0x05, 0x01, 0x24,           // HEADERS: PRIORITY | END_HEADERS
    0x00, 0x00, 0x00, 0x01,           // Stream 1
    0x00, 0x00, 0x00, 0x00,           // Priority 0
    0x82,                             // :method: GET

    0x00, 0x01, 0x01, 0x04,           // HEADERS: END_HEADERS
    0x00, 0x00, 0x00, 0x01,           // Stream 1
    0x8c,                             // :status: 200

    0x00, 0x0c, 0x00, 0x00,           // DATA on Stream #1
    0x00, 0x00, 0x00, 0x01,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x04, 0x00, 0x01,           // DATA on Stream #1, with FIN
    0x00, 0x00, 0x00, 0x01,
    0xde, 0xad, 0xbe, 0xef,
  };

  TestSpdyVisitor visitor(spdy_version_);
  if (IsSpdy2()) {
    visitor.SimulateInFramer(kV2Input, sizeof(kV2Input));
  } else if (IsSpdy3()) {
    visitor.SimulateInFramer(kV3Input, sizeof(kV3Input));
  } else {
    visitor.SimulateInFramer(kV4Input, sizeof(kV4Input));
  }

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  if (IsSpdy4()) {
    EXPECT_EQ(0, visitor.syn_reply_frame_count_);
    EXPECT_EQ(1, visitor.headers_frame_count_);
  } else {
    EXPECT_EQ(1, visitor.syn_reply_frame_count_);
    EXPECT_EQ(0, visitor.headers_frame_count_);
  }
  EXPECT_EQ(16, visitor.data_bytes_);
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  EXPECT_EQ(2, visitor.data_frame_count_);
}

// Test that the FIN flag on a SYN reply frame signifies EOF.
TEST_P(SpdyFramerTest, FinOnSynReplyFrame) {
  const unsigned char kV2Input[] = {
    0x80, spdy_version_ch_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x80, spdy_version_ch_, 0x00, 0x02,  // SYN REPLY Stream #1
    0x01, 0x00, 0x00, 0x10,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'a', 'a',
    0x00, 0x02, 'b', 'b',
  };
  const unsigned char kV3Input[] = {
    0x80, spdy_version_ch_, 0x00, 0x01,  // SYN Stream #1
    0x00, 0x00, 0x00, 0x1a,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00,
    0x00, 0x02, 'h', 'h',
    0x00, 0x00, 0x00, 0x02,
    'v', 'v',

    0x80, spdy_version_ch_, 0x00, 0x02,  // SYN REPLY Stream #1
    0x01, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    'a', 'a',   0x00, 0x00,
    0x00, 0x02, 'b', 'b',
  };

  // SYN_STREAM and SYN_REPLY don't exist in SPDY4, so instead we send
  // HEADERS frames with PRIORITY(SYN_STREAM only) and END_HEADERS set.
  const unsigned char kV4Input[] = {
    0x00, 0x05, 0x01, 0x24,           // HEADERS: PRIORITY | END_HEADERS
    0x00, 0x00, 0x00, 0x01,           // Stream 1
    0x00, 0x00, 0x00, 0x00,           // Priority 0
    0x82,                             // :method: GET

    0x00, 0x01, 0x01, 0x05,           // HEADERS: FIN | END_HEADERS
    0x00, 0x00, 0x00, 0x01,           // Stream 1
    0x8c,                             // :status: 200
  };

  TestSpdyVisitor visitor(spdy_version_);
  if (IsSpdy2()) {
    visitor.SimulateInFramer(kV2Input, sizeof(kV2Input));
  } else if (IsSpdy3()) {
    visitor.SimulateInFramer(kV3Input, sizeof(kV3Input));
  } else {
    visitor.SimulateInFramer(kV4Input, sizeof(kV4Input));
  }

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  if (IsSpdy4()) {
    EXPECT_EQ(0, visitor.syn_reply_frame_count_);
    EXPECT_EQ(1, visitor.headers_frame_count_);
  } else {
    EXPECT_EQ(1, visitor.syn_reply_frame_count_);
    EXPECT_EQ(0, visitor.headers_frame_count_);
  }
  EXPECT_EQ(0, visitor.data_bytes_);
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(1, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  EXPECT_EQ(0, visitor.data_frame_count_);
}

TEST_P(SpdyFramerTest, HeaderCompression) {
  if (spdy_version_ > SPDY3) {
    // Deflate compression doesn't apply to HPACK.
    return;
  }
  SpdyFramer send_framer(spdy_version_);
  SpdyFramer recv_framer(spdy_version_);

  send_framer.set_enable_compression(true);
  recv_framer.set_enable_compression(true);

  const char kHeader1[] = "header1";
  const char kHeader2[] = "header2";
  const char kHeader3[] = "header3";
  const char kValue1[] = "value1";
  const char kValue2[] = "value2";
  const char kValue3[] = "value3";

  // SYN_STREAM #1
  SpdyHeaderBlock block;
  block[kHeader1] = kValue1;
  block[kHeader2] = kValue2;
  SpdySynStreamIR syn_ir_1(1);
  syn_ir_1.set_name_value_block(block);
  scoped_ptr<SpdyFrame> syn_frame_1(send_framer.SerializeFrame(syn_ir_1));
  EXPECT_TRUE(syn_frame_1.get() != NULL);

  // SYN_STREAM #2
  block[kHeader3] = kValue3;
  SpdySynStreamIR syn_stream(3);
  syn_stream.set_name_value_block(block);
  scoped_ptr<SpdyFrame> syn_frame_2(send_framer.SerializeSynStream(syn_stream));
  EXPECT_TRUE(syn_frame_2.get() != NULL);

  // Now start decompressing
  scoped_ptr<SpdyFrame> decompressed;
  scoped_ptr<SpdyFrame> uncompressed;
  base::StringPiece serialized_headers;
  SpdyHeaderBlock decompressed_headers;

  // Decompress SYN_STREAM #1
  decompressed.reset(SpdyFramerTestUtil::DecompressFrame(
      &recv_framer, *syn_frame_1.get()));
  EXPECT_TRUE(decompressed.get() != NULL);
  serialized_headers = GetSerializedHeaders(decompressed.get(), send_framer);
  EXPECT_TRUE(recv_framer.ParseHeaderBlockInBuffer(serialized_headers.data(),
                                                   serialized_headers.size(),
                                                   &decompressed_headers));
  EXPECT_EQ(2u, decompressed_headers.size());
  EXPECT_EQ(kValue1, decompressed_headers[kHeader1]);
  EXPECT_EQ(kValue2, decompressed_headers[kHeader2]);

  // Decompress SYN_STREAM #2
  decompressed.reset(SpdyFramerTestUtil::DecompressFrame(
      &recv_framer, *syn_frame_2.get()));
  EXPECT_TRUE(decompressed.get() != NULL);
  serialized_headers = GetSerializedHeaders(decompressed.get(), send_framer);
  decompressed_headers.clear();
  EXPECT_TRUE(recv_framer.ParseHeaderBlockInBuffer(serialized_headers.data(),
                                                   serialized_headers.size(),
                                                   &decompressed_headers));
  EXPECT_EQ(3u, decompressed_headers.size());
  EXPECT_EQ(kValue1, decompressed_headers[kHeader1]);
  EXPECT_EQ(kValue2, decompressed_headers[kHeader2]);
  EXPECT_EQ(kValue3, decompressed_headers[kHeader3]);
}

// Verify we don't leak when we leave streams unclosed
TEST_P(SpdyFramerTest, UnclosedStreamDataCompressors) {
  SpdyFramer send_framer(spdy_version_);

  send_framer.set_enable_compression(true);

  const char kHeader1[] = "header1";
  const char kHeader2[] = "header2";
  const char kValue1[] = "value1";
  const char kValue2[] = "value2";

  SpdySynStreamIR syn_stream(1);
  syn_stream.SetHeader(kHeader1, kValue1);
  syn_stream.SetHeader(kHeader2, kValue2);
  scoped_ptr<SpdyFrame> syn_frame(send_framer.SerializeSynStream(syn_stream));
  EXPECT_TRUE(syn_frame.get() != NULL);

  StringPiece bytes = "this is a test test test test test!";
  SpdyDataIR data_ir(1, bytes);
  data_ir.set_fin(true);
  scoped_ptr<SpdyFrame> send_frame(send_framer.SerializeData(data_ir));
  EXPECT_TRUE(send_frame.get() != NULL);

  // Run the inputs through the framer.
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  const unsigned char* data;
  data = reinterpret_cast<const unsigned char*>(syn_frame->data());
  visitor.SimulateInFramer(data, syn_frame->size());
  data = reinterpret_cast<const unsigned char*>(send_frame->data());
  visitor.SimulateInFramer(data, send_frame->size());

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);
  EXPECT_EQ(bytes.size(), static_cast<unsigned>(visitor.data_bytes_));
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  EXPECT_EQ(1, visitor.data_frame_count_);
}

// Verify we can decompress the stream even if handed over to the
// framer 1 byte at a time.
TEST_P(SpdyFramerTest, UnclosedStreamDataCompressorsOneByteAtATime) {
  SpdyFramer send_framer(spdy_version_);

  send_framer.set_enable_compression(true);

  const char kHeader1[] = "header1";
  const char kHeader2[] = "header2";
  const char kValue1[] = "value1";
  const char kValue2[] = "value2";

  SpdySynStreamIR syn_stream(1);
  syn_stream.SetHeader(kHeader1, kValue1);
  syn_stream.SetHeader(kHeader2, kValue2);
  scoped_ptr<SpdyFrame> syn_frame(send_framer.SerializeSynStream(syn_stream));
  EXPECT_TRUE(syn_frame.get() != NULL);

  const char bytes[] = "this is a test test test test test!";
  SpdyDataIR data_ir(1, StringPiece(bytes, arraysize(bytes)));
  data_ir.set_fin(true);
  scoped_ptr<SpdyFrame> send_frame(send_framer.SerializeData(data_ir));
  EXPECT_TRUE(send_frame.get() != NULL);

  // Run the inputs through the framer.
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  const unsigned char* data;
  data = reinterpret_cast<const unsigned char*>(syn_frame->data());
  for (size_t idx = 0; idx < syn_frame->size(); ++idx) {
    visitor.SimulateInFramer(data + idx, 1);
    ASSERT_EQ(0, visitor.error_count_);
  }
  data = reinterpret_cast<const unsigned char*>(send_frame->data());
  for (size_t idx = 0; idx < send_frame->size(); ++idx) {
    visitor.SimulateInFramer(data + idx, 1);
    ASSERT_EQ(0, visitor.error_count_);
  }

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);
  EXPECT_EQ(arraysize(bytes), static_cast<unsigned>(visitor.data_bytes_));
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  EXPECT_EQ(1, visitor.data_frame_count_);
}

TEST_P(SpdyFramerTest, WindowUpdateFrame) {
  SpdyFramer framer(spdy_version_);
  scoped_ptr<SpdyFrame> frame(framer.SerializeWindowUpdate(
      SpdyWindowUpdateIR(1, 0x12345678)));

  const char kDescription[] = "WINDOW_UPDATE frame, stream 1, delta 0x12345678";
  const unsigned char kV3FrameData[] = {  // Also applies for V2.
    0x80, spdy_version_ch_, 0x00, 0x09,
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x12, 0x34, 0x56, 0x78
  };
  const unsigned char kV4FrameData[] = {
    0x00, 0x04, 0x08, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x12, 0x34, 0x56, 0x78
  };

  if (IsSpdy4()) {
    CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
  } else {
    CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
  }
}

TEST_P(SpdyFramerTest, CreateDataFrame) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "'hello' data frame, no FIN";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x05,
      'h',  'e',  'l',  'l',
      'o'
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x05, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x01,
      'h',  'e',  'l',  'l',
      'o'
    };
    const char bytes[] = "hello";

    SpdyDataIR data_ir(1, StringPiece(bytes, strlen(bytes)));
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    if (IsSpdy4()) {
       CompareFrame(
           kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
       CompareFrame(
           kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }

    SpdyDataIR data_header_ir(1);
    data_header_ir.SetDataShallow(base::StringPiece(bytes, strlen(bytes)));
    frame.reset(framer.SerializeDataFrameHeaderWithPaddingLengthField(
        data_header_ir));
    CompareCharArraysWithHexError(
        kDescription,
        reinterpret_cast<const unsigned char*>(frame->data()),
        framer.GetDataFrameMinimumSize(),
        IsSpdy4() ? kV4FrameData : kV3FrameData,
        framer.GetDataFrameMinimumSize());
  }

  {
    const char kDescription[] = "'hello' data frame with more padding, no FIN";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x05,
      'h',  'e',  'l',  'l',
      'o'
    };

    const unsigned char kV4FrameData[] = {
      0x01, 0x0b, 0x00, 0x18,      // Length = 267.  PAD_HIGH and PAD_LOW set.
      0x00, 0x00, 0x00, 0x01,
      0x01, 0x04,                  // Pad Low and Pad High fields.
      'h',  'e',  'l',  'l',       // Data
      'o',
      // Padding of 260 zeros (so both PAD_HIGH and PAD_LOW fields are used).
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
      '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0', '0',  '0',  '0',  '0', '0',
    };
    const char bytes[] = "hello";

    SpdyDataIR data_ir(1, StringPiece(bytes, strlen(bytes)));
    // 260 zeros and the pad low/high fields make the overall padding to be 262
    // bytes.
    data_ir.set_padding_len(262);
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    if (IsSpdy4()) {
       CompareFrame(
           kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
       CompareFrame(
           kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }

    frame.reset(framer.SerializeDataFrameHeaderWithPaddingLengthField(data_ir));
    CompareCharArraysWithHexError(
        kDescription,
        reinterpret_cast<const unsigned char*>(frame->data()),
        framer.GetDataFrameMinimumSize(),
        IsSpdy4() ? kV4FrameData : kV3FrameData,
        framer.GetDataFrameMinimumSize());
  }

  {
    const char kDescription[] = "'hello' data frame with few padding, no FIN";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x05,
      'h',  'e',  'l',  'l',
      'o'
    };

    const unsigned char kV4FrameData[] = {
      0x00, 0x0d, 0x00, 0x08,      // Length = 13.  PAD_LOW set.
      0x00, 0x00, 0x00, 0x01,
      0x07,                        // Pad Low field.
      'h',  'e',  'l',  'l',       // Data
      'o',
      '0',  '0',  '0',  '0',       // Padding
      '0',  '0',  '0'
    };
    const char bytes[] = "hello";

    SpdyDataIR data_ir(1, StringPiece(bytes, strlen(bytes)));
    // 7 zeros and the pad low field make the overall padding to be 8 bytes.
    data_ir.set_padding_len(8);
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    if (IsSpdy4()) {
       CompareFrame(
           kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
       CompareFrame(
           kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }

  {
    const char kDescription[] =
        "'hello' data frame with 1 byte padding, no FIN";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x05,
      'h',  'e',  'l',  'l',
      'o'
    };

    const unsigned char kV4FrameData[] = {
      0x00, 0x06, 0x00, 0x08,      // Length = 6.  PAD_LOW set.
      0x00, 0x00, 0x00, 0x01,
      0x00,                        // Pad Low field.
      'h',  'e',  'l',  'l',       // Data
      'o',
    };
    const char bytes[] = "hello";

    SpdyDataIR data_ir(1, StringPiece(bytes, strlen(bytes)));
    // The pad low field itself is used for the 1-byte padding and no padding
    // payload is needed.
    data_ir.set_padding_len(1);
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    if (IsSpdy4()) {
       CompareFrame(
           kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
       CompareFrame(
           kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }

    frame.reset(framer.SerializeDataFrameHeaderWithPaddingLengthField(data_ir));
    CompareCharArraysWithHexError(
        kDescription,
        reinterpret_cast<const unsigned char*>(frame->data()),
        framer.GetDataFrameMinimumSize(),
        IsSpdy4() ? kV4FrameData : kV3FrameData,
        framer.GetDataFrameMinimumSize());
  }

  {
    const char kDescription[] = "Data frame with negative data byte, no FIN";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
      0xff
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x01, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x01,
      0xff
    };
    SpdyDataIR data_ir(1, StringPiece("\xff", 1));
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    if (IsSpdy4()) {
       CompareFrame(
           kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
       CompareFrame(
           kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }

  {
    const char kDescription[] = "'hello' data frame, with FIN";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x00, 0x00, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x05,
      'h', 'e', 'l', 'l',
      'o'
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x05, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
      'h',  'e',  'l',  'l',
      'o'
    };
    SpdyDataIR data_ir(1, StringPiece("hello", 5));
    data_ir.set_fin(true);
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    if (IsSpdy4()) {
       CompareFrame(
           kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
       CompareFrame(
           kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }

  {
    const char kDescription[] = "Empty data frame";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x01,
    };
    SpdyDataIR data_ir(1, StringPiece());
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    if (IsSpdy4()) {
       CompareFrame(
           kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
       CompareFrame(
           kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }

    frame.reset(framer.SerializeDataFrameHeaderWithPaddingLengthField(data_ir));
    CompareCharArraysWithHexError(
        kDescription,
        reinterpret_cast<const unsigned char*>(frame->data()),
        framer.GetDataFrameMinimumSize(),
        IsSpdy4() ? kV4FrameData : kV3FrameData,
        framer.GetDataFrameMinimumSize());
  }

  {
    const char kDescription[] = "Data frame with max stream ID";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x7f, 0xff, 0xff, 0xff,
      0x01, 0x00, 0x00, 0x05,
      'h',  'e',  'l',  'l',
      'o'
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x05, 0x00, 0x01,
      0x7f, 0xff, 0xff, 0xff,
      'h',  'e',  'l',  'l',
      'o'
    };
    SpdyDataIR data_ir(0x7fffffff, "hello");
    data_ir.set_fin(true);
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    if (IsSpdy4()) {
       CompareFrame(
           kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
       CompareFrame(
           kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }

  if (!IsSpdy4()) {
    // This test does not apply to SPDY 4 because the max frame size is smaller
    // than 4MB.
    const char kDescription[] = "Large data frame";
    const int kDataSize = 4 * 1024 * 1024;  // 4 MB
    const string kData(kDataSize, 'A');
    const unsigned char kFrameHeader[] = {
      0x00, 0x00, 0x00, 0x01,
      0x01, 0x40, 0x00, 0x00,
    };

    const int kFrameSize = arraysize(kFrameHeader) + kDataSize;
    scoped_ptr<unsigned char[]> expected_frame_data(
        new unsigned char[kFrameSize]);
    memcpy(expected_frame_data.get(), kFrameHeader, arraysize(kFrameHeader));
    memset(expected_frame_data.get() + arraysize(kFrameHeader), 'A', kDataSize);

    SpdyDataIR data_ir(1, StringPiece(kData.data(), kData.size()));
    data_ir.set_fin(true);
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    CompareFrame(kDescription, *frame, expected_frame_data.get(), kFrameSize);
  }
}

TEST_P(SpdyFramerTest, CreateSynStreamUncompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);

  {
    const char kDescription[] = "SYN_STREAM frame, lowest pri, no FIN";

    const unsigned char kPri = IsSpdy2() ? 0xC0 : 0xE0;
    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x20,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
      kPri, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'b',  'a',  'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x2a,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
      kPri, 0x00, 0x00, 0x00,
      0x00, 0x02, 0x00, 0x00,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x00, 0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00, 0x00, 0x03, 'b',
      'a',  'r'
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x17, 0x01, 0x24,  // HEADERS: PRIORITY | END_HEADERS
      0x00, 0x00, 0x00, 0x01,  // Stream 1
      0x00, 0x00, 0x00, 0x00,  // Non-exclusive dependency 0. Weight 0.
      0x00, 0x00, 0x03, 0x62,
      0x61, 0x72, 0x03, 0x66,
      0x6f, 0x6f, 0x00, 0x03,
      0x66, 0x6f, 0x6f, 0x03,
      0x62, 0x61, 0x72,
    };
    SpdySynStreamIR syn_stream(1);
    syn_stream.set_priority(framer.GetLowestPriority());
    syn_stream.SetHeader("bar", "foo");
    syn_stream.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }

  {
    const char kDescription[] =
        "SYN_STREAM frame with a 0-length header name, highest pri, FIN, "
        "max stream ID";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x1D,
      0x7f, 0xff, 0xff, 0xff,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x03, 'b',  'a',
      'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x27,
      0x7f, 0xff, 0xff, 0xff,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x02, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r'
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x14, 0x01, 0x25,  // HEADERS: PRIORITY | FIN | END_HEADERS
      0x7f, 0xff, 0xff, 0xff,  // Stream 0x7fffffff
      0x00, 0x00, 0x00, 0x00,  // Non-exclusive dependency 0. Weight 255.
      0xff, 0x00, 0x00, 0x03,
      0x66, 0x6f, 0x6f, 0x00,
      0x03, 0x66, 0x6f, 0x6f,
      0x03, 0x62, 0x61, 0x72,
    };
    SpdySynStreamIR syn_stream(0x7fffffff);
    syn_stream.set_associated_to_stream_id(0x7fffffff);
    syn_stream.set_priority(framer.GetHighestPriority());
    syn_stream.set_fin(true);
    syn_stream.SetHeader("", "foo");
    syn_stream.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }

  {
    const char kDescription[] =
        "SYN_STREAM frame with a 0-length header val, high pri, FIN, "
        "max stream ID";

    const unsigned char kPri = IsSpdy2() ? 0x40 : 0x20;
    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x1D,
      0x7f, 0xff, 0xff, 0xff,
      0x7f, 0xff, 0xff, 0xff,
      kPri, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x27,
      0x7f, 0xff, 0xff, 0xff,
      0x7f, 0xff, 0xff, 0xff,
      kPri, 0x00, 0x00, 0x00,
      0x00, 0x02, 0x00, 0x00,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x00, 0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00, 0x00, 0x00
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x14, 0x01, 0x25,  // HEADERS: PRIORITY | FIN | END_HEADERS
      0x7f, 0xff, 0xff, 0xff,  // Stream 0x7fffffff
      0x00, 0x00, 0x00, 0x00,  // Non-exclusive dependency 0. Weight 219.
      0xdb, 0x00, 0x03, 0x62,
      0x61, 0x72, 0x03, 0x66,
      0x6f, 0x6f, 0x00, 0x03,
      0x66, 0x6f, 0x6f, 0x00,
    };
    SpdySynStreamIR syn_stream(0x7fffffff);
    syn_stream.set_associated_to_stream_id(0x7fffffff);
    syn_stream.set_priority(1);
    syn_stream.set_fin(true);
    syn_stream.SetHeader("bar", "foo");
    syn_stream.SetHeader("foo", "");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }
}

// TODO(phajdan.jr): Clean up after we no longer need
// to workaround http://crbug.com/139744.
#if !defined(USE_SYSTEM_ZLIB)
TEST_P(SpdyFramerTest, CreateSynStreamCompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(true);

  {
    const char kDescription[] =
        "SYN_STREAM frame, low pri, no FIN";

    const SpdyPriority priority = IsSpdy2() ? 2 : 4;
    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x36,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
      0x80, 0x00, 0x38, 0xea,
      0xdf, 0xa2, 0x51, 0xb2,
      0x62, 0x60, 0x62, 0x60,
      0x4e, 0x4a, 0x2c, 0x62,
      0x60, 0x06, 0x08, 0xa0,
      0xb4, 0xfc, 0x7c, 0x80,
      0x00, 0x62, 0x60, 0x4e,
      0xcb, 0xcf, 0x67, 0x60,
      0x06, 0x08, 0xa0, 0xa4,
      0xc4, 0x22, 0x80, 0x00,
      0x02, 0x00, 0x00, 0x00,
      0xff, 0xff,
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x37,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
      0x80, 0x00, 0x38, 0xEA,
      0xE3, 0xC6, 0xA7, 0xC2,
      0x02, 0xE5, 0x0E, 0x50,
      0xC2, 0x4B, 0x4A, 0x04,
      0xE5, 0x0B, 0x66, 0x80,
      0x00, 0x4A, 0xCB, 0xCF,
      0x07, 0x08, 0x20, 0x10,
      0x95, 0x96, 0x9F, 0x0F,
      0xA2, 0x00, 0x02, 0x28,
      0x29, 0xB1, 0x08, 0x20,
      0x80, 0x00, 0x00, 0x00,
      0x00, 0xFF, 0xFF,
    };
    SpdySynStreamIR syn_stream(1);
    syn_stream.set_priority(priority);
    syn_stream.SetHeader("bar", "foo");
    syn_stream.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      // Deflate compression doesn't apply to HPACK.
    }
  }
}
#endif  // !defined(USE_SYSTEM_ZLIB)

TEST_P(SpdyFramerTest, CreateSynReplyUncompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);

  {
    const char kDescription[] = "SYN_REPLY frame, no FIN";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x1C,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'b',  'a',  'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x24,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x03, 'b',  'a',  'r'
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x12, 0x01, 0x04,  // HEADER: END_HEADERS
      0x00, 0x00, 0x00, 0x01,  // Stream 1
      0x00, 0x03, 0x62, 0x61,  // @.ba
      0x72, 0x03, 0x66, 0x6f,  // r.fo
      0x6f, 0x00, 0x03, 0x66,  // o@.f
      0x6f, 0x6f, 0x03, 0x62,  // oo.b
      0x61, 0x72,              // ar
    };
    SpdySynReplyIR syn_reply(1);
    syn_reply.SetHeader("bar", "foo");
    syn_reply.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynReply(syn_reply));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }

  {
    const char kDescription[] =
        "SYN_REPLY frame with a 0-length header name, FIN, max stream ID";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x02,
      0x01, 0x00, 0x00, 0x19,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x03, 'b',  'a',
      'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x02,
      0x01, 0x00, 0x00, 0x21,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'b',  'a',
      'r'
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x0f, 0x01, 0x05,  // HEADER: FIN | END_HEADERS
      0x7f, 0xff, 0xff, 0xff,  // Stream 0x7fffffff
      0x00, 0x00, 0x03, 0x66,  // @..f
      0x6f, 0x6f, 0x00, 0x03,  // oo@.
      0x66, 0x6f, 0x6f, 0x03,  // foo.
      0x62, 0x61, 0x72,        // bar
    };
    SpdySynReplyIR syn_reply(0x7fffffff);
    syn_reply.set_fin(true);
    syn_reply.SetHeader("", "foo");
    syn_reply.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynReply(syn_reply));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }

  {
    const char kDescription[] =
        "SYN_REPLY frame with a 0-length header val, FIN, max stream ID";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x02,
      0x01, 0x00, 0x00, 0x19,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x02,
      0x01, 0x00, 0x00, 0x21,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x00
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x0f, 0x01, 0x05,  // HEADER: FIN | END_HEADERS
      0x7f, 0xff, 0xff, 0xff,  // Stream 0x7fffffff
      0x00, 0x03, 0x62, 0x61,  // @.ba
      0x72, 0x03, 0x66, 0x6f,  // r.fo
      0x6f, 0x00, 0x03, 0x66,  // o@.f
      0x6f, 0x6f, 0x00,        // oo.
    };
    SpdySynReplyIR syn_reply(0x7fffffff);
    syn_reply.set_fin(true);
    syn_reply.SetHeader("bar", "foo");
    syn_reply.SetHeader("foo", "");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynReply(syn_reply));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }
}

// TODO(phajdan.jr): Clean up after we no longer need
// to workaround http://crbug.com/139744.
#if !defined(USE_SYSTEM_ZLIB)
TEST_P(SpdyFramerTest, CreateSynReplyCompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(true);

  {
    const char kDescription[] = "SYN_REPLY frame, no FIN";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x32,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x38, 0xea,
      0xdf, 0xa2, 0x51, 0xb2,
      0x62, 0x60, 0x62, 0x60,
      0x4e, 0x4a, 0x2c, 0x62,
      0x60, 0x06, 0x08, 0xa0,
      0xb4, 0xfc, 0x7c, 0x80,
      0x00, 0x62, 0x60, 0x4e,
      0xcb, 0xcf, 0x67, 0x60,
      0x06, 0x08, 0xa0, 0xa4,
      0xc4, 0x22, 0x80, 0x00,
      0x02, 0x00, 0x00, 0x00,
      0xff, 0xff,
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x31,
      0x00, 0x00, 0x00, 0x01,
      0x38, 0xea, 0xe3, 0xc6,
      0xa7, 0xc2, 0x02, 0xe5,
      0x0e, 0x50, 0xc2, 0x4b,
      0x4a, 0x04, 0xe5, 0x0b,
      0x66, 0x80, 0x00, 0x4a,
      0xcb, 0xcf, 0x07, 0x08,
      0x20, 0x10, 0x95, 0x96,
      0x9f, 0x0f, 0xa2, 0x00,
      0x02, 0x28, 0x29, 0xb1,
      0x08, 0x20, 0x80, 0x00,
      0x00, 0x00, 0x00, 0xff,
      0xff,
    };
    SpdySynReplyIR syn_reply(1);
    syn_reply.SetHeader("bar", "foo");
    syn_reply.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynReply(syn_reply));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      // Deflate compression doesn't apply to HPACK.
    }
  }
}
#endif  // !defined(USE_SYSTEM_ZLIB)

TEST_P(SpdyFramerTest, CreateRstStream) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "RST_STREAM frame";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x03,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x07, 0x03, 0x00,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
      0x52, 0x53, 0x54
    };
    SpdyRstStreamIR rst_stream(1, RST_STREAM_PROTOCOL_ERROR, "RST");
    scoped_ptr<SpdyFrame> frame(framer.SerializeRstStream(rst_stream));
    if (IsSpdy4()) {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }

  {
    const char kDescription[] = "RST_STREAM frame with max stream ID";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x03,
      0x00, 0x00, 0x00, 0x08,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x01,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x04, 0x03, 0x00,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x01,
    };
    SpdyRstStreamIR rst_stream(0x7FFFFFFF,
                               RST_STREAM_PROTOCOL_ERROR,
                               "");
    scoped_ptr<SpdyFrame> frame(framer.SerializeRstStream(rst_stream));
    if (IsSpdy4()) {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }

  {
    const char kDescription[] = "RST_STREAM frame with max status code";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x03,
      0x00, 0x00, 0x00, 0x08,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x06,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x04, 0x03, 0x00,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x06,
    };
    SpdyRstStreamIR rst_stream(0x7FFFFFFF,
                               RST_STREAM_INTERNAL_ERROR,
                               "");
    scoped_ptr<SpdyFrame> frame(framer.SerializeRstStream(rst_stream));
    if (IsSpdy4()) {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }
}

TEST_P(SpdyFramerTest, CreateSettings) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "Network byte order SETTINGS frame";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x0c,
      0x00, 0x00, 0x00, 0x01,
      0x07, 0x00, 0x00, 0x01,
      0x0a, 0x0b, 0x0c, 0x0d,
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x0c,
      0x00, 0x00, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x07,
      0x0a, 0x0b, 0x0c, 0x0d,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x05, 0x04, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x04, 0x0a, 0x0b, 0x0c,
      0x0d,
    };

    uint32 kValue = 0x0a0b0c0d;
    SpdySettingsIR settings_ir;

    SpdySettingsFlags kFlags = static_cast<SpdySettingsFlags>(0x01);
    SpdySettingsIds kId = SETTINGS_INITIAL_WINDOW_SIZE;
    SettingsMap settings;
    settings[kId] = SettingsFlagsAndValue(kFlags, kValue);
    EXPECT_EQ(kFlags, settings[kId].first);
    EXPECT_EQ(kValue, settings[kId].second);
    settings_ir.AddSetting(kId,
                           kFlags & SETTINGS_FLAG_PLEASE_PERSIST,
                           kFlags & SETTINGS_FLAG_PERSISTED,
                           kValue);

    scoped_ptr<SpdyFrame> frame(framer.SerializeSettings(settings_ir));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }

  {
    const char kDescription[] = "Basic SETTINGS frame";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x24,
      0x00, 0x00, 0x00, 0x04,
      0x01, 0x00, 0x00, 0x00,  // 1st Setting
      0x00, 0x00, 0x00, 0x05,
      0x02, 0x00, 0x00, 0x00,  // 2nd Setting
      0x00, 0x00, 0x00, 0x06,
      0x03, 0x00, 0x00, 0x00,  // 3rd Setting
      0x00, 0x00, 0x00, 0x07,
      0x04, 0x00, 0x00, 0x00,  // 4th Setting
      0x00, 0x00, 0x00, 0x08,
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x24,
      0x00, 0x00, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x01,  // 1st Setting
      0x00, 0x00, 0x00, 0x05,
      0x00, 0x00, 0x00, 0x02,  // 2nd Setting
      0x00, 0x00, 0x00, 0x06,
      0x00, 0x00, 0x00, 0x03,  // 3rd Setting
      0x00, 0x00, 0x00, 0x07,
      0x00, 0x00, 0x00, 0x04,  // 4th Setting
      0x00, 0x00, 0x00, 0x08,
    };
    // These end up seemingly out of order because of the way that our internal
    // ordering for settings_ir works. HTTP2 has no requirement on ordering on
    // the wire.
    const unsigned char kV4FrameData[] = {
      0x00, 0x14, 0x04, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x03,                    // 3rd Setting
      0x00, 0x00, 0x00, 0x07,
      0x04,                    // 4th Setting
      0x00, 0x00, 0x00, 0x08,
      0x01,                    // 1st Setting
      0x00, 0x00, 0x00, 0x05,
      0x02,                    // 2nd Setting
      0x00, 0x00, 0x00, 0x06,
    };

    SpdySettingsIR settings_ir;
    settings_ir.AddSetting(SpdyConstants::ParseSettingId(spdy_version_, 1),
                           false,  // persist
                           false,  // persisted
                           5);
    settings_ir.AddSetting(SpdyConstants::ParseSettingId(spdy_version_, 2),
                           false,  // persist
                           false,  // persisted
                           6);
    settings_ir.AddSetting(SpdyConstants::ParseSettingId(spdy_version_, 3),
                           false,  // persist
                           false,  // persisted
                           7);
    settings_ir.AddSetting(SpdyConstants::ParseSettingId(spdy_version_, 4),
                           false,  // persist
                           false,  // persisted
                           8);
    scoped_ptr<SpdyFrame> frame(framer.SerializeSettings(settings_ir));

    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }

  {
    const char kDescription[] = "Empty SETTINGS frame";

    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x00,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x00, 0x04, 0x00,
      0x00, 0x00, 0x00, 0x00,
    };
    SpdySettingsIR settings_ir;
    scoped_ptr<SpdyFrame> frame(framer.SerializeSettings(settings_ir));
    if (IsSpdy4()) {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }
}

TEST_P(SpdyFramerTest, CreatePingFrame) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "PING frame";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x06,
      0x00, 0x00, 0x00, 0x04,
      0x12, 0x34, 0x56, 0x78,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x08, 0x06, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x12, 0x34, 0x56, 0x78,
      0x9a, 0xbc, 0xde, 0xff,
    };
    const unsigned char kV4FrameDataWithAck[] = {
      0x00, 0x08, 0x06, 0x01,
      0x00, 0x00, 0x00, 0x00,
      0x12, 0x34, 0x56, 0x78,
      0x9a, 0xbc, 0xde, 0xff,
    };
    scoped_ptr<SpdyFrame> frame;
    if (IsSpdy4()) {
      const SpdyPingId kPingId = 0x123456789abcdeffULL;
      SpdyPingIR ping_ir(kPingId);
      // Tests SpdyPingIR when the ping is not an ack.
      ASSERT_FALSE(ping_ir.is_ack());
      frame.reset(framer.SerializePing(ping_ir));
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));

      // Tests SpdyPingIR when the ping is an ack.
      ping_ir.set_is_ack(true);
      frame.reset(framer.SerializePing(ping_ir));
      CompareFrame(kDescription, *frame,
                   kV4FrameDataWithAck, arraysize(kV4FrameDataWithAck));

    } else {
      frame.reset(framer.SerializePing(SpdyPingIR(0x12345678ull)));
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }
}

TEST_P(SpdyFramerTest, CreateGoAway) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "GOAWAY frame";
    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x07,
      0x00, 0x00, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x00,  // Stream Id
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x07,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x00,  // Stream Id
      0x00, 0x00, 0x00, 0x00,  // Status
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x0a, 0x07, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,  // Stream id
      0x00, 0x00, 0x00, 0x00,  // Status
      0x47, 0x41,              // Opaque Description
    };
    SpdyGoAwayIR goaway_ir(0, GOAWAY_OK, "GA");
    scoped_ptr<SpdyFrame> frame(framer.SerializeGoAway(goaway_ir));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }

  {
    const char kDescription[] = "GOAWAY frame with max stream ID, status";
    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x07,
      0x00, 0x00, 0x00, 0x04,
      0x7f, 0xff, 0xff, 0xff,  // Stream Id
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x07,
      0x00, 0x00, 0x00, 0x08,
      0x7f, 0xff, 0xff, 0xff,  // Stream Id
      0x00, 0x00, 0x00, 0x01,  // Status: PROTOCOL_ERROR.
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x0a, 0x07, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x7f, 0xff, 0xff, 0xff,  // Stream Id
      0x00, 0x00, 0x00, 0x02,  // Status: INTERNAL_ERROR.
      0x47, 0x41,              // Opaque Description
    };
    SpdyGoAwayIR goaway_ir(0x7FFFFFFF, GOAWAY_INTERNAL_ERROR, "GA");
    scoped_ptr<SpdyFrame> frame(framer.SerializeGoAway(goaway_ir));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }
}

TEST_P(SpdyFramerTest, CreateHeadersUncompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);

  {
    const char kDescription[] = "HEADERS frame, no FIN";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x1C,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'b',  'a',  'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x24,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x03, 'b',  'a',  'r'
    };
    const unsigned char kV4FrameData[] = {
       0x00, 0x12, 0x01, 0x04,  // Headers: END_HEADERS
       0x00, 0x00, 0x00, 0x01,  // Stream 1
       0x00, 0x03, 0x62, 0x61,  // @.ba
       0x72, 0x03, 0x66, 0x6f,  // r.fo
       0x6f, 0x00, 0x03, 0x66,  // o@.f
       0x6f, 0x6f, 0x03, 0x62,  // oo.b
       0x61, 0x72,              // ar
    };
    SpdyHeadersIR headers_ir(1);
    headers_ir.SetHeader("bar", "foo");
    headers_ir.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeHeaders(headers_ir));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }

  {
    const char kDescription[] =
        "HEADERS frame with a 0-length header name, FIN, max stream ID";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x08,
      0x01, 0x00, 0x00, 0x19,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x03, 'b',  'a',
      'r'
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x08,
      0x01, 0x00, 0x00, 0x21,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'b',  'a',
      'r'
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x0f, 0x01, 0x05,  // HEADER: FIN | END_HEADERS
      0x7f, 0xff, 0xff, 0xff,  // Stream 0x7fffffff
      0x00, 0x00, 0x03, 0x66,  // @..f
      0x6f, 0x6f, 0x00, 0x03,  // oo@.
      0x66, 0x6f, 0x6f, 0x03,  // foo.
      0x62, 0x61, 0x72,        // bar
    };
    SpdyHeadersIR headers_ir(0x7fffffff);
    headers_ir.set_fin(true);
    headers_ir.SetHeader("", "foo");
    headers_ir.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeHeaders(headers_ir));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }

  {
    const char kDescription[] =
        "HEADERS frame with a 0-length header val, FIN, max stream ID";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x08,
      0x01, 0x00, 0x00, 0x19,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x08,
      0x01, 0x00, 0x00, 0x21,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'b',  'a',  'r',  0x00,
      0x00, 0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x00,
      0x00, 0x03, 'f',  'o',
      'o',  0x00, 0x00, 0x00,
      0x00
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x0f, 0x01, 0x05,  // HEADER: FIN | END_HEADERS
      0x7f, 0xff, 0xff, 0xff,  // Stream 0x7fffffff
      0x00, 0x03, 0x62, 0x61,  // @.ba
      0x72, 0x03, 0x66, 0x6f,  // r.fo
      0x6f, 0x00, 0x03, 0x66,  // o@.f
      0x6f, 0x6f, 0x00,        // oo.
    };
    SpdyHeadersIR headers_ir(0x7fffffff);
    headers_ir.set_fin(true);
     headers_ir.SetHeader("bar", "foo");
    headers_ir.SetHeader("foo", "");
    scoped_ptr<SpdyFrame> frame(framer.SerializeHeaders(headers_ir));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    }
  }
}

// TODO(phajdan.jr): Clean up after we no longer need
// to workaround http://crbug.com/139744.
#if !defined(USE_SYSTEM_ZLIB)
TEST_P(SpdyFramerTest, CreateHeadersCompressed) {
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(true);

  {
    const char kDescription[] = "HEADERS frame, no FIN";

    const unsigned char kV2FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x32,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x38, 0xea,
      0xdf, 0xa2, 0x51, 0xb2,
      0x62, 0x60, 0x62, 0x60,
      0x4e, 0x4a, 0x2c, 0x62,
      0x60, 0x06, 0x08, 0xa0,
      0xb4, 0xfc, 0x7c, 0x80,
      0x00, 0x62, 0x60, 0x4e,
      0xcb, 0xcf, 0x67, 0x60,
      0x06, 0x08, 0xa0, 0xa4,
      0xc4, 0x22, 0x80, 0x00,
      0x02, 0x00, 0x00, 0x00,
      0xff, 0xff,
    };
    const unsigned char kV3FrameData[] = {
      0x80, spdy_version_ch_, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x31,
      0x00, 0x00, 0x00, 0x01,
      0x38, 0xea, 0xe3, 0xc6,
      0xa7, 0xc2, 0x02, 0xe5,
      0x0e, 0x50, 0xc2, 0x4b,
      0x4a, 0x04, 0xe5, 0x0b,
      0x66, 0x80, 0x00, 0x4a,
      0xcb, 0xcf, 0x07, 0x08,
      0x20, 0x10, 0x95, 0x96,
      0x9f, 0x0f, 0xa2, 0x00,
      0x02, 0x28, 0x29, 0xb1,
      0x08, 0x20, 0x80, 0x00,
      0x00, 0x00, 0x00, 0xff,
      0xff,
    };
    SpdyHeadersIR headers_ir(1);
    headers_ir.SetHeader("bar", "foo");
    headers_ir.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeHeaders(headers_ir));
    if (IsSpdy2()) {
      CompareFrame(kDescription, *frame, kV2FrameData, arraysize(kV2FrameData));
    } else if (IsSpdy3()) {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    } else {
      // Deflate compression doesn't apply to HPACK.
    }
  }
}
#endif  // !defined(USE_SYSTEM_ZLIB)

TEST_P(SpdyFramerTest, CreateWindowUpdate) {
  SpdyFramer framer(spdy_version_);

  {
    const char kDescription[] = "WINDOW_UPDATE frame";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x09,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x04, 0x08, 0x00,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
    };
    scoped_ptr<SpdyFrame> frame(
        framer.SerializeWindowUpdate(SpdyWindowUpdateIR(1, 1)));
    if (IsSpdy4()) {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }

  {
    const char kDescription[] = "WINDOW_UPDATE frame with max stream ID";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x09,
      0x00, 0x00, 0x00, 0x08,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x01,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x04, 0x08, 0x00,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x01,
    };
    scoped_ptr<SpdyFrame> frame(framer.SerializeWindowUpdate(
        SpdyWindowUpdateIR(0x7FFFFFFF, 1)));
    if (IsSpdy4()) {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }

  {
    const char kDescription[] = "WINDOW_UPDATE frame with max window delta";
    const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x09,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x01,
      0x7f, 0xff, 0xff, 0xff,
    };
    const unsigned char kV4FrameData[] = {
      0x00, 0x04, 0x08, 0x00,
      0x00, 0x00, 0x00, 0x01,
      0x7f, 0xff, 0xff, 0xff,
    };
    scoped_ptr<SpdyFrame> frame(framer.SerializeWindowUpdate(
        SpdyWindowUpdateIR(1, 0x7FFFFFFF)));
    if (IsSpdy4()) {
      CompareFrame(kDescription, *frame, kV4FrameData, arraysize(kV4FrameData));
    } else {
      CompareFrame(kDescription, *frame, kV3FrameData, arraysize(kV3FrameData));
    }
  }
}

TEST_P(SpdyFramerTest, SerializeBlocked) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  SpdyFramer framer(spdy_version_);

  const char kDescription[] = "BLOCKED frame";
  const unsigned char kType = static_cast<unsigned char>(
      SpdyConstants::SerializeFrameType(spdy_version_, BLOCKED));
  const unsigned char kFrameData[] = {
    0x00, 0x00, kType, 0x00,
    0x00, 0x00, 0x00,  0x00,
  };
  SpdyBlockedIR blocked_ir(0);
  scoped_ptr<SpdySerializedFrame> frame(framer.SerializeFrame(blocked_ir));
  CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
}

TEST_P(SpdyFramerTest, CreateBlocked) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  SpdyFramer framer(spdy_version_);

  const char kDescription[] = "BLOCKED frame";
  const SpdyStreamId kStreamId = 3;

  scoped_ptr<SpdySerializedFrame> frame_serialized(
      framer.SerializeBlocked(SpdyBlockedIR(kStreamId)));
  SpdyBlockedIR blocked_ir(kStreamId);
  scoped_ptr<SpdySerializedFrame> frame_created(
      framer.SerializeFrame(blocked_ir));

  CompareFrames(kDescription, *frame_serialized, *frame_created);
}

TEST_P(SpdyFramerTest, CreatePushPromiseUncompressed) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);
  const char kDescription[] = "PUSH_PROMISE frame";

  const unsigned char kFrameData[] = {
    0x00, 0x16, 0x05, 0x04,  // PUSH_PROMISE: END_HEADERS
    0x00, 0x00, 0x00, 0x2a,  // Stream 42
    0x00, 0x00, 0x00, 0x39,  // Promised stream 57
    0x00, 0x03, 0x62, 0x61,  // @.ba
    0x72, 0x03, 0x66, 0x6f,  // r.fo
    0x6f, 0x00, 0x03, 0x66,  // o@.f
    0x6f, 0x6f, 0x03, 0x62,  // oo.b
    0x61, 0x72,              // ar
  };

  SpdyPushPromiseIR push_promise(42, 57);
  push_promise.SetHeader("bar", "foo");
  push_promise.SetHeader("foo", "bar");
  scoped_ptr<SpdySerializedFrame> frame(
    framer.SerializePushPromise(push_promise));
  CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
}

TEST_P(SpdyFramerTest, CreateAltSvc) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  SpdyFramer framer(spdy_version_);

  const char kDescription[] = "ALTSVC frame";
  const unsigned char kType = static_cast<unsigned char>(
      SpdyConstants::SerializeFrameType(spdy_version_, ALTSVC));
  const unsigned char kFrameData[] = {
    0x00, 0x17, kType, 0x00,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x05,
    0x01, 0xbb, 0x00, 0x04,  // Port = 443
    'p',  'i',  'd',  '1',   // Protocol-ID
    0x04, 'h',  'o',  's',
    't',  'o',  'r',  'i',
    'g',  'i',  'n',
  };
  SpdyAltSvcIR altsvc_ir(3);
  altsvc_ir.set_max_age(5);
  altsvc_ir.set_port(443);
  altsvc_ir.set_protocol_id("pid1");
  altsvc_ir.set_host("host");
  altsvc_ir.set_origin("origin");
  scoped_ptr<SpdySerializedFrame> frame(framer.SerializeFrame(altsvc_ir));
  CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
}

TEST_P(SpdyFramerTest, ReadCompressedSynStreamHeaderBlock) {
  SpdyFramer framer(spdy_version_);
  SpdySynStreamIR syn_stream(1);
  syn_stream.set_priority(1);
  syn_stream.SetHeader("aa", "vv");
  syn_stream.SetHeader("bb", "ww");
  SpdyHeaderBlock headers = syn_stream.name_value_block();
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSynStream(syn_stream));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
}

TEST_P(SpdyFramerTest, ReadCompressedSynReplyHeaderBlock) {
  SpdyFramer framer(spdy_version_);
  SpdySynReplyIR syn_reply(1);
  syn_reply.SetHeader("alpha", "beta");
  syn_reply.SetHeader("gamma", "delta");
  SpdyHeaderBlock headers = syn_reply.name_value_block();
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSynReply(syn_reply));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  if (IsSpdy4()) {
    EXPECT_EQ(0, visitor.syn_reply_frame_count_);
    EXPECT_EQ(1, visitor.headers_frame_count_);
  } else {
    EXPECT_EQ(1, visitor.syn_reply_frame_count_);
    EXPECT_EQ(0, visitor.headers_frame_count_);
  }
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
}

TEST_P(SpdyFramerTest, ReadCompressedHeadersHeaderBlock) {
  SpdyFramer framer(spdy_version_);
  SpdyHeadersIR headers_ir(1);
  headers_ir.SetHeader("alpha", "beta");
  headers_ir.SetHeader("gamma", "delta");
  SpdyHeaderBlock headers = headers_ir.name_value_block();
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeHeaders(headers_ir));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(1, visitor.headers_frame_count_);
  // control_frame_header_data_count_ depends on the random sequence
  // produced by rand(), so adding, removing or running single tests
  // alters this value.  The best we can do is assert that it happens
  // at least twice.
  EXPECT_LE(2, visitor.control_frame_header_data_count_);
  EXPECT_EQ(1, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(0, visitor.zero_length_data_frame_count_);
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
}

TEST_P(SpdyFramerTest, ReadCompressedHeadersHeaderBlockWithHalfClose) {
  SpdyFramer framer(spdy_version_);
  SpdyHeadersIR headers_ir(1);
  headers_ir.set_fin(true);
  headers_ir.SetHeader("alpha", "beta");
  headers_ir.SetHeader("gamma", "delta");
  SpdyHeaderBlock headers = headers_ir.name_value_block();
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeHeaders(headers_ir));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(1, visitor.headers_frame_count_);
  // control_frame_header_data_count_ depends on the random sequence
  // produced by rand(), so adding, removing or running single tests
  // alters this value.  The best we can do is assert that it happens
  // at least twice.
  EXPECT_LE(2, visitor.control_frame_header_data_count_);
  EXPECT_EQ(1, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
}

TEST_P(SpdyFramerTest, ControlFrameAtMaxSizeLimit) {
  if (spdy_version_ > SPDY3) {
    // TODO(jgraettinger): This test setup doesn't work with HPACK.
    return;
  }
  // First find the size of the header value in order to just reach the control
  // frame max size.
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);
  SpdySynStreamIR syn_stream(1);
  syn_stream.set_priority(1);
  syn_stream.SetHeader("aa", "");
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSynStream(syn_stream));
  const size_t kBigValueSize =
      framer.GetControlFrameBufferMaxSize() - control_frame->size();

  // Create a frame at exactly that size.
  string big_value(kBigValueSize, 'x');
  syn_stream.SetHeader("aa", big_value);
  control_frame.reset(framer.SerializeSynStream(syn_stream));
  EXPECT_TRUE(control_frame.get() != NULL);
  EXPECT_EQ(framer.GetControlFrameBufferMaxSize(), control_frame->size());

  TestSpdyVisitor visitor(spdy_version_);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_TRUE(visitor.header_buffer_valid_);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(1, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(0, visitor.zero_length_data_frame_count_);
  EXPECT_LT(kBigValueSize, visitor.header_buffer_length_);
}

TEST_P(SpdyFramerTest, ControlFrameTooLarge) {
  if (spdy_version_ > SPDY3) {
    // TODO(jgraettinger): This test setup doesn't work with HPACK.
    return;
  }
  // First find the size of the header value in order to just reach the control
  // frame max size.
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);
  SpdySynStreamIR syn_stream(1);
  syn_stream.SetHeader("aa", "");
  syn_stream.set_priority(1);
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSynStream(syn_stream));
  const size_t kBigValueSize =
      framer.GetControlFrameBufferMaxSize() - control_frame->size() + 1;

  // Create a frame at exatly that size.
  string big_value(kBigValueSize, 'x');
  syn_stream.SetHeader("aa", big_value);
  // Upstream branches here and wraps SPDY4 with EXPECT_DEBUG_DFATAL. We
  // neither support that in Chromium, nor do we use the same DFATAL (see
  // SpdyFrameBuilder::WriteFramePrefix()).
  control_frame.reset(framer.SerializeSynStream(syn_stream));

  EXPECT_TRUE(control_frame.get() != NULL);
  EXPECT_EQ(framer.GetControlFrameBufferMaxSize() + 1,
            control_frame->size());

  TestSpdyVisitor visitor(spdy_version_);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_FALSE(visitor.header_buffer_valid_);
  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(SpdyFramer::SPDY_CONTROL_PAYLOAD_TOO_LARGE,
            visitor.framer_.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
  EXPECT_EQ(0, visitor.syn_frame_count_);
  EXPECT_EQ(0u, visitor.header_buffer_length_);
}

TEST_P(SpdyFramerTest, TooLargeHeadersFrameUsesContinuation) {
  if (spdy_version_ <= SPDY3) {
    return;
  }
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);
  SpdyHeadersIR headers(1);

  // Exact payload length will change with HPACK, but this should be long
  // enough to cause an overflow.
  const size_t kBigValueSize = framer.GetControlFrameBufferMaxSize();
  string big_value(kBigValueSize, 'x');
  headers.SetHeader("aa", big_value);
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeHeaders(headers));
  EXPECT_TRUE(control_frame.get() != NULL);
  EXPECT_GT(control_frame->size(), framer.GetControlFrameBufferMaxSize());

  TestSpdyVisitor visitor(spdy_version_);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_TRUE(visitor.header_buffer_valid_);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(1, visitor.continuation_count_);
  EXPECT_EQ(1, visitor.zero_length_control_frame_header_data_count_);
}

TEST_P(SpdyFramerTest, TooLargePushPromiseFrameUsesContinuation) {
  if (spdy_version_ <= SPDY3) {
    return;
  }
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);
  SpdyPushPromiseIR push_promise(1, 2);

  // Exact payload length will change with HPACK, but this should be long
  // enough to cause an overflow.
  const size_t kBigValueSize = framer.GetControlFrameBufferMaxSize();
  string big_value(kBigValueSize, 'x');
  push_promise.SetHeader("aa", big_value);
  scoped_ptr<SpdyFrame> control_frame(
      framer.SerializePushPromise(push_promise));
  EXPECT_TRUE(control_frame.get() != NULL);
  EXPECT_GT(control_frame->size(), framer.GetControlFrameBufferMaxSize());

  TestSpdyVisitor visitor(spdy_version_);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_TRUE(visitor.header_buffer_valid_);
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.push_promise_frame_count_);
  EXPECT_EQ(1, visitor.continuation_count_);
  EXPECT_EQ(1, visitor.zero_length_control_frame_header_data_count_);
}

// Check that the framer stops delivering header data chunks once the visitor
// declares it doesn't want any more. This is important to guard against
// "zip bomb" types of attacks.
TEST_P(SpdyFramerTest, ControlFrameMuchTooLarge) {
  const size_t kHeaderBufferChunks = 4;
  const size_t kHeaderBufferSize =
      TestSpdyVisitor::header_data_chunk_max_size() * kHeaderBufferChunks;
  const size_t kBigValueSize = kHeaderBufferSize * 2;
  string big_value(kBigValueSize, 'x');
  SpdyFramer framer(spdy_version_);
  SpdySynStreamIR syn_stream(1);
  syn_stream.set_priority(1);
  syn_stream.set_fin(true);
  syn_stream.SetHeader("aa", big_value);
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSynStream(syn_stream));
  EXPECT_TRUE(control_frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.set_header_buffer_size(kHeaderBufferSize);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_FALSE(visitor.header_buffer_valid_);
  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(SpdyFramer::SPDY_CONTROL_PAYLOAD_TOO_LARGE,
            visitor.framer_.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());

  // The framer should have stoped delivering chunks after the visitor
  // signaled "stop" by returning false from OnControlFrameHeaderData().
  //
  // control_frame_header_data_count_ depends on the random sequence
  // produced by rand(), so adding, removing or running single tests
  // alters this value.  The best we can do is assert that it happens
  // at least kHeaderBufferChunks + 1.
  EXPECT_LE(kHeaderBufferChunks + 1,
            static_cast<unsigned>(visitor.control_frame_header_data_count_));
  EXPECT_EQ(0, visitor.zero_length_control_frame_header_data_count_);

  // The framer should not have sent half-close to the visitor.
  EXPECT_EQ(0, visitor.zero_length_data_frame_count_);
}

TEST_P(SpdyFramerTest, DecompressCorruptHeaderBlock) {
  if (spdy_version_ > SPDY3) {
    // Deflate compression doesn't apply to HPACK.
    return;
  }
  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);
  // Construct a SYN_STREAM control frame without compressing the header block,
  // and have the framer try to decompress it. This will cause the framer to
  // deal with a decompression error.
  SpdySynStreamIR syn_stream(1);
  syn_stream.set_priority(1);
  syn_stream.SetHeader("aa", "alpha beta gamma delta");
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSynStream(syn_stream));
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(SpdyFramer::SPDY_DECOMPRESS_FAILURE, visitor.framer_.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
  EXPECT_EQ(0u, visitor.header_buffer_length_);
}

TEST_P(SpdyFramerTest, ControlFrameSizesAreValidated) {
  SpdyFramer framer(spdy_version_);
  // Create a GoAway frame that has a few extra bytes at the end.
  // We create enough overhead to overflow the framer's control frame buffer.
  ASSERT_GE(250u, SpdyFramer::kControlFrameBufferSize);
  const unsigned char length =  1 + SpdyFramer::kControlFrameBufferSize;
  const unsigned char kV3FrameData[] = {  // Also applies for V2.
    0x80, spdy_version_ch_, 0x00, 0x07,
    0x00, 0x00, 0x00, length,
    0x00, 0x00, 0x00, 0x00,  // Stream ID
    0x00, 0x00, 0x00, 0x00,  // Status
  };

  // SPDY version 4 and up GOAWAY frames are only bound to a minimal length,
  // since it may carry opaque data. Verify that minimal length is tested.
  const unsigned char less_than_min_length =
      framer.GetGoAwayMinimumSize() - framer.GetControlFrameHeaderSize() - 1;
  const unsigned char kV4FrameData[] = {
    0x00, static_cast<uint8>(less_than_min_length), 0x07, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,  // Stream Id
    0x00, 0x00, 0x00, 0x00,  // Status
  };
  const size_t pad_length =
      length + framer.GetControlFrameHeaderSize() -
      (IsSpdy4() ? sizeof(kV4FrameData) : sizeof(kV3FrameData));
  string pad('A', pad_length);
  TestSpdyVisitor visitor(spdy_version_);

  if (IsSpdy4()) {
    visitor.SimulateInFramer(kV4FrameData, sizeof(kV4FrameData));
  } else {
    visitor.SimulateInFramer(kV3FrameData, sizeof(kV3FrameData));
  }
  visitor.SimulateInFramer(
      reinterpret_cast<const unsigned char*>(pad.c_str()),
      pad.length());

  EXPECT_EQ(1, visitor.error_count_);  // This generated an error.
  EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME,
            visitor.framer_.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
  EXPECT_EQ(0, visitor.goaway_count_);  // Frame not parsed.
}

TEST_P(SpdyFramerTest, ReadZeroLenSettingsFrame) {
  SpdyFramer framer(spdy_version_);
  SpdySettingsIR settings_ir;
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSettings(settings_ir));
  SetFrameLength(control_frame.get(), 0, spdy_version_);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      framer.GetControlFrameHeaderSize());
  if (spdy_version_ <= SPDY3) {
    // Should generate an error, since zero-len settings frames are unsupported.
    EXPECT_EQ(1, visitor.error_count_);
  } else {
    // Zero-len settings frames are permitted as of SPDY 4.
    EXPECT_EQ(0, visitor.error_count_);
  }
}

// Tests handling of SETTINGS frames with invalid length.
TEST_P(SpdyFramerTest, ReadBogusLenSettingsFrame) {
  SpdyFramer framer(spdy_version_);
  SpdySettingsIR settings_ir;

  // Add a setting to pad the frame so that we don't get a buffer overflow when
  // calling SimulateInFramer() below.
  settings_ir.AddSetting(SETTINGS_INITIAL_WINDOW_SIZE,
                         false,
                         false,
                         0x00000002);
  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSettings(settings_ir));
  const size_t kNewLength = 14;
  SetFrameLength(control_frame.get(), kNewLength, spdy_version_);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      framer.GetControlFrameHeaderSize() + kNewLength);
  // Should generate an error, since its not possible to have a
  // settings frame of length kNewLength.
  EXPECT_EQ(1, visitor.error_count_);
}

// Tests handling of SETTINGS frames larger than the frame buffer size.
TEST_P(SpdyFramerTest, ReadLargeSettingsFrame) {
  SpdyFramer framer(spdy_version_);
  SpdySettingsIR settings_ir;
  settings_ir.AddSetting(SpdyConstants::ParseSettingId(spdy_version_, 1),
                         false,  // persist
                         false,  // persisted
                         5);
  settings_ir.AddSetting(SpdyConstants::ParseSettingId(spdy_version_, 2),
                         false,  // persist
                         false,  // persisted
                         6);
  settings_ir.AddSetting(SpdyConstants::ParseSettingId(spdy_version_, 3),
                         false,  // persist
                         false,  // persisted
                         7);

  scoped_ptr<SpdyFrame> control_frame(framer.SerializeSettings(settings_ir));
  EXPECT_LT(SpdyFramer::kControlFrameBufferSize,
            control_frame->size());
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;

  // Read all at once.
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(3, visitor.setting_count_);
  if (spdy_version_ > SPDY3) {
    EXPECT_EQ(1, visitor.settings_ack_sent_);
  }

  // Read data in small chunks.
  size_t framed_data = 0;
  size_t unframed_data = control_frame->size();
  size_t kReadChunkSize = 5;  // Read five bytes at a time.
  while (unframed_data > 0) {
    size_t to_read = min(kReadChunkSize, unframed_data);
    visitor.SimulateInFramer(
        reinterpret_cast<unsigned char*>(control_frame->data() + framed_data),
        to_read);
    unframed_data -= to_read;
    framed_data += to_read;
  }
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(3 * 2, visitor.setting_count_);
  if (spdy_version_ > SPDY3) {
    EXPECT_EQ(2, visitor.settings_ack_sent_);
  }
}

// Tests handling of SETTINGS frame with duplicate entries.
TEST_P(SpdyFramerTest, ReadDuplicateSettings) {
  SpdyFramer framer(spdy_version_);

  const unsigned char kV2FrameData[] = {
    0x80, spdy_version_ch_, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x00, 0x03,
    0x01, 0x00, 0x00, 0x00,  // 1st Setting
    0x00, 0x00, 0x00, 0x02,
    0x01, 0x00, 0x00, 0x00,  // 2nd (duplicate) Setting
    0x00, 0x00, 0x00, 0x03,
    0x03, 0x00, 0x00, 0x00,  // 3rd (unprocessed) Setting
    0x00, 0x00, 0x00, 0x03,
  };
  const unsigned char kV3FrameData[] = {
    0x80, spdy_version_ch_, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x01,  // 1st Setting
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x01,  // 2nd (duplicate) Setting
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x03,  // 3rd (unprocessed) Setting
    0x00, 0x00, 0x00, 0x03,
  };
  const unsigned char kV4FrameData[] = {
    0x00, 0x0f, 0x04, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x01,  // 1st Setting
    0x00, 0x00, 0x00, 0x02,
    0x01,  // 2nd (duplicate) Setting
    0x00, 0x00, 0x00, 0x03,
    0x03,  // 3rd (unprocessed) Setting
    0x00, 0x00, 0x00, 0x03,
  };

  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  if (IsSpdy2()) {
    visitor.SimulateInFramer(kV2FrameData, sizeof(kV2FrameData));
  } else if (IsSpdy3()) {
    visitor.SimulateInFramer(kV3FrameData, sizeof(kV3FrameData));
  } else {
    visitor.SimulateInFramer(kV4FrameData, sizeof(kV4FrameData));
  }

  if (!IsSpdy4()) {
    EXPECT_EQ(1, visitor.setting_count_);
    EXPECT_EQ(1, visitor.error_count_);
  } else {
    // In SPDY 4+, duplicate settings are allowed;
    // each setting replaces the previous value for that setting.
    EXPECT_EQ(3, visitor.setting_count_);
    EXPECT_EQ(0, visitor.error_count_);
    EXPECT_EQ(1, visitor.settings_ack_sent_);
  }
}

// Tests handling of SETTINGS_COMPRESS_DATA.
TEST_P(SpdyFramerTest, AcceptSettingsCompressData) {
  if (!IsSpdy4()) { return; }
  SpdyFramer framer(spdy_version_);

  const unsigned char kFrameData[] = {
    0x00, 0x05, 0x04, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00,
    0x01,
  };

  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(kFrameData, sizeof(kFrameData));
  EXPECT_EQ(1, visitor.setting_count_);
  EXPECT_EQ(0, visitor.error_count_);
}

// Tests handling of SETTINGS frame with entries out of order.
TEST_P(SpdyFramerTest, ReadOutOfOrderSettings) {
  SpdyFramer framer(spdy_version_);

  const unsigned char kV2FrameData[] = {
    0x80, spdy_version_ch_, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x00, 0x03,
    0x02, 0x00, 0x00, 0x00,  // 1st Setting
    0x00, 0x00, 0x00, 0x02,
    0x01, 0x00, 0x00, 0x00,  // 2nd (out of order) Setting
    0x00, 0x00, 0x00, 0x03,
    0x03, 0x00, 0x00, 0x00,  // 3rd (unprocessed) Setting
    0x00, 0x00, 0x00, 0x03,
  };
  const unsigned char kV3FrameData[] = {
    0x80, spdy_version_ch_, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x02,  // 1st Setting
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x01,  // 2nd (out of order) Setting
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x01, 0x03,  // 3rd (unprocessed) Setting
    0x00, 0x00, 0x00, 0x03,
  };
  const unsigned char kV4FrameData[] = {
    0x00, 0x0f, 0x04, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x02,  // 1st Setting
    0x00, 0x00, 0x00, 0x02,
    0x01,  // 2nd (out of order) Setting
    0x00, 0x00, 0x00, 0x03,
    0x03,  // 3rd (unprocessed) Setting
    0x00, 0x00, 0x00, 0x03,
  };

  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  if (IsSpdy2()) {
    visitor.SimulateInFramer(kV2FrameData, sizeof(kV2FrameData));
  } else if (IsSpdy3()) {
    visitor.SimulateInFramer(kV3FrameData, sizeof(kV3FrameData));
  } else {
    visitor.SimulateInFramer(kV4FrameData, sizeof(kV4FrameData));
  }

  if (!IsSpdy4()) {
    EXPECT_EQ(1, visitor.setting_count_);
    EXPECT_EQ(1, visitor.error_count_);
  } else {
    // In SPDY 4+, settings are allowed in any order.
    EXPECT_EQ(3, visitor.setting_count_);
    EXPECT_EQ(0, visitor.error_count_);
    // EXPECT_EQ(1, visitor.settings_ack_count_);
  }
}

TEST_P(SpdyFramerTest, ProcessSettingsAckFrame) {
  if (spdy_version_ <= SPDY3) {
    return;
  }
  SpdyFramer framer(spdy_version_);

  const unsigned char kFrameData[] = {
    0x00, 0x00, 0x04, 0x01,
    0x00, 0x00, 0x00, 0x00,
  };

  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(kFrameData, sizeof(kFrameData));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(0, visitor.setting_count_);
  EXPECT_EQ(1, visitor.settings_ack_received_);
}


TEST_P(SpdyFramerTest, ProcessDataFrameWithPadding) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  const int kPaddingLen = 512;  // So we get two bytes for padding length field.
  const char data_payload[] = "hello";

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  SpdyDataIR data_ir(1, StringPiece(data_payload, strlen(data_payload)));
  data_ir.set_padding_len(kPaddingLen);
  scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
  ASSERT_TRUE(frame.get() != NULL);

  int bytes_consumed = 0;

  // Send the frame header.
  EXPECT_CALL(visitor, OnDataFrameHeader(1,
                                         kPaddingLen + strlen(data_payload),
                                         false));
  CHECK_EQ(8u, framer.ProcessInput(frame->data(), 8));
  CHECK_EQ(framer.state(), SpdyFramer::SPDY_READ_PADDING_LENGTH);
  CHECK_EQ(framer.error_code(), SpdyFramer::SPDY_NO_ERROR);
  bytes_consumed += 8;

  // Send the first byte of the padding length field.
  CHECK_EQ(1u, framer.ProcessInput(frame->data() + bytes_consumed, 1));
  CHECK_EQ(framer.state(), SpdyFramer::SPDY_READ_PADDING_LENGTH);
  CHECK_EQ(framer.error_code(), SpdyFramer::SPDY_NO_ERROR);
  bytes_consumed += 1;

  // Send the second byte of the padding length field.
  CHECK_EQ(1u, framer.ProcessInput(frame->data() + bytes_consumed, 1));
  CHECK_EQ(framer.state(), SpdyFramer::SPDY_FORWARD_STREAM_FRAME);
  CHECK_EQ(framer.error_code(), SpdyFramer::SPDY_NO_ERROR);
  bytes_consumed += 1;

  // Send the first two bytes of the data payload.
  EXPECT_CALL(visitor, OnStreamFrameData(1, _, 2, false));
  CHECK_EQ(2u, framer.ProcessInput(frame->data() + bytes_consumed, 2));
  CHECK_EQ(framer.state(), SpdyFramer::SPDY_FORWARD_STREAM_FRAME);
  CHECK_EQ(framer.error_code(), SpdyFramer::SPDY_NO_ERROR);
  bytes_consumed += 2;

  // Send the rest three bytes of the data payload.
  EXPECT_CALL(visitor, OnStreamFrameData(1, _, 3, false));
  CHECK_EQ(3u, framer.ProcessInput(frame->data() + bytes_consumed, 3));
  CHECK_EQ(framer.state(), SpdyFramer::SPDY_CONSUME_PADDING);
  CHECK_EQ(framer.error_code(), SpdyFramer::SPDY_NO_ERROR);
  bytes_consumed += 3;

  // Send the first 100 bytes of the padding payload.
  EXPECT_CALL(visitor, OnStreamFrameData(1, NULL, 100, false));
  CHECK_EQ(100u, framer.ProcessInput(frame->data() + bytes_consumed, 100));
  CHECK_EQ(framer.state(), SpdyFramer::SPDY_CONSUME_PADDING);
  CHECK_EQ(framer.error_code(), SpdyFramer::SPDY_NO_ERROR);
  bytes_consumed += 100;

  // Send rest of the padding payload.
  EXPECT_CALL(visitor, OnStreamFrameData(1, NULL, 410, false));
  CHECK_EQ(410u, framer.ProcessInput(frame->data() + bytes_consumed, 410));
  CHECK_EQ(framer.state(), SpdyFramer::SPDY_RESET);
  CHECK_EQ(framer.error_code(), SpdyFramer::SPDY_NO_ERROR);
}

TEST_P(SpdyFramerTest, ReadWindowUpdate) {
  SpdyFramer framer(spdy_version_);
  scoped_ptr<SpdyFrame> control_frame(
      framer.SerializeWindowUpdate(SpdyWindowUpdateIR(1, 2)));
  TestSpdyVisitor visitor(spdy_version_);
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame->data()),
      control_frame->size());
  EXPECT_EQ(1u, visitor.last_window_update_stream_);
  EXPECT_EQ(2u, visitor.last_window_update_delta_);
}

TEST_P(SpdyFramerTest, ReceiveCredentialFrame) {
  if (!IsSpdy3()) {
    return;
  }
  SpdyFramer framer(spdy_version_);
  const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x0A,
      0x00, 0x00, 0x00, 0x33,
      0x00, 0x03, 0x00, 0x00,
      0x00, 0x05, 'p',  'r',
      'o',  'o',  'f',  0x00,
      0x00, 0x00, 0x06, 'a',
      ' ',  'c',  'e',  'r',
      't',  0x00, 0x00, 0x00,
      0x0C, 'a',  'n',  'o',
      't',  'h',  'e',  'r',
      ' ',  'c',  'e',  'r',
      't',  0x00, 0x00, 0x00,
      0x0A, 'f',  'i',  'n',
      'a',  'l',  ' ',  'c',
      'e',  'r',  't',
  };
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(kV3FrameData, arraysize(kV3FrameData));
  EXPECT_EQ(0, visitor.error_count_);
}

TEST_P(SpdyFramerTest, ReadCredentialFrameFollowedByAnotherFrame) {
  if (!IsSpdy3()) {
    return;
  }
  SpdyFramer framer(spdy_version_);
  const unsigned char kV3FrameData[] = {  // Also applies for V2.
      0x80, spdy_version_ch_, 0x00, 0x0A,
      0x00, 0x00, 0x00, 0x33,
      0x00, 0x03, 0x00, 0x00,
      0x00, 0x05, 'p',  'r',
      'o',  'o',  'f',  0x00,
      0x00, 0x00, 0x06, 'a',
      ' ',  'c',  'e',  'r',
      't',  0x00, 0x00, 0x00,
      0x0C, 'a',  'n',  'o',
      't',  'h',  'e',  'r',
      ' ',  'c',  'e',  'r',
      't',  0x00, 0x00, 0x00,
      0x0A, 'f',  'i',  'n',
      'a',  'l',  ' ',  'c',
      'e',  'r',  't',
  };
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  string multiple_frame_data(reinterpret_cast<const char*>(kV3FrameData),
                             arraysize(kV3FrameData));
  scoped_ptr<SpdyFrame> control_frame(
      framer.SerializeWindowUpdate(SpdyWindowUpdateIR(1, 2)));
  multiple_frame_data.append(string(control_frame->data(),
                                    control_frame->size()));
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned const char*>(multiple_frame_data.data()),
      multiple_frame_data.length());
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1u, visitor.last_window_update_stream_);
  EXPECT_EQ(2u, visitor.last_window_update_delta_);
}

TEST_P(SpdyFramerTest, CreateContinuationUncompressed) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  SpdyFramer framer(spdy_version_);
  framer.set_enable_compression(false);
  const char kDescription[] = "CONTINUATION frame";

  const unsigned char kFrameData[] = {
     0x00, 0x12, 0x09, 0x00,  // CONTINUATION
     0x00, 0x00, 0x00, 0x2a,  // Stream 42
     0x00, 0x03, 0x62, 0x61,  // @.ba
     0x72, 0x03, 0x66, 0x6f,  // r.fo
     0x6f, 0x00, 0x03, 0x66,  // o@.f
     0x6f, 0x6f, 0x03, 0x62,  // oo.b
     0x61, 0x72,              // ar
  };

  SpdyContinuationIR continuation(42);
  continuation.SetHeader("bar", "foo");
  continuation.SetHeader("foo", "bar");
  scoped_ptr<SpdySerializedFrame> frame(
    framer.SerializeContinuation(continuation));
  CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
}

TEST_P(SpdyFramerTest, ReadCompressedPushPromise) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  SpdyFramer framer(spdy_version_);
  SpdyPushPromiseIR push_promise(42, 57);
  push_promise.SetHeader("foo", "bar");
  push_promise.SetHeader("bar", "foofoo");
  SpdyHeaderBlock headers = push_promise.name_value_block();
  scoped_ptr<SpdySerializedFrame> frame(
    framer.SerializePushPromise(push_promise));
  EXPECT_TRUE(frame.get() != NULL);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = true;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(frame->data()),
      frame->size());
  EXPECT_EQ(42u, visitor.last_push_promise_stream_);
  EXPECT_EQ(57u, visitor.last_push_promise_promised_stream_);
  EXPECT_TRUE(CompareHeaderBlocks(&headers, &visitor.headers_));
}

TEST_P(SpdyFramerTest, ReadHeadersWithContinuationAndPadding) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  const unsigned char kInput[] = {
    0x00, 0x14, 0x01, 0x08,  // HEADERS: PAD_LOW
    0x00, 0x00, 0x00, 0x01,  // Stream 1
    0x03,                    // Padding of 3.
    0x00, 0x06, 0x63, 0x6f,
    0x6f, 0x6b, 0x69, 0x65,
    0x07, 0x66, 0x6f, 0x6f,
    0x3d, 0x62, 0x61, 0x72,
    0x00, 0x00, 0x00,

    0x00, 0x1a, 0x09, 0x18,  // CONTINUATION: PAD_LOW & PAD_HIGH
    0x00, 0x00, 0x00, 0x01,  // Stream 1
    0x00, 0x04,              // Padding of 4.
    0x00, 0x06, 0x63, 0x6f,
    0x6f, 0x6b, 0x69, 0x65,
    0x08, 0x62, 0x61, 0x7a,
    0x3d, 0x62, 0x69, 0x6e,
    0x67, 0x00, 0x06, 0x63,
    0x00, 0x00, 0x00, 0x00,

    0x00, 0x13, 0x09, 0x0c,  // CONTINUATION: PAD_LOW & END_HEADERS
    0x00, 0x00, 0x00, 0x01,  // Stream 1
    0x00,                    // Padding of 0.
    0x6f, 0x6f, 0x6b, 0x69,
    0x65, 0x00, 0x00, 0x04,
    0x6e, 0x61, 0x6d, 0x65,
    0x05, 0x76, 0x61, 0x6c,
    0x75, 0x65,
  };

  TestSpdyVisitor visitor(spdy_version_);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(2, visitor.continuation_count_);
  EXPECT_EQ(1, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(0, visitor.zero_length_data_frame_count_);

  EXPECT_THAT(visitor.headers_, ElementsAre(
      Pair("cookie", "foo=bar; baz=bing; "),
      Pair("name", "value")));
}

TEST_P(SpdyFramerTest, ReadHeadersWithContinuationAndFin) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  const unsigned char kInput[] = {
    0x00, 0x10, 0x01, 0x01,  // HEADERS: FIN
    0x00, 0x00, 0x00, 0x01,  // Stream 1
    0x00, 0x06, 0x63, 0x6f,
    0x6f, 0x6b, 0x69, 0x65,
    0x07, 0x66, 0x6f, 0x6f,
    0x3d, 0x62, 0x61, 0x72,

    0x00, 0x14, 0x09, 0x00,  // CONTINUATION
    0x00, 0x00, 0x00, 0x01,  // Stream 1
    0x00, 0x06, 0x63, 0x6f,
    0x6f, 0x6b, 0x69, 0x65,
    0x08, 0x62, 0x61, 0x7a,
    0x3d, 0x62, 0x69, 0x6e,
    0x67, 0x00, 0x06, 0x63,

    0x00, 0x12, 0x09, 0x04,  // CONTINUATION: END_HEADERS
    0x00, 0x00, 0x00, 0x01,  // Stream 1
    0x6f, 0x6f, 0x6b, 0x69,
    0x65, 0x00, 0x00, 0x04,
    0x6e, 0x61, 0x6d, 0x65,
    0x05, 0x76, 0x61, 0x6c,
    0x75, 0x65,
  };

  SpdyFramer framer(spdy_version_);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(2, visitor.continuation_count_);
  EXPECT_EQ(1, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);

  EXPECT_THAT(visitor.headers_, ElementsAre(
      Pair("cookie", "foo=bar; baz=bing; "),
      Pair("name", "value")));
}

TEST_P(SpdyFramerTest, ReadPushPromiseWithContinuationAndPadding) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  const unsigned char kInput[] = {
    0x00, 0x18, 0x05, 0x18,  // PUSH_PROMISE: PAD_LOW & PAD_HIGH
    0x00, 0x00, 0x00, 0x01,  // Stream 1
    0x00, 0x00, 0x00, 0x2A,  // Promised stream 42
    0x00, 0x02,              // Padding of 2.
    0x00, 0x06, 0x63, 0x6f,
    0x6f, 0x6b, 0x69, 0x65,
    0x07, 0x66, 0x6f, 0x6f,
    0x3d, 0x62, 0x61, 0x72,
    0x00, 0x00,

    0x00, 0x14, 0x09, 0x00,  // CONTINUATION:
    0x00, 0x00, 0x00, 0x01,  // Stream 1
    0x00, 0x06, 0x63, 0x6f,
    0x6f, 0x6b, 0x69, 0x65,
    0x08, 0x62, 0x61, 0x7a,
    0x3d, 0x62, 0x69, 0x6e,
    0x67, 0x00, 0x06, 0x63,

    0x00, 0x17, 0x09, 0x0c,  // CONTINUATION: PAD_LOW & END_HEADERS
    0x00, 0x00, 0x00, 0x01,  // Stream 1
    0x04,                    // Padding of 4.
    0x6f, 0x6f, 0x6b, 0x69,
    0x65, 0x00, 0x00, 0x04,
    0x6e, 0x61, 0x6d, 0x65,
    0x05, 0x76, 0x61, 0x6c,
    0x75, 0x65, 0x00, 0x00,
    0x00, 0x00,
  };

  SpdyFramer framer(spdy_version_);
  TestSpdyVisitor visitor(spdy_version_);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1u, visitor.last_push_promise_stream_);
  EXPECT_EQ(42u, visitor.last_push_promise_promised_stream_);
  EXPECT_EQ(2, visitor.continuation_count_);
  EXPECT_EQ(1, visitor.zero_length_control_frame_header_data_count_);
  EXPECT_EQ(0, visitor.zero_length_data_frame_count_);

  EXPECT_THAT(visitor.headers_, ElementsAre(
      Pair("cookie", "foo=bar; baz=bing; "),
      Pair("name", "value")));
}

TEST_P(SpdyFramerTest, ReadContinuationWithWrongStreamId) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  const unsigned char kInput[] = {
    0x00, 0x10, 0x01, 0x00,  // HEADERS
    0x00, 0x00, 0x00, 0x01,  // Stream 1
    0x00, 0x06, 0x63, 0x6f,
    0x6f, 0x6b, 0x69, 0x65,
    0x07, 0x66, 0x6f, 0x6f,
    0x3d, 0x62, 0x61, 0x72,

    0x00, 0x14, 0x09, 0x00,  // CONTINUATION
    0x00, 0x00, 0x00, 0x02,  // Stream 2
    0x00, 0x06, 0x63, 0x6f,
    0x6f, 0x6b, 0x69, 0x65,
    0x08, 0x62, 0x61, 0x7a,
    0x3d, 0x62, 0x69, 0x6e,
    0x67, 0x00, 0x06, 0x63,
  };

  SpdyFramer framer(spdy_version_);
  TestSpdyVisitor visitor(spdy_version_);
  framer.set_visitor(&visitor);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME,
            visitor.framer_.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.continuation_count_);
  EXPECT_EQ(0u, visitor.header_buffer_length_);
}

TEST_P(SpdyFramerTest, ReadContinuationOutOfOrder) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  const unsigned char kInput[] = {
    0x00, 0x10, 0x09, 0x00,  // CONTINUATION
    0x00, 0x00, 0x00, 0x01,  // Stream 1
    0x00, 0x06, 0x63, 0x6f,
    0x6f, 0x6b, 0x69, 0x65,
    0x07, 0x66, 0x6f, 0x6f,
    0x3d, 0x62, 0x61, 0x72,
  };

  SpdyFramer framer(spdy_version_);
  TestSpdyVisitor visitor(spdy_version_);
  framer.set_visitor(&visitor);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(SpdyFramer::SPDY_UNEXPECTED_FRAME,
            visitor.framer_.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
  EXPECT_EQ(0, visitor.continuation_count_);
  EXPECT_EQ(0u, visitor.header_buffer_length_);
}

TEST_P(SpdyFramerTest, ExpectContinuationReceiveData) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  const unsigned char kInput[] = {
    0x00, 0x10, 0x01, 0x00,  // HEADERS
    0x00, 0x00, 0x00, 0x01,  // Stream 1
    0x00, 0x06, 0x63, 0x6f,
    0x6f, 0x6b, 0x69, 0x65,
    0x07, 0x66, 0x6f, 0x6f,
    0x3d, 0x62, 0x61, 0x72,

    0x00, 0x00, 0x00, 0x01,  // DATA on Stream #1
    0x00, 0x00, 0x00, 0x04,
    0xde, 0xad, 0xbe, 0xef,
  };

  SpdyFramer framer(spdy_version_);
  TestSpdyVisitor visitor(spdy_version_);
  framer.set_visitor(&visitor);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(SpdyFramer::SPDY_UNEXPECTED_FRAME,
            visitor.framer_.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.continuation_count_);
  EXPECT_EQ(0u, visitor.header_buffer_length_);
  EXPECT_EQ(0, visitor.data_frame_count_);
}

TEST_P(SpdyFramerTest, ExpectContinuationReceiveControlFrame) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  const unsigned char kInput[] = {
    0x00, 0x10, 0x01, 0x00,  // HEADERS
    0x00, 0x00, 0x00, 0x01,  // Stream 1
    0x00, 0x06, 0x63, 0x6f,
    0x6f, 0x6b, 0x69, 0x65,
    0x07, 0x66, 0x6f, 0x6f,
    0x3d, 0x62, 0x61, 0x72,

    0x00, 0x14, 0x08, 0x00,  // HEADERS
    0x00, 0x00, 0x00, 0x01,  // Stream 1
    0x00, 0x06, 0x63, 0x6f,  // (Note this is a valid continued encoding).
    0x6f, 0x6b, 0x69, 0x65,
    0x08, 0x62, 0x61, 0x7a,
    0x3d, 0x62, 0x69, 0x6e,
    0x67, 0x00, 0x06, 0x63,
  };

  SpdyFramer framer(spdy_version_);
  TestSpdyVisitor visitor(spdy_version_);
  framer.set_visitor(&visitor);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  EXPECT_EQ(1, visitor.error_count_);
  EXPECT_EQ(SpdyFramer::SPDY_UNEXPECTED_FRAME,
            visitor.framer_.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.continuation_count_);
  EXPECT_EQ(0u, visitor.header_buffer_length_);
  EXPECT_EQ(0, visitor.data_frame_count_);
}

TEST_P(SpdyFramerTest, EndSegmentOnDataFrame) {
  if (spdy_version_ <= SPDY3) {
    return;
  }
  const unsigned char kInput[] = {
    0x00, 0x0c, 0x00, 0x02,  // DATA: END_SEGMENT
    0x00, 0x00, 0x00, 0x01,  // Stream 1
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
  };

  TestSpdyVisitor visitor(spdy_version_);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  // TODO(jgraettinger): Verify END_SEGMENT when support is added.
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(12, visitor.data_bytes_);
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
}

TEST_P(SpdyFramerTest, EndSegmentOnHeadersFrame) {
  if (spdy_version_ <= SPDY3) {
    return;
  }
  const unsigned char kInput[] = {
    0x00, 0x10, 0x01, 0x06,  // HEADERS: END_SEGMENT | END_HEADERS
    0x00, 0x00, 0x00, 0x01,  // Stream 1
    0x00, 0x06, 0x63, 0x6f,
    0x6f, 0x6b, 0x69, 0x65,
    0x07, 0x66, 0x6f, 0x6f,
    0x3d, 0x62, 0x61, 0x72,
  };

  TestSpdyVisitor visitor(spdy_version_);
  visitor.SimulateInFramer(kInput, sizeof(kInput));

  // TODO(jgraettinger): Verify END_SEGMENT when support is added.
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(1, visitor.zero_length_control_frame_header_data_count_);

  EXPECT_THAT(visitor.headers_, ElementsAre(
      Pair("cookie", "foo=bar")));
}

TEST_P(SpdyFramerTest, ReadGarbage) {
  SpdyFramer framer(spdy_version_);
  unsigned char garbage_frame[256];
  memset(garbage_frame, ~0, sizeof(garbage_frame));
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(garbage_frame, sizeof(garbage_frame));
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_P(SpdyFramerTest, ReadGarbageWithValidLength) {
  if (!IsSpdy4()) {
    return;
  }
  SpdyFramer framer(spdy_version_);
  const unsigned char kFrameData[] = {
    0x00, 0x10, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,
  };
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(kFrameData, arraysize(kFrameData));
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_P(SpdyFramerTest, ReadGarbageWithValidVersion) {
  if (IsSpdy4()) {
    // Not valid for SPDY 4 since there is no version field.
    return;
  }
  SpdyFramer framer(spdy_version_);
  const unsigned char kFrameData[] = {
    0x80, spdy_version_ch_, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,
  };
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(kFrameData, arraysize(kFrameData));
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_P(SpdyFramerTest, ReadGarbageHPACKEncoding) {
  if (spdy_version_ <= SPDY3) {
    return;
  }
  const unsigned char kInput[] = {
    0x00, 0x12, 0x01, 0x04,  // HEADER: END_HEADERS
    0x00, 0x00, 0x00, 0x01,  // Stream 1
    0xef, 0xef, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff,
  };

  TestSpdyVisitor visitor(spdy_version_);
  visitor.SimulateInFramer(kInput, arraysize(kInput));
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_P(SpdyFramerTest, SizesTest) {
  SpdyFramer framer(spdy_version_);
  EXPECT_EQ(8u, framer.GetDataFrameMinimumSize());
  if (IsSpdy4() || IsSpdy5()) {
    EXPECT_EQ(8u, framer.GetSynReplyMinimumSize());
    EXPECT_EQ(12u, framer.GetRstStreamMinimumSize());
    EXPECT_EQ(8u, framer.GetSettingsMinimumSize());
    EXPECT_EQ(16u, framer.GetPingSize());
    EXPECT_EQ(16u, framer.GetGoAwayMinimumSize());
    EXPECT_EQ(8u, framer.GetHeadersMinimumSize());
    EXPECT_EQ(12u, framer.GetWindowUpdateSize());
    EXPECT_EQ(8u, framer.GetBlockedSize());
    EXPECT_EQ(12u, framer.GetPushPromiseMinimumSize());
    EXPECT_EQ(17u, framer.GetAltSvcMinimumSize());
    EXPECT_EQ(8u, framer.GetFrameMinimumSize());
    EXPECT_EQ(16383u, framer.GetFrameMaximumSize());
    EXPECT_EQ(16375u, framer.GetDataFrameMaximumPayload());
  } else {
    EXPECT_EQ(8u, framer.GetControlFrameHeaderSize());
    EXPECT_EQ(18u, framer.GetSynStreamMinimumSize());
    EXPECT_EQ(IsSpdy2() ? 14u : 12u, framer.GetSynReplyMinimumSize());
    EXPECT_EQ(16u, framer.GetRstStreamMinimumSize());
    EXPECT_EQ(12u, framer.GetSettingsMinimumSize());
    EXPECT_EQ(12u, framer.GetPingSize());
    EXPECT_EQ(IsSpdy2() ? 12u : 16u, framer.GetGoAwayMinimumSize());
    EXPECT_EQ(IsSpdy2() ? 14u : 12u, framer.GetHeadersMinimumSize());
    EXPECT_EQ(16u, framer.GetWindowUpdateSize());
    EXPECT_EQ(8u, framer.GetFrameMinimumSize());
    EXPECT_EQ(16777223u, framer.GetFrameMaximumSize());
    EXPECT_EQ(16777215u, framer.GetDataFrameMaximumPayload());
  }
}

TEST_P(SpdyFramerTest, StateToStringTest) {
  EXPECT_STREQ("ERROR",
               SpdyFramer::StateToString(SpdyFramer::SPDY_ERROR));
  EXPECT_STREQ("AUTO_RESET",
               SpdyFramer::StateToString(SpdyFramer::SPDY_AUTO_RESET));
  EXPECT_STREQ("RESET",
               SpdyFramer::StateToString(SpdyFramer::SPDY_RESET));
  EXPECT_STREQ("READING_COMMON_HEADER",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_READING_COMMON_HEADER));
  EXPECT_STREQ("CONTROL_FRAME_PAYLOAD",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_CONTROL_FRAME_PAYLOAD));
  EXPECT_STREQ("IGNORE_REMAINING_PAYLOAD",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_IGNORE_REMAINING_PAYLOAD));
  EXPECT_STREQ("FORWARD_STREAM_FRAME",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_FORWARD_STREAM_FRAME));
  EXPECT_STREQ("SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK));
  EXPECT_STREQ("SPDY_CONTROL_FRAME_HEADER_BLOCK",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_CONTROL_FRAME_HEADER_BLOCK));
  EXPECT_STREQ("SPDY_SETTINGS_FRAME_PAYLOAD",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_SETTINGS_FRAME_PAYLOAD));
  EXPECT_STREQ("SPDY_ALTSVC_FRAME_PAYLOAD",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_ALTSVC_FRAME_PAYLOAD));
  EXPECT_STREQ("UNKNOWN_STATE",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_ALTSVC_FRAME_PAYLOAD + 1));
}

TEST_P(SpdyFramerTest, ErrorCodeToStringTest) {
  EXPECT_STREQ("NO_ERROR",
               SpdyFramer::ErrorCodeToString(SpdyFramer::SPDY_NO_ERROR));
  EXPECT_STREQ("INVALID_CONTROL_FRAME",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_INVALID_CONTROL_FRAME));
  EXPECT_STREQ("CONTROL_PAYLOAD_TOO_LARGE",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_CONTROL_PAYLOAD_TOO_LARGE));
  EXPECT_STREQ("ZLIB_INIT_FAILURE",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_ZLIB_INIT_FAILURE));
  EXPECT_STREQ("UNSUPPORTED_VERSION",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_UNSUPPORTED_VERSION));
  EXPECT_STREQ("DECOMPRESS_FAILURE",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_DECOMPRESS_FAILURE));
  EXPECT_STREQ("COMPRESS_FAILURE",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_COMPRESS_FAILURE));
  EXPECT_STREQ("SPDY_INVALID_DATA_FRAME_FLAGS",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_INVALID_DATA_FRAME_FLAGS));
  EXPECT_STREQ("SPDY_INVALID_CONTROL_FRAME_FLAGS",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS));
  EXPECT_STREQ("UNKNOWN_ERROR",
               SpdyFramer::ErrorCodeToString(SpdyFramer::LAST_ERROR));
}

TEST_P(SpdyFramerTest, StatusCodeToStringTest) {
  EXPECT_STREQ("INVALID",
               SpdyFramer::StatusCodeToString(RST_STREAM_INVALID));
  EXPECT_STREQ("PROTOCOL_ERROR",
               SpdyFramer::StatusCodeToString(RST_STREAM_PROTOCOL_ERROR));
  EXPECT_STREQ("INVALID_STREAM",
               SpdyFramer::StatusCodeToString(RST_STREAM_INVALID_STREAM));
  EXPECT_STREQ("REFUSED_STREAM",
               SpdyFramer::StatusCodeToString(RST_STREAM_REFUSED_STREAM));
  EXPECT_STREQ("UNSUPPORTED_VERSION",
               SpdyFramer::StatusCodeToString(RST_STREAM_UNSUPPORTED_VERSION));
  EXPECT_STREQ("CANCEL",
               SpdyFramer::StatusCodeToString(RST_STREAM_CANCEL));
  EXPECT_STREQ("INTERNAL_ERROR",
               SpdyFramer::StatusCodeToString(RST_STREAM_INTERNAL_ERROR));
  EXPECT_STREQ("FLOW_CONTROL_ERROR",
               SpdyFramer::StatusCodeToString(RST_STREAM_FLOW_CONTROL_ERROR));
  EXPECT_STREQ("UNKNOWN_STATUS",
               SpdyFramer::StatusCodeToString(-1));
}

TEST_P(SpdyFramerTest, FrameTypeToStringTest) {
  EXPECT_STREQ("DATA",
               SpdyFramer::FrameTypeToString(DATA));
  EXPECT_STREQ("SYN_STREAM",
               SpdyFramer::FrameTypeToString(SYN_STREAM));
  EXPECT_STREQ("SYN_REPLY",
               SpdyFramer::FrameTypeToString(SYN_REPLY));
  EXPECT_STREQ("RST_STREAM",
               SpdyFramer::FrameTypeToString(RST_STREAM));
  EXPECT_STREQ("SETTINGS",
               SpdyFramer::FrameTypeToString(SETTINGS));
  EXPECT_STREQ("NOOP",
               SpdyFramer::FrameTypeToString(NOOP));
  EXPECT_STREQ("PING",
               SpdyFramer::FrameTypeToString(PING));
  EXPECT_STREQ("GOAWAY",
               SpdyFramer::FrameTypeToString(GOAWAY));
  EXPECT_STREQ("HEADERS",
               SpdyFramer::FrameTypeToString(HEADERS));
  EXPECT_STREQ("WINDOW_UPDATE",
               SpdyFramer::FrameTypeToString(WINDOW_UPDATE));
  EXPECT_STREQ("PUSH_PROMISE",
               SpdyFramer::FrameTypeToString(PUSH_PROMISE));
  EXPECT_STREQ("CREDENTIAL",
               SpdyFramer::FrameTypeToString(CREDENTIAL));
  EXPECT_STREQ("CONTINUATION",
               SpdyFramer::FrameTypeToString(CONTINUATION));
}

TEST_P(SpdyFramerTest, CatchProbableHttpResponse) {
  if (IsSpdy4()) {
    // TODO(hkhalil): catch probable HTTP response in SPDY 4?
    return;
  }
  {
    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    EXPECT_CALL(visitor, OnError(_));
    framer.ProcessInput("HTTP/1.1", 8);
    EXPECT_TRUE(framer.probable_http_response());
    EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
    EXPECT_EQ(SpdyFramer::SPDY_INVALID_DATA_FRAME_FLAGS, framer.error_code())
        << SpdyFramer::ErrorCodeToString(framer.error_code());
  }
  {
    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    EXPECT_CALL(visitor, OnError(_));
    framer.ProcessInput("HTTP/1.0", 8);
    EXPECT_TRUE(framer.probable_http_response());
    EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
    EXPECT_EQ(SpdyFramer::SPDY_INVALID_DATA_FRAME_FLAGS, framer.error_code())
        << SpdyFramer::ErrorCodeToString(framer.error_code());
  }
}

TEST_P(SpdyFramerTest, DataFrameFlagsV2V3) {
  if (spdy_version_ > SPDY3) {
    return;
  }

  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    SpdyDataIR data_ir(1, StringPiece("hello", 5));
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags & ~DATA_FLAG_FIN) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnDataFrameHeader(1, 5, flags & DATA_FLAG_FIN));
      EXPECT_CALL(visitor, OnStreamFrameData(_, _, 5, false));
      if (flags & DATA_FLAG_FIN) {
        EXPECT_CALL(visitor, OnStreamFrameData(_, _, 0, true));
      }
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags & ~DATA_FLAG_FIN) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_DATA_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, DataFrameFlagsV4) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  uint8 valid_data_flags = DATA_FLAG_FIN | DATA_FLAG_END_SEGMENT |
      DATA_FLAG_PAD_LOW | DATA_FLAG_PAD_HIGH;

  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    SpdyDataIR data_ir(1, StringPiece("hello", 5));
    scoped_ptr<SpdyFrame> frame(framer.SerializeData(data_ir));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags & ~valid_data_flags) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnDataFrameHeader(1, 5, flags & DATA_FLAG_FIN));
      if ((flags & DATA_FLAG_PAD_LOW) || (flags & DATA_FLAG_PAD_HIGH)) {
        // Expect Error since we don't set pad_high and pad_low in payload.
        EXPECT_CALL(visitor, OnError(_));
      } else {
        EXPECT_CALL(visitor, OnStreamFrameData(_, _, 5, false));
        if (flags & DATA_FLAG_FIN) {
          EXPECT_CALL(visitor, OnStreamFrameData(_, _, 0, true));
        }
      }
    }

    framer.ProcessInput(frame->data(), frame->size());
    if ((flags & ~valid_data_flags) || (flags & DATA_FLAG_PAD_LOW) ||
        (flags & DATA_FLAG_PAD_HIGH)) {
        EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
        EXPECT_EQ(SpdyFramer::SPDY_INVALID_DATA_FRAME_FLAGS,
                  framer.error_code())
            << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, SynStreamFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    testing::StrictMock<test::MockDebugVisitor> debug_visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);
    framer.set_debug_visitor(&debug_visitor);

    EXPECT_CALL(debug_visitor, OnSendCompressedFrame(8, SYN_STREAM, _, _));

    SpdySynStreamIR syn_stream(8);
    syn_stream.set_associated_to_stream_id(3);
    syn_stream.set_priority(1);
    syn_stream.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
    int set_flags = flags;
    if (IsSpdy4()) {
      // PRIORITY required for SYN_STREAM simulation.
      set_flags |= HEADERS_FLAG_PRIORITY;

      // TODO(jgraettinger): Add padding to SynStreamIR, and implement framing.
      set_flags &= ~HEADERS_FLAG_PAD_LOW;
      set_flags &= ~HEADERS_FLAG_PAD_HIGH;
    }
    SetFrameFlags(frame.get(), set_flags, spdy_version_);

    if (!IsSpdy4() &&
        flags & ~(CONTROL_FLAG_FIN | CONTROL_FLAG_UNIDIRECTIONAL)) {
      EXPECT_CALL(visitor, OnError(_));
    } else if (IsSpdy4() &&
               flags & ~(CONTROL_FLAG_FIN |
                         HEADERS_FLAG_PRIORITY |
                         HEADERS_FLAG_END_HEADERS |
                         HEADERS_FLAG_END_SEGMENT |
                         HEADERS_FLAG_PAD_LOW |
                         HEADERS_FLAG_PAD_HIGH)) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(debug_visitor, OnReceiveCompressedFrame(8, SYN_STREAM, _));
      if (IsSpdy4()) {
        EXPECT_CALL(visitor, OnSynStream(8, 0, 1, flags & CONTROL_FLAG_FIN,
                                         false));
      } else {
        EXPECT_CALL(visitor, OnSynStream(8, 3, 1, flags & CONTROL_FLAG_FIN,
                                         flags & CONTROL_FLAG_UNIDIRECTIONAL));
      }
      EXPECT_CALL(visitor, OnControlFrameHeaderData(8, _, _))
          .WillRepeatedly(testing::Return(true));
      if (flags & DATA_FLAG_FIN && (!IsSpdy4() ||
                                    flags & HEADERS_FLAG_END_HEADERS)) {
        EXPECT_CALL(visitor, OnStreamFrameData(_, _, 0, true));
      } else {
        // Do not close the stream if we are expecting a CONTINUATION frame.
        EXPECT_CALL(visitor, OnStreamFrameData(_, _, 0, true)).Times(0);
      }
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (!IsSpdy4() &&
        flags & ~(CONTROL_FLAG_FIN | CONTROL_FLAG_UNIDIRECTIONAL)) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else if (IsSpdy4() &&
        flags & ~(CONTROL_FLAG_FIN |
                  HEADERS_FLAG_PRIORITY |
                  HEADERS_FLAG_END_HEADERS |
                  HEADERS_FLAG_END_SEGMENT |
                  HEADERS_FLAG_PAD_LOW |
                  HEADERS_FLAG_PAD_HIGH)) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, SynReplyFrameFlags) {
  if (IsSpdy4()) {
    // Covered by HEADERS case.
    return;
  }
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    SpdySynReplyIR syn_reply(37);
    syn_reply.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeSynReply(syn_reply));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags & ~CONTROL_FLAG_FIN) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnSynReply(37, flags & CONTROL_FLAG_FIN));
      EXPECT_CALL(visitor, OnControlFrameHeaderData(37, _, _))
          .WillRepeatedly(testing::Return(true));
      if (flags & DATA_FLAG_FIN) {
        EXPECT_CALL(visitor, OnStreamFrameData(_, _, 0, true));
      }
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags & ~CONTROL_FLAG_FIN) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, RstStreamFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    SpdyRstStreamIR rst_stream(13, RST_STREAM_CANCEL, "");
    scoped_ptr<SpdyFrame> frame(framer.SerializeRstStream(rst_stream));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags != 0) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnRstStream(13, RST_STREAM_CANCEL));
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags != 0) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, SettingsFrameFlagsOldFormat) {
  if (spdy_version_ > SPDY3) { return; }
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    SpdySettingsIR settings_ir;
    settings_ir.AddSetting(SETTINGS_UPLOAD_BANDWIDTH,
                           false,
                           false,
                           54321);
    scoped_ptr<SpdyFrame> frame(framer.SerializeSettings(settings_ir));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags & ~SETTINGS_FLAG_CLEAR_PREVIOUSLY_PERSISTED_SETTINGS) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnSettings(
          flags & SETTINGS_FLAG_CLEAR_PREVIOUSLY_PERSISTED_SETTINGS));
      EXPECT_CALL(visitor, OnSetting(SETTINGS_UPLOAD_BANDWIDTH,
                                     SETTINGS_FLAG_NONE, 54321));
      EXPECT_CALL(visitor, OnSettingsEnd());
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags & ~SETTINGS_FLAG_CLEAR_PREVIOUSLY_PERSISTED_SETTINGS) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, SettingsFrameFlags) {
  if (spdy_version_ <= SPDY3) { return; }
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    SpdySettingsIR settings_ir;
    settings_ir.AddSetting(SETTINGS_INITIAL_WINDOW_SIZE, 0, 0, 16);
    scoped_ptr<SpdyFrame> frame(framer.SerializeSettings(settings_ir));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags != 0) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnSettings(flags & SETTINGS_FLAG_ACK));
      EXPECT_CALL(visitor, OnSetting(SETTINGS_INITIAL_WINDOW_SIZE, 0, 16));
      EXPECT_CALL(visitor, OnSettingsEnd());
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags & ~SETTINGS_FLAG_ACK) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else if (flags & SETTINGS_FLAG_ACK) {
      // The frame is invalid because ACK frames should have no payload.
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, GoawayFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    SpdyGoAwayIR goaway_ir(97, GOAWAY_OK, "test");
    scoped_ptr<SpdyFrame> frame(framer.SerializeGoAway(goaway_ir));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags != 0) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnGoAway(97, GOAWAY_OK));
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags != 0) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, HeadersFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    if (IsSpdy4() && flags & HEADERS_FLAG_PRIORITY) {
      // Covered by SYN_STREAM case.
      continue;
    }
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    SpdyHeadersIR headers_ir(57);
    headers_ir.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame(framer.SerializeHeaders(headers_ir));
    int set_flags = flags;
    if (IsSpdy4()) {
      // TODO(jgraettinger): Add padding to SpdyHeadersIR,
      // and implement framing.
      set_flags &= ~HEADERS_FLAG_PAD_LOW;
      set_flags &= ~HEADERS_FLAG_PAD_HIGH;
    }
    SetFrameFlags(frame.get(), set_flags, spdy_version_);

    if (!IsSpdy4() && flags & ~CONTROL_FLAG_FIN) {
      EXPECT_CALL(visitor, OnError(_));
    } else if (IsSpdy4() && flags & ~(CONTROL_FLAG_FIN |
                                      HEADERS_FLAG_END_HEADERS |
                                      HEADERS_FLAG_END_SEGMENT |
                                      HEADERS_FLAG_PAD_LOW |
                                      HEADERS_FLAG_PAD_HIGH)) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnHeaders(57,
                                     flags & CONTROL_FLAG_FIN,
                                     (flags & HEADERS_FLAG_END_HEADERS) ||
                                      !IsSpdy4()));
      EXPECT_CALL(visitor, OnControlFrameHeaderData(57, _, _))
          .WillRepeatedly(testing::Return(true));
      if (flags & DATA_FLAG_FIN  && (!IsSpdy4() ||
                                     flags & HEADERS_FLAG_END_HEADERS)) {
        EXPECT_CALL(visitor, OnStreamFrameData(_, _, 0, true));
      } else {
        // Do not close the stream if we are expecting a CONTINUATION frame.
        EXPECT_CALL(visitor, OnStreamFrameData(_, _, 0, true)).Times(0);
      }
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (!IsSpdy4() && flags & ~CONTROL_FLAG_FIN) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else if (IsSpdy4() && flags & ~(CONTROL_FLAG_FIN |
                                      HEADERS_FLAG_END_HEADERS |
                                      HEADERS_FLAG_END_SEGMENT |
                                      HEADERS_FLAG_PAD_LOW |
                                      HEADERS_FLAG_PAD_HIGH)) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else if (IsSpdy4() && ~(flags & HEADERS_FLAG_END_HEADERS)) {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, PingFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    scoped_ptr<SpdyFrame> frame(framer.SerializePing(SpdyPingIR(42)));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (spdy_version_ > SPDY3 &&
        flags == PING_FLAG_ACK) {
      EXPECT_CALL(visitor, OnPing(42, true));
    } else if (flags == 0) {
      EXPECT_CALL(visitor, OnPing(42, false));
    } else {
      EXPECT_CALL(visitor, OnError(_));
    }

    framer.ProcessInput(frame->data(), frame->size());
    if ((spdy_version_ > SPDY3 && flags == PING_FLAG_ACK) ||
        flags == 0) {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, WindowUpdateFrameFlags) {
  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    scoped_ptr<SpdyFrame> frame(framer.SerializeWindowUpdate(
        SpdyWindowUpdateIR(4, 1024)));
    SetFrameFlags(frame.get(), flags, spdy_version_);

    if (flags != 0) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(visitor, OnWindowUpdate(4, 1024));
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags != 0) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, PushPromiseFrameFlags) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<net::test::MockSpdyFramerVisitor> visitor;
    testing::StrictMock<net::test::MockDebugVisitor> debug_visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);
    framer.set_debug_visitor(&debug_visitor);

    EXPECT_CALL(debug_visitor, OnSendCompressedFrame(42, PUSH_PROMISE, _, _));

    SpdyPushPromiseIR push_promise(42, 57);
    push_promise.SetHeader("foo", "bar");
    scoped_ptr<SpdySerializedFrame> frame(
    framer.SerializePushPromise(push_promise));
    // TODO(jgraettinger): Add padding to SpdyPushPromiseIR,
    // and implement framing.
    int set_flags = flags & ~HEADERS_FLAG_PAD_LOW & ~HEADERS_FLAG_PAD_HIGH;
    SetFrameFlags(frame.get(), set_flags, spdy_version_);

    if (flags & ~(PUSH_PROMISE_FLAG_END_PUSH_PROMISE |
                  HEADERS_FLAG_PAD_LOW |
                  HEADERS_FLAG_PAD_HIGH)) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(debug_visitor, OnReceiveCompressedFrame(42, PUSH_PROMISE, _));
      EXPECT_CALL(visitor, OnPushPromise(42, 57,
          flags & PUSH_PROMISE_FLAG_END_PUSH_PROMISE));
      EXPECT_CALL(visitor, OnControlFrameHeaderData(42, _, _))
          .WillRepeatedly(testing::Return(true));
    }

    framer.ProcessInput(frame->data(), frame->size());
    if (flags & ~(PUSH_PROMISE_FLAG_END_PUSH_PROMISE |
                  HEADERS_FLAG_PAD_LOW |
                  HEADERS_FLAG_PAD_HIGH)) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

TEST_P(SpdyFramerTest, ContinuationFrameFlags) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  for (int flags = 0; flags < 256; ++flags) {
    SCOPED_TRACE(testing::Message() << "Flags " << flags);

    testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
    testing::StrictMock<net::test::MockDebugVisitor> debug_visitor;
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);
    framer.set_debug_visitor(&debug_visitor);

    EXPECT_CALL(debug_visitor, OnSendCompressedFrame(42, HEADERS, _, _));
    EXPECT_CALL(debug_visitor, OnReceiveCompressedFrame(42, HEADERS, _));
    EXPECT_CALL(visitor, OnHeaders(42, 0, false));
    EXPECT_CALL(visitor, OnControlFrameHeaderData(42, _, _))
          .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(debug_visitor, OnSendCompressedFrame(42, CONTINUATION, _, _));

    SpdyHeadersIR headers_ir(42);
    headers_ir.SetHeader("foo", "bar");
    scoped_ptr<SpdyFrame> frame0(framer.SerializeHeaders(headers_ir));
    SetFrameFlags(frame0.get(), 0, spdy_version_);

    SpdyContinuationIR continuation(42);
    continuation.SetHeader("foo", "bar");
    scoped_ptr<SpdySerializedFrame> frame(
    framer.SerializeContinuation(continuation));
    // TODO(jgraettinger): Add padding to the eventual continuation
    // serialization implementation.
    int set_flags = flags & ~HEADERS_FLAG_PAD_LOW & ~HEADERS_FLAG_PAD_HIGH;
    SetFrameFlags(frame.get(), set_flags, spdy_version_);

    if (flags & ~(HEADERS_FLAG_END_HEADERS |
                  HEADERS_FLAG_PAD_LOW |
                  HEADERS_FLAG_PAD_HIGH)) {
      EXPECT_CALL(visitor, OnError(_));
    } else {
      EXPECT_CALL(debug_visitor, OnReceiveCompressedFrame(42, CONTINUATION, _));
      EXPECT_CALL(visitor, OnContinuation(42,
                                          flags & HEADERS_FLAG_END_HEADERS));
      EXPECT_CALL(visitor, OnControlFrameHeaderData(42, _, _))
          .WillRepeatedly(testing::Return(true));
    }

    framer.ProcessInput(frame0->data(), frame0->size());
    framer.ProcessInput(frame->data(), frame->size());
    if (flags & ~(HEADERS_FLAG_END_HEADERS |
                  HEADERS_FLAG_PAD_LOW |
                  HEADERS_FLAG_PAD_HIGH)) {
      EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME_FLAGS,
                framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    } else {
      EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
      EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
          << SpdyFramer::ErrorCodeToString(framer.error_code());
    }
  }
}

// TODO(mlavan): Add TEST_P(SpdyFramerTest, AltSvcFrameFlags)

// TODO(hkhalil): Add TEST_P(SpdyFramerTest, BlockedFrameFlags)

TEST_P(SpdyFramerTest, EmptySynStream) {
  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  testing::StrictMock<test::MockDebugVisitor> debug_visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);
  framer.set_debug_visitor(&debug_visitor);

  EXPECT_CALL(debug_visitor, OnSendCompressedFrame(1, SYN_STREAM, _, _));

  SpdySynStreamIR syn_stream(1);
  syn_stream.set_priority(1);
  scoped_ptr<SpdyFrame> frame(framer.SerializeSynStream(syn_stream));
  // Adjust size to remove the name/value block.
  SetFrameLength(
      frame.get(),
      framer.GetSynStreamMinimumSize() - framer.GetControlFrameHeaderSize(),
      spdy_version_);

  EXPECT_CALL(debug_visitor, OnReceiveCompressedFrame(1, SYN_STREAM, _));
  EXPECT_CALL(visitor, OnSynStream(1, 0, 1, false, false));
  EXPECT_CALL(visitor, OnControlFrameHeaderData(1, NULL, 0));

  framer.ProcessInput(frame->data(), framer.GetSynStreamMinimumSize());
  EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
  EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
}

TEST_P(SpdyFramerTest, SettingsFlagsAndId) {
  const uint32 kId = 0x020304;
  const uint32 kFlags = 0x01;
  const uint32 kWireFormat = htonl(IsSpdy2() ? 0x04030201 : 0x01020304);

  SettingsFlagsAndId id_and_flags =
      SettingsFlagsAndId::FromWireFormat(spdy_version_, kWireFormat);
  EXPECT_EQ(kId, id_and_flags.id());
  EXPECT_EQ(kFlags, id_and_flags.flags());
  EXPECT_EQ(kWireFormat, id_and_flags.GetWireFormat(spdy_version_));
}

// Test handling of a RST_STREAM with out-of-bounds status codes.
TEST_P(SpdyFramerTest, RstStreamStatusBounds) {
  const unsigned char kRstStreamStatusTooLow = 0x00;
  const unsigned char kRstStreamStatusTooHigh = 0xff;
  const unsigned char kV3RstStreamInvalid[] = {
    0x80, spdy_version_ch_, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, kRstStreamStatusTooLow
  };
  const unsigned char kV4RstStreamInvalid[] = {
    0x00, 0x04, 0x03, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, kRstStreamStatusTooLow
  };

  const unsigned char kV3RstStreamNumStatusCodes[] = {
    0x80, spdy_version_ch_, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, kRstStreamStatusTooHigh
  };
  const unsigned char kV4RstStreamNumStatusCodes[] = {
    0x00, 0x04, 0x03, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, kRstStreamStatusTooHigh
  };

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  if (IsSpdy4()) {
    EXPECT_CALL(visitor, OnError(_));
    framer.ProcessInput(reinterpret_cast<const char*>(kV4RstStreamInvalid),
                        arraysize(kV4RstStreamInvalid));
    EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
    EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
  } else {
    EXPECT_CALL(visitor, OnRstStream(1, RST_STREAM_INVALID));
    framer.ProcessInput(reinterpret_cast<const char*>(kV3RstStreamInvalid),
                        arraysize(kV3RstStreamInvalid));
    EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
    EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
  }


  framer.Reset();

  if (IsSpdy4()) {
    EXPECT_CALL(visitor, OnError(_));
    framer.ProcessInput(
        reinterpret_cast<const char*>(kV4RstStreamNumStatusCodes),
        arraysize(kV4RstStreamNumStatusCodes));
    EXPECT_EQ(SpdyFramer::SPDY_ERROR, framer.state());
    EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
  } else {
    EXPECT_CALL(visitor, OnRstStream(1, RST_STREAM_INVALID));
    framer.ProcessInput(
        reinterpret_cast<const char*>(kV3RstStreamNumStatusCodes),
        arraysize(kV3RstStreamNumStatusCodes));
    EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
    EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
  }
}

// Tests handling of a GOAWAY frame with out-of-bounds stream ID.
TEST_P(SpdyFramerTest, GoAwayStreamIdBounds) {
  const unsigned char kV2FrameData[] = {
    0x80, spdy_version_ch_, 0x00, 0x07,
    0x00, 0x00, 0x00, 0x04,
    0xff, 0xff, 0xff, 0xff,
  };
  const unsigned char kV3FrameData[] = {
    0x80, spdy_version_ch_, 0x00, 0x07,
    0x00, 0x00, 0x00, 0x08,
    0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00,
  };
  const unsigned char kV4FrameData[] = {
    0x00, 0x08, 0x07, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00,
  };

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  EXPECT_CALL(visitor, OnGoAway(0x7fffffff, GOAWAY_OK));
  if (IsSpdy2()) {
    framer.ProcessInput(reinterpret_cast<const char*>(kV2FrameData),
                        arraysize(kV2FrameData));
  } else if (IsSpdy3()) {
    framer.ProcessInput(reinterpret_cast<const char*>(kV3FrameData),
                        arraysize(kV3FrameData));
  } else {
    framer.ProcessInput(reinterpret_cast<const char*>(kV4FrameData),
                        arraysize(kV4FrameData));
  }
  EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
  EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
}

TEST_P(SpdyFramerTest, OnBlocked) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  const SpdyStreamId kStreamId = 0;

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  EXPECT_CALL(visitor, OnBlocked(kStreamId));

  SpdyBlockedIR blocked_ir(0);
  scoped_ptr<SpdySerializedFrame> frame(framer.SerializeFrame(blocked_ir));
  framer.ProcessInput(frame->data(), framer.GetBlockedSize());

  EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
  EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
}

TEST_P(SpdyFramerTest, OnAltSvc) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  const SpdyStreamId kStreamId = 1;

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  EXPECT_CALL(visitor, OnAltSvc(kStreamId,
                                10,
                                443,
                                StringPiece("pid"),
                                StringPiece("h1"),
                                StringPiece("o1")));

  SpdyAltSvcIR altsvc_ir(1);
  altsvc_ir.set_max_age(10);
  altsvc_ir.set_port(443);
  altsvc_ir.set_protocol_id("pid");
  altsvc_ir.set_host("h1");
  altsvc_ir.set_origin("o1");
  scoped_ptr<SpdySerializedFrame> frame(framer.SerializeFrame(altsvc_ir));
  framer.ProcessInput(frame->data(), framer.GetAltSvcMinimumSize() +
                      altsvc_ir.protocol_id().length() +
                      altsvc_ir.host().length() +
                      altsvc_ir.origin().length());

  EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
  EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
}

TEST_P(SpdyFramerTest, OnAltSvcNoOrigin) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  const SpdyStreamId kStreamId = 1;

  testing::StrictMock<test::MockSpdyFramerVisitor> visitor;
  SpdyFramer framer(spdy_version_);
  framer.set_visitor(&visitor);

  EXPECT_CALL(visitor, OnAltSvc(kStreamId,
                                10,
                                443,
                                StringPiece("pid"),
                                StringPiece("h1"),
                                StringPiece("")));

  SpdyAltSvcIR altsvc_ir(1);
  altsvc_ir.set_max_age(10);
  altsvc_ir.set_port(443);
  altsvc_ir.set_protocol_id("pid");
  altsvc_ir.set_host("h1");
  scoped_ptr<SpdySerializedFrame> frame(framer.SerializeFrame(altsvc_ir));
  framer.ProcessInput(frame->data(), framer.GetAltSvcMinimumSize() +
                      altsvc_ir.protocol_id().length() +
                      altsvc_ir.host().length());

  EXPECT_EQ(SpdyFramer::SPDY_RESET, framer.state());
  EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, framer.error_code())
      << SpdyFramer::ErrorCodeToString(framer.error_code());
}

TEST_P(SpdyFramerTest, OnAltSvcBadLengths) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  const unsigned char kType = static_cast<unsigned char>(
      SpdyConstants::SerializeFrameType(spdy_version_, ALTSVC));
  {
    TestSpdyVisitor visitor(spdy_version_);
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    const unsigned char kFrameDataLargePIDLen[] = {
      0x00, 0x17, kType, 0x00,
      0x00, 0x00, 0x00, 0x03,
      0x00, 0x00, 0x00, 0x05,
      0x01, 0xbb, 0x00, 0x05,  // Port = 443
      'p',  'i',  'd',  '1',   // Protocol-ID
      0x04, 'h',  'o',  's',
      't',  'o',  'r',  'i',
      'g',  'i',  'n',
    };

    visitor.SimulateInFramer(kFrameDataLargePIDLen,
                             sizeof(kFrameDataLargePIDLen));
    EXPECT_EQ(1, visitor.error_count_);
    EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME,
              visitor.framer_.error_code());
  }

  {
    TestSpdyVisitor visitor(spdy_version_);
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);
    const unsigned char kFrameDataPIDLenLargerThanFrame[] = {
      0x00, 0x17, kType, 0x00,
      0x00, 0x00, 0x00, 0x03,
      0x00, 0x00, 0x00, 0x05,
      0x01, 0xbb, 0x00, 0x99,  // Port = 443
      'p',  'i',  'd',  '1',   // Protocol-ID
      0x04, 'h',  'o',  's',
      't',  'o',  'r',  'i',
      'g',  'i',  'n',
    };

    visitor.SimulateInFramer(kFrameDataPIDLenLargerThanFrame,
                             sizeof(kFrameDataPIDLenLargerThanFrame));
    EXPECT_EQ(1, visitor.error_count_);
    EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME,
              visitor.framer_.error_code());
  }

  {
    TestSpdyVisitor visitor(spdy_version_);
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);

    const unsigned char kFrameDataLargeHostLen[] = {
      0x00, 0x17, kType, 0x00,
      0x00, 0x00, 0x00, 0x03,
      0x00, 0x00, 0x00, 0x05,
      0x01, 0xbb, 0x00, 0x04,  // Port = 443
      'p',  'i',  'd',  '1',   // Protocol-ID
      0x0f, 'h',  'o',  's',
      't',  'o',  'r',  'i',
      'g',  'i',  'n',
    };

    visitor.SimulateInFramer(kFrameDataLargeHostLen,
                             sizeof(kFrameDataLargeHostLen));
    EXPECT_EQ(1, visitor.error_count_);
    EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME,
              visitor.framer_.error_code());
  }

  {
    TestSpdyVisitor visitor(spdy_version_);
    SpdyFramer framer(spdy_version_);
    framer.set_visitor(&visitor);
    const unsigned char kFrameDataSmallPIDLen[] = {
      0x00, 0x17, kType, 0x00,
      0x00, 0x00, 0x00, 0x03,
      0x00, 0x00, 0x00, 0x05,
      0x01, 0xbb, 0x00, 0x01,  // Port = 443
      'p',  'i',  'd',  '1',   // Protocol-ID
      0x04, 'h',  'o',  's',
      't',  'o',  'r',  'i',
      'g',  'i',  'n',
    };

    visitor.SimulateInFramer(kFrameDataSmallPIDLen,
                             sizeof(kFrameDataSmallPIDLen));
    EXPECT_EQ(1, visitor.error_count_);
    EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME,
              visitor.framer_.error_code());
  }
}

// Tests handling of ALTSVC frames delivered in small chunks.
TEST_P(SpdyFramerTest, ReadChunkedAltSvcFrame) {
  if (spdy_version_ <= SPDY3) {
    return;
  }
  SpdyFramer framer(spdy_version_);
  SpdyAltSvcIR altsvc_ir(1);
  altsvc_ir.set_max_age(20);
  altsvc_ir.set_port(443);
  altsvc_ir.set_protocol_id("protocolid");
  altsvc_ir.set_host("hostname");

  scoped_ptr<SpdyFrame> control_frame(framer.SerializeAltSvc(altsvc_ir));
  TestSpdyVisitor visitor(spdy_version_);
  visitor.use_compression_ = false;

  // Read data in small chunks.
  size_t framed_data = 0;
  size_t unframed_data = control_frame->size();
  size_t kReadChunkSize = 5;  // Read five bytes at a time.
  while (unframed_data > 0) {
    size_t to_read = min(kReadChunkSize, unframed_data);
    visitor.SimulateInFramer(
        reinterpret_cast<unsigned char*>(control_frame->data() + framed_data),
        to_read);
    unframed_data -= to_read;
    framed_data += to_read;
  }
  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.altsvc_count_);
  EXPECT_EQ(20u, visitor.test_altsvc_ir_.max_age());
  EXPECT_EQ(443u, visitor.test_altsvc_ir_.port());
  EXPECT_EQ("protocolid", visitor.test_altsvc_ir_.protocol_id());
  EXPECT_EQ("hostname", visitor.test_altsvc_ir_.host());
}

// Tests handling of PRIORITY frames.
TEST_P(SpdyFramerTest, ReadPriority) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  const unsigned char kFrameData[] = {
    0x00, 0x05, 0x02, 0x00,  // PRIORITY frame
    0x00, 0x00, 0x00, 0x03,  // stream ID 3
    0x00, 0x00, 0x00, 0x01,  // dependent on stream id 1
    0x00                     // weight 0
  };

  TestSpdyVisitor visitor(spdy_version_);
  visitor.SimulateInFramer(kFrameData, sizeof(kFrameData));

  EXPECT_EQ(SpdyFramer::SPDY_RESET, visitor.framer_.state());
  EXPECT_EQ(SpdyFramer::SPDY_NO_ERROR, visitor.framer_.error_code())
      << SpdyFramer::ErrorCodeToString(visitor.framer_.error_code());
}

// Tests handling of PRIORITY frame with incorrect size.
TEST_P(SpdyFramerTest, ReadIncorrectlySizedPriority) {
  if (spdy_version_ <= SPDY3) {
    return;
  }

  // PRIORITY frame of size 4, which isn't correct.
  const unsigned char kFrameData[] = {
    0x00, 0x04, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x01,
  };

  TestSpdyVisitor visitor(spdy_version_);
  visitor.SimulateInFramer(kFrameData, sizeof(kFrameData));

  EXPECT_EQ(SpdyFramer::SPDY_ERROR, visitor.framer_.state());
  EXPECT_EQ(SpdyFramer::SPDY_INVALID_CONTROL_FRAME,
            visitor.framer_.error_code())
      << SpdyFramer::ErrorCodeToString(visitor.framer_.error_code());
}

TEST_P(SpdyFramerTest, PriorityWeightMapping) {
  if (spdy_version_ <= SPDY3) {
    return;
  }
  SpdyFramer framer(spdy_version_);

  EXPECT_EQ(255u, framer.MapPriorityToWeight(0));
  EXPECT_EQ(219u, framer.MapPriorityToWeight(1));
  EXPECT_EQ(182u, framer.MapPriorityToWeight(2));
  EXPECT_EQ(146u, framer.MapPriorityToWeight(3));
  EXPECT_EQ(109u, framer.MapPriorityToWeight(4));
  EXPECT_EQ(73u, framer.MapPriorityToWeight(5));
  EXPECT_EQ(36u, framer.MapPriorityToWeight(6));
  EXPECT_EQ(0u, framer.MapPriorityToWeight(7));

  EXPECT_EQ(0u, framer.MapWeightToPriority(255));
  EXPECT_EQ(0u, framer.MapWeightToPriority(220));
  EXPECT_EQ(1u, framer.MapWeightToPriority(219));
  EXPECT_EQ(1u, framer.MapWeightToPriority(183));
  EXPECT_EQ(2u, framer.MapWeightToPriority(182));
  EXPECT_EQ(2u, framer.MapWeightToPriority(147));
  EXPECT_EQ(3u, framer.MapWeightToPriority(146));
  EXPECT_EQ(3u, framer.MapWeightToPriority(110));
  EXPECT_EQ(4u, framer.MapWeightToPriority(109));
  EXPECT_EQ(4u, framer.MapWeightToPriority(74));
  EXPECT_EQ(5u, framer.MapWeightToPriority(73));
  EXPECT_EQ(5u, framer.MapWeightToPriority(37));
  EXPECT_EQ(6u, framer.MapWeightToPriority(36));
  EXPECT_EQ(6u, framer.MapWeightToPriority(1));
  EXPECT_EQ(7u, framer.MapWeightToPriority(0));
}

}  // namespace net
